/**
 * \file lib/hash.c
 * Hash table implemenation
 */

#include "system.h"
#include <rpmlib.h>
#include "hash.h"
#include "debug.h"

typedef /*@owned@*/ const void * voidptr;

/** */
struct hashBucket {
    voidptr key;			/*!< hash key */
/*@owned@*/ voidptr * data;		/*!< pointer to hashed data */
    int dataCount;			/*!< length of data (0 if unknown) */
/*@dependent@*/struct hashBucket * next;/*!< pointer to next item in bucket */
};

/** */
struct hashTable_s {
    int numBuckets;			/*!< number of hash buckets */
    int keySize;			/*!< size of key (0 if unknown) */
    int freeData;	/*!< should data be freed when table is destroyed? */
    struct hashBucket ** buckets;	/*!< hash bucket array */
    hashFunctionType fn;		/*!< generate hash value for key */
    hashEqualityType eq;		/*!< compare hash keys for equality */
};

/**
 * Find entry in hash table.
 * @param ht            pointer to hash table
 * @param key           pointer to key value
 * @return pointer to hash bucket of key (or NULL)
 */
static /*@shared@*/ /*@null@*/
struct hashBucket *findEntry(hashTable ht, const void * key)
{
    unsigned int hash;
    struct hashBucket * b;

    hash = ht->fn(key) % ht->numBuckets;
    b = ht->buckets[hash];

    while (b && b->key && ht->eq(b->key, key))
	b = b->next;

    return b;
}

int hashEqualityString(const void * key1, const void * key2)
{
    const char *k1 = (const char *)key1;
    const char *k2 = (const char *)key2;
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

    return ((((unsigned)len) << 16) + (((unsigned)sum) << 8) + xorValue);
}

hashTable htCreate(int numBuckets, int keySize, int freeData, hashFunctionType fn,
		   hashEqualityType eq)
{
    hashTable ht;

    ht = xmalloc(sizeof(*ht));
    ht->numBuckets = numBuckets;
    ht->buckets = xcalloc(numBuckets, sizeof(*ht->buckets));
    ht->keySize = keySize;
    ht->freeData = freeData;
    ht->fn = fn;
    ht->eq = eq;

    return ht;
}

void htAddEntry(hashTable ht, const void * key, const void * data)
{
    unsigned int hash;
    struct hashBucket * b;

    hash = ht->fn(key) % ht->numBuckets;
    b = ht->buckets[hash];

    while (b && b->key && ht->eq(b->key, key))
	b = b->next;

    if (b == NULL) {
	b = xmalloc(sizeof(*b));
	if (ht->keySize) {
	    char *k = xmalloc(ht->keySize);
	    memcpy(k, key, ht->keySize);
	    b->key = k;
	} else {
	    b->key = key;
	}
	b->dataCount = 0;
	b->next = ht->buckets[hash];
	b->data = NULL;
	ht->buckets[hash] = b;
    }

    b->data = xrealloc(b->data, sizeof(*b->data) * (b->dataCount + 1));
    b->data[b->dataCount++] = data;
}

void htFree(hashTable ht)
{
    struct hashBucket * b, * n;
    int i;

    for (i = 0; i < ht->numBuckets; i++) {
	b = ht->buckets[i];
	if (ht->keySize && b) free((void *)b->key);
	while (b) {
	    n = b->next;
	    if (b->data) {
		if (ht->freeData)
		    *b->data = _free(*b->data);
		b->data = _free(b->data);
	    }
	    b = _free(b);
	    b = n;
	}
    }

    ht->buckets = _free(ht->buckets);
    ht = _free(ht);
}

int htHasEntry(hashTable ht, const void * key)
{
    struct hashBucket * b;

    if (!(b = findEntry(ht, key))) return 0; else return 1;
}

int htGetEntry(hashTable ht, const void * key, const void *** data,
	       int * dataCount, const void ** tableKey)
{
    struct hashBucket * b;

    if ((b = findEntry(ht, key)) == NULL)
	return 1;

    if (data)
	*data = (const void **) b->data;
    if (dataCount)
	*dataCount = b->dataCount;
    if (tableKey)
	*tableKey = b->key;

    return 0;
}
