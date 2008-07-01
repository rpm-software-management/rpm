#ifndef _RPMKEYRING_H
#define _RPMKEYRING_H

#include <rpm/rpmtypes.h>
#include <rpm/rpmpgp.h>

rpmKeyring rpmKeyringNew(void);
rpmKeyring rpmKeyringFree(rpmKeyring keyring);
int rpmKeyringAddKey(rpmKeyring keyring, rpmPubkey key);

rpmRC rpmKeyringLookup(rpmKeyring keyring, pgpDig sig);

rpmPubkey rpmPubkeyNew(const uint8_t *pkt, size_t pktlen);
rpmPubkey rpmPubkeyRead(const char *filename);
rpmPubkey rpmPubkeyFree(rpmPubkey key);

#endif /* _RPMKEYDB_H */
