#ifndef H_HASH
#define H_HASH

/* These hash tables are *very* skeptical about duplicating data. Be warned. */

typedef struct hashTable_s * hashTable;

hashTable htCreate(int numBuckets);
void htAddEntry(hashTable ht, const char * key, void * data);
void htFree(hashTable ht);
/* returns 0 on success, 1 if the item is not found */
int htGetEntry(hashTable ht, const char * key, void *** data, int * dataCount);
/* returns 1 if the item is present, 0 otherwise */
int htHasEntry(hashTable ht, const char * key);

#endif
