#ifndef HASH_FILE_H
#define HASH_FILE_H

#define MAX_OPEN_FILES 20
#define MAX_NAME_LEN 30

typedef int tid;

typedef struct
{ //μπορειτε να αλλαξετε τη δομη συμφωνα  με τις ανάγκες σας
	char surname[20];
	char city[20];
	int oldTupleId; // η παλια θέση της εγγραφής πριν την εισαγωγή της νέας
	int newTupleId; // η νέα θέση της εγγραφής που μετακινήθηκε μετα την εισαγωγή της νέας εγγραφής

} UpdateRecordArray;

typedef struct
{
	int fd;
	int used;
	char filename[MAX_NAME_LEN];
} IndexNode;

IndexNode indexArray[MAX_OPEN_FILES];

typedef struct
{
	int size;
	int local_depth;
} DataHeader;

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_RECORDS ((BF_BLOCK_SIZE - sizeof(DataHeader)) / sizeof(Record))
#define MAX_HNODES ((BF_BLOCK_SIZE - sizeof(HashHeader)) / sizeof(HashNode))

typedef struct
{
	DataHeader header;
	Record record[MAX_RECORDS];
} Entry;

#define CALL_OR_DIE(call)         \
	{                             \
		HT_ErrorCode code = call; \
		if (code != HT_OK)        \
		{                         \
			printf("Error\n");    \
			exit(code);           \
		}                         \
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Η συνάρτηση HT_Init χρησιμοποιείται για την αρχικοποίηση κάποιον δομών που μπορεί να χρειαστείτε.
 * Σε περίπτωση που εκτελεστεί επιτυχώς, επιστρέφεται HT_OK, ενώ σε διαφορετική περίπτωση κωδικός λάθους.
 */
HT_ErrorCode HT_Init();

/*
 * Η συνάρτηση HT_CreateIndex χρησιμοποιείται για τη δημιουργία και κατάλληλη αρχικοποίηση ενός άδειου αρχείου κατακερματισμού με όνομα fileName.
 * Στην περίπτωση που το αρχείο υπάρχει ήδη, τότε επιστρέφεται ένας κωδικός λάθους.
 * Σε περίπτωση που εκτελεστεί επιτυχώς επιστρέφεται HΤ_OK, ενώ σε διαφορετική περίπτωση κωδικός λάθους.
 */
HT_ErrorCode HT_CreateIndex(
	const char *fileName, /* όνομααρχείου */
	int depth);

/*
 * Η ρουτίνα αυτή ανοίγει το αρχείο με όνομα fileName.
 * Εάν το αρχείο ανοιχτεί κανονικά, η ρουτίνα επιστρέφει HT_OK, ενώ σε διαφορετική περίπτωση κωδικός λάθους.
 */
HT_ErrorCode HT_OpenIndex(
	const char *fileName, /* όνομα αρχείου */
	int *indexDesc		  /* θέση στον πίνακα με τα ανοιχτά αρχεία  που επιστρέφεται */
);

/*
 * Η ρουτίνα αυτή κλείνει το αρχείο του οποίου οι πληροφορίες βρίσκονται στην θέση indexDesc του πίνακα ανοιχτών αρχείων.
 * Επίσης σβήνει την καταχώρηση που αντιστοιχεί στο αρχείο αυτό στον πίνακα ανοιχτών αρχείων.
 * Η συνάρτηση επιστρέφει ΗΤ_OK εάν το αρχείο κλείσει επιτυχώς, ενώ σε διαφορετική σε περίπτωση κωδικός λάθους.
 */
HT_ErrorCode HT_CloseFile(
	int indexDesc /* θέση στον πίνακα με τα ανοιχτά αρχεία */
);

/*
 * Η συνάρτηση HT_InsertEntry χρησιμοποιείται για την εισαγωγή μίας εγγραφής στο αρχείο κατακερματισμού.
 * Οι πληροφορίες που αφορούν το αρχείο βρίσκονται στον πίνακα ανοιχτών αρχείων, ενώ η εγγραφή προς εισαγωγή προσδιορίζεται από τη δομή record.
 * Σε περίπτωση που εκτελεστεί επιτυχώς επιστρέφεται HT_OK, ενώ σε διαφορετική περίπτωση κάποιος κωδικός λάθους.
 */
HT_ErrorCode HT_InsertEntry(
	int indexDesc,				   /* θέση στον πίνακα με τα ανοιχτά αρχεία */
	Record record,				   /* δομή που προσδιορίζει την εγγραφή */
	tid *tupleId,				   /* το tuple id της καινούργιας εγγραφής*/
	UpdateRecordArray *updateArray /* πίνακας με τις αλλαγές */
);

/*
 * Η συνάρτηση HΤ_PrintAllEntries χρησιμοποιείται για την εκτύπωση όλων των εγγραφών που το record.id έχει τιμή id.
 * Αν το id είναι NULL τότε θα εκτυπώνει όλες τις εγγραφές του αρχείου κατακερματισμού.
 * Σε περίπτωση που εκτελεστεί επιτυχώς επιστρέφεται HP_OK, ενώ σε διαφορετική περίπτωση κάποιος κωδικός λάθους.
 */
HT_ErrorCode HT_PrintAllEntries(
	int indexDesc, /* θέση στον πίνακα με τα ανοιχτά αρχεία */
	int *id		   /* τιμή του πεδίου κλειδιού προς αναζήτηση */
);

/*
 * Η συνάρτηση HashStatistics χρησιμοποιείται για την εκτύπωση στατιστικών στοιχείων
 */
HT_ErrorCode HashStatistics(
	char *filename);

// Utility functions
unsigned int hashFunction(int, int);

tid getTid(int, int);

HT_ErrorCode getNewBlock(int, BF_Block *, int *);
HT_ErrorCode getDepth(int, BF_Block *, int *);
HT_ErrorCode setDepth(int, BF_Block *, int);
HT_ErrorCode getEntry(int, BF_Block *, int, Entry *);

int getBlockNumFromTID(tid);
int getIndexFromTID(tid);

void printUpdateArray(UpdateRecordArray *array);

#endif // HASH_FILE_H
