#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bf.h"
#include "sht_file.h"
#include "hash_file.h"

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return HT_ERROR;        \
    }                         \
  }

#ifndef HASH_FILE_H
#define HASH_FILE_H

typedef enum HT_ErrorCode
{
  HT_OK,
  HT_ERROR
} HT_ErrorCode;

typedef struct Record
{
  int id;
  char name[15];
  char surname[20];
  char city[20];
} Record;

typedef struct
{
  char index_key[20];
  int tupleId; /*Ακέραιος που προσδιορίζει το block και τη θέση μέσα στο block στην οποία     έγινε η εισαγωγή της εγγραφής στο πρωτεύον ευρετήριο.*/
} SecondaryRecord;

#endif // HASH_FILE_H

typedef struct
{
  int fd;
  int used;
  char primary_name[255];
} SecIndexNode;

typedef struct
{
  SecHeader secHeader;
  SecondaryRecord secRecord[SEC_MAX_RECORDS];
} SecEntry;

typedef struct
{
  int size;
  int next_hblock;    // το επόμενο block στο ευρετήριο
  char attribute[20]; // ο τύπος τιμών που κάνουμε hash (city or surname)
} SecHashHeader;

typedef struct
{
  int h_value;
  int block_num;
} SecHashNode;

typedef struct
{
  SecHashHeader secHeader;
  SecHashNode secHashNode[SEC_MAX_NODES];
} SecHashEntry;

SecIndexNode secIndexArray[MAX_OPEN_FILES]; // πινακας μεα τα ανοικτα αρχεια δευτερευοντος ευρετηριου

unsigned int hashAttr(const char *str, int depth)
{
  unsigned int hash = 5381;
  int c;

  while (c = *str++)
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  hash = hash >> (32 - depth);

  return hash;
}

HT_ErrorCode SHT_Init()
{
  if (MAX_OPEN_FILES <= 0)
  {
    printf("Runner.exe needs at least one file to run. Please ensure that MAX_OPEN_FILES is not 0\n");
    return HT_ERROR;
  }

  for (int i = 0; i < MAX_OPEN_FILES; i++)
    secIndexArray[i].used = 0;

  return HT_OK;
}

void printSecRecord(SecondaryRecord record)
{
  printf("index_key = %s, tupleId = %i \n", record.index_key, record.tupleId);
}

/*
  checks the input for SHT_CreateSecondaryIndex
*/
HT_ErrorCode checkShtCreate(const char *sfileName, char *attrName, int attrLength, int depth, char *fileName)
{
  if (sfileName == NULL || strcmp(sfileName, "") == 0)
  {
    printf("Please provide a name for the output file!\n");
    return HT_ERROR;
  }

  if (depth < 0)
  {
    printf("Depth input is wrong! Please give a NON-NEGATIVE number!\n");
    return HT_ERROR;
  }

  if (fileName == NULL || strcmp(fileName, "") == 0)
  {
    printf("Please provide a name for the primary file!\n");
    return HT_ERROR;
  }

  if (attrName == NULL || strcmp(attrName, "") == 0)
  {
    printf("Please provide a name for the attribute!\n");
    return HT_ERROR;
  }

  if (attrLength < 1)
  {
    printf("Please provide a valid length for the attribute name!\n");
    return HT_ERROR;
  }

  return HT_OK;
}

/*
  Checks if the primary index exists.
  fileName: the name of the primary index file.
*/
HT_ErrorCode primaryExists(char *fileName)
{
  for (int i = 0; i < MAX_OPEN_FILES; i++)
    if (strcmp(indexArray[i].filename, fileName) == 0)
      return HT_OK;

  return HT_ERROR;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  allocates and stores to the first block of the file with fileDesc 'fd', the global 'depht'
*/
HT_ErrorCode createSecInfoBlock(int sfd, BF_Block *block, int depth)
{
  CALL_BF(BF_AllocateBlock(sfd, block));
  char *data = BF_Block_GetData(block);
  memcpy(data, &depth, sizeof(int));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  allocates and stores to the second block of the file with fileDesc 'sfd', a HashTable with 2^'depht' values.
*/
HT_ErrorCode createSecHashTable(int sfd, BF_Block *block, int depth, char *attrName)
{
  SecHashEntry secHashEntry;
  int hashN = pow(2.0, (double)depth);
  secHashEntry.secHeader.next_hblock = -1;
  strcpy(secHashEntry.secHeader.attribute, attrName);
  int blockN;
  char *data;

  // allocate space for the HashTable
  CALL_BF(BF_AllocateBlock(sfd, block));
  CALL_BF(BF_UnpinBlock(block));

  // set empty entry header
  SecEntry empty;
  empty.secHeader.local_depth = depth;
  empty.secHeader.size = 0;

  // Link every hash value an empty data block
  secHashEntry.secHeader.size = hashN;
  for (int i = 0; i < hashN; i++)
  {
    secHashEntry.secHashNode[i].h_value = i;
    BF_GetBlockCounter(sfd, &blockN);
    CALL_BF(BF_AllocateBlock(sfd, block));
    data = BF_Block_GetData(block);
    memcpy(data, &empty, sizeof(SecEntry));
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    secHashEntry.secHashNode[i].block_num = blockN;
  }

  // Store HashTable
  BF_GetBlock(sfd, 1, block);
  data = BF_Block_GetData(block);
  memcpy(data, &secHashEntry, sizeof(SecHashEntry));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

HT_ErrorCode SHT_CreateSecondaryIndex(const char *sfileName, char *attrName, int attrLength, int depth, char *fileName)
{
  CALL_OR_DIE(checkShtCreate(sfileName, attrName, attrLength, depth, fileName));
  CALL_OR_DIE(primaryExists(fileName));

  CALL_BF(BF_CreateFile(sfileName));

  BF_Block *block;
  BF_Block_Init(&block);

  int id;
  SHT_OpenSecondaryIndex(sfileName, &id);
  int sfd = secIndexArray[id].fd;
  memcpy(secIndexArray[id].primary_name, fileName, strlen(fileName));

  // create info block and sec hash table
  CALL_OR_DIE(createSecInfoBlock(sfd, block, depth));
  CALL_OR_DIE(createSecHashTable(sfd, block, depth, attrName));

  BF_Block_Destroy(&block);
  SHT_CloseSecondaryIndex(id);
  return HT_OK;
}

HT_ErrorCode SHT_OpenSecondaryIndex(const char *sfileName, int *indexDesc)
{
  // find empty spot
  int found = 0;
  for (int i = 0; i < MAX_OPEN_FILES; i++)
    if (secIndexArray[i].used == 0)
    {
      (*indexDesc) = i;
      found = 1;
      break;
    }

  // if table is full return error
  if (found == 0)
  {
    printf("The maximum number of open files has been reached.Please close a file and try again!\n");
    return HT_ERROR;
  }

  int fd;
  CALL_BF(BF_OpenFile(sfileName, &fd));
  int pos = (*indexDesc);      // Return position
  secIndexArray[pos].fd = fd;  // Save fileDesc
  secIndexArray[pos].used = 1; // Set position to used

  return HT_OK;
}

HT_ErrorCode SHT_CloseSecondaryIndex(int indexDesc)
{
  if (secIndexArray[indexDesc].used == 0)
  {
    printf("Trying to close an already closed file!\n");
    return HT_ERROR;
  }

  int fd = secIndexArray[indexDesc].fd;
  secIndexArray[indexDesc].used = 0;
  CALL_BF(BF_CloseFile(fd));

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  stores the HashTable from file with fileDesc 'fd', at 'block_num' to 'hashEntry' variable.
*/
HT_ErrorCode getSecHashTable(int fd, BF_Block *block, int block_num, SecHashEntry *hashEntry)
{
  CALL_BF(BF_GetBlock(fd, block_num, block));
  char *data = BF_Block_GetData(block);
  memcpy(hashEntry, data, sizeof(SecHashEntry));
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  Returns the block_num of the data block that hash 'value' from HashTable 'hashEntry' points to.
*/
int getSecBucket(int value, SecHashEntry hashEntry)
{
  int pos = -1;
  for (pos = 0; pos < hashEntry.secHeader.size; pos++)
    if (hashEntry.secHashNode[pos].h_value == value)
      return hashEntry.secHashNode[pos].block_num;
}

/*
  checks the input of HT_InsertEntry
*/
HT_ErrorCode checkSecInsertEntry(int indexDesc, SecondaryRecord record)
{
  if (secIndexArray[indexDesc].used == 0)
  {
    printf("Trying to insert into a closed file!\n");
    return HT_ERROR;
  }

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed)
  Stores the Entry from block with block_num 'bucket' at file with fileDesc 'fd', at 'entry' variable.
*/
HT_ErrorCode getSecEntry(int fd, BF_Block *block, int bucket, SecEntry *entry)
{
  BF_GetBlock(fd, bucket, block);
  char *data = BF_Block_GetData(block);
  memcpy(entry, data, sizeof(SecEntry));
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed).
  fd: fileDesc of file we want.
  Saves the HashTable 'hashEntry' at the disk at 'block_num'.
*/
HT_ErrorCode setSecHashTable(int fd, BF_Block *block, int block_num, SecHashEntry *hashEntry)
{
  CALL_BF(BF_GetBlock(fd, block_num, block));
  char *data = BF_Block_GetData(block);
  memcpy(data, hashEntry, sizeof(SecHashEntry));
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
HT_ErrorCode getSecEndPoints(int *first, int *half, int *end, int local_depth, int depth, int bucket, SecHashEntry hashEntry)
{
  int dif = depth - local_depth;
  int numOfHashes = pow(2.0, (double)dif);
  for (int pos = 0; pos < hashEntry.secHeader.size; pos++)
    if (hashEntry.secHashNode[pos].block_num == bucket)
    {
      *first = pos;
      break;
    }

  *half = (*first) + numOfHashes / 2 - 1;
  *end = (*first) + numOfHashes - 1;
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
HT_ErrorCode reassignSecRecords(int fd, BF_Block *block, SecEntry entry, int blockOld, int blockNew, int half, int depth, SecEntry *old, SecEntry *new)
{
  for (int i = 0; i < entry.secHeader.size; i++)
  {
    if (hashAttr(entry.secRecord[i].index_key, depth) <= half)
    {
      // reassign to new position in old block
      old->secRecord[old->secHeader.size] = entry.secRecord[i];
      old->secHeader.size++;
    }
    else
    {
      // assign to new block
      new->secRecord[new->secHeader.size] = entry.secRecord[i];
      new->secHeader.size++;
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
HT_ErrorCode insertSecRecordAfterSplit(SecondaryRecord secondaryRecord, int depth, int half, int blockOld, int blockNew, SecEntry *old, SecEntry *new)
{
  // store given record
  if (hashAttr(secondaryRecord.index_key, depth) <= half)
  {
    old->secRecord[old->secHeader.size] = secondaryRecord;

    old->secHeader.size++;
  }
  else
  {
    new->secRecord[new->secHeader.size] = secondaryRecord;

    new->secHeader.size++;
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
HT_ErrorCode setSecEntry(int fd, BF_Block *block, int dest_block_num, SecEntry *entry)
{
  CALL_BF(BF_GetBlock(fd, dest_block_num, block));
  char *data = BF_Block_GetData(block);
  memcpy(data, entry, sizeof(SecEntry));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

/*
  block: previously initialized BF_Block pointer (does not get destroyed).
  fd: fileDesc of file we want.
  Doubles the HashTable 'hashEntry', updates memory and disk data at the end.
*/
HT_ErrorCode doubleSecHashTable(int fd, BF_Block *block, SecHashEntry *hashEntry)
{
  // double table
  SecHashEntry new = (*hashEntry);
  new.secHeader.size = (*hashEntry).secHeader.size * 2;
  for (unsigned int i = 0; i < new.secHeader.size; i++)
  {
    new.secHashNode[i].block_num = (*hashEntry).secHashNode[i >> 1].block_num;
    new.secHashNode[i].h_value = i;
  }

  // update changes in disk and memory
  CALL_OR_DIE(setSecHashTable(fd, block, 1, &new));
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
HT_ErrorCode splitSecHashTable(int fd, BF_Block *block, int depth, int bucket, SecondaryRecord record, SecEntry entry)
{
  // get HashTable
  SecHashEntry hashEntry;
  CALL_OR_DIE(getSecHashTable(fd, block, 1, &hashEntry));

  // get end points
  int local_depth = entry.secHeader.local_depth;
  int first, half, end;
  CALL_OR_DIE(getSecEndPoints(&first, &half, &end, local_depth, depth, bucket, hashEntry));

  // get a new block
  int blockNew;
  CALL_OR_DIE(getNewBlock(fd, block, &blockNew));

  SecEntry old, new;
  new.secHeader.local_depth = local_depth + 1;
  new.secHeader.size = 0;

  old = entry;
  old.secHeader.local_depth++;
  old.secHeader.size = 0;

  for (int i = half + 1; i <= end; i++)
    hashEntry.secHashNode[i].block_num = blockNew;

  // update HashTable and re-assing records
  CALL_OR_DIE(setSecHashTable(fd, block, 1, &hashEntry));
  CALL_OR_DIE(reassignSecRecords(fd, block, entry, bucket, blockNew, half, depth, &old, &new));

  if (new.secHeader.size == 0)
  {
    printf("Reassign Records Error! No secRecords assigned to new\n");
    CALL_OR_DIE(setSecEntry(fd, block, bucket, &old));
    CALL_OR_DIE(setSecEntry(fd, block, blockNew, &new));
    return 2;
  }

  if (old.secHeader.size == 0)
  {
    printf("Reassign Records Error! No secRecords assigned to old\n");
    CALL_OR_DIE(setSecEntry(fd, block, bucket, &old));
    CALL_OR_DIE(setSecEntry(fd, block, blockNew, &new));
    return 2;
  }

  // insert new record (after splitting)
  CALL_OR_DIE(insertSecRecordAfterSplit(record, depth, half, bucket, blockNew, &old, &new));

  // store created/modified entries
  CALL_OR_DIE(setSecEntry(fd, block, bucket, &old));
  CALL_OR_DIE(setSecEntry(fd, block, blockNew, &new));

  return HT_OK;
}

HT_ErrorCode SHT_SecondaryInsertEntry(int indexDesc, SecondaryRecord record)
{
  // insert code here
  // printSecRecord(record);
  CALL_OR_DIE(checkSecInsertEntry(indexDesc, record));

  // Initialize block
  BF_Block *block;
  BF_Block_Init(&block);

  // get depth
  int depth;
  int fd = secIndexArray[indexDesc].fd;
  CALL_OR_DIE(getDepth(fd, block, &depth));

  // get HashTable
  SecHashEntry hashEntry;
  CALL_OR_DIE(getSecHashTable(fd, block, 1, &hashEntry));

  // get bucket
  unsigned int value = hashAttr(record.index_key, depth);
  int blockN = getSecBucket(value, hashEntry);

  // get bucket's entry
  SecEntry entry;
  CALL_OR_DIE(getSecEntry(fd, block, blockN, &entry));

  char *data;

  // number of Records in the block
  int size = entry.secHeader.size;
  int local_depth = entry.secHeader.local_depth;

  // check for available space
  if (size >= SEC_MAX_RECORDS)
  {
    // check local depth
    if (local_depth == depth)
    {
      // double HashTable
      CALL_OR_DIE(doubleSecHashTable(fd, block, &hashEntry));
      depth++;
      CALL_OR_DIE(setDepth(fd, block, depth));
    }
    // spit hashTable's pointers
    int res = splitSecHashTable(fd, block, depth, blockN, record, entry);
    if (res == 2)
      return SHT_SecondaryInsertEntry(indexDesc, record);
    return HT_OK;
  }

  // space available
  CALL_OR_DIE(getSecEntry(fd, block, blockN, &entry));

  // insert new record (whithout splitting)
  entry.secRecord[entry.secHeader.size] = record;

  (entry.secHeader.size)++;

  CALL_OR_DIE(setSecEntry(fd, block, blockN, &entry));
  return HT_OK;
}

HT_ErrorCode SHT_SecondaryUpdateEntry(int indexDesc, UpdateRecordArray *updateArray)
{
  if (indexArray[indexDesc].used == 0)
  {
    printf("CLOSED FILE\n");
    return HT_ERROR;
  }

  if (updateArray[0].oldTupleId == -1)
  {
    return HT_OK;
  }

  // insert code here
  BF_Block *block;
  BF_Block_Init(&block);

  for (int i = 0; i < MAX_RECORDS; i++)
  {

    if (updateArray[i].oldTupleId == updateArray[i].newTupleId)
    {
      continue;
    }

    int depth;
    int fd = secIndexArray[indexDesc].fd;
    CALL_OR_DIE(getDepth(fd, block, &depth));

    // get HashTable
    SecHashEntry hashEntry;
    CALL_OR_DIE(getSecHashTable(fd, block, 1, &hashEntry));

    // get bucket
    int value = hashAttr(updateArray[i].surname, depth); /// Otan arxisoyme na kanoyme hash me vasi to surname/city prepei na to allaksoyme

    if ((strcmp(hashEntry.secHeader.attribute, "cities") == 0) || (strcmp(hashEntry.secHeader.attribute, "city") == 0))
    {
      value = hashAttr(updateArray[i].city, depth);
    }

    int blockN = getSecBucket(value, hashEntry);

    // get bucket's entry
    SecEntry entry;
    CALL_OR_DIE(getSecEntry(fd, block, blockN, &entry));

    for (int j = 0; j < entry.secHeader.size; j++)
    {
      if (entry.secRecord[j].tupleId == updateArray[i].oldTupleId)
      {
        entry.secRecord[j].tupleId = updateArray[i].newTupleId;
      }
    }

    CALL_OR_DIE(setSecEntry(fd, block, blockN, &entry));
  }

  BF_Block_Destroy(&block);
  return HT_OK;
}

/*
  prints SecHashNodes values
*/
void SHT_PrintHashNode(SecHashNode node)
{
  printf("SecHashNode: block_num = %i, h_value=%i\n", node.block_num, node.h_value);
}

/*
  prints the contents of a SecondaryRecord
*/
void SHT_PrintSecondaryRecord(SecondaryRecord record)
{
  printf("index_key: %s, tupleId: %i\n", record.index_key, record.tupleId);
}

/*
  Prints the contents of a SecEntry
*/
void SHT_PrintSecEntry(SecEntry entry)
{
  printf("local_depth = %i\n", entry.secHeader.local_depth);
  for (int i = 0; i < entry.secHeader.size; i++)
    SHT_PrintSecondaryRecord(entry.secRecord[i]);
}

/*
  Prints SecHashEntry's SecHashNodes.
  If full is given as 1, it prints the block each SecHashNode points to, also
  fd: file's fileDesc
  block: previously initialized BF_Block stracture (must be destroyed by calling function)
*/
void SHT_PrintSecHashEntry(SecHashEntry hEntry, int full, int fd, BF_Block *block)
{
  SecEntry entry;

  for (int i = 0; i < hEntry.secHeader.size; i++)
  {
    if (full)
      printf("\n");
    SHT_PrintHashNode(hEntry.secHashNode[i]);
    if (full == 1)
    {
      int bn = hEntry.secHashNode[i].block_num;
      printf("Secondary Entry with block_num = %i\n", bn);
      CALL_OR_DIE(getSecEntry(fd, block, bn, &entry));
      SHT_PrintSecEntry(entry);
    }
  }
}

/*
  Prints for every SecHashEntry in the Hash Table, it's SecHashNodes.
  If full is given as 1, it prints the block each SecHashNode points to, also
  fd: file's fileDesc
  block: previously initialized BF_Block stracture (must be destroyed by calling function)
*/
void SHT_PrintSecHashTable(int fd, BF_Block *block, int full)
{
  SecHashEntry table;
  int block_num;

  block_num = 1; // get start of hash table first

  do
  {
    CALL_OR_DIE(getSecHashTable(fd, block, block_num, &table));
    block_num = table.secHeader.next_hblock;

    SHT_PrintSecHashEntry(table, full, fd, block);
  } while (block_num != -1);
}

/*
  Prints all records in a file.
  fd: the fileDesc of the file.
  block: previously initialized BF_Block pointer (does not get destroyed).
  depth: the global depth.
  hashEntry: the hash table.
*/
HT_ErrorCode printAllSecRecords(int fd, BF_Block *block, int depth, SecHashEntry hashEntry)
{
  SHT_PrintSecHashTable(fd, block, 1);

  return HT_OK;
}

HT_ErrorCode printSecSepcificRecord(int fd, BF_Block *block, char *id, int depth, SecHashEntry hashEntry)
{
  int value = hashAttr(id, depth);
  int blockN = getSecBucket(value, hashEntry);

  // check if block was allocated
  if (blockN == 0)
  {
    printf("Block was not allocated\n");
    return HT_ERROR;
  }

  // print record with that id
  SecEntry entry;
  CALL_OR_DIE(getSecEntry(fd, block, blockN, &entry));
  for (int i = 0; i < entry.secHeader.size; i++)
    if (strcmp(entry.secRecord[i].index_key, id) == 0)
      printSecRecord(entry.secRecord[i]);

  return HT_OK;
}

HT_ErrorCode SHT_PrintAllEntries(int sindexDesc, char *index_key)
{
  BF_Block *block;
  BF_Block_Init(&block);

  int fd = secIndexArray[sindexDesc].fd;

  // get depth
  int depth;
  CALL_OR_DIE(getDepth(fd, block, &depth));

  // get HashTable
  SecHashEntry hashEntry;
  CALL_OR_DIE(getSecHashTable(fd, block, 1, &hashEntry));

  HT_ErrorCode htCode;
  if (index_key == NULL)
    htCode = printAllSecRecords(fd, block, depth, hashEntry);
  else
    htCode = printSecSepcificRecord(fd, block, index_key, depth, hashEntry);

  BF_Block_Destroy(&block);
  return htCode;
}

HT_ErrorCode SHT_HashStatistics(char *filename)
{
  // insert code here
  BF_Block *block;
  BF_Block_Init(&block);

  int id;
  SHT_OpenSecondaryIndex(filename, &id);
  int fd = secIndexArray[id].fd;

  // get number of blocks
  int nblocks;
  CALL_BF(BF_GetBlockCounter(fd, &nblocks));
  printf("File %s has %d blocks.\n", filename, nblocks);

  // // get depth
  int depth;
  CALL_OR_DIE(getDepth(fd, block, &depth));

  // // get hash table
  SecHashEntry hashEntry;
  CALL_OR_DIE(getSecHashTable(fd, block, 1, &hashEntry));

  int iter = hashEntry.secHeader.size;
  int dataN = iter;
  int min, max, total;
  max = total = 0;
  min = -1;

  int sum = 0;
  for (int i = 0; i < iter; i++)
  {
    sum++;
    if (sum - 1 < i)
      sum = i + 1;
    if (sum == iter)
      break;
    int blockN = hashEntry.secHashNode[i].block_num;
    SecEntry entry;
    CALL_OR_DIE(getSecEntry(fd, block, blockN, &entry));

    int num = entry.secHeader.size;
    total += num;
    if (num > max)
      max = num;
    if (num < min || min == -1)
      min = num;

    int dif = depth - entry.secHeader.local_depth;
    i += pow(2.0, (double)dif) - 1;
    dataN -= pow(2.0, (double)dif) - 1;
  }

  printf("Max number of records in bucket is %i\n", max);
  printf("Min number of records in bucket is %i\n", min);
  printf("Mean number of records in bucket is %f\n", (double)total / (double)dataN);

  BF_Block_Destroy(&block);
  SHT_CloseSecondaryIndex(id);

  return HT_OK;
}

HT_ErrorCode SHT_InnerJoin(int sindexDesc1, int sindexDesc2, char *index_key)
{

  // initialize blocks
  BF_Block *block1;
  BF_Block_Init(&block1);

  BF_Block *block2;
  BF_Block_Init(&block2);

  // get secondary indexes
  int fd1 = secIndexArray[sindexDesc1].fd;
  int depth1;
  CALL_OR_DIE(getDepth(fd1, block1, &depth1));
  SecHashEntry hashEntry1;
  CALL_OR_DIE(getSecHashTable(fd1, block1, 1, &hashEntry1));

  int fd2 = secIndexArray[sindexDesc2].fd;
  int depth2;
  CALL_OR_DIE(getDepth(fd2, block2, &depth2));
  SecHashEntry hashEntry2;
  CALL_OR_DIE(getSecHashTable(fd2, block2, 1, &hashEntry2));

  // get corresponding primary indexes
  int pid1;
  HT_OpenIndex(secIndexArray[sindexDesc1].primary_name, &pid1);
  int pid2;
  HT_OpenIndex(secIndexArray[sindexDesc2].primary_name, &pid2);
  int pfd1 = indexArray[pid1].fd;
  int pfd2 = indexArray[pid2].fd;

  BF_Block *block3;
  BF_Block_Init(&block3);

  BF_Block *block4;
  BF_Block_Init(&block4);

  // if key is NULL print all records of join
  if (index_key == NULL)
  {
    for (int i = 0; i < hashEntry1.secHeader.size; i++)
    {
      int blockN1 = hashEntry1.secHashNode[i].block_num;
      SecEntry entry1;
      CALL_OR_DIE(getSecEntry(fd1, block1, blockN1, &entry1));

      for (int j = 0; j < entry1.secHeader.size; j++)
      {
        for (int z = 0; z < hashEntry2.secHeader.size; z++)
        {
          int blockN2 = hashEntry2.secHashNode[z].block_num;
          SecEntry entry2;
          CALL_OR_DIE(getSecEntry(fd2, block2, blockN2, &entry2));

          for (int w = 0; w < entry2.secHeader.size; w++)
          {
            if (strcmp(entry1.secRecord[j].index_key, entry2.secRecord[w].index_key) == 0)
            {
              int block_num1 = getBlockNumFromTID(entry1.secRecord[j].tupleId);
              int index_in_block1 = getIndexFromTID(entry1.secRecord[j].tupleId);
              int block_num2 = getBlockNumFromTID(entry2.secRecord[w].tupleId);
              int index_in_block2 = getIndexFromTID(entry2.secRecord[w].tupleId);

              Entry pentry1;
              Entry pentry2;
              CALL_OR_DIE(getEntry(pfd1, block3, block_num1, &pentry1));
              CALL_OR_DIE(getEntry(pfd2, block4, block_num2, &pentry2));

              if (strcmp(hashEntry1.secHeader.attribute, "surnames") == 0)
              {
                printf("%s, %d, %s, %s, ", entry1.secRecord[j].index_key, entry1.secRecord[j].tupleId, pentry1.record[index_in_block1].name, pentry1.record[index_in_block1].city);
                printf("%d, %s, %s\n", entry2.secRecord[w].tupleId, pentry2.record[index_in_block2].name, pentry2.record[index_in_block2].city);
              }
              else
              {
                printf("%s, %d, %s, %s, ", entry1.secRecord[j].index_key, entry1.secRecord[j].tupleId, pentry1.record[index_in_block1].name, pentry1.record[index_in_block1].surname);
                printf("%d, %s, %s\n", entry2.secRecord[w].tupleId, pentry2.record[index_in_block2].name, pentry2.record[index_in_block2].surname);
              }
            }
          }
          // skip hash values that point to the same block
          int dif2 = depth2 - entry2.secHeader.local_depth;
          z += pow(2.0, (double)dif2) - 1;
        }
      }
      // skip hash values that point to the same block
      int dif1 = depth1 - entry1.secHeader.local_depth;
      i += pow(2.0, (double)dif1) - 1;
    }
  }
  else
  {
    int hash_val1 = hashAttr(index_key, depth1);
    int hash_val2 = hashAttr(index_key, depth2);
    int bn1 = hashEntry1.secHashNode[hash_val1].block_num;
    int bn2 = hashEntry2.secHashNode[hash_val2].block_num;

    SecEntry entry1;
    CALL_OR_DIE(getSecEntry(fd1, block1, bn1, &entry1));

    SecEntry entry2;
    CALL_OR_DIE(getSecEntry(fd2, block2, bn2, &entry2));

    for (int i = 0; i < entry1.secHeader.size; i++)
    {
      for (int j = 0; j < entry2.secHeader.size; j++)
      {
        if ((strcmp(entry1.secRecord[i].index_key, index_key) == 0) && (strcmp(index_key, entry2.secRecord[j].index_key) == 0))
        {
          int block_num1 = getBlockNumFromTID(entry1.secRecord[i].tupleId);
          int index_in_block1 = getIndexFromTID(entry1.secRecord[i].tupleId);
          int block_num2 = getBlockNumFromTID(entry2.secRecord[j].tupleId);
          int index_in_block2 = getIndexFromTID(entry2.secRecord[j].tupleId);

          Entry pentry1;
          Entry pentry2;
          CALL_OR_DIE(getEntry(pfd1, block3, block_num1, &pentry1));
          CALL_OR_DIE(getEntry(pfd2, block4, block_num2, &pentry2));

          // check record types of secondary directories in order to adjust prints
          if (strcmp(hashEntry1.secHeader.attribute, "surnames") == 0)
          {
            printf("%s, %d, %s, %s, ", entry1.secRecord[i].index_key, entry1.secRecord[i].tupleId, pentry1.record[index_in_block1].name, pentry1.record[index_in_block1].city);
            printf("%d, %s, %s\n", entry2.secRecord[j].tupleId, pentry2.record[index_in_block2].name, pentry2.record[index_in_block2].city);
          }
          else
          {
            printf("%s, %d, %s, %s, ", entry1.secRecord[i].index_key, entry1.secRecord[i].tupleId, pentry1.record[index_in_block1].name, pentry1.record[index_in_block1].surname);
            printf("%d, %s, %s\n", entry2.secRecord[j].tupleId, pentry2.record[index_in_block2].name, pentry2.record[index_in_block2].surname);
          }
        }
      }
    }
  }
  return HT_OK;
}
