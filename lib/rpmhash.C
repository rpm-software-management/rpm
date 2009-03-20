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
    Bucket next;	/*!< pointer to next item in bucket */
    HTKEYTYPE key;      /*!< hash key */
#ifdef HTDATATYPE
    int dataCount;	/*!< data entries */
    HTDATATYPE data[1];	/*!< data - grows by resizing whole bucket */
#endif
};

/**
 */
struct HASHSTRUCT {
    int numBuckets;			/*!< number of hash buckets */
    Bucket * buckets;			/*!< hash bucket array */
    hashFunctionType fn;		/*!< generate hash value for key */
    hashEqualityType eq;		/*!< compare hash keys for equality */
    hashFreeKey freeKey;
#ifdef HTDATATYPE
    hashFreeData freeData;
#endif
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

    while (b && ht->eq(b->key, key))
	b = b->next;

    return b;
}

HASHTYPE HASHPREFIX(Create)(int numBuckets,
			    hashFunctionType fn, hashEqualityType eq,
			    hashFreeKey freeKey
#ifdef HTDATATYPE
, hashFreeData freeData
#endif
)
{
    HASHTYPE ht;

    ht = xmalloc(sizeof(*ht));
    ht->numBuckets = numBuckets;
    ht->buckets = xcalloc(numBuckets, sizeof(*ht->buckets));
    ht->freeKey = freeKey;
#ifdef HTDATATYPE
    ht->freeData = freeData;
#endif
    ht->fn = fn;
    ht->eq = eq;
    return ht;
}

void HASHPREFIX(AddEntry)(HASHTYPE ht, HTKEYTYPE key
#ifdef HTDATATYPE
, HTDATATYPE data
#endif
)
{
    unsigned int hash;
    Bucket b;
    Bucket * b_addr;

    hash = ht->fn(key) % ht->numBuckets;
    b = ht->buckets[hash];
    b_addr = ht->buckets + hash;

    while (b && ht->eq(b->key, key)) {
	b_addr = &(b->next);
	b = b->next;
    }

    if (b == NULL) {
	b = xmalloc(sizeof(*b));
	b->key = key;
#ifdef HTDATATYPE
	b->dataCount = 1;
	b->data[0] = data;
#endif
	b->next = ht->buckets[hash];
	ht->buckets[hash] = b;
    }
#ifdef HTDATATYPE
    else {
	// resizing bucket TODO: increase exponentially
	// Bucket_s already contains space for one dataset
	b = *b_addr = xrealloc(
	    b, sizeof(*b) + sizeof(b->data[0]) * (b->dataCount));
	// though increasing dataCount after the resize
	b->data[b->dataCount++] = data;
    }
#endif
}

HASHTYPE HASHPREFIX(Free)(HASHTYPE ht)
{
    Bucket b, n;
    int i;
    if (ht==NULL)
	return ht;
    for (i = 0; i < ht->numBuckets; i++) {
	b = ht->buckets[i];
	if (b == NULL)
	    continue;
	ht->buckets[i] = NULL;

	do {
	    n = b->next;
	    if (ht->freeKey)
		b->key = ht->freeKey(b->key);
#ifdef HTDATATYPE
	    if (ht->freeData) {
		int j;
		for (j=0; j < b->dataCount; j++ ) {
		    b->data[j] = ht->freeData(b->data[j]);
		}
	    }
#endif
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

#ifdef HTDATATYPE

int HASHPREFIX(GetEntry)(HASHTYPE ht, HTKEYTYPE key, HTDATATYPE** data,
	       int * dataCount, HTKEYTYPE* tableKey)
{
    Bucket b;
    int rc = ((b = HASHPREFIX(findEntry)(ht, key)) != NULL);

    if (data)
	*data = rc ? b->data : NULL;
    if (dataCount)
	*dataCount = rc ? b->dataCount : 0;
    if (tableKey && rc)
	*tableKey = b->key;

    return rc;
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
#ifdef HTDATATYPE
	    datacnt += bucket->dataCount;
#endif
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

#endif
