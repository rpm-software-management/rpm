#include "system.h"

#include <pthread.h>

#include <rpm/rpmstring.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmkeyring.h>
#include <rpm/rpmbase64.h>

#include "rpmio/digest.h"

#include "debug.h"

struct rpmPubkey_s {
    uint8_t *pkt;
    size_t pktlen;
    pgpKeyID_t keyid;
    pgpDigParams pgpkey;
    int nrefs;
    pthread_rwlock_t lock;
};

struct rpmKeyring_s {
    struct rpmPubkey_s **keys;
    size_t numkeys;
    int nrefs;
    pthread_rwlock_t lock;
};

static int keyidcmp(const void *k1, const void *k2)
{
    const struct rpmPubkey_s *key1 = *(const struct rpmPubkey_s **) k1;
    const struct rpmPubkey_s *key2 = *(const struct rpmPubkey_s **) k2;
    
    return memcmp(key1->keyid, key2->keyid, sizeof(key1->keyid));
}

rpmKeyring rpmKeyringNew(void)
{
    rpmKeyring keyring = xcalloc(1, sizeof(*keyring));
    keyring->keys = NULL;
    keyring->numkeys = 0;
    keyring->nrefs = 1;
    pthread_rwlock_init(&keyring->lock, NULL);
    return keyring;
}

rpmKeyring rpmKeyringFree(rpmKeyring keyring)
{
    if (keyring == NULL)
	return NULL;

    pthread_rwlock_wrlock(&keyring->lock);
    if (--keyring->nrefs == 0) {
	if (keyring->keys) {
	    for (int i = 0; i < keyring->numkeys; i++) {
		keyring->keys[i] = rpmPubkeyFree(keyring->keys[i]);
	    }
	    free(keyring->keys);
	}
	pthread_rwlock_unlock(&keyring->lock);
	pthread_rwlock_destroy(&keyring->lock);
	free(keyring);
    } else {
	pthread_rwlock_unlock(&keyring->lock);
    }
    return NULL;
}

static rpmPubkey rpmKeyringFindKeyid(rpmKeyring keyring, rpmPubkey key)
{
    rpmPubkey *found = NULL;
    found = bsearch(&key, keyring->keys, keyring->numkeys, sizeof(*keyring->keys), keyidcmp);
    return found ? *found : NULL;
}

int rpmKeyringAddKey(rpmKeyring keyring, rpmPubkey key)
{
    int rc = 1; /* assume already seen key */
    if (keyring == NULL || key == NULL)
	return -1;

    /* check if we already have this key, but always wrlock for simplicity */
    pthread_rwlock_wrlock(&keyring->lock);
    if (!rpmKeyringFindKeyid(keyring, key)) {
	keyring->keys = xrealloc(keyring->keys,
				 (keyring->numkeys + 1) * sizeof(rpmPubkey));
	keyring->keys[keyring->numkeys] = rpmPubkeyLink(key);
	keyring->numkeys++;
	qsort(keyring->keys, keyring->numkeys, sizeof(*keyring->keys),
		keyidcmp);
	rc = 0;
    }
    pthread_rwlock_unlock(&keyring->lock);

    return rc;
}

rpmKeyring rpmKeyringLink(rpmKeyring keyring)
{
    if (keyring) {
	pthread_rwlock_wrlock(&keyring->lock);
	keyring->nrefs++;
	pthread_rwlock_unlock(&keyring->lock);
    }
    return keyring;
}

rpmPubkey rpmPubkeyRead(const char *filename)
{
    uint8_t *pkt = NULL;
    size_t pktlen;
    rpmPubkey key = NULL;

    if (pgpReadPkts(filename, &pkt, &pktlen) <= 0) {
	goto exit;
    }
    key = rpmPubkeyNew(pkt, pktlen);
    free(pkt);

exit:
    return key;
}

rpmPubkey rpmPubkeyNew(const uint8_t *pkt, size_t pktlen)
{
    rpmPubkey key = NULL;
    pgpDigParams pgpkey = NULL;
    pgpKeyID_t keyid;
    
    if (pkt == NULL || pktlen == 0)
	goto exit;

    if (pgpPubkeyFingerprint(pkt, pktlen, keyid))
	goto exit;

    if (pgpPrtParams(pkt, pktlen, PGPTAG_PUBLIC_KEY, &pgpkey))
	goto exit;

    key = xcalloc(1, sizeof(*key));
    key->pkt = xmalloc(pktlen);
    key->pktlen = pktlen;
    key->pgpkey = pgpkey;
    key->nrefs = 1;
    memcpy(key->pkt, pkt, pktlen);
    memcpy(key->keyid, keyid, sizeof(keyid));
    pthread_rwlock_init(&key->lock, NULL);

exit:
    return key;
}

rpmPubkey *rpmGetSubkeys(rpmPubkey mainkey, int *count)
{
    rpmPubkey *subkeys = NULL;
    pgpDigParams *pgpsubkeys = NULL;
    int pgpsubkeysCount = 0;
    int i;

    if (mainkey && !pgpPrtParamsSubkeys(mainkey->pkt, mainkey->pktlen,
			mainkey->pgpkey, &pgpsubkeys, &pgpsubkeysCount)) {

	subkeys = xmalloc(pgpsubkeysCount * sizeof(*subkeys));

	for (i = 0; i < pgpsubkeysCount; i++) {
	    rpmPubkey subkey = xcalloc(1, sizeof(*subkey));
	    subkeys[i] = subkey;

	    /* Packets with all subkeys already stored in main key */
	    subkey->pkt = NULL;
	    subkey->pktlen = 0;

	    subkey->pgpkey = pgpsubkeys[i];
	    memcpy(subkey->keyid, pgpsubkeys[i]->signid, sizeof(subkey->keyid));
	    subkey->nrefs = 1;
	    pthread_rwlock_init(&subkey->lock, NULL);
	}
	free(pgpsubkeys);
    }
    *count = pgpsubkeysCount;

    return subkeys;
}

rpmPubkey rpmPubkeyFree(rpmPubkey key)
{
    if (key == NULL)
	return NULL;

    pthread_rwlock_wrlock(&key->lock);
    if (--key->nrefs == 0) {
	pgpDigParamsFree(key->pgpkey);
	free(key->pkt);
	pthread_rwlock_unlock(&key->lock);
	pthread_rwlock_destroy(&key->lock);
	free(key);
    } else {
	pthread_rwlock_unlock(&key->lock);
    }
    return NULL;
}

rpmPubkey rpmPubkeyLink(rpmPubkey key)
{
    if (key) {
	pthread_rwlock_wrlock(&key->lock);
	key->nrefs++;
	pthread_rwlock_unlock(&key->lock);
    }
    return key;
}

pgpDig rpmPubkeyDig(rpmPubkey key)
{
    pgpDig dig = NULL;
    static unsigned char zeros[] = 
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int rc;
    if (key == NULL)
	return NULL;

    dig = pgpNewDig();

    pthread_rwlock_rdlock(&key->lock);
    rc = pgpPrtPkts(key->pkt, key->pktlen, dig, 0);
    pthread_rwlock_unlock(&key->lock);

    if (rc == 0) {
	pgpDigParams pubp = pgpDigGetParams(dig, PGPTAG_PUBLIC_KEY);
	if (!pubp || !memcmp(pubp->signid, zeros, sizeof(pubp->signid)) ||
		!memcmp(pubp->time, zeros, sizeof(pubp->time)) ||
		pubp->userid == NULL) {
	    rc = -1;
	}
    }

    if (rc)
	dig = pgpFreeDig(dig);

    return dig;
}

char * rpmPubkeyBase64(rpmPubkey key)
{
    char *enc = NULL;

    if (key) {
	pthread_rwlock_rdlock(&key->lock);
	enc = rpmBase64Encode(key->pkt, key->pktlen, -1);
	pthread_rwlock_unlock(&key->lock);
    }
    return enc;
}

pgpDigParams rpmPubkeyPgpDigParams(rpmPubkey key)
{
    pgpDigParams params= NULL;

    if (key) {
	params = key->pgpkey;
    }
    return params;
}

static rpmPubkey findbySig(rpmKeyring keyring, pgpDigParams sig)
{
    rpmPubkey key = NULL;

    if (keyring && sig) {
	struct rpmPubkey_s needle;
	memset(&needle, 0, sizeof(needle));
	memcpy(needle.keyid, sig->signid, sizeof(needle.keyid));
	
	key = rpmKeyringFindKeyid(keyring, &needle);
	if (key) {
	    pgpDigParams pub = key->pgpkey;
	    /* Do the parameters match the signature? */
	    if ((sig->pubkey_algo != pub->pubkey_algo) ||
		    memcmp(sig->signid, pub->signid, sizeof(sig->signid))) {
		key = NULL;
	    }
	}
    }
    return key;
}

rpmRC rpmKeyringLookup(rpmKeyring keyring, pgpDig sig)
{
    pthread_rwlock_rdlock(&keyring->lock);

    rpmRC res = RPMRC_NOKEY;
    pgpDigParams sigp = pgpDigGetParams(sig, PGPTAG_SIGNATURE);
    rpmPubkey key = findbySig(keyring, sigp);

    if (key) {
	/*
 	 * Callers expect sig to have the key data parsed into pgpDig
 	 * on (successful) return, sigh. No need to check for return
 	 * here as this is validated at rpmPubkeyNew() already.
 	 */
	pgpPrtPkts(key->pkt, key->pktlen, sig, 0);
	res = RPMRC_OK;
    }

    pthread_rwlock_unlock(&keyring->lock);
    return res;
}

rpmRC rpmKeyringVerifySig(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx)
{
    rpmRC rc = RPMRC_FAIL;

    if (sig && ctx) {
	pthread_rwlock_rdlock(&keyring->lock);

	pgpDigParams pgpkey = NULL;
	rpmPubkey key = findbySig(keyring, sig);

	if (key)
	    pgpkey = key->pgpkey;

	/* We call verify even if key not found for a signature sanity check */
	rc = pgpVerifySignature(pgpkey, sig, ctx);

	pthread_rwlock_unlock(&keyring->lock);
    }

    return rc;
}
