#include "system.h"

#include <mutex>
#include <atomic>
#include <vector>
#include <shared_mutex>
#include <string>
#include <map>
#include <cassert>

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
    std::string keyid;		/* hex keyid */
    std::string fp;		/* hex fingerprint */
    rpmPubkey primarykey;
    pgpDigParams pgpkey;
    std::atomic_int nrefs;
};

using wrlock = std::unique_lock<std::shared_mutex>;
using rdlock = std::shared_lock<std::shared_mutex>;

struct rpmKeyring_s {
    std::multimap<std::string,rpmPubkey> keys; /* pointers to keys */
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
    char *hexid = rpmhex(keyid, PGP_KEYID_LEN);
    std::string s { hexid };
    free(hexid);
    return s;
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

size_t rpmKeyringSize(rpmKeyring keyring, int count_subkeys)
{
    if (!keyring) return 0;
    rdlock lock(keyring->mutex);

    size_t size = 0;
    for (auto &pair : keyring->keys) {
	if (count_subkeys || !pair.second->primarykey)
	    size++;
    }
    return size;
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
    rpmPubkey mergedkey = NULL;
    if (keyring == NULL || key == NULL)
	return -1;
    if (mode != RPMKEYRING_ADD && mode != RPMKEYRING_DELETE)
	return -1;

    if (mode == RPMKEYRING_ADD) {
	rpmPubkey oldkey = rpmKeyringLookupKey(keyring, key);
	if (oldkey) {
	    if (rpmPubkeyMerge(oldkey, key, &mergedkey) != RPMRC_OK) {
		rpmPubkeyFree(oldkey);
		return -1;
	    }
	    if (mergedkey) {
		key = mergedkey;
	    }
	    rpmPubkeyFree(oldkey);
	}
    }

    /* check if we already have this key, but always wrlock for simplicity */
    wrlock lock(keyring->mutex);
    auto range = keyring->keys.equal_range(key->keyid);
    auto item = range.first;
    for (; item != range.second; ++item) {
	if (item->second->fp == key->fp)
	    break;
    }
    if (item != range.second) {
	/* remove subkeys */
	auto it = keyring->keys.begin();
	while (it != keyring->keys.end()) {
	    if (it->second->primarykey == item->second) {
		rpmPubkeyFree(it->second);
		it = keyring->keys.erase(it);
	    } else {
		++it;
	    }
	}
	rpmPubkeyFree(item->second);
	keyring->keys.erase(item);
	rc = 0;
    }
    if (mode == RPMKEYRING_ADD) {
	int subkeysCount = 0;
	rpmPubkey *subkeys = rpmGetSubkeys(key, &subkeysCount);
	keyring->keys.insert({key->keyid, rpmPubkeyLink(key)});
	rpmlog(RPMLOG_DEBUG, "added key %s to keyring\n", rpmPubkeyFingerprintAsHex(key));
	/* add subkeys */
	for (int i = 0; i < subkeysCount; i++) {
	    rpmPubkey subkey = subkeys[i];
	    keyring->keys.insert({subkey->keyid, subkey});
	    rpmlog(RPMLOG_DEBUG,
		   "added subkey %d of main key %s to keyring\n", i, rpmPubkeyFingerprintAsHex(key));
	}
	free(subkeys);
	rc = 0;
    }
    /* strip initial nref */
    rpmPubkeyFree(mergedkey);

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

    auto range = keyring->keys.equal_range(key->keyid);
    for (auto item = range.first; item != range.second; ++item) {
	if (item->second->fp == key->fp)
	    return rpmPubkeyLink(item->second);
    }
    return NULL;
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
    uint8_t *fp = NULL;
    size_t fplen = 0;
    char *hexfp = NULL;
    
    if (pkt == NULL || pktlen == 0)
	goto exit;

    if (pgpPubkeyKeyID(pkt, pktlen, keyid))
	goto exit;

    if (pgpPrtParams(pkt, pktlen, PGPTAG_PUBLIC_KEY, &pgpkey))
	goto exit;

    if (pgpPubkeyFingerprint(pkt, pktlen, &fp, &fplen))
	goto exit;
    hexfp = rpmhex(fp, fplen);

    key = new rpmPubkey_s {};
    key->primarykey = NULL;
    key->pkt.resize(pktlen);
    key->pgpkey = pgpkey;
    key->nrefs = 1;
    memcpy(key->pkt.data(), pkt, pktlen);
    key->keyid = key2str(keyid);
    key->fp = hexfp;

exit:
    free(hexfp);
    free(fp);
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
	rc = pgpPubkeyFingerprint(primarykey->pkt.data(), primarykey->pkt.size(), fp, fplen);
    }
    primarykey = rpmPubkeyFree(primarykey);
    return rc;
}

const char * rpmPubkeyFingerprintAsHex(rpmPubkey key)
{
    const char * result = NULL;
    if (key) {
	result = key->fp.c_str();
    }
    return result;
}

const char * rpmPubkeyKeyIDAsHex(rpmPubkey key)
{
    return key ? key->keyid.c_str() : NULL;
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
	enc = rpmBase64Encode(key->pkt.data(), key->pkt.size(), -1);
    }
    return enc;
}

char * rpmPubkeyArmorWrap(rpmPubkey key)
{
    char *enc = NULL;

    if (key) {
	enc = pgpArmorWrap(PGPARMOR_PUBKEY, key->pkt.data(), key->pkt.size());
    }
    return enc;
}

rpmRC rpmPubkeyMerge(rpmPubkey oldkey, rpmPubkey newkey, rpmPubkey *mergedkeyp)
{
    rpmPubkey mergedkey = NULL;
    uint8_t *mergedpkt = NULL;
    size_t mergedpktlen = 0;

    rpmRC rc = pgpPubkeyMerge(oldkey->pkt.data(), oldkey->pkt.size(), newkey->pkt.data(), newkey->pkt.size(), &mergedpkt, &mergedpktlen, 0);
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

rpmRC pubKeyVerify(rpmPubkey key, pgpDigParams sig, DIGEST_CTX ctx, std::vector<std::pair<int, std::string>> &results)
{
    rpmRC rc = RPMRC_FAIL;
    char *lints = NULL;
    rc = pgpVerifySignature2(key ? key->pgpkey : NULL, sig, ctx, &lints);

    /* found right key, discard error messages from wrong keys */
    if (rc == RPMRC_OK)
	results.clear();

    results.push_back({rc, std::string(lints ? lints : "")});
    free(lints);
    return rc;
}

rpmRC rpmKeyringVerifySig2(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx, rpmPubkey * keyptr)
{
    rpmRC rc = RPMRC_FAIL;
    rpmPubkey key = NULL;

    if (sig && ctx) {
	rdlock lock(keyring->mutex);
	auto keyid = key2str(pgpDigParamsSignID(sig));
	std::vector<std::pair<int, std::string>> results = {};

	/* Look for verifying key */
	auto range = keyring->keys.equal_range(keyid);

	for (auto it = range.first; it != range.second; ++it) {
	    key = it->second;

	    /* Do the parameters match the signature? */
	    if (pgpDigParamsAlgo(sig, PGPVAL_PUBKEYALGO)
		!= pgpDigParamsAlgo(key->pgpkey, PGPVAL_PUBKEYALGO))
	    {
		continue;
	    }

	    rc = pubKeyVerify(key, sig, ctx, results);

	    if (rc == RPMRC_OK)
		break;
	}

	/* No key, sanity check signature*/
	if (results.empty()) {
	    key = NULL;
	    rc = pubKeyVerify(NULL, sig, ctx, results);
	    assert(rc != RPMRC_OK);
	}

	for (auto const & item : results) {
	    if (!item.second.empty()) {
		rpmlog(item.first ? RPMLOG_ERR : RPMLOG_WARNING, "%s\n",
		       item.second.c_str());
	    }
	}
    }

    if (keyptr) {
	*keyptr = pubkeyPrimarykey(key);
    }

    return rc;
}

rpmRC rpmKeyringVerifySig(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx)
{
    return rpmKeyringVerifySig2(keyring, sig, ctx, NULL);
}
