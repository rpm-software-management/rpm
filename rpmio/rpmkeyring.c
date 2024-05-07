#include "system.h"

#include <vector>
#include <string>
#include <map>
#include <pthread.h>

#include <rpm/rpmstring.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmkeyring.h>
#include <rpm/rpmbase64.h>

#include "debug.h"

int _print_pkts = 0;

struct rpmPubkey_s {
    std::vector<uint8_t> pkt;
    std::string keyid;
    pgpDigParams pgpkey;
    int nrefs;
    pthread_rwlock_t lock;
};

struct rpmKeyring_s {
    std::map<std::string,rpmPubkey> keys; /* pointers to keys */
    int nrefs;
    pthread_rwlock_t lock;
};

static std::string key2str(const uint8_t *keyid)
{
    return std::string(reinterpret_cast<const char *>(keyid), PGP_KEYID_LEN);
}

rpmKeyring rpmKeyringNew(void)
{
    rpmKeyring keyring = new rpmKeyring_s {};
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
	for (auto & item : keyring->keys)
	    rpmPubkeyFree(item.second);
	pthread_rwlock_unlock(&keyring->lock);
	pthread_rwlock_destroy(&keyring->lock);
	delete keyring;
    } else {
	pthread_rwlock_unlock(&keyring->lock);
    }
    return NULL;
}

int rpmKeyringModify(rpmKeyring keyring, rpmPubkey key, rpmKeyringModifyMode mode)
{
    int rc = 1; /* assume already seen key */
    if (keyring == NULL || key == NULL)
	return -1;
    if (mode != RPMKEYRING_ADD && mode != RPMKEYRING_DELETE && mode != RPMKEYRING_REPLACE)
	return -1;

    /* check if we already have this key, but always wrlock for simplicity */
    pthread_rwlock_wrlock(&keyring->lock);
    auto item = keyring->keys.find(key->keyid);
    if (item != keyring->keys.end() && mode == RPMKEYRING_DELETE) {
	rpmPubkeyFree(item->second);
	keyring->keys.erase(item);
	rc = 0;
    } else if (item != keyring->keys.end() && mode == RPMKEYRING_REPLACE) {
	rpmPubkeyFree(item->second);
	item->second = rpmPubkeyLink(key);
	rc = 0;
    } else if (item == keyring->keys.end() && (mode == RPMKEYRING_ADD ||mode == RPMKEYRING_REPLACE) ) {
	keyring->keys.insert({key->keyid, rpmPubkeyLink(key)});
	rc = 0;
    }
    pthread_rwlock_unlock(&keyring->lock);

    return rc;
}

int rpmKeyringAddKey(rpmKeyring keyring, rpmPubkey key)
{
    return rpmKeyringModify(keyring, key, RPMKEYRING_ADD);
}

rpmPubkey rpmKeyringLookupKey(rpmKeyring keyring, rpmPubkey key)
{
    if (keyring == NULL || key == NULL)
	return NULL;
    pthread_rwlock_rdlock(&keyring->lock);
    auto item = keyring->keys.find(key->keyid);
    rpmPubkey rkey = item == keyring->keys.end() ? NULL : rpmPubkeyLink(item->second);
    pthread_rwlock_unlock(&keyring->lock);
    return rkey;
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

    if (pgpPubkeyKeyID(pkt, pktlen, keyid))
	goto exit;

    if (pgpPrtParams(pkt, pktlen, PGPTAG_PUBLIC_KEY, &pgpkey))
	goto exit;

    key = new rpmPubkey_s {};
    key->pkt.resize(pktlen);
    key->pgpkey = pgpkey;
    key->nrefs = 1;
    memcpy(key->pkt.data(), pkt, pktlen);
    key->keyid = key2str(keyid);
    pthread_rwlock_init(&key->lock, NULL);

exit:
    return key;
}

static rpmPubkey rpmPubkeyNewSubkey(pgpDigParams pgpkey)
{
    rpmPubkey key = new rpmPubkey_s {};
    /* Packets with all subkeys already stored in main key */
    key->pgpkey = pgpkey;
    key->keyid = key2str(pgpDigParamsSignID(pgpkey));
    key->nrefs = 1;
    pthread_rwlock_init(&key->lock, NULL);
    return key;
}

rpmPubkey *rpmGetSubkeys(rpmPubkey mainkey, int *count)
{
    rpmPubkey *subkeys = NULL;
    pgpDigParams *pgpsubkeys = NULL;
    int pgpsubkeysCount = 0;
    int i;

    if (mainkey && !pgpPrtParamsSubkeys(mainkey->pkt.data(), mainkey->pkt.size(),
			mainkey->pgpkey, &pgpsubkeys, &pgpsubkeysCount)) {

	/* Returned to C, can't use new */
	subkeys = (rpmPubkey *)xmalloc(pgpsubkeysCount * sizeof(*subkeys));
	for (i = 0; i < pgpsubkeysCount; i++)
	    subkeys[i] = rpmPubkeyNewSubkey(pgpsubkeys[i]);
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
	pthread_rwlock_unlock(&key->lock);
	pthread_rwlock_destroy(&key->lock);
	delete key;
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

char * rpmPubkeyBase64(rpmPubkey key)
{
    char *enc = NULL;

    if (key) {
	pthread_rwlock_rdlock(&key->lock);
	enc = rpmBase64Encode(key->pkt.data(), key->pkt.size(), -1);
	pthread_rwlock_unlock(&key->lock);
    }
    return enc;
}

rpmRC rpmPubkeyMerge(rpmPubkey oldkey, rpmPubkey newkey, rpmPubkey *mergedkeyp)
{
    rpmPubkey mergedkey = NULL;
    uint8_t *mergedpkt = NULL;
    size_t mergedpktlen = 0;
    rpmRC rc;

    pthread_rwlock_rdlock(&oldkey->lock);
    pthread_rwlock_rdlock(&newkey->lock);
    rc = pgpPubkeyMerge(oldkey->pkt.data(), oldkey->pkt.size(), newkey->pkt.data(), newkey->pkt.size(), &mergedpkt, &mergedpktlen, 0);
    if (rc == RPMRC_OK && (mergedpktlen != oldkey->pkt.size() || memcmp(mergedpkt, oldkey->pkt.data(), mergedpktlen) != 0)) {
	mergedkey = rpmPubkeyNew(mergedpkt, mergedpktlen);
	if (!mergedkey)
	    rc = RPMRC_FAIL;
    }
    *mergedkeyp = mergedkey;
    free(mergedpkt);
    pthread_rwlock_unlock(&newkey->lock);
    pthread_rwlock_unlock(&oldkey->lock);
    return rc;
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
	auto keyid = key2str(pgpDigParamsSignID(sig));
	auto item = keyring->keys.find(keyid);
	if (item != keyring->keys.end()) {
	    key = item->second;
	    pgpDigParams pub = key->pgpkey;
	    /* Do the parameters match the signature? */
	    if ((pgpDigParamsAlgo(sig, PGPVAL_PUBKEYALGO)
                 != pgpDigParamsAlgo(pub, PGPVAL_PUBKEYALGO)) ||
                memcmp(pgpDigParamsSignID(sig), pgpDigParamsSignID(pub),
                       PGP_KEYID_LEN)) {
		key = NULL;
	    }
	}
    }
    return key;
}

rpmRC rpmKeyringVerifySig(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx)
{
    rpmRC rc = RPMRC_FAIL;

    if (keyring)
	pthread_rwlock_rdlock(&keyring->lock);

    if (sig && ctx) {
	pgpDigParams pgpkey = NULL;
	rpmPubkey key = findbySig(keyring, sig);

	if (key)
	    pgpkey = key->pgpkey;

	/* We call verify even if key not found for a signature sanity check */
	char *lints = NULL;
	rc = pgpVerifySignature2(pgpkey, sig, ctx, &lints);
	if (lints) {
	    rpmlog(rc ? RPMLOG_ERR : RPMLOG_WARNING, "%s\n", lints);
	    free(lints);
	}
    }

    if (keyring)
	pthread_rwlock_unlock(&keyring->lock);

    return rc;
}
