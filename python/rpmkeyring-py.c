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
    s->pubkey = rpmPubkeyFree(s->pubkey);
    Py_TYPE(s)->tp_free((PyObject *)s);
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

PyTypeObject rpmPubkey_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.pubkey",			/* tp_name */
	sizeof(rpmPubkeyObject),	/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmPubkey_dealloc,/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmPubkey_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmPubkey_methods,		/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	rpmPubkey_new,			/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

struct rpmKeyringObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    rpmKeyring keyring;
};

static void rpmKeyring_dealloc(rpmKeyringObject * s)
{
    rpmKeyringFree(s->keyring);
    Py_TYPE(s)->tp_free((PyObject *)s);
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

    if (!PyArg_Parse(arg, "O!", &rpmPubkey_Type, &pubkey))
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

PyTypeObject rpmKeyring_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.keyring",			/* tp_name */
	sizeof(rpmKeyringObject),	/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmKeyring_dealloc,/* tp_dealloc */
	0,				/* tp_print */
	0,		 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmKeyring_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmKeyring_methods,		/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	rpmKeyring_new,			/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

PyObject * rpmPubkey_Wrap(PyTypeObject *subtype, rpmPubkey pubkey)
{
    rpmPubkeyObject *s = (rpmPubkeyObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->pubkey = pubkey;
    return (PyObject*) s;
}

PyObject * rpmKeyring_Wrap(PyTypeObject *subtype, rpmKeyring keyring)
{
    rpmKeyringObject *s = (rpmKeyringObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->keyring = keyring;
    return (PyObject*) s;
}

int rpmKeyringFromPyObject(PyObject *item, rpmKeyring *keyring)
{
    rpmKeyringObject *kro;
    if (!PyArg_Parse(item, "O!", &rpmKeyring_Type, &kro))
	return 0;
    *keyring = kro->keyring;
    return 1;
}
