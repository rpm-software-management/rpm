#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmstrpool.h>
#include "debug.h"

#define STRDATA_CHUNKS 1024
#define STRDATA_CHUNK 65536
#define STROFFS_CHUNK 2048
/* XXX this is ridiculously small... */
#define STRHASH_INITSIZE 1024

static int pool_debug = 0;

typedef struct poolHash_s * poolHash;
typedef struct poolHashBucket_s poolHashBucket;

struct poolHashBucket_s {
    rpmsid keyid;
};

struct poolHash_s {
    int numBuckets;
    poolHashBucket * buckets;
    int keyCount;
};

struct rpmstrPool_s {
    const char ** offs;		/* pointers into data area */
    rpmsid offs_size;		/* largest offset index */;
    rpmsid offs_alloced;	/* offsets allocation size */

    char ** chunks;		/* memory chunks for storing the strings */
    size_t chunks_size;		/* current chunk */
    size_t chunks_allocated;	/* allocated size of the chunks array */
    size_t chunk_allocated;	/* size of the current chunk */
    size_t chunk_used;		/* usage of the current chunk */

    poolHash hash;		/* string -> sid hash table */
    int frozen;			/* are new id additions allowed? */
    int nrefs;			/* refcount */
};

/* calculate hash and string length on at once */
static inline unsigned int rstrlenhash(const char * str, size_t * len)
{
    /* Jenkins One-at-a-time hash */
    unsigned int hash = 0xe4721b68;
    const char * s = str;

    while (*s != '\0') {
      hash += *s;
      hash += (hash << 10);
      hash ^= (hash >> 6);
      s++;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    if (len)
	*len = (s - str);

    return hash;
}

static inline unsigned int rstrnlenhash(const char * str, size_t n, size_t * len)
{
    /* Jenkins One-at-a-time hash */
    unsigned int hash = 0xe4721b68;
    const char * s = str;

    while (*s != '\0' && n-- > 0) {
      hash += *s;
      hash += (hash << 10);
      hash ^= (hash >> 6);
      s++;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    if (len)
	*len = (s - str);

    return hash;
}

static inline unsigned int rstrnhash(const char * string, size_t n)
{
    return rstrnlenhash(string, n, NULL);
}

unsigned int rstrhash(const char * string)
{
    return rstrlenhash(string, NULL);
}

static inline unsigned int hashbucket(unsigned int hash, unsigned int number)
{
    return hash + number*number;
}

static poolHash poolHashCreate(int numBuckets)
{
    poolHash ht;

    ht = xmalloc(sizeof(*ht));
    ht->numBuckets = numBuckets;
    ht->buckets = xcalloc(numBuckets, sizeof(*ht->buckets));
    ht->keyCount = 0;
    return ht;
}

static void poolHashResize(rpmstrPool pool, int numBuckets)
{
    poolHash ht = pool->hash;
    poolHashBucket * buckets = xcalloc(numBuckets, sizeof(*ht->buckets));

    for (int i=0; i<ht->numBuckets; i++) {
        if (!ht->buckets[i].keyid) continue;
        unsigned int keyHash = rstrhash(rpmstrPoolStr(pool, ht->buckets[i].keyid));
        for (unsigned int j=0;;j++) {
            unsigned int hash = hashbucket(keyHash, j) % numBuckets;
            if (!buckets[hash].keyid) {
                buckets[hash].keyid = ht->buckets[i].keyid;
                break;
            }
        }
    }
    free((void *)(ht->buckets));
    ht->buckets = buckets;
    ht->numBuckets = numBuckets;
}

static void poolHashAddHEntry(rpmstrPool pool, const char * key, unsigned int keyHash, rpmsid keyid)
{
    poolHash ht = pool->hash;

    /* keep load factor between 0.25 and 0.5 */
    if (2*(ht->keyCount) > ht->numBuckets) {
        poolHashResize(pool, ht->numBuckets * 2);
    }

    for (unsigned int i=0;;i++) {
        unsigned int hash = hashbucket(keyHash, i) % ht->numBuckets;
        if (!ht->buckets[hash].keyid) {
            ht->buckets[hash].keyid = keyid;
            ht->keyCount++;
            break;
        } else if (!strcmp(rpmstrPoolStr(pool, ht->buckets[hash].keyid), key)) {
            return;
        }
    }
}

static void poolHashAddEntry(rpmstrPool pool, const char * key, rpmsid keyid)
{
    poolHashAddHEntry(pool, key, rstrhash(key), keyid);
}

static void poolHashEmpty( poolHash ht)
{
    unsigned int i;

    if (ht->keyCount == 0) return;

    for (i = 0; i < ht->numBuckets; i++) {
	ht->buckets[i].keyid = 0;
    }
    ht->keyCount = 0;
}

static poolHash poolHashFree(poolHash ht)
{
    if (ht==NULL)
        return ht;
    poolHashEmpty(ht);
    ht->buckets = _free(ht->buckets);
    ht = _free(ht);

    return NULL;
}

static void poolHashPrintStats(rpmstrPool pool)
{
    poolHash ht = pool->hash;
    int i;
    int collisions = 0;
    int maxcollisions = 0;

    for (i=0; i<ht->numBuckets; i++) {
        unsigned int keyHash = rstrhash(rpmstrPoolStr(pool, ht->buckets[i].keyid));
        for (unsigned int j=0;;j++) {
            unsigned int hash = hashbucket(keyHash, i) % ht->numBuckets;
            if (hash==i) {
                collisions += j;
                maxcollisions = maxcollisions > j ? maxcollisions : j;
                break;
            }
        }
    }
    fprintf(stderr, "Hashsize: %i\n", ht->numBuckets);
    fprintf(stderr, "Keys: %i\n", ht->keyCount);
    fprintf(stderr, "Collisions: %i\n", collisions);
    fprintf(stderr, "Max collisions: %i\n", maxcollisions);
}

static void rpmstrPoolRehash(rpmstrPool pool)
{
    int sizehint;

    if (pool->offs_size < STRHASH_INITSIZE)
	sizehint = STRHASH_INITSIZE;
    else
	sizehint = pool->offs_size * 2;

    if (pool->hash)
	pool->hash = poolHashFree(pool->hash);

    pool->hash = poolHashCreate(sizehint);
    for (int i = 1; i <= pool->offs_size; i++)
	poolHashAddEntry(pool, rpmstrPoolStr(pool, i), i);
}

rpmstrPool rpmstrPoolCreate(void)
{
    rpmstrPool pool = xcalloc(1, sizeof(*pool));

    pool->offs_alloced = STROFFS_CHUNK;
    pool->offs = xcalloc(pool->offs_alloced, sizeof(*pool->offs));

    pool->chunks_allocated = STRDATA_CHUNKS;
    pool->chunks = xcalloc(pool->chunks_allocated, sizeof(*pool->chunks));
    pool->chunks_size = 1;
    pool->chunk_allocated = STRDATA_CHUNK;
    pool->chunks[pool->chunks_size] = xcalloc(1, pool->chunk_allocated);
    pool->offs[1] = pool->chunks[pool->chunks_size];

    rpmstrPoolRehash(pool);
    pool->nrefs = 1;
    return pool;
}

rpmstrPool rpmstrPoolFree(rpmstrPool pool)
{
    if (pool) {
	if (pool->nrefs > 1) {
	    pool->nrefs--;
	} else {
	    if (pool_debug)
		poolHashPrintStats(pool);
	    poolHashFree(pool->hash);
	    free(pool->offs);
	    for (int i=1;i<=pool->chunks_size;i++) {
		pool->chunks[i] = _free(pool->chunks[i]);
	    }
	    free(pool->chunks);
	    free(pool);
	}
    }
    return NULL;
}

rpmstrPool rpmstrPoolLink(rpmstrPool pool)
{
    if (pool)
	pool->nrefs++;
    return pool;
}

void rpmstrPoolFreeze(rpmstrPool pool, int keephash)
{
    if (pool && !pool->frozen) {
	if (!keephash) {
	    pool->hash = poolHashFree(pool->hash);
	}
	pool->offs_alloced = pool->offs_size + 2; /* space for end marker */
	pool->offs = xrealloc(pool->offs,
			      pool->offs_alloced * sizeof(*pool->offs));
	pool->frozen = 1;
    }
}

void rpmstrPoolUnfreeze(rpmstrPool pool)
{
    if (pool) {
	if (pool->hash == NULL) {
	    rpmstrPoolRehash(pool);
	}
	pool->frozen = 0;
    }
}

static rpmsid rpmstrPoolPut(rpmstrPool pool, const char *s, size_t slen, unsigned int hash)
{
    char *t = NULL;
    size_t ssize = slen + 1;

    pool->offs_size += 1;
    if (pool->offs_alloced <= pool->offs_size) {
	pool->offs_alloced += STROFFS_CHUNK;
	pool->offs = xrealloc(pool->offs,
			      pool->offs_alloced * sizeof(*pool->offs));
    }

    /* Do we need a new chunk to store the string? */
    if (ssize > pool->chunk_allocated - pool->chunk_used) {
	pool->chunks_size += 1;
	/* Grow chunks array if needed */
	if (pool->chunks_size >= pool->chunks_allocated) {
	    pool->chunks_allocated += pool->chunks_allocated;
	    pool->chunks = xrealloc(pool->chunks,
				pool->chunks_allocated * sizeof(*pool->chunks));
	}

	/* Ensure the string fits in the new chunk we're about to allocate */
	if (ssize > pool->chunk_allocated) {
	    pool->chunk_allocated = 2 * ssize;
	}

	pool->chunks[pool->chunks_size] = xcalloc(1, pool->chunk_allocated);
	pool->chunk_used = 0;
    }

    /* Copy the string into current chunk, ensure termination */
    t = memcpy(pool->chunks[pool->chunks_size] + pool->chunk_used, s, slen);
    t[slen] = '\0';
    pool->chunk_used += ssize;

    /* Actually add the string to the pool */
    pool->offs[pool->offs_size] = t;
    poolHashAddHEntry(pool, t, hash, pool->offs_size);

    return pool->offs_size;
}

static rpmsid rpmstrPoolGet(rpmstrPool pool, const char * key, size_t keylen,
			    unsigned int keyHash)
{
    poolHash ht = pool->hash;
    const char * s;


    for (unsigned int i=0;; i++) {
        unsigned int hash = hashbucket(keyHash, i) % ht->numBuckets;

        if (!ht->buckets[hash].keyid) {
            return 0;
        }

	s = rpmstrPoolStr(pool, ht->buckets[hash].keyid);
	/* pool string could be longer than keylen, require exact matche */
	if (strncmp(s, key, keylen) == 0 && s[keylen] == '\0')
	    return ht->buckets[hash].keyid;
    }
}

static inline rpmsid strn2id(rpmstrPool pool, const char *s, size_t slen,
			     unsigned int hash, int create)
{
    rpmsid sid = 0;

    if (pool && pool->hash) {
	sid = rpmstrPoolGet(pool, s, slen, hash);
	if (sid == 0 && create && !pool->frozen)
	    sid = rpmstrPoolPut(pool, s, slen, hash);
    }
    return sid;
}

rpmsid rpmstrPoolIdn(rpmstrPool pool, const char *s, size_t slen, int create)
{
    rpmsid sid = 0;

    if (s != NULL) {
	unsigned int hash = rstrnhash(s, slen);
	sid = strn2id(pool, s, slen, hash, create);
    }
    return sid;
}

rpmsid rpmstrPoolId(rpmstrPool pool, const char *s, int create)
{
    rpmsid sid = 0;

    if (s != NULL) {
	size_t slen;
	unsigned int hash = rstrlenhash(s, &slen);
	sid = strn2id(pool, s, slen, hash, create);
    }
    return sid;
}

const char * rpmstrPoolStr(rpmstrPool pool, rpmsid sid)
{
    const char *s = NULL;
    if (pool && sid > 0 && sid <= pool->offs_size)
	s = pool->offs[sid];
    return s;
}

size_t rpmstrPoolStrlen(rpmstrPool pool, rpmsid sid)
{
    size_t slen = 0;
    if (pool && sid > 0 && sid <= pool->offs_size) {
	slen = strlen(pool->offs[sid]);
    }
    return slen;
}

int rpmstrPoolStreq(rpmstrPool poolA, rpmsid sidA,
		    rpmstrPool poolB, rpmsid sidB)
{
    if (poolA == poolB)
	return (sidA == sidB);
    else
	return rstreq(rpmstrPoolStr(poolA, sidA), rpmstrPoolStr(poolB, sidB));
}

rpmsid rpmstrPoolNumStr(rpmstrPool pool)
{
    return (pool != NULL) ? pool->offs_size : 0;
}
