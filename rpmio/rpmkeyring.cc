#include "system.h"

#include <mutex>
#include <atomic>
#include <vector>
#include <shared_mutex>
#include <string>
#include <map>

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
    rpmPubkey primarykey;
    pgpDigParams pgpkey;
    std::atomic_int nrefs;
    std::shared_mutex mutex;
};

using wrlock = std::unique_lock<std::shared_mutex>;
using rdlock = std::shared_lock<std::shared_mutex>;

struct rpmKeyring_s {
    std::map<std::string,rpmPubkey> keys; /* pointers to keys */
    std::atomic_int nrefs;
    std::shared_mutex mutex;
};

struct rpmKeyringIterator_s {
    rpmKeyring keyring;
    rdlock keyringlock;
    std::map<std::string,rpmPubkey>::const_iterator iterator;
    rpmPubkey current;
};

static std::string key2str(const uint8_t *keyid)
{
    return std::string(reinterpret_cast<const char *>(keyid), PGP_KEYID_LEN);
}

rpmKeyring rpmKeyringNew(void)
{
    rpmKeyring keyring = new rpmKeyring_s {};
    keyring->nrefs = 1;
    return keyring;
}

rpmKeyring rpmKeyringFree(rpmKeyring keyring)
{
    if (keyring == NULL || --keyring->nrefs > 0)
	return NULL;

    for (auto & item : keyring->keys)
	rpmPubkeyFree(item.second);
    delete keyring;

    return NULL;
}

rpmKeyringIterator rpmKeyringInitIterator(rpmKeyring keyring, int unused)
{
    if (!keyring || unused != 0)
	return NULL;

    return new rpmKeyringIterator_s {
	rpmKeyringLink(keyring),
	rdlock(keyring->mutex),
	keyring->keys.cbegin(),
	NULL,
    };
}

rpmPubkey rpmKeyringIteratorNext(rpmKeyringIterator iterator)
{
    rpmPubkey next = NULL;

    if (!iterator)
	return NULL;

    while (iterator->iterator != iterator->keyring->keys.end()) {
	next = iterator->iterator->second;
	iterator->iterator++;
	if (!next->primarykey)
	    break;
	else
            next = NULL;
    }
    rpmPubkeyFree(iterator->current);
    iterator->current = rpmPubkeyLink(next);
    return iterator->current;
}

rpmKeyringIterator rpmKeyringIteratorFree(rpmKeyringIterator iterator)
{
    if (!iterator)
	return NULL;

    rpmPubkeyFree(iterator->current);
    iterator->keyringlock.unlock(); /* needed or rpmKeyringFree locks up */
    rpmKeyringFree(iterator->keyring);
    delete iterator;
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
    wrlock lock(keyring->mutex);
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
    rdlock lock(keyring->mutex);
    auto item = keyring->keys.find(key->keyid);
    rpmPubkey rkey = item == keyring->keys.end() ? NULL : rpmPubkeyLink(item->second);
    return rkey;
}

rpmKeyring rpmKeyringLink(rpmKeyring keyring)
{
    if (keyring) {
	keyring->nrefs++;
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
    key->primarykey = NULL;
    key->pkt.resize(pktlen);
    key->pgpkey = pgpkey;
    key->nrefs = 1;
    memcpy(key->pkt.data(), pkt, pktlen);
    key->keyid = key2str(keyid);

exit:
    return key;
}

static rpmPubkey rpmPubkeyNewSubkey(rpmPubkey primarykey, pgpDigParams pgpkey)
{
    rpmPubkey key = new rpmPubkey_s {};
    /* Packets with all subkeys already stored in primary key */
    key->primarykey = primarykey;
    key->pgpkey = pgpkey;
    key->keyid = key2str(pgpDigParamsSignID(pgpkey));
    key->nrefs = 1;
    return key;
}

rpmPubkey *rpmGetSubkeys(rpmPubkey primarykey, int *count)
{
    rpmPubkey *subkeys = NULL;
    pgpDigParams *pgpsubkeys = NULL;
    int pgpsubkeysCount = 0;
    int i;

    if (primarykey) {

	rdlock lock(primarykey->mutex);

	if (!pgpPrtParamsSubkeys(
		primarykey->pkt.data(), primarykey->pkt.size(),
		primarykey->pgpkey, &pgpsubkeys, &pgpsubkeysCount)) {
	    /* Returned to C, can't use new */
	    subkeys = (rpmPubkey *)xmalloc(pgpsubkeysCount * sizeof(*subkeys));
	    for (i = 0; i < pgpsubkeysCount; i++) {
		subkeys[i] = rpmPubkeyNewSubkey(primarykey, pgpsubkeys[i]);
		primarykey = rpmPubkeyLink(primarykey);
	    }
	    free(pgpsubkeys);
	}
    }
    *count = pgpsubkeysCount;

    return subkeys;
}

rpmPubkey pubkeyPrimarykey(rpmPubkey key)
{
    rpmPubkey primarykey = NULL;
    if (key) {
	wrlock lock(key->mutex);
	if (key->primarykey == NULL) {
	    primarykey = rpmPubkeyLink(key);
	} else {
	    primarykey = rpmPubkeyLink(key->primarykey);
	}
    }
    return primarykey;
}

int rpmPubkeyFingerprint(rpmPubkey key, uint8_t **fp, size_t *fplen)
{
    if (key == NULL)
	return -1;
    int rc = -1;
    rpmPubkey primarykey = pubkeyPrimarykey(key);
    if (primarykey) {
	rdlock lock(primarykey->mutex);
	rc = pgpPubkeyFingerprint(primarykey->pkt.data(), primarykey->pkt.size(), fp, fplen);
    }
    primarykey = rpmPubkeyFree(primarykey);
    return rc;
}

char * rpmPubkeyFingerprintAsHex(rpmPubkey key)
{
    char * result = NULL;
    uint8_t *fp = NULL;
    size_t fplen = 0;
    if (!rpmPubkeyFingerprint(key, &fp, &fplen)) {
	result = rpmhex(fp, fplen);
	free(fp);
    }
    return result;
}

char * rpmPubkeyKeyIDAsHex(rpmPubkey key)
{
    return rpmhex((const uint8_t*)(key->keyid.c_str()), key->keyid.length());
}


rpmPubkey rpmPubkeyFree(rpmPubkey key)
{
    if (key == NULL || --key->nrefs > 0)
	return NULL;

    pgpDigParamsFree(key->pgpkey);
    rpmPubkeyFree(key->primarykey);
    delete key;

    return NULL;
}

rpmPubkey rpmPubkeyLink(rpmPubkey key)
{
    if (key) {
	key->nrefs++;
    }
    return key;
}

char * rpmPubkeyBase64(rpmPubkey key)
{
    char *enc = NULL;

    if (key) {
	rdlock lock(key->mutex);
	enc = rpmBase64Encode(key->pkt.data(), key->pkt.size(), -1);
    }
    return enc;
}

rpmRC rpmPubkeyMerge(rpmPubkey oldkey, rpmPubkey newkey, rpmPubkey *mergedkeyp)
{
    rpmPubkey mergedkey = NULL;
    uint8_t *mergedpkt = NULL;
    size_t mergedpktlen = 0;
    rpmRC rc;

    rdlock olock(oldkey->mutex);
    rdlock nlock(newkey->mutex);
    rc = pgpPubkeyMerge(oldkey->pkt.data(), oldkey->pkt.size(), newkey->pkt.data(), newkey->pkt.size(), &mergedpkt, &mergedpktlen, 0);
    if (rc == RPMRC_OK && (mergedpktlen != oldkey->pkt.size() || memcmp(mergedpkt, oldkey->pkt.data(), mergedpktlen) != 0)) {
	mergedkey = rpmPubkeyNew(mergedpkt, mergedpktlen);
	if (!mergedkey)
	    rc = RPMRC_FAIL;
    }
    *mergedkeyp = mergedkey;
    free(mergedpkt);
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
	rdlock lock(keyring->mutex);
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

rpmRC rpmKeyringVerifySig2(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx, rpmPubkey * keyptr)
{
    rpmRC rc = RPMRC_FAIL;

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
	if (keyptr) {
	    *keyptr = pubkeyPrimarykey(key);
	}
    }

    return rc;
}

rpmRC rpmKeyringVerifySig(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx)
{
    return rpmKeyringVerifySig2(keyring, sig, ctx, NULL);
}
