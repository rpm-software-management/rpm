#include "system.h"

#include "rpmlib.h"

#include "hash.h"

struct hashBucket {
    const char * key;
    void ** data;
    int dataCount;
    struct hashBucket * next;
};

struct hashTable_s {
    int numBuckets;
    struct hashBucket ** buckets;
};

static unsigned int hashFunction(const char * string, int numBuckets);
static struct hashBucket * findEntry(hashTable ht, const char * key);

static unsigned int hashFunction(const char * string, int numBuckets) {
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

    return ((len << 16) + (sum << 8) + xorValue) % numBuckets;
};

hashTable htCreate(int numBuckets) {
    hashTable ht;

    ht = malloc(sizeof(*ht));
    ht->numBuckets = numBuckets;
    ht->buckets = calloc(sizeof(*ht->buckets), numBuckets);

    return ht;
}

void htAddEntry(hashTable ht, const char * key, void * data) {
    unsigned int hash;
    struct hashBucket * b, * ob;

    hash = hashFunction(key, ht->numBuckets);
    b = ob = ht->buckets[hash]; 

    while (b && b->key && strcmp(b->key, key)) 
	b = b->next;
   
    if (!b) {
	b = malloc(sizeof(*b));
	b->key = key;
	b->dataCount = 0;
	b->next = ht->buckets[hash];
	b->data = NULL;
	ht->buckets[hash] = b;
    }

    b->data = realloc(b->data, sizeof(*b->data) * (b->dataCount + 1));
    b->data[b->dataCount++] = data;
}

void htFree(hashTable ht) {
    int i;
    struct hashBucket * b, * n;

    for (i = 0; i < ht->numBuckets; i++) {
	b = ht->buckets[i];
	while (b) {
	    n = b->next;
	    free(b);
	    b = n;
	}
    }

    free(ht->buckets);
    free(ht);
}

static struct hashBucket * findEntry(hashTable ht, const char * key) {
    unsigned int hash;
    struct hashBucket * b;

    hash = hashFunction(key, ht->numBuckets);
    b = ht->buckets[hash]; 

    while (b && b->key && strcmp(b->key, key)) 
	b = b->next;

    return b;
}

int htHasEntry(hashTable ht, const char * key) {
    struct hashBucket * b;

    if (!(b = findEntry(ht, key))) return 0; else return 1;
}

int htGetEntry(hashTable ht, const char * key, void *** data, 
	       int * dataCount) {
    struct hashBucket * b;

    if (!(b = findEntry(ht, key))) return 1;

    *data = b->data;
    *dataCount = b->dataCount;

    return 0;
}
