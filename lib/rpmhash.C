/**
 * \file lib/rpmhash.c
 * Hash table implemenation
 */

#include "system.h"
#include "debug.h"

#define Bucket JOIN(HASHTYPE,Buket)
#define Bucket_s JOIN(HASHTYPE,Buket_s)

typedef	struct  Bucket_s * Bucket;

/**
 */
struct  Bucket_s {
    HTKEYTYPE key;      /*!< hash key */
    HTDATATYPE * data;	/*!< pointer to hashed data */
    int dataCount;	/*!< length of data (0 if unknown) */
    Bucket next;	/*!< pointer to next item in bucket */
};

/**
 */
struct HASHSTRUCT {
    int numBuckets;			/*!< number of hash buckets */
    Bucket * buckets;		/*!< hash bucket array */
    hashFunctionType fn;		/*!< generate hash value for key */
    hashEqualityType eq;		/*!< compare hash keys for equality */
    hashFreeKey freeKey;
    hashFreeData freeData;
};

/**
 * Find entry in hash table.
 * @param ht            pointer to hash table
 * @param key           pointer to key value
 * @return pointer to hash bucket of key (or NULL)
 */
static
Bucket HASHPREFIX(findEntry)(HASHTYPE ht, HTKEYTYPE key)
{
    unsigned int hash;
    Bucket b;

    hash = ht->fn(key) % ht->numBuckets;
    b = ht->buckets[hash];

    while (b && b->key && ht->eq(b->key, key))
	b = b->next;

    return b;
}

HASHTYPE HASHPREFIX(Create)(int numBuckets,
			    hashFunctionType fn, hashEqualityType eq,
			    hashFreeKey freeKey, hashFreeData freeData)
{
    HASHTYPE ht;

    ht = xmalloc(sizeof(*ht));
    ht->numBuckets = numBuckets;
    ht->buckets = xcalloc(numBuckets, sizeof(*ht->buckets));
    ht->freeKey = freeKey;
    ht->freeData = freeData;
    ht->fn = fn;
    ht->eq = eq;
    return ht;
}

void HASHPREFIX(AddEntry)(HASHTYPE ht, HTKEYTYPE key, HTDATATYPE data)
{
    unsigned int hash;
    Bucket b;

    hash = ht->fn(key) % ht->numBuckets;
    b = ht->buckets[hash];

    while (b && b->key && ht->eq(b->key, key))
	b = b->next;

    if (b == NULL) {
	b = xmalloc(sizeof(*b));
	b->key = key;
	b->dataCount = 0;
	b->next = ht->buckets[hash];
	b->data = NULL;
	ht->buckets[hash] = b;
    }

    b->data = xrealloc(b->data, sizeof(*b->data) * (b->dataCount + 1));
    b->data[b->dataCount++] = data;
}

HASHTYPE HASHPREFIX(Free)(HASHTYPE ht)
{
    Bucket b, n;
    int i, j;

    for (i = 0; i < ht->numBuckets; i++) {
	b = ht->buckets[i];
	if (b == NULL)
	    continue;
	ht->buckets[i] = NULL;
	if (ht->freeKey)
	    b->key = ht->freeKey(b->key);

	do {
	    n = b->next;
	    if (b->data) {
		if (ht->freeData) {
		  for (j=0; j < b->dataCount; j++ ) {
		    b->data[j] = ht->freeData(b->data[j]);
		  }
		}
		b->data = _free(b->data);
	    }
	    b = _free(b);
	} while ((b = n) != NULL);
    }

    ht->buckets = _free(ht->buckets);
    ht = _free(ht);

    return NULL;
}

int HASHPREFIX(HasEntry)(HASHTYPE ht, HTKEYTYPE key)
{
    Bucket b;

    if (!(b = HASHPREFIX(findEntry)(ht, key))) return 0; else return 1;
}

int HASHPREFIX(GetEntry)(HASHTYPE ht, HTKEYTYPE key, HTDATATYPE** data,
	       int * dataCount, HTKEYTYPE* tableKey)
{
    Bucket b;

    if ((b = HASHPREFIX(findEntry)(ht, key)) == NULL)
	return 1;

    if (data)
	*data = b->data;
    if (dataCount)
	*dataCount = b->dataCount;
    if (tableKey)
	*tableKey = b->key;

    return 0;
}

void HASHPREFIX(PrintStats)(HASHTYPE ht) {
    int i;
    Bucket bucket;

    int hashcnt=0, bucketcnt=0, datacnt=0;
    int maxbuckets=0;

    for (i=0; i<ht->numBuckets; i++) {
        int buckets = 0;
        for (bucket=ht->buckets[i]; bucket; bucket=bucket->next){
	    buckets++;
	    datacnt += bucket->dataCount;
	}
	if (maxbuckets < buckets) maxbuckets = buckets;
	if (buckets) hashcnt++;
	bucketcnt += buckets;
    }
    fprintf(stderr, "Hashsize: %i\n", ht->numBuckets);
    fprintf(stderr, "Hashbuckets: %i\n", hashcnt);
    fprintf(stderr, "Keys: %i\n", bucketcnt);
    fprintf(stderr, "Values: %i\n", datacnt);
    fprintf(stderr, "Max Keys/Bucket: %i\n", maxbuckets);
}
