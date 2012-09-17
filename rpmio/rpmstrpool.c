#include "system.h"
#include <rpm/rpmstring.h>
#include <rpm/rpmstrpool.h>
#include "debug.h"

#define HASHTYPE poolHash
#define HTKEYTYPE const char *
#define HTDATATYPE rpmsid
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

#define STRDATA_CHUNK 65536
#define STROFFS_CHUNK 2048
/* XXX this is ridiculously small... */
#define STRHASH_INITSIZE 5

struct rpmstrPool_s {
    size_t * offs;		/* offsets into data area */
    rpmsid offs_size;		/* largest offset index */;
    rpmsid offs_alloced;	/* offsets allocation size */
    char * data;		/* string data area */
    size_t data_size;		/* string data area size */
    size_t data_alloced;	/* string data area allocation size */
    poolHash hash;		/* string -> sid hash table */
    int frozen;			/* are new id additions allowed? */
    int nrefs;			/* refcount */
};

static void rpmstrPoolRehash(rpmstrPool pool)
{
    int sizehint;

    if (pool->offs_size < STRHASH_INITSIZE)
	sizehint = STRHASH_INITSIZE;
    else
	sizehint = pool->offs_size * 2;

    if (pool->hash)
	pool->hash = poolHashFree(pool->hash);

    pool->hash = poolHashCreate(sizehint, rstrhash, strcmp, NULL, NULL);
    for (int i = 1; i < pool->offs_size; i++)
	poolHashAddEntry(pool->hash, rpmstrPoolStr(pool, i), i);
}

rpmstrPool rpmstrPoolCreate(void)
{
    rpmstrPool pool = xcalloc(1, sizeof(*pool));
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
	    poolHashFree(pool->hash);
	    free(pool->offs);
	    free(pool->data);
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
	/*
	 * realloc() might require rehashing even when downsizing,
	 * dont bother unless we're also discarding the hash.
	 */
	if (!keephash) {
	    pool->hash = poolHashFree(pool->hash);
	    pool->data_alloced = pool->data_size;
	    pool->data = xrealloc(pool->data, pool->data_alloced);
	    pool->offs_alloced = pool->offs_size + 1;
	    pool->offs = xrealloc(pool->offs,
				  pool->offs_alloced * sizeof(*pool->offs));
	}
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
    const char *t = NULL;
    size_t ssize = slen + 1;

    if (ssize > pool->data_alloced - pool->data_size) {
	const char * prev_data = pool->data;
	size_t need = pool->data_size + ssize;
	size_t alloced = pool->data_alloced;

	while (alloced < need)
	    alloced += STRDATA_CHUNK;

	pool->data = xrealloc(pool->data, alloced);
	pool->data_alloced = alloced;

	/* ouch, need to rehash the whole lot if key addresses change */
	if (pool->offs_size > 0 && pool->data != prev_data) {
	    rpmstrPoolRehash(pool);
	}
    }

    pool->offs_size += 1;
    if (pool->offs_alloced <= pool->offs_size) {
	pool->offs_alloced += STROFFS_CHUNK;
	pool->offs = xrealloc(pool->offs,
			      pool->offs_alloced * sizeof(*pool->offs));
    }

    t = memcpy(pool->data + pool->data_size, s, ssize);
    pool->offs[pool->offs_size] = pool->data_size;
    pool->data_size += ssize;

    poolHashAddHEntry(pool->hash, t, hash, pool->offs_size);

    return pool->offs_size;
}

rpmsid rpmstrPoolIdn(rpmstrPool pool, const char *s, size_t slen, int create)
{
    rpmsid sid = 0;

    if (pool && pool->hash && s) {
	unsigned int hash = poolHashKeyHash(pool->hash, s);
	rpmsid *sids;
	if (poolHashGetHEntry(pool->hash, s, hash, &sids, NULL, NULL)) {
	    sid = sids[0];
	} else if (create && !pool->frozen) {
	    sid = rpmstrPoolPut(pool, s, slen, hash);
	}
    }

    return sid;
}

rpmsid rpmstrPoolId(rpmstrPool pool, const char *s, int create)
{
    return rpmstrPoolIdn(pool, s, strlen(s), create);
}

const char * rpmstrPoolStr(rpmstrPool pool, rpmsid sid)
{
    const char *s = NULL;
    if (pool && sid > 0 && sid <= pool->offs_size)
	s = pool->data + pool->offs[sid];
    return s;
}

size_t rpmstrPoolStrlen(rpmstrPool pool, rpmsid sid)
{
    size_t slen = 0;
    if (pool && sid <= pool->offs_size) {
	size_t end = (sid < pool->offs_size) ? pool->offs[sid + 1] :
					       pool->offs_size;
	slen = end - pool->offs[sid] - 1;
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
