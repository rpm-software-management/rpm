#include "system.h"

#include <pthread.h>
#include <nss.h>
#include <sechash.h>
#include <signal.h>
#include <keyhi.h>
#include <cryptohi.h>
#include <blapit.h>

#include <rpm/rpmlog.h>
#include "rpmio/digest.h"
#include "debug.h"


static int _crypto_initialized = 0;
static int _new_process = 1;

#if HAVE_NSS_INITCONTEXT
static NSSInitContext * _nss_ctx = NULL;
#endif

/**
 * MD5/SHA1 digest private data.
 */
struct DIGEST_CTX_s {
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    HASHContext *hashctx;	/*!< Internal NSS hash context. */
    int algo;			/*!< Used hash algorithm */
};

/*
 * Only flag for re-initialization here, in the common case the child
 * exec()'s something else shutting down NSS here would be waste of time.
 */
static void at_forkchild(void)
{
    _new_process = 1;
}

int rpmInitCrypto(void)
{
    int rc = 0;

    /* Lazy NSS shutdown for re-initialization after fork() */
    if (_new_process && _crypto_initialized) {
	rpmFreeCrypto();
    }

    /*
     * Initialize NSS if not already done.
     * NSS prior to 3.12.5 only supports a global context which can cause
     * trouble when an API user wants to use NSS for their own purposes, use
     * a private context if possible.
     */
    if (!_crypto_initialized) {
	/* NSPR sets SIGPIPE to ignore behind our back, save and restore */
	struct sigaction oact;
	sigaction(SIGPIPE, NULL, &oact);
#if HAVE_NSS_INITCONTEXT
	PRUint32 flags = (NSS_INIT_READONLY|NSS_INIT_NOCERTDB|
			  NSS_INIT_NOMODDB|NSS_INIT_FORCEOPEN|
			  NSS_INIT_NOROOTINIT|NSS_INIT_OPTIMIZESPACE);
	_nss_ctx = NSS_InitContext(NULL, NULL, NULL, NULL, NULL, flags);
	if (_nss_ctx == NULL) {
#else
	if (NSS_NoDB_Init(NULL) != SECSuccess) {
#endif
	    rpmlog(RPMLOG_ERR, _("Failed to initialize NSS library\n"));
	    rc = -1;
	} else {
	    _crypto_initialized = 1;
	}
	sigaction(SIGPIPE, &oact, NULL);
    }

    /* Register one post-fork handler per process */
    if (_new_process) {
	if (pthread_atfork(NULL, NULL, at_forkchild) != 0) {
	    rpmlog(RPMLOG_WARNING, _("Failed to register fork handler: %m\n"));
	}
	_new_process = 0;
    }
    return rc;
}

int rpmFreeCrypto(void) 
{
    int rc = 0;
    if (_crypto_initialized) {
#if HAVE_NSS_INITCONTEXT
	rc = (NSS_ShutdownContext(_nss_ctx) != SECSuccess);
	_nss_ctx = NULL;
#else
	rc = (NSS_Shutdown() != SECSuccess);
#endif
    	_crypto_initialized = 0;
    }
    return rc;
}

DIGEST_CTX rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx = NULL;
    if (octx) {
	HASHContext *hctx = HASH_Clone(octx->hashctx);
	if (hctx) {
	    nctx = memcpy(xcalloc(1, sizeof(*nctx)), octx, sizeof(*nctx));
	    nctx->hashctx = hctx;
	}
    }
    return nctx;
}

RPM_GNUC_PURE
static HASH_HashType getHashType(int hashalgo)
{
    switch (hashalgo) {
    case PGPHASHALGO_MD5:	return HASH_AlgMD5;
    case PGPHASHALGO_SHA1:	return HASH_AlgSHA1;
#ifdef SHA224_LENGTH
    case PGPHASHALGO_SHA224:	return HASH_AlgSHA224;
#endif
    case PGPHASHALGO_SHA256:	return HASH_AlgSHA256;
    case PGPHASHALGO_SHA384:	return HASH_AlgSHA384;
    case PGPHASHALGO_SHA512:	return HASH_AlgSHA512;
    }
    return HASH_AlgNULL;
}

size_t rpmDigestLength(int hashalgo)
{
    return HASH_ResultLen(getHashType(hashalgo));
}

DIGEST_CTX rpmDigestInit(int hashalgo, rpmDigestFlags flags)
{
    HASH_HashType type = getHashType(hashalgo);
    HASHContext *hashctx = NULL;
    DIGEST_CTX ctx = NULL;

    if (type == HASH_AlgNULL || rpmInitCrypto() < 0)
	goto exit;

    if ((hashctx = HASH_Create(type)) != NULL) {
	ctx = xcalloc(1, sizeof(*ctx));
	ctx->flags = flags;
	ctx->algo = hashalgo;
	ctx->hashctx = hashctx;
    	HASH_Begin(ctx->hashctx);
    }
    
exit:
    return ctx;
}

int rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
    size_t partlen;
    const unsigned char *ptr = data;

    if (ctx == NULL)
	return -1;

   partlen = ~(unsigned int)0xFF;
   while (len > 0) {
   	if (len < partlen) {
   		partlen = len;
   	}
	HASH_Update(ctx->hashctx, ptr, partlen);
	ptr += partlen;
	len -= partlen;
   }
   return 0;
}

int rpmDigestFinal(DIGEST_CTX ctx, void ** datap, size_t *lenp, int asAscii)
{
    unsigned char * digest;
    unsigned int digestlen;

    if (ctx == NULL)
	return -1;
    digestlen = HASH_ResultLenContext(ctx->hashctx);
    digest = xmalloc(digestlen);

/* FIX: check rc */
    HASH_End(ctx->hashctx, digest, (unsigned int *) &digestlen, digestlen);

    /* Return final digest. */
    if (!asAscii) {
	if (lenp) *lenp = digestlen;
	if (datap) {
	    *datap = digest;
	    digest = NULL;
	}
    } else {
	if (lenp) *lenp = (2*digestlen) + 1;
	if (datap) {
	    const uint8_t * s = (const uint8_t *) digest;
	    *datap = pgpHexStr(s, digestlen);
	}
    }
    if (digest) {
	memset(digest, 0, digestlen);	/* In case it's sensitive */
	free(digest);
    }
    HASH_Destroy(ctx->hashctx);
    memset(ctx, 0, sizeof(*ctx));	/* In case it's sensitive */
    free(ctx);
    return 0;
}

RPM_GNUC_PURE
static SECOidTag getHashAlg(unsigned int hashalgo)
{
    switch (hashalgo) {
    case PGPHASHALGO_MD5:	return SEC_OID_MD5;
    case PGPHASHALGO_SHA1:	return SEC_OID_SHA1;
#ifdef SHA224_LENGTH
    case PGPHASHALGO_SHA224:	return SEC_OID_SHA224;
#endif
    case PGPHASHALGO_SHA256:	return SEC_OID_SHA256;
    case PGPHASHALGO_SHA384:	return SEC_OID_SHA384;
    case PGPHASHALGO_SHA512:	return SEC_OID_SHA512;
    }
    return SEC_OID_UNKNOWN;
}

static int pgpMpiSet(unsigned int lbits, uint8_t *dest, const uint8_t * p)
{
    unsigned int mbits = pgpMpiBits(p);
    unsigned int nbits;
    size_t nbytes;
    uint8_t *t = dest;
    unsigned int ix;

    if (mbits > lbits)
	return 1;

    nbits = (lbits > mbits ? lbits : mbits);
    nbytes = ((nbits + 7) >> 3);
    ix = (nbits - mbits) >> 3;

    if (ix > 0)
	memset(t, '\0', ix);
    memcpy(t+ix, p+2, nbytes-ix);

    return 0;
}

static SECItem *pgpMpiItem(PRArenaPool *arena, SECItem *item, const uint8_t *p)
{
    size_t nbytes = pgpMpiLen(p)-2;

    if (item == NULL) {
    	if ((item=SECITEM_AllocItem(arena, item, nbytes)) == NULL)
    	    return item;
    } else {
    	if (arena != NULL)
    	    item->data = PORT_ArenaGrow(arena, item->data, item->len, nbytes);
    	else
    	    item->data = PORT_Realloc(item->data, nbytes);
    	
    	if (item->data == NULL) {
    	    if (arena == NULL)
    		SECITEM_FreeItem(item, PR_TRUE);
    	    return NULL;
    	}
    }

    memcpy(item->data, p+2, nbytes);
    item->len = nbytes;
    return item;
}

static SECKEYPublicKey *pgpNewPublicKey(KeyType type)
{
    PRArenaPool *arena;
    SECKEYPublicKey *key;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	return NULL;
    
    key = PORT_ArenaZAlloc(arena, sizeof(SECKEYPublicKey));
    
    if (key == NULL) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }
    
    key->keyType = type;
    key->pkcs11ID = CK_INVALID_HANDLE;
    key->pkcs11Slot = NULL;
    key->arena = arena;
    return key;
}

/* compatibility with nss < 3.14 */
#ifndef DSA1_SUBPRIME_LEN
#define DSA1_SUBPRIME_LEN DSA_SUBPRIME_LEN
#endif
#ifndef DSA1_SIGNATURE_LEN
#define DSA1_SIGNATURE_LEN DSA_SIGNATURE_LEN
#endif
#ifndef DSA1_Q_BITS
#define DSA1_Q_BITS DSA_Q_BITS
#endif

static int pgpSetSigMpiDSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    SECItem *sig = pgpsig->data;
    unsigned int qbits = DSA1_Q_BITS;
    unsigned int subprlen = DSA1_SUBPRIME_LEN;
    unsigned int siglen = DSA1_SIGNATURE_LEN;
    int rc = 1; /* assume failure */

    switch (num) {
    case 0:
	sig = pgpsig->data = SECITEM_AllocItem(NULL, NULL, siglen);
	if (sig) {
	    memset(sig->data, 0, siglen);
	    rc = pgpMpiSet(qbits, sig->data, p);
	}
	break;
    case 1:
	if (sig && pgpMpiSet(qbits, sig->data+subprlen, p) == 0) {
	    SECItem *signew = SECITEM_AllocItem(NULL, NULL, 0);
	    if (signew == NULL)
		break;
	    if (DSAU_EncodeDerSigWithLen(signew, sig, siglen) == SECSuccess) {
		SECITEM_FreeItem(sig, PR_TRUE);
		pgpsig->data = signew;
		rc = 0;
	    }
	}
	break;
    }

    return rc;
}

static int pgpSetKeyMpiDSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    SECItem *mpi = NULL;
    SECKEYPublicKey *key = pgpkey->data;

    if (key == NULL)
	key = pgpkey->data = pgpNewPublicKey(dsaKey);

    if (key) {
	switch (num) {
	case 0:
	    mpi = pgpMpiItem(key->arena, &key->u.dsa.params.prime, p);
	    break;
	case 1:
	    mpi = pgpMpiItem(key->arena, &key->u.dsa.params.subPrime, p);
	    break;
	case 2:
	    mpi = pgpMpiItem(key->arena, &key->u.dsa.params.base, p);
	    break;
	case 3:
	    mpi = pgpMpiItem(key->arena, &key->u.dsa.publicValue, p);
	    break;
	}
    }

    return (mpi == NULL);
}

static int pgpVerifySigDSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
			   uint8_t *hash, size_t hashlen, int hash_algo)
{
    SECItem digest = { .type = siBuffer, .data = hash, .len = hashlen };
    SECOidTag encAlg = SEC_OID_ANSIX9_DSA_SIGNATURE;
    SECOidTag hashAlg = getHashAlg(hash_algo);
    SECStatus rc;

    if (hashAlg == SEC_OID_UNKNOWN)
	return 1;

    rc = VFY_VerifyDigestDirect(&digest, pgpkey->data, pgpsig->data,
				encAlg, hashAlg, NULL);

    return (rc != SECSuccess);
}

static int pgpSetSigMpiRSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    SECItem *sigitem = NULL;

    if (num == 0) {
       sigitem = pgpMpiItem(NULL, pgpsig->data, p);
       if (sigitem)
           pgpsig->data = sigitem;
    }
    return (sigitem == NULL);
}

static int pgpSetKeyMpiRSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    SECItem *kitem = NULL;
    SECKEYPublicKey *key = pgpkey->data;

    if (key == NULL)
	key = pgpkey->data = pgpNewPublicKey(rsaKey);

    if (key) {
	switch (num) {
	case 0:
	    kitem = pgpMpiItem(key->arena, &key->u.rsa.modulus, p);
	    break;
	case 1:
	    kitem = pgpMpiItem(key->arena, &key->u.rsa.publicExponent, p);
	    break;
	}
    }

    return (kitem == NULL);
}

static int pgpVerifySigRSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
			   uint8_t *hash, size_t hashlen, int hash_algo)
{
    SECItem digest = { .type = siBuffer, .data = hash, .len = hashlen };
    SECItem *sig = pgpsig->data;
    SECKEYPublicKey *key = pgpkey->data;
    SECItem *padded = NULL;
    SECOidTag encAlg = SEC_OID_PKCS1_RSA_ENCRYPTION;
    SECOidTag hashAlg = getHashAlg(hash_algo);
    SECStatus rc = SECFailure;
    size_t siglen, padlen;

    if (hashAlg == SEC_OID_UNKNOWN)
	return 1;

    /* Zero-pad signature to expected size if necessary */
    siglen = SECKEY_SignatureLen(key);
    padlen = siglen - sig->len;
    if (padlen) {
	padded = SECITEM_AllocItem(NULL, NULL, siglen);
	if (padded == NULL)
	    return 1;
	memset(padded->data, 0, padlen);
	memcpy(padded->data + padlen, sig->data, sig->len);
	sig = padded;
    }

    rc = VFY_VerifyDigestDirect(&digest, key, sig, encAlg, hashAlg, NULL);

    if (padded)
	SECITEM_ZfreeItem(padded, PR_TRUE);

    return (rc != SECSuccess);
}

static void pgpFreeSigRSADSA(pgpDigAlg sa)
{
    SECITEM_ZfreeItem(sa->data, PR_TRUE);
    sa->data = NULL;
}

static void pgpFreeKeyRSADSA(pgpDigAlg ka)
{
    SECKEY_DestroyPublicKey(ka->data);
    ka->data = NULL;
}

static int pgpSetMpiNULL(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    return 1;
}

static int pgpVerifyNULL(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
			 uint8_t *hash, size_t hashlen, int hash_algo)
{
    return 1;
}

pgpDigAlg pgpPubkeyNew(int algo)
{
    pgpDigAlg ka = xcalloc(1, sizeof(*ka));;

    switch (algo) {
    case PGPPUBKEYALGO_RSA:
	ka->setmpi = pgpSetKeyMpiRSA;
	ka->free = pgpFreeKeyRSADSA;
	ka->mpis = 2;
	break;
    case PGPPUBKEYALGO_DSA:
	ka->setmpi = pgpSetKeyMpiDSA;
	ka->free = pgpFreeKeyRSADSA;
	ka->mpis = 4;
	break;
    default:
	ka->setmpi = pgpSetMpiNULL;
	ka->mpis = -1;
	break;
    }

    ka->verify = pgpVerifyNULL; /* keys can't be verified */

    return ka;
}

pgpDigAlg pgpSignatureNew(int algo)
{
    pgpDigAlg sa = xcalloc(1, sizeof(*sa));

    switch (algo) {
    case PGPPUBKEYALGO_RSA:
	sa->setmpi = pgpSetSigMpiRSA;
	sa->free = pgpFreeSigRSADSA;
	sa->verify = pgpVerifySigRSA;
	sa->mpis = 1;
	break;
    case PGPPUBKEYALGO_DSA:
	sa->setmpi = pgpSetSigMpiDSA;
	sa->free = pgpFreeSigRSADSA;
	sa->verify = pgpVerifySigDSA;
	sa->mpis = 2;
	break;
    default:
	sa->setmpi = pgpSetMpiNULL;
	sa->verify = pgpVerifyNULL;
	sa->mpis = -1;
	break;
    }
    return sa;
}

