/** \ingroup signature
 * \file rpmio/digest.c
 */

#include "system.h"

#include "rpmio/digest.h"

#include "debug.h"

#define DIGESTS_MAX 12
struct rpmDigestBundle_s {
    int index_min;			/*!< Smallest index of active digest */
    int index_max;			/*!< Largest index of active digest */
    off_t nbytes;			/*!< Length of total input data */
    DIGEST_CTX digests[DIGESTS_MAX];	/*!< Digest contexts indexed by algo */
};

rpmDigestBundle rpmDigestBundleNew(void)
{
    rpmDigestBundle bundle = xcalloc(1, sizeof(*bundle));
    return bundle;
}

rpmDigestBundle rpmDigestBundleFree(rpmDigestBundle bundle)
{
    if (bundle) {
	for (int i = bundle->index_min; i <= bundle->index_max ; i++) {
	    if (bundle->digests[i] == NULL)
		continue;
	    rpmDigestFinal(bundle->digests[i], NULL, NULL, 0);
	    bundle->digests[i] = NULL;
	}
	memset(bundle, 0, sizeof(*bundle));
	free(bundle);
    }
    return NULL;
}

int rpmDigestBundleAdd(rpmDigestBundle bundle, int algo,
			rpmDigestFlags flags)
{
    DIGEST_CTX ctx = NULL;
    if (bundle && algo > 0 && algo < DIGESTS_MAX) {
	if (bundle->digests[algo] == NULL) {
	    ctx = rpmDigestInit(algo, flags);
	    if (ctx) {
		bundle->digests[algo] = ctx;
		if (algo < bundle->index_min) {
		    bundle->index_min = algo;
		}
		if (algo > bundle->index_max) {
		    bundle->index_max = algo;
		}
	    }
	}
    }
    return (ctx != NULL);
}

int rpmDigestBundleUpdate(rpmDigestBundle bundle, const void *data, size_t len)
{
    int rc = 0;
    if (bundle && data && len > 0) {
	for (int i = bundle->index_min; i <= bundle->index_max; i++) {
	    DIGEST_CTX ctx = bundle->digests[i];
	    if (ctx == NULL)
		continue;
	    rc += rpmDigestUpdate(ctx, data, len);
	}
	bundle->nbytes += len;
    }
    return rc;
}

int rpmDigestBundleFinal(rpmDigestBundle bundle, 
		int algo, void ** datap, size_t * lenp, int asAscii)
{
    int rc = 0;
    if (bundle && algo >= bundle->index_min && algo <= bundle->index_max) {
	rc = rpmDigestFinal(bundle->digests[algo], datap, lenp, asAscii);
	bundle->digests[algo] = NULL;
    }
    return rc;
}

DIGEST_CTX rpmDigestBundleDupCtx(rpmDigestBundle bundle, int algo)
{
    DIGEST_CTX dup = NULL;
    if (bundle && algo >= bundle->index_min && algo <= bundle->index_max) {
	dup = rpmDigestDup(bundle->digests[algo]);
    }
    return dup;
}

