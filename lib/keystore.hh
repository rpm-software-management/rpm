#ifndef _KEYSTORE_H
#define _KEYSTORE_H

#include <string>

#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>

namespace rpm {

class keystore {
public:
    virtual rpmRC load_keys(rpmtxn txn, rpmKeyring keyring) = 0;
    virtual rpmRC import_key(rpmtxn txn, rpmPubkey key, int replace = 1, rpmFlags flags = 0) = 0;
    virtual rpmRC delete_key(rpmtxn txn, rpmPubkey key) = 0;
    virtual rpmRC delete_store(rpmtxn txn) = 0;

    virtual ~keystore() = default;
};

class keystore_fs : public keystore {
public:
    rpmRC load_keys(rpmtxn txn, rpmKeyring keyring) override;
    rpmRC import_key(rpmtxn txn, rpmPubkey key, int replace = 1, rpmFlags flags = 0) override;
    rpmRC delete_key(rpmtxn txn, rpmPubkey key) override;
    rpmRC delete_store(rpmtxn txn) override;
private:
    rpmRC delete_key(rpmtxn txn, const std::string & keyid, const std::string & newname);

    friend rpmRC delete_key_compat(auto keystore, rpmtxn txn, rpmPubkey key, auto skip);
};

class keystore_rpmdb : public keystore {
public:
    rpmRC load_keys(rpmtxn txn, rpmKeyring keyring) override;
    rpmRC import_key(rpmtxn txn, rpmPubkey key, int replace = 1, rpmFlags flags = 0) override;
    rpmRC delete_key(rpmtxn txn, rpmPubkey key) override;
    rpmRC delete_store(rpmtxn txn) override;
private:
    rpmRC delete_key(rpmtxn txn, const std::string & keyid, unsigned int newinstance);

    friend rpmRC delete_key_compat(auto keystore, rpmtxn txn, rpmPubkey key, auto skip);
};

class keystore_openpgp_cert_d : public keystore {
public:
    rpmRC load_keys(rpmtxn txn, rpmKeyring keyring) override;
    rpmRC import_key(rpmtxn txn, rpmPubkey key, int replace = 1, rpmFlags flags = 0) override;
    rpmRC delete_key(rpmtxn txn, rpmPubkey key) override;
    rpmRC delete_store(rpmtxn txn) override;
};

RPM_GNUC_INTERNAL
keystore *getKeystore(const char * krtype);

}; /* namespace */

#endif /* _KEYSTORE_H */
