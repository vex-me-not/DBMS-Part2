#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bf.h"
#include "hash_file.h"
#include "sht_file.h"

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return HT_ERROR;        \
    }                         \
  }

typedef struct
{
  int size;
} HashHeader;

typedef struct
{
  int value;
  int block_num;
} HashNode;

typedef struct
{
  HashHeader header;
  HashNode hashNode[MAX_HNODES];
} HashEntry;

tid getTid(int blockId, int index)
{
  tid temp = (blockId + 1) * MAX_RECORDS + index;
  return temp;
}

/*
  prints the contents of the 'array' of size 'size'
*/
void printUpdateArray(UpdateRecordArray *array)
{
  if (array[0].oldTupleId == -1)
    return;
  printf("\n\n");
  for (int i = 0; i < MAX_RECORDS; i++)
    printf("city: %s, surname: %s, oldTid: %i, newTid: %i\n",
           array[i].city, array[i].surname, array[i].oldTupleId, array[i].newTupleId);
}

/*
  prints the contents of a 'record'
*/
void printRecord(Record record)
{
  printf("Entry with id : %i, city : %s, name : %s, and surname : %s\n",
         record.id, record.city, record.name, record.surname);
}

/*
  prints the contents of a 'hashNode'
*/
void printHashNode(HashNode hashNode)
{
  printf("HashNode with value : %i, and block_num : %i\n", hashNode.value, hashNode.block_num);
}

/*
  returns the hash value of an 'id' for a table of global 'depth'
*/
unsigned int hashFunction(int id, int depth)
{

  char *p = (char *)&id;
  unsigned int h = 0x811c9dc5;
  int i;

  for (i = 0; i < sizeof(int); i++)
    h = (h ^ p[i]) * 0x01000193;
  h = h >> (32 - depth);

  return h;
}

HT_ErrorCode HT_Init()
{
  if (MAX_OPEN_FILES <= 0)
  {
    printf("Runner.exe needs at least one file to run. Please ensure that MAX_OPEN_FILES is not 0\n");
    return HT_ERROR;
  }
  for (int i = 0; i < MAX_OPEN_FILES; i++)
  {
    indexArray[i].used = 0;
  }
  return HT_OK;
}

/*
  checks the input for HT_CreateIndex.
*/
HT_ErrorCode checkCreateIndex(const char *filename, int depth)
{
  if (filename == NULL || strcmp(filename, "") == 0)
  {
    printf("Please provide a name for the output file!\n");
    return HT_ERROR;
  }
  if (depth < 0)
  {
    printf("Depth input is wrong! Please give a NON-NEGATIVE number!\n");
    return HT_ERROR;
  }
  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  allocates and stores to the first block of the file with fileDesc 'fd', the global 'depht'
*/
HT_ErrorCode createInfoBlock(int fd, BF_Block *block, int depth)
{
  CALL_BF(BF_AllocateBlock(fd, block));
  char *data = BF_Block_GetData(block);
  memcpy(data, &depth, sizeof(int));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  allocates and stores to the second block of the file with fileDesc 'fd', a HashTable with 2^'depht' values.
*/
HT_ErrorCode createHashTable(int fd, BF_Block *block, int depth)
{
  HashEntry hashEntry;
  int hashN = pow(2.0, (double)depth);
  int blockN;
  char *data;

  // allocate space for the HashTable
  CALL_BF(BF_AllocateBlock(fd, block));
  CALL_BF(BF_UnpinBlock(block));

  // set empty entry header
  Entry empty;
  empty.header.local_depth = depth;
  empty.header.size = 0;

  // Link every hash value an empty data block
  hashEntry.header.size = hashN;
  for (int i = 0; i < hashN; i++)
  {
    hashEntry.hashNode[i].value = i;
    BF_GetBlockCounter(fd, &blockN);
    CALL_BF(BF_AllocateBlock(fd, block));
    data = BF_Block_GetData(block);
    memcpy(data, &empty, sizeof(Entry));
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    hashEntry.hashNode[i].block_num = blockN;
  }

  // Store HashTable
  BF_GetBlock(fd, 1, block);
  data = BF_Block_GetData(block);
  memcpy(data, &hashEntry, sizeof(HashEntry));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

HT_ErrorCode HT_CreateIndex(const char *filename, int depth)
{
  CALL_OR_DIE(checkCreateIndex(filename, depth));
  CALL_BF(BF_CreateFile(filename));

  // initialize block
  BF_Block *block;
  BF_Block_Init(&block);

  // open file
  int id;
  HT_OpenIndex(filename, &id);
  int fd = indexArray[id].fd;
  strcpy(indexArray[id].filename, filename);

  // Create Info block and HashTable
  CALL_OR_DIE(createInfoBlock(fd, block, depth));
  CALL_OR_DIE(createHashTable(fd, block, depth));

  // destroy block
  BF_Block_Destroy(&block);
  HT_CloseFile(id);

  return HT_OK;
}

HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc)
{
  int found = 0; // bool flag.

  // find empty spot
  for (int i = 0; i < MAX_OPEN_FILES; i++)
  {
    if (indexArray[i].used == 0)
    {
      (*indexDesc) = i;
      found = 1;
      break;
    }
  }

  // if table is full return error
  if (found == 0)
  {
    printf("The maximum number of open files has been reached.Please close a file and try again!\n");
    return HT_ERROR;
  }

  int fd;
  CALL_BF(BF_OpenFile(fileName, &fd));
  int pos = (*indexDesc);   // Get position
  indexArray[pos].fd = fd;  // Store fileDesc
  indexArray[pos].used = 1; // Set position to used

  return HT_OK;
}

HT_ErrorCode HT_CloseFile(int indexDesc)
{
  if (indexArray[indexDesc].used == 0)
  {
    printf("Trying to close an already closed file!\n");
    return HT_ERROR;
  }

  int fd = indexArray[indexDesc].fd;
  indexArray[indexDesc].used = 0; // Free up position

  CALL_BF(BF_CloseFile(fd));
  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  stores the global depht from file with fileDesc 'fd', to 'depht' variable.
*/
HT_ErrorCode getDepth(int fd, BF_Block *block, int *depth)
{
  CALL_BF(BF_GetBlock(fd, 0, block));
  char *data = BF_Block_GetData(block);
  memcpy(depth, data, sizeof(int));
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  stores the HashTable from file with fileDesc 'fd', to 'hashEntry' variable.
*/
HT_ErrorCode getHashTable(int fd, BF_Block *block, HashEntry *hashEntry)
{
  CALL_BF(BF_GetBlock(fd, 1, block));
  char *data = BF_Block_GetData(block);
  memcpy(hashEntry, data, sizeof(HashEntry));
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  Returns the block_num of the data block that hash 'value' from HashTable 'hashEntry' points to.
*/
int getBucket(int value, HashEntry hashEntry)
{
  int pos = -1;
  for (pos = 0; pos < hashEntry.header.size; pos++)
    if (hashEntry.hashNode[pos].value == value)
      return hashEntry.hashNode[pos].block_num;
}

/*
  checks the input of HT_InsertEntry
*/
HT_ErrorCode checkInsertEntry(int indexDesc, UpdateRecordArray *updateArray)
{
  if (indexArray[indexDesc].used == 0)
  {
    printf("Trying to insert into a closed file!\n");
    return HT_ERROR;
  }
  if (updateArray == NULL)
  {
    printf("updateArray is NULL\n");
    return HT_ERROR;
  }

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  Stores the Entry from block with block_num 'bucket' at file with fileDesc 'fd', at 'entry' variable.
*/
HT_ErrorCode getEntry(int fd, BF_Block *block, int bucket, Entry *entry)
{
  BF_GetBlock(fd, bucket, block);
  char *data = BF_Block_GetData(block);
  memcpy(entry, data, sizeof(Entry));
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed).
  fd: fileDesc of file we want.
  Updates the value of the global depth at the disk. (calling function must update memory, if need to)
*/
HT_ErrorCode setDepth(int fd, BF_Block *block, int depth)
{
  CALL_BF(BF_GetBlock(fd, 0, block));
  char *data = BF_Block_GetData(block);
  memcpy(data, &depth, sizeof(int));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed).
  fd: fileDesc of file we want.
  Saves the HashTable 'hashEntry' at the disk.
*/
HT_ErrorCode setHashTable(int fd, BF_Block *block, HashEntry *hashEntry)
{
  CALL_BF(BF_GetBlock(fd, 1, block));
  char *data = BF_Block_GetData(block);
  memcpy(data, hashEntry, sizeof(HashEntry));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  return HT_OK;
}

/*
  'hashEntry' : hash table.
  'bucket' : block we are interested in.
  'depth' : global depth of the hash table.
  'local_depth' : local depth of bucket.
   Stores at first and end the first and last index of the hash table, that points to the bucket. Stores at half the medium of [first, last].
*/
HT_ErrorCode getEndPoints(int *first, int *half, int *end, int local_depth, int depth, int bucket, HashEntry hashEntry)
{
  // int local_depth = entry.header.local_depth;
  int dif = depth - local_depth;
  int numOfHashes = pow(2.0, (double)dif);
  for (int pos = 0; pos < hashEntry.header.size; pos++)
    if (hashEntry.hashNode[pos].block_num == bucket)
    {
      *first = pos;
      break;
    }

  *half = (*first) + numOfHashes / 2 - 1;
  *end = (*first) + numOfHashes - 1;
  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed).
  fd: fileDesc of file we want.
  Allocates a new block at the end of the file, and stores it's 'block_num'.
*/
HT_ErrorCode getNewBlock(int fd, BF_Block *block, int *block_num)
{
  BF_GetBlockCounter(fd, block_num);
  CALL_BF(BF_AllocateBlock(fd, block));
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  Reassigns records from one block to two. Used when need to split. !It doesn't split, it reassigns!
  The two new blocks consist of the old block and a new one that has been allocated.
  block: previously initialized BF_Block pointer (does not get destroyed).
  fd: fileDesc of file we want.
  entry: the Entry that was in the block we are reassigning from.
  old: address of the entry that will remain in the old block.
  new: address of the entry that will be in the new block.
  blockOld: block_num of old block.
  blockNew: block_num of new block.
  depth: global depth.
  half: medium of [first, end]. first is the first index of the hash table that points to the old block and end is the last.
  updateArray: the array we stores record updates.
*/

int getBlockNumFromTID(tid td)
{

  return (td / MAX_RECORDS) - 1;
}

int getIndexFromTID(tid td)
{

  return td % MAX_RECORDS;
}

HT_ErrorCode reassignRecords(int fd, BF_Block *block, Entry entry, int blockOld, int blockNew, int half, int depth, UpdateRecordArray *updateArray, Entry *old, Entry *new)
{
  for (int i = 0; i < entry.header.size; i++)
  {
    if (hashFunction(entry.record[i].id, depth) <= half)
    {
      // reassign to new position in old block
      old->record[old->header.size] = entry.record[i];

      // update array
      if (i < MAX_RECORDS)
      {
        strcpy(updateArray[i].city, entry.record[i].city);
        strcpy(updateArray[i].surname, entry.record[i].surname);
        updateArray[i].oldTupleId = getTid(blockOld, i);
        updateArray[i].newTupleId = getTid(blockOld, old->header.size);
      }
      old->header.size++;
    }
    else
    {
      // assign to new block
      new->record[new->header.size] = entry.record[i];

      // update array
      if (i < MAX_RECORDS)
      {
        strcpy(updateArray[i].city, entry.record[i].city);
        strcpy(updateArray[i].surname, entry.record[i].surname);
        updateArray[i].oldTupleId = getTid(blockOld, i);
        updateArray[i].newTupleId = getTid(blockNew, new->header.size);
      }
      new->header.size++;
    }
  }

  return HT_OK;
}

/*
  Inserts a record after the block it should go, was splitted.
  The two new blocks (after spliting) consist of the old block and a new one that has been allocated.
  record: the record we're inserting.
  depth: the global depth.
  half: medium of [first, end]. first is the first index of the hash table that points to the old block and end is the last.
  tupleId: the tupleId of the record after insertion.
  old: address of the entry that will remain in the old block.
  new: address of the entry that will be in the new block.
  blockOld: block_num of old block.
  blockNew: block_num of new block.
*/
HT_ErrorCode insertRecordAfterSplit(Record record, int depth, int half, tid *tupleId, int blockOld, int blockNew, Entry *old, Entry *new)
{
  // store given record
  if (hashFunction(record.id, depth) <= half)
  {
    old->record[old->header.size] = record;
    *tupleId = getTid(blockOld, old->header.size);
    old->header.size++;
  }
  else
  {
    new->record[new->header.size] = record;
    *tupleId = getTid(blockNew, new->header.size);
    new->header.size++;
  }

  return HT_OK;
}

/*
  Stores given Entry at a block.
  fd: fileDesc of file we are interested in.
  block: previously initialized BF_Block pointer (does not get destroyed).
  dest_block_num: block_num of the block we are storing Entry at.
  entry: the Entry.
*/
HT_ErrorCode setEntry(int fd, BF_Block *block, int dest_block_num, Entry *entry)
{
  CALL_BF(BF_GetBlock(fd, dest_block_num, block));
  char *data = BF_Block_GetData(block);
  memcpy(data, entry, sizeof(Entry));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed).
  fd: fileDesc of file we want.
  Doubles the HashTable 'hashEntry', updates memory and disk data at the end.
*/
HT_ErrorCode doubleHashTable(int fd, BF_Block *block, HashEntry *hashEntry)
{
  // double table
  HashEntry new = (*hashEntry);
  new.header.size = (*hashEntry).header.size * 2;
  for (unsigned int i = 0; i < new.header.size; i++)
  {
    new.hashNode[i].block_num = (*hashEntry).hashNode[i >> 1].block_num;
    new.hashNode[i].value = i;
  }

  // update changes in disk and memory
  CALL_OR_DIE(setHashTable(fd, block, &new));
  (*hashEntry) = new;
  return HT_OK;
}

/*
  Splits a HashTable's block, reassigns records, and stores updated data.
  fd: fileDesc of file we are interested in.
  block: previously initialized BF_Block pointer (does not get destroyed).
  depth: global depth.
  bucket: the block_num of the block we are spliting.
  record: the record that when added caused the spliting. Inserted at the end.
  tupleId: the tupleId of the record after it is inserted.
  updateArray: the array we are storing records' updates.
  entry: the Entry of the block before it splitted.
*/
HT_ErrorCode splitHashTable(int fd, BF_Block *block, int depth, int bucket, Record record, tid *tupleId, UpdateRecordArray *updateArray, Entry entry)
{
  // get HashTable
  HashEntry hashEntry;
  CALL_OR_DIE(getHashTable(fd, block, &hashEntry));

  // get end points
  int local_depth = entry.header.local_depth;
  int first, half, end;
  CALL_OR_DIE(getEndPoints(&first, &half, &end, local_depth, depth, bucket, hashEntry));

  // get a new block
  int blockNew;
  CALL_OR_DIE(getNewBlock(fd, block, &blockNew));

  Entry old, new;
  new.header.local_depth = local_depth + 1;
  new.header.size = 0;

  old = entry;
  old.header.local_depth++;
  old.header.size = 0;

  for (int i = half + 1; i <= end; i++)
    hashEntry.hashNode[i].block_num = blockNew;

  // update HashTable and re-assing records
  CALL_OR_DIE(setHashTable(fd, block, &hashEntry));
  CALL_OR_DIE(reassignRecords(fd, block, entry, bucket, blockNew, half, depth, updateArray, &old, &new));

  // insert new record (after splitting)
  CALL_OR_DIE(insertRecordAfterSplit(record, depth, half, tupleId, bucket, blockNew, &old, &new));

  // store created/modified entries
  CALL_OR_DIE(setEntry(fd, block, bucket, &old));
  CALL_OR_DIE(setEntry(fd, block, blockNew, &new));

  return HT_OK;
}

HT_ErrorCode HT_InsertEntry(int indexDesc, Record record, tid *tupleId, UpdateRecordArray *updateArray)
{
  for (int i = 0; i < MAX_RECORDS; i++)
  {
    updateArray[i].oldTupleId = -1;
    updateArray[i].newTupleId = updateArray[i].oldTupleId;
    strcpy(updateArray[i].city, "DUMBVILLE");
    strcpy(updateArray[i].surname, "DUMMY");
  }

  CALL_OR_DIE(checkInsertEntry(indexDesc, updateArray));

  // Initialize block
  BF_Block *block;
  BF_Block_Init(&block);

  // get depth
  int depth;
  int fd = indexArray[indexDesc].fd;
  CALL_OR_DIE(getDepth(fd, block, &depth));

  // get HashTable
  HashEntry hashEntry;
  CALL_OR_DIE(getHashTable(fd, block, &hashEntry));

  // get bucket
  int value = hashFunction(record.id, depth);
  int blockN = getBucket(value, hashEntry);

  // get bucket's entry
  Entry entry;
  CALL_OR_DIE(getEntry(fd, block, blockN, &entry));

  char *data;

  // number of Records in the block
  int size = entry.header.size;
  int local_depth = entry.header.local_depth;

  // check for available space
  if (size >= MAX_RECORDS)
  {
    // check local depth
    if (local_depth == depth)
    {
      // double HashTable
      CALL_OR_DIE(doubleHashTable(fd, block, &hashEntry));
      depth++;
      CALL_OR_DIE(setDepth(fd, block, depth));
    }
    // spit hashTable's pointers
    CALL_OR_DIE(splitHashTable(fd, block, depth, blockN, record, tupleId, updateArray, entry));
    return HT_OK;
  }

  // space available
  CALL_OR_DIE(getEntry(fd, block, blockN, &entry));

  // insert new record (whithout splitting)
  entry.record[entry.header.size] = record;
  *tupleId = getTid(blockN, entry.header.size);
  (entry.header.size)++;

  CALL_OR_DIE(setEntry(fd, block, blockN, &entry));
  return HT_OK;
}

/*
  checks the input of HT_PrintAllEntries
*/
HT_ErrorCode checkPrintAllEntries(int indexDesc)
{
  if (indexArray[indexDesc].used == 0)
  {
    printf("Can't print from a closed file!\n");
    return HT_ERROR;
  }

  return HT_OK;
}

/*
  Prints all records in a file.
  fd: the fileDesc of the file.
  block: previously initialized BF_Block pointer (does not get destroyed).
  depth: the global depth.
  hashEntry: the hash table.
*/
HT_ErrorCode printAllRecords(int fd, BF_Block *block, int depth, HashEntry hashEntry)
{
  for (int i = 0; i < hashEntry.header.size; i++)
  {
    int blockN = hashEntry.hashNode[i].block_num;
    printf("Records with hash value %i (block_num = %i)\n", i, blockN);

    // print all records
    Entry entry;
    CALL_OR_DIE(getEntry(fd, block, blockN, &entry));
    for (int i = 0; i < entry.header.size; i++)
      printRecord(entry.record[i]);

    // skip hash values that point to the same block
    int dif = depth - entry.header.local_depth;
    i += pow(2.0, (double)dif) - 1;
  }

  return HT_OK;
}

/*
  Prints record in a file with specific 'id'.
  fd: the fileDesc of the file.
  block: previously initialized BF_Block pointer (does not get destroyed).
  depth: the global depth.
  hashEntry: the hash table.
*/
HT_ErrorCode printSepcificRecord(int fd, BF_Block *block, int id, int depth, HashEntry hashEntry)
{
  int value = hashFunction(id, depth);
  int blockN = getBucket(value, hashEntry);

  // check if block was allocated
  if (blockN == 0)
  {
    printf("Block was not allocated\n");
    return HT_ERROR;
  }

  // print record with that id
  Entry entry;
  CALL_OR_DIE(getEntry(fd, block, blockN, &entry));
  for (int i = 0; i < entry.header.size; i++)
    if (entry.record[i].id == id)
      printRecord(entry.record[i]);

  return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id)
{
  BF_Block *block;
  BF_Block_Init(&block);

  CALL_OR_DIE(checkPrintAllEntries(indexDesc));
  int fd = indexArray[indexDesc].fd;

  // get depth
  int depth;
  CALL_OR_DIE(getDepth(fd, block, &depth));

  // get HashTable
  HashEntry hashEntry;
  CALL_OR_DIE(getHashTable(fd, block, &hashEntry));

  HT_ErrorCode htCode;
  if (id == NULL)
    htCode = printAllRecords(fd, block, depth, hashEntry);
  else
    htCode = printSepcificRecord(fd, block, (*id), depth, hashEntry);

  BF_Block_Destroy(&block);
  return htCode;
}

HT_ErrorCode HashStatistics(char *filename)
{
  BF_Block *block;
  BF_Block_Init(&block);

  int id;
  HT_OpenIndex(filename, &id);
  int fd = indexArray[id].fd;

  // get number of blocks
  int nblocks;
  CALL_BF(BF_GetBlockCounter(fd, &nblocks));
  printf("File %s has %d blocks.\n", filename, nblocks);

  // get depth
  int depth;
  CALL_OR_DIE(getDepth(fd, block, &depth));

  // get hash table
  HashEntry hashEntry;
  CALL_OR_DIE(getHashTable(fd, block, &hashEntry));

  int iter = hashEntry.header.size;
  int dataN = iter;
  int min, max, total;
  max = total = 0;
  min = -1;
  for (int i = 0; i < iter; i++)
  {
    int blockN = hashEntry.hashNode[i].block_num;
    Entry entry;
    CALL_OR_DIE(getEntry(fd, block, blockN, &entry));

    int num = entry.header.size;
    total += num;
    if (num > max)
      max = num;
    if (num < min || min == -1)
      min = num;

    int dif = depth - entry.header.local_depth;
    i += pow(2.0, (double)dif) - 1;
    dataN -= pow(2.0, (double)dif) - 1;
  }

  printf("Max number of records in bucket is %i\n", max);
  printf("Min number of records in bucket is %i\n", min);
  printf("Mean number of records in bucket is %f\n", (double)total / (double)dataN);

  BF_Block_Destroy(&block);
  HT_CloseFile(id);
  return HT_OK;
}