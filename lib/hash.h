#ifndef H_HASH
#define H_HASH

typedef struct hashTable_s * hashTable;

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int (*hashFunctionType)(const void * string);
typedef int (*hashEqualityType)(const void * key1, const void * key2);

unsigned int hashFunctionString(const void * string);
int hashEqualityString(const void * key1, const void * key2);

/* if keySize > 0, the key is duplicated within the table (which costs
   memory, but may be usefull anyway */
hashTable htCreate(int numBuckets, int keySize, hashFunctionType fn,
		   hashEqualityType eq); 
void htAddEntry(hashTable ht, const void * key, const void * data);
void htFree(hashTable ht);
/* returns 0 on success, 1 if the item is not found. tableKey may be NULL */
int htGetEntry(hashTable ht, const void * key, /*@out@*/const void *** data, /*@out@*/int * dataCount,
	       /*@out@*/const void ** tableKey);
/* returns 1 if the item is present, 0 otherwise */
int htHasEntry(hashTable ht, const void * key);

#ifdef __cplusplus
}
#endif

#endif
