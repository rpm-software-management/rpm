#include "system.h"

#include <rpmlib.h>
#include "hash.h"

struct hashBucket {
    void * key;
    void ** data;
    int dataCount;
    struct hashBucket * next;
};

struct hashTable_s {
    int numBuckets;
    int keySize;
    struct hashBucket ** buckets;
    hashFunctionType fn;
    hashEqualityType eq;
};

static struct hashBucket * findEntry(hashTable ht, const void * key)
{
    unsigned int hash;
    struct hashBucket * b;

    hash = ht->fn(key) % ht->numBuckets;
    b = ht->buckets[hash]; 

    while (b && b->key && !ht->eq(b->key, key)) 
	b = b->next;

    return b;
}

int hashEqualityString(const void * key1, const void * key2)
{
    char *k1 = (char *)key1;
    char *k2 = (char *)key2;
    return strcmp(k1, k2);
}

unsigned int hashFunctionString(const void * string)
{
    char xorValue = 0;
    char sum = 0;
    short len;
    int i;
    const char * chp = string;

    len = strlen(string);
    for (i = 0; i < len; i++, chp++) {
	xorValue ^= *chp;
	sum += *chp;
    }

    return ((len << 16) + (sum << 8) + xorValue);
}

hashTable htCreate(int numBuckets, int keySize, hashFunctionType fn,
		   hashEqualityType eq)
{
    hashTable ht;

    ht = malloc(sizeof(*ht));
    ht->numBuckets = numBuckets;
    ht->buckets = calloc(sizeof(*ht->buckets), numBuckets);
    ht->keySize = keySize;
    ht->fn = fn;
    ht->eq = eq;

    return ht;
}

void htAddEntry(hashTable ht, const void * key, void * data)
{
    unsigned int hash;
    struct hashBucket * b, * ob;

    hash = ht->fn(key) % ht->numBuckets;
    b = ob = ht->buckets[hash]; 

    while (b && b->key && !ht->eq(b->key, key)) 
	b = b->next;
   
    if (!b) {
	b = malloc(sizeof(*b));
	if (ht->keySize) {
	    b->key = malloc(ht->keySize);
	    memcpy(b->key, key, ht->keySize);
	} else {
	    b->key = (void *) key;
	}
	b->dataCount = 0;
	b->next = ht->buckets[hash];
	b->data = NULL;
	ht->buckets[hash] = b;
    }

    b->data = realloc(b->data, sizeof(*b->data) * (b->dataCount + 1));
    b->data[b->dataCount++] = data;
}

void htFree(hashTable ht)
{
    int i;
    struct hashBucket * b, * n;

    for (i = 0; i < ht->numBuckets; i++) {
	b = ht->buckets[i];
	if (ht->keySize && b) free(b->key);
	while (b) {
	    n = b->next;
	    if (b->data) free(b->data);		/* XXX ==> LEAK */
	    free(b);
	    b = n;
	}
    }

    free(ht->buckets);
    free(ht);
}

int htHasEntry(hashTable ht, const void * key)
{
    struct hashBucket * b;

    if (!(b = findEntry(ht, key))) return 0; else return 1;
}

int htGetEntry(hashTable ht, const void * key, void *** data, 
	       int * dataCount, void ** tableKey)
{
    struct hashBucket * b;

    if (!(b = findEntry(ht, key))) return 1;

    *data = b->data;
    *dataCount = b->dataCount;
    if (tableKey) *tableKey = b->key;

    return 0;
}
