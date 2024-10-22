#ifndef _KEYSTORE_H

#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>

RPM_GNUC_INTERNAL
int rpmKeystoreLoad(rpmts ts, rpmKeyring keyring);

RPM_GNUC_INTERNAL
rpmRC rpmKeystoreImportPubkey(rpmtxn txn, rpmPubkey key, int replace = 0);

RPM_GNUC_INTERNAL
rpmRC rpmKeystoreDeletePubkey(rpmtxn txn, rpmPubkey key);

#endif /* _KEYSTORE_H */
