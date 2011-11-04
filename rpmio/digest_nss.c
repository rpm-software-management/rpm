#include "system.h"

#include <pthread.h>
#include <nss.h>
#include <sechash.h>
#include <keyhi.h>
#include <cryptohi.h>

#include <rpm/rpmlog.h>
#include "rpmio/digest.h"
#include "debug.h"


static int _crypto_initialized = 0;
static int _new_process = 1;

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

    /* Initialize NSS if not already done */
    if (!_crypto_initialized) {
	if (NSS_NoDB_Init(NULL) != SECSuccess) {
	    rc = -1;
	} else {
	    _crypto_initialized = 1;
	}
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
	rc = (NSS_Shutdown() != SECSuccess);
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
    case PGPHASHALGO_MD5:
	return HASH_AlgMD5;
	break;
    case PGPHASHALGO_MD2:
	return HASH_AlgMD2;
	break;
    case PGPHASHALGO_SHA1:
	return HASH_AlgSHA1;
	break;
    case PGPHASHALGO_SHA256:
	return HASH_AlgSHA256;
	break;
    case PGPHASHALGO_SHA384:
	return HASH_AlgSHA384;
	break;
    case PGPHASHALGO_SHA512:
	return HASH_AlgSHA512;
	break;
    case PGPHASHALGO_RIPEMD160:
    case PGPHASHALGO_TIGER192:
    case PGPHASHALGO_HAVAL_5_160:
    default:
	return HASH_AlgNULL;
	break;
    }
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

static int pgpMpiSet(unsigned int lbits, uint8_t *dest,
		     const uint8_t * p, const uint8_t * pend)
{
    unsigned int mbits = pgpMpiBits(p);
    unsigned int nbits;
    size_t nbytes;
    uint8_t *t = dest;
    unsigned int ix;

    if ((p + ((mbits+7) >> 3)) > pend)
	return 1;

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

static SECItem *pgpMpiItem(PRArenaPool *arena, SECItem *item,
			   const uint8_t *p, const uint8_t *pend)
{
    size_t nbytes = pgpMpiLen(p)-2;

    if (p + nbytes + 2 > pend)
	return NULL;

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

#ifndef DSA_SUBPRIME_LEN
#define DSA_SUBPRIME_LEN 20
#endif

static int pgpSetSigMpiDSA(pgpDigAlg pgpsig, int num,
			   const uint8_t *p, const uint8_t *pend)
{
    SECItem *sig = pgpsig->data;
    int lbits = DSA_SUBPRIME_LEN * 8;
    int rc = 1; /* assume failure */

    switch (num) {
    case 0:
	sig = pgpsig->data = SECITEM_AllocItem(NULL, NULL, 2*DSA_SUBPRIME_LEN);
	memset(sig->data, 0, 2 * DSA_SUBPRIME_LEN);
	rc = pgpMpiSet(lbits, sig->data, p, pend);
	break;
    case 1:
	if (sig && pgpMpiSet(lbits, sig->data+DSA_SUBPRIME_LEN, p, pend) == 0) {
	    SECItem *signew = SECITEM_AllocItem(NULL, NULL, 0);
	    if (signew && DSAU_EncodeDerSig(signew, sig) == SECSuccess) {
		SECITEM_FreeItem(sig, PR_TRUE);
		pgpsig->data = signew;
		rc = 0;
	    }
	}
	break;
    }

    return rc;
}

static int pgpSetKeyMpiDSA(pgpDigAlg pgpkey, int num,
			   const uint8_t *p, const uint8_t *pend)
{
    SECItem *mpi = NULL;
    SECKEYPublicKey *key = pgpkey->data;

    if (key == NULL)
	key = pgpkey->data = pgpNewPublicKey(dsaKey);

    if (key) {
	switch (num) {
	case 0:
	    mpi = pgpMpiItem(key->arena, &key->u.dsa.params.prime, p, pend);
	    break;
	case 1:
	    mpi = pgpMpiItem(key->arena, &key->u.dsa.params.subPrime, p, pend);
	    break;
	case 2:
	    mpi = pgpMpiItem(key->arena, &key->u.dsa.params.base, p, pend);
	    break;
	case 3:
	    mpi = pgpMpiItem(key->arena, &key->u.dsa.publicValue, p, pend);
	    break;
	}
    }

    return (mpi == NULL);
}

static int pgpVerifySigDSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
			   uint8_t *hash, size_t hashlen, int hash_algo)
{
    SECItem digest = { .type = siBuffer, .data = hash, .len = hashlen };
    SECOidTag sigalg = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
    SECStatus rc;

    /* XXX VFY_VerifyDigest() is deprecated in NSS 3.12 */ 
    rc = VFY_VerifyDigest(&digest, pgpkey->data, pgpsig->data, sigalg, NULL);

    return (rc != SECSuccess);
}

static int pgpSetSigMpiRSA(pgpDigAlg pgpsig, int num,
			   const uint8_t *p, const uint8_t *pend)
{
    SECItem *sigitem = NULL;

    if (num == 0) {
       sigitem = pgpMpiItem(NULL, pgpsig->data, p, pend);
       if (sigitem)
           pgpsig->data = sigitem;
    }
    return (sigitem == NULL);
}

static int pgpSetKeyMpiRSA(pgpDigAlg pgpkey, int num,
			   const uint8_t *p, const uint8_t *pend)
{
    SECItem *kitem = NULL;
    SECKEYPublicKey *key = pgpkey->data;

    if (key == NULL)
	key = pgpkey->data = pgpNewPublicKey(rsaKey);

    if (key) {
	switch (num) {
	case 0:
	    kitem = pgpMpiItem(key->arena, &key->u.rsa.modulus, p, pend);
	    break;
	case 1:
	    kitem = pgpMpiItem(key->arena, &key->u.rsa.publicExponent, p, pend);
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
    SECOidTag sigalg;
    SECStatus rc = SECFailure;
    size_t siglen, padlen;

    switch (hash_algo) {
    case PGPHASHALGO_MD5:
	sigalg = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_MD2:
	sigalg = SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA1:
	sigalg = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA256:
	sigalg = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA384:
	 sigalg = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA512:
	sigalg = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
	break;
    default:
	return 1; /* dont bother with unknown hash types */
	break;
    }

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

    /* XXX VFY_VerifyDigest() is deprecated in NSS 3.12 */ 
    rc = VFY_VerifyDigest(&digest, key, sig, sigalg, NULL);

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

static int pgpSetMpiNULL(pgpDigAlg pgpkey, int num,
			 const uint8_t *p, const uint8_t *pend)
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

