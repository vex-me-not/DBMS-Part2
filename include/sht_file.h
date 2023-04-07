#include "hash_file.h"
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

typedef int tid;

typedef struct
{ //μπορειτε να αλλαξετε τη δομη συμφωνα  με τις ανάγκες σας
	char surname[20];
	char city[20];
	int oldTupleId; // η παλια θέση της εγγραφής πριν την εισαγωγή της νέας
	int newTupleId; // η νέα θέση της εγγραφής που μετακινήθηκε μετα την εισαγωγή της νέας εγγραφής

} UpdateRecordArray;

tid getTid(int, int);

#endif // HASH_FILE_H

typedef struct
{
	char index_key[20];
	int tupleId; /*Ακέραιος που προσδιορίζει το block και τη θέση μέσα στο block στην οποία     έγινε η εισαγωγή της εγγραφής στο πρωτεύον ευρετήριο.*/
} SecondaryRecord;

typedef struct
{
	int size;
	int local_depth;
} SecHeader;

#define SEC_MAX_NODES ((BF_BLOCK_SIZE - sizeof(SecHashHeader)) / sizeof(SecHashNode))
#define SEC_MAX_RECORDS ((BF_BLOCK_SIZE - sizeof(SecHeader)) / sizeof(SecondaryRecord))

//////////////////////////////////////////////////////////////////////////

HT_ErrorCode SHT_Init();

HT_ErrorCode SHT_CreateSecondaryIndex(
	const char *sfileName, /* όνομα αρχείου */
	char *attrName,		   /* όνομα πεδίου-κλειδιού */
	int attrLength,		   /* μήκος πεδίου-κλειδιού */
	int depth,			   /* το ολικό βάθος ευρετηρίου επεκτατού κατακερματισμού */
	char *fileName /* όνομα αρχείου πρωτεύοντος ευρετηρίου*/);

HT_ErrorCode SHT_OpenSecondaryIndex(
	const char *sfileName, /* όνομα αρχείου */
	int *indexDesc /* θέση στον πίνακα με τα ανοιχτά αρχεία που επιστρέφεται */);

HT_ErrorCode SHT_CloseSecondaryIndex(int indexDesc /* θέση στον πίνακα με τα ανοιχτά αρχεία */);

HT_ErrorCode SHT_SecondaryInsertEntry(
	int indexDesc, /* θέση στον πίνακα με τα ανοιχτά αρχεία */
	SecondaryRecord record /* δομή που προσδιορίζει την εγγραφή */);

HT_ErrorCode SHT_SecondaryUpdateEntry(
	int indexDesc, /* θέση στον πίνακα με τα ανοιχτά αρχεία */
	UpdateRecordArray *updateArray /* δομή που προσδιορίζει την παλιά εγγραφή */);

HT_ErrorCode SHT_PrintAllEntries(
	int sindexDesc, /* θέση στον πίνακα με τα ανοιχτά αρχεία  του αρχείου δευτερεύοντος ευρετηρίου */
	char *index_key /* τιμή του πεδίου-κλειδιού προς αναζήτηση */);

HT_ErrorCode SHT_HashStatistics(char *filename /* όνομα του αρχείου που ενδιαφέρει */);

HT_ErrorCode SHT_InnerJoin(
	int sindexDesc1, /* θέση στον πίνακα με τα ανοιχτά αρχεία  του αρχείου δευτερεύοντος ευρετηρίου για το πρώτο αρχείο εισόδου */
	int sindexDesc2, /* θέση στον πίνακα με τα ανοιχτά αρχεία του αρχείου δευτερεύοντος ευρετηρίου για το δεύτερο αρχείο εισόδου */
	char *index_key /* το κλειδι πανω στο οποιο θα γινει το join. Αν  NULL τοτε επιστρέφεί όλες τις πλειάδες*/);

void SHT_PrintSecHashTable(int fd, BF_Block *block, int full);

unsigned int hashAttr(const char *str, int depth);