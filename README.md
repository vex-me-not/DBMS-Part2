# DBMS-Part2
Continuation of DBMS project for the respective course of DIT,UoA. Part1 can be found  <a href="https://github.com/vex-me-not/DBMS-Part1" style="underline:none;">here</a>
# Contributors
* <a href="https://github.com/vex-me-not" style="underline:none;">Nikos Charisis</a> <br>
* <a href="https://github.com/Mastoropoulos-Loukas" style="underline:none;">Loukas Mastoropoulos</a> <br>
* <a href="https://github.com/AlexLinardos" style="underline:none;">Alex Linardos</a> <br>

# Οδηγίες μεταγλώττισης και εκτέλεσης
* Για τη μεταγλώττιση του προγράμματος χρησιμοποιήστε την εντολή `make sht`
* Για την εκτέλεση χρησιμοποιήστε την εντολή `./build/runner`
* Για την διαγραφή των αρχείων .db και των εκτελέσιμων χρησιμοποιήστε την `make clean`

# Παραδοχές
* Έχουν υλοποιηθεί όλες οι ζητούμενες συναρτήσεις και λειτουργίες, παρολ' αυτά ο αριθμός των εγγραφών που μπορούν να εισαχθούν είναι περιορισμένος. Πιο συγκεκριμένα, στην τωρινή υλοποίηση του προγράμματος μπορούν να εισαχθούν σωστά μέχρι και 90 εγγραφές. Ο αριθμός αυτός ενδέχεται να διαφοροποιείται ανάλογα με το seed της srand() που ορίζεται στην main, λόγω του ότι η rand μπορεί να επιλέγει κάποια στοιχεία με μεγάλη συχνότητα. Επιπλέον, τα ευρετήρια περιορίζονται σε ένα μόνο μπλοκ. Ο αριθμός αυτός θα μπορούσε να είναι μεγαλύτερος αν οι πίνακες cities και surnames είχανε περισσότερα διαφορετικά στοιχεία έτσι ώστε να μην επιλέγονται τόσο συχνά ίδιες εγγραφές.
* Πήραμε την απόφαση να "σπάσουμε" τον κώδικα σε πολλές μικρότερες συναρτήσεις προκειμένου να είναι οι ζητούμενες συναρτήσεις πιο ευανάγνωστες. Στη συνέχεια ακολουθεί κατάλογος των εν λόγω συναρτήσεων.
* Υποθέτουμε ότι όλες οι εγγραφές με ίδια τιμή κλειδιού χωράνε στο ίδιο μπλοκ (όπως αναφέρθηκε κατά την παράδοση της εργασίας).
* Σε αυτή τη μορφή της main, προκειμένου να δοκιμαστεί, η InnerJoin καλείται να εκτελεστεί μεταξύ του αρχείου δευτερεύοντος ευρετηρίου που έχει δημιουργηθεί και του εαυτού του.
* Ο αριθμός των εγγραφών που θα περαστούν, το αρχικό global depth, καθώς και τα ονόματα του πρωτεύοντος και του δευτερεύοντος ευρετηρίου που θα δημιουργηθούν μπορούν να αλλαχθούν από τα αντίστοιχα defines στην αρχή του αρχείου "sht_main.c".

Στο αρχείο εικόνας "planning.png" μπορείτε να βρείτε το σχεδιάγραμμα ενός αρχικού πλάνου για τη δόμηση των ευρετηρίων το οποίο χρησιμοποιήσαμε για να μας βοηθήσει κατά την υλοποίηση της εργασίας.

# Κατάλογος συναρτήσεων
Για πιο αναλυτικές πληροφορίες σχετικά με κάθε συνάρτηση και τις παραμέτρους της έχουμε αφήσει επαρκή σχόλια πάνω στον κώδικα.

## Βοηθητικές συναρτήσεις ανά αρχείο
* __hash_file.c__:
    * checkCreateIndex : Συνάρτηση που ελέγχει για την σωστή κλήση της HT_CreateIndex
    * checkInsertEntry : Συνάρτηση που ελέγχει για την σωστή κλήση της HT_InsertEntry
    * checkPrintAllEntries : Συνάρτηση που ελέγχει για την σωστή κλήση της HT_PrintAllEntries
    * getTid :Συνάρτηση που υπολογίζει το tupleID μιας εγγραφής με βάση τον τύπο της εκφώνησης (tupleId= (blockId+1) *num_of_rec_in_block)+index_of_rec_in_block)
    * getDepth
    * getHashTable
    * getBucket
    * getEntry
    * getEndPoints
    * getNewBlock
    * getBlockNumFromTID
    * getIndexFromTID
    * setDepth
    * setHashTable
    * setEntry
    * createInfoBlock
    * createHashTable
    * reassignRecords
    * insertRecordAfterSplit
    * doubleHashTable
    * splitHashTable
    * printUpdateArray
    * printRecord
    * printHashNode
    * printAllRecords
    * printSepsificRecord
    * hashFunction

* __sht_file.c__:
    * checkShtCreate : Συνάρτηση που ελέγχει για την σωστή κλήση της SHT_CreateSecondaryIndex
    * checkSecInsertEntry : Συνάρτηση που ελέγχει για την σωστή κλήση της SHT_SecondaryInsertEntry
    * getSecHashTable
    * getSecBucket
    * getSecEntry
    * getSecEndPoints : Συνάρτηση που καλειται απο την splitSecHashTable    
    * setSecHashTable
    * setSecEntry
    * createSecInfoBlock
    * createSecHashTable
    * reassignSecRecords : Συνάρτηση που καλειται απο την splitSecHashTable   
    * insertSecRecordAfterSplit : Συνάρτηση που καλειται απο την splitSecHashTable   
    * doubleSecHashTable : Συνάρτηση που καλειται απο την SHT_SecondaryInsertEntry
    * splitSecHashTable : Συνάρτηση που καλειται απο την SHT_SecondaryInsertEntry
    * printSecRecord : Συνάρτηση που εκτυπώνει όλα τα παιδία ενός SecRecord
    * SHT_PrintHashNode : Συνάρτηση που εκτυπώνει όλα τα παιδία ενός SecHashNode
    * SHT_PrintSecondaryRecord : Συνάρτηση που εκτυπώνει όλα τα παιδία ενός SecRecord
    * SHT_PrintSecEntry : Συνάρτηση που εκτυπώνει όλα τα παιδία ενός SecEntry
    * SHT_PrintSecHashEntry : Συνάρτηση που εκτυπώνει όλα τα παιδία ενός SecHashEntry 
    * SHT_PrintSecHashTable : Συνάρτηση που εκτυπώνει όλα τα παιδία ενός Secondary HashTable
    * printAllSecRecords : Συνάρτηση που χρησιμοποιείται από την SHT_PrintAllEntries αν δωθεί NULL είσοδος    
    * printSecSepsificRecord : Συνάρτηση που εκτυπώνει ένα συγκεκριμένο SecRecord. Καλείται από την SHT_PrintAllEntries αν η είσοδος δεν είναι NULL.    
    * primaryExists : Συνάρτηση που ελέγχει αν υπάρχει το δωθέν πρωτεύον αρχείο.    
    * hashAttr : Συνάρτηση κατακερματισμού ενός attribute. Εκτελεί διάφορες πράξεις πάνω στα περιοχόμενα του attribute.

## Ζητούμενες συναρτήσεις (στο αρχέιο sht_file.c)
* SHT_Init
* SHT_CreateSecondaryIndex
* SHT_CloseSecondaryIndex
* SHT_SecondaryInsertEntry
* SHT_SecondaryUpdateEntry : Για να λειτουργήσει σωστα, πρέπει να γνωρίζουμε απο πριν το μέγεθος του updateArray. Στην δική μας περίπτωση το μέγεθος αυτό είναι MAX_RECORDS. Αν το oldTupleID του 1ου στοιχείου του updateArray είναι ίσο με -1 , σημαίνει πως δεν χρειάζεται να κάνουμε καμία ενήμερωση στις εγγραφές. Η αρχικοποίση του updateArray συμβαίνει στην HT_InsertEntry, όπως και η ενημέρωσή του.
* SHT_PrintAllEntries
* SHT_HashStatistics
* SHT_InnerJoin : Όταν η συνάρτηση καλείται με την τιμή NULL για την παράμετρο index_key, επιστρέφονται όλες οι εγγραφές ζεύξης μέσω αλγορίθμου εμφωλευμένης επανάληψης. Όταν η συνάρτηση καλείται με κάποια άλλη τιμή κλειδιού, τότε η τιμή αυτή κατακερματίζεται προκειμένου να βρεθεί το μπλοκ όπου καταλήγουν οι εγγραφές με αυτό το κλειδί και ύστερα επιστρέφονται οι αντίστοιχες εγγραφές ζεύξης και πάλι με τη χρήση εμφωλευμένης επανάληψης.
