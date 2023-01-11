#ifndef H_RPMKEYRING_PY
#define H_RPMKEYRING_PY

#include <rpm/rpmtypes.h>

typedef struct rpmPubkeyObject_s rpmPubkeyObject;
typedef struct rpmKeyringObject_s rpmKeyringObject;

extern PyTypeObject* rpmKeyring_Type;
extern PyType_Spec rpmKeyring_Type_Spec;
extern PyTypeObject* rpmPubkey_Type;
extern PyType_Spec rpmPubkey_Type_Spec;

PyObject * rpmPubkey_Wrap(PyTypeObject *subtype, rpmPubkey pubkey);
PyObject * rpmKeyring_Wrap(PyTypeObject *subtype, rpmKeyring keyring);

int rpmKeyringFromPyObject(PyObject *item, rpmKeyring *keyring);
#endif
