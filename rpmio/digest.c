/** \ingroup signature
 * \file rpmio/digest.c
 */

#include "system.h"

#include <map>

#include <rpm/rpmcrypto.h>

#include "debug.h"

struct rpmDigestBundle_s {
    std::map<int,DIGEST_CTX> digs;	/*!< ID based map of digests. */
};

rpmDigestBundle rpmDigestBundleNew(void)
{
    rpmDigestBundle bundle = new rpmDigestBundle_s {};
    return bundle;
}

rpmDigestBundle rpmDigestBundleFree(rpmDigestBundle bundle)
{
    if (bundle) {
	for (auto & it : bundle->digs) {
	    rpmDigestFinal(it.second, NULL, NULL, 0);
	}
	delete bundle;
    }
    return NULL;
}

int rpmDigestBundleAdd(rpmDigestBundle bundle, int algo,
			rpmDigestFlags flags)
{
    return rpmDigestBundleAddID(bundle, algo, algo, flags);
}

int rpmDigestBundleAddID(rpmDigestBundle bundle, int algo, int id,
			rpmDigestFlags flags)
{
    int rc = -1;
    if (id > 0) {
	DIGEST_CTX ctx = rpmDigestInit(algo, flags);
	if (ctx) {
	    auto ret = bundle->digs.insert({id, ctx});
	    if (ret.second == true) {
		rc = 0;
	    } else {
		rpmDigestFinal(ctx, NULL, NULL, 0);
	    }
	}
    }
    return rc;
}
int rpmDigestBundleUpdate(rpmDigestBundle bundle, const void *data, size_t len)
{
    int rc = -1;
    if (bundle && data && len > 0) {
	rc = 0;
	for (auto & dig : bundle->digs) {
	    rc += rpmDigestUpdate(dig.second, data, len);
	}
    }
    return rc;
}

int rpmDigestBundleFinal(rpmDigestBundle bundle, int id,
			 void ** datap, size_t * lenp, int asAscii)
{
    int rc = -1;
    if (bundle) {
	auto it = bundle->digs.find(id);
	if (it != bundle->digs.end()) {
	    rc = rpmDigestFinal(it->second, datap, lenp, asAscii);
	    bundle->digs.erase(it);
	}
    }
    return rc;
}

DIGEST_CTX rpmDigestBundleDupCtx(rpmDigestBundle bundle, int id)
{
    DIGEST_CTX dup = NULL;
    if (bundle) {
	auto it = bundle->digs.find(id);
	if (it != bundle->digs.end()) {
	    dup = rpmDigestDup(it->second);
	}
    }
    return dup;
}

