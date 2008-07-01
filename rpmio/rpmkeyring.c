#include "system.h"

#include <rpm/rpmstring.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "rpmio/rpmkeyring.h"
#include "rpmio/base64.h"
#include "rpmio/digest.h"

#include "debug.h"

struct rpmPubkey_s {
    uint8_t *pkt;
    size_t pktlen;
    pgpKeyID_t keyid;
};

struct rpmKeyring_s {
    struct rpmPubkey_s **keys;
    size_t numkeys;
};

rpmKeyring rpmKeyringNew(void)
{
    rpmKeyring keyring = xcalloc(1, sizeof(*keyring));
    keyring->keys = NULL;
    keyring->numkeys = 0;
    return keyring;
}

rpmKeyring rpmKeyringFree(rpmKeyring keyring)
{
    if (keyring && keyring->keys) {
	for (int i = 0; i < keyring->numkeys; i++) {
	    keyring->keys[i] = rpmPubkeyFree(keyring->keys[i]);
	}
	free(keyring->keys);
    }
    free(keyring);
    return NULL;
}

int rpmKeyringAddKey(rpmKeyring keyring, rpmPubkey key)
{
    if (keyring == NULL || key == NULL)
	return 0;
    
    /* XXX TODO: check if we already have this key */
    keyring->keys = xrealloc(keyring->keys, (keyring->numkeys + 1) * sizeof(rpmPubkey));
    keyring->keys[keyring->numkeys] = key;
    keyring->numkeys++;

    return 1;
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
    
    if (pkt == NULL || pktlen == 0)
	goto exit;

    key = xcalloc(1, sizeof(*key));
    pgpPubkeyFingerprint(pkt, pktlen, key->keyid);
    key->pkt = xmalloc(pktlen);
    key->pktlen = pktlen;
    memcpy(key->pkt, pkt, pktlen);

exit:
    return key;
}

rpmPubkey rpmPubkeyFree(rpmPubkey key)
{
    if (key == NULL)
	return NULL;

    free(key->pkt);
    free(key);
    return NULL;
}

rpmRC rpmKeyringLookup(rpmKeyring keyring, pgpDig sig)
{
    pgpDigParams sigp = sig ? &sig->signature : NULL;
    rpmRC res = RPMRC_NOKEY;
    
    if (keyring == NULL || sig == NULL)
	goto exit;

    for (int i = 0; i < keyring->numkeys; i++) {
    	const struct rpmPubkey_s *key = keyring->keys[i];
	if (memcmp(key->keyid, sigp->signid, sizeof(key->keyid)) == 0) {
	    if (pgpPrtPkts(key->pkt, key->pktlen, sig, 0) == 0) {
		res = RPMRC_OK;
	    }
	}
    }

exit:
    return res;
}
