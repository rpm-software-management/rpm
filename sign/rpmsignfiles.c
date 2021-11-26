/**
 * Copyright (C) 2014 IBM Corporation
 *
 * Author: Fionnuala Gunter <fin@linux.vnet.ibm.com>
 */

#include "system.h"
#include "imaevm.h"

#include <rpm/rpmlog.h>		/* rpmlog */
#include <rpm/rpmfi.h>
#include <rpm/rpmpgp.h>		/* rpmDigestLength */
#include "lib/header.h"		/* HEADERGET_MINMEM */
#include "lib/rpmtypes.h"	/* rpmRC */

#include "sign/rpmsignfiles.h"

#define MAX_SIGNATURE_LENGTH 1024

static const char *hash_algo_name[] = {
    [PGPHASHALGO_MD5]          = "md5",
    [PGPHASHALGO_SHA1]         = "sha1",
    [PGPHASHALGO_RIPEMD160]    = "rmd160",
    [PGPHASHALGO_MD2]          = "md2",
    [PGPHASHALGO_TIGER192]     = "tgr192",
    [PGPHASHALGO_HAVAL_5_160]  = "haval5160",
    [PGPHASHALGO_SHA256]       = "sha256",
    [PGPHASHALGO_SHA384]       = "sha384",
    [PGPHASHALGO_SHA512]       = "sha512",
    [PGPHASHALGO_SHA224]       = "sha224",
};

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof(a[0]))

static char *signFile(const char *algo, const uint8_t *fdigest, int diglen,
const char *key, char *keypass, uint32_t *siglenp)
{
    char *fsignature;
    unsigned char zeros[diglen];
    unsigned char signature[MAX_SIGNATURE_LENGTH];
    int siglen;

    /* some entries don't have a digest - we return an empty signature */
    memset(zeros, 0, diglen);
    if (memcmp(zeros, fdigest, diglen) == 0)
        return strdup("");

    /* prepare file signature */
    memset(signature, 0, MAX_SIGNATURE_LENGTH);
    signature[0] = '\x03';

    /* calculate file signature */
    siglen = sign_hash(algo, fdigest, diglen, key, keypass, signature+1);
    if (siglen < 0) {
	rpmlog(RPMLOG_ERR, _("sign_hash failed\n"));
	return NULL;
    }

    *siglenp = siglen + 1;
    /* convert file signature binary to hex */
    fsignature = pgpHexStr(signature, siglen+1);
    return fsignature;
}

rpmRC rpmSignFiles(Header sigh, Header h, const char *key, char *keypass)
{
    struct rpmtd_s td;
    int algo;
    int diglen;
    uint32_t siglen = 0;
    const char *algoname;
    const uint8_t *digest;
    char *signature = NULL;
    rpmRC rc = RPMRC_FAIL;
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, RPMFI_FLAGS_QUERY);

    if (rpmfiFC(fi) == 0) {
	rc = RPMRC_OK;
	goto exit;
    }

    algo = rpmfiDigestAlgo(fi);
    if (algo >= ARRAY_SIZE(hash_algo_name)) {
	rpmlog(RPMLOG_ERR, _("File digest algorithm id is invalid"));
	goto exit;
    }

    diglen = rpmDigestLength(algo);
    algoname = hash_algo_name[algo];

    headerDel(sigh, RPMTAG_FILESIGNATURELENGTH);
    headerDel(sigh, RPMTAG_FILESIGNATURES);

    rpmtdReset(&td);
    td.tag = RPMSIGTAG_FILESIGNATURES;
    td.type = RPM_STRING_ARRAY_TYPE;
    td.data = NULL; /* set in the loop below */
    td.count = 1;

    while (rpmfiNext(fi) >= 0) {
	uint32_t slen;
	digest = rpmfiFDigest(fi, NULL, NULL);
	signature = signFile(algoname, digest, diglen, key, keypass, &slen);
	if (!signature) {
	    rpmlog(RPMLOG_ERR, _("signFile failed\n"));
	    goto exit;
	}
	td.data = &signature;
	if (!headerPut(sigh, &td, HEADERPUT_APPEND)) {
	    rpmlog(RPMLOG_ERR, _("headerPutString failed\n"));
	    goto exit;
	}
	signature = _free(signature);
	if (slen > siglen)
	    siglen = slen;
    }

    if (siglen > 0) {
	rpmtdReset(&td);
	td.tag = RPMSIGTAG_FILESIGNATURELENGTH;
	td.type = RPM_INT32_TYPE;
	td.data = &siglen;
	td.count = 1;
	headerPut(sigh, &td, HEADERPUT_DEFAULT);
    }

    rc = RPMRC_OK;

exit:
    free(signature);
    rpmfiFree(fi);
    return rc;
}
