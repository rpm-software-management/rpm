#include "rpmsystem-py.h"
#include <rpm/rpmkeyring.h>
#include "rpmkeyring-py.h"

struct rpmPubkeyObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    rpmPubkey pubkey;
};

static void rpmPubkey_dealloc(rpmPubkeyObject * s)
{
    PyObject_GC_UnTrack(s);
    s->pubkey = rpmPubkeyFree(s->pubkey);
    PyTypeObject *type = Py_TYPE(s);
    freefunc free = PyType_GetSlot(type, Py_tp_free);
    free(s);
    Py_DECREF(type);
}

static int rpmPubkey_traverse(rpmPubkeyObject * s, visitproc visit, void *arg)
{
    if (python_version >= 0x03090000) {
        Py_VISIT(Py_TYPE(s));
    }
    return 0;
}

static PyObject *rpmPubkey_new(PyTypeObject *subtype, 
			   PyObject *args, PyObject *kwds)
{
    PyObject *key, *ret;
    char *kwlist[] = { "key", NULL };
    rpmPubkey pubkey = NULL;
    uint8_t *pkt;
    size_t pktlen;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "S", kwlist, &key))
	return NULL;

    if (pgpParsePkts(PyBytes_AsString(key), &pkt, &pktlen) <= 0) {
	PyErr_SetString(PyExc_ValueError, "invalid PGP armor");
	return NULL;
    }
    pubkey = rpmPubkeyNew(pkt, pktlen);
    if (pubkey == NULL) {
	PyErr_SetString(PyExc_ValueError, "invalid pubkey");
	ret = NULL;
    } else
	ret = rpmPubkey_Wrap(subtype, pubkey);

    free(pkt);
    return ret;
}

static PyObject * rpmPubkey_Base64(rpmPubkeyObject *s)
{
    char *b64 = rpmPubkeyBase64(s->pubkey);
    PyObject *res = utf8FromString(b64);
    free(b64);
    return res;
}

static struct PyMethodDef rpmPubkey_methods[] = {
    { "base64", (PyCFunction) rpmPubkey_Base64, METH_NOARGS, NULL },
    { NULL }
};

static char rpmPubkey_doc[] = "";

static PyType_Slot rpmPubkey_Type_Slots[] = {
    {Py_tp_dealloc, rpmPubkey_dealloc},
    {Py_tp_traverse, rpmPubkey_traverse},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, rpmPubkey_doc},
    {Py_tp_methods, rpmPubkey_methods},
    {Py_tp_new, rpmPubkey_new},
    {0, NULL},
};
PyType_Spec rpmPubkey_Type_Spec = {
    .name = "rpm.pubkey",
    .basicsize = sizeof(rpmPubkeyObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmPubkey_Type_Slots,
};

struct rpmKeyringObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    rpmKeyring keyring;
};

static void rpmKeyring_dealloc(rpmKeyringObject * s)
{
    PyObject_GC_UnTrack(s);
    rpmKeyringFree(s->keyring);
    PyTypeObject *type = Py_TYPE(s);
    freefunc free = PyType_GetSlot(type, Py_tp_free);
    free(s);
    Py_DECREF(type);
}

static int rpmKeyring_traverse(rpmKeyringObject * s, visitproc visit, void *arg)
{
    if (python_version >= 0x03090000) {
        Py_VISIT(Py_TYPE(s));
    }
    return 0;
}

static PyObject *rpmKeyring_new(PyTypeObject *subtype, 
			   PyObject *args, PyObject *kwds)
{
    rpmKeyring keyring = rpmKeyringNew();
    return rpmKeyring_Wrap(subtype, keyring);
}

static PyObject *rpmKeyring_addKey(rpmKeyringObject *s, PyObject *arg)
{
    rpmPubkeyObject *pubkey = NULL;

    if (!PyArg_Parse(arg, "O!", modstate->rpmPubkey_Type, &pubkey))
	return NULL;

    return Py_BuildValue("i", rpmKeyringAddKey(s->keyring, pubkey->pubkey));
};

static struct PyMethodDef rpmKeyring_methods[] = {
    { "addKey", (PyCFunction) rpmKeyring_addKey, METH_O,
        NULL },
    {NULL,		NULL}		/* sentinel */
};

static char rpmKeyring_doc[] =
"";

static PyType_Slot rpmKeyring_Type_Slots[] = {
    {Py_tp_dealloc, rpmKeyring_dealloc},
    {Py_tp_traverse, rpmKeyring_traverse},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, rpmKeyring_doc},
    {Py_tp_methods, rpmKeyring_methods},
    {Py_tp_new, rpmKeyring_new},
    {0, NULL},
};
PyType_Spec rpmKeyring_Type_Spec = {
    .name = "rpm.keyring",
    .basicsize = sizeof(rpmKeyringObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmKeyring_Type_Slots,
};

PyObject * rpmPubkey_Wrap(PyTypeObject *subtype, rpmPubkey pubkey)
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    rpmPubkeyObject *s = (rpmPubkeyObject *)subtype_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->pubkey = pubkey;
    return (PyObject*) s;
}

PyObject * rpmKeyring_Wrap(PyTypeObject *subtype, rpmKeyring keyring)
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    rpmKeyringObject *s = (rpmKeyringObject *)subtype_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->keyring = keyring;
    return (PyObject*) s;
}

int rpmKeyringFromPyObject(PyObject *item, rpmKeyring *keyring)
{
    rpmKeyringObject *kro;
    if (!PyArg_Parse(item, "O!", modstate->rpmKeyring_Type, &kro))
	return 0;
    *keyring = kro->keyring;
    return 1;
}
