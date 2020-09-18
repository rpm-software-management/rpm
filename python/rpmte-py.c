#include "rpmsystem-py.h"

#include "header-py.h"	/* XXX tagNumFromPyObject */
#include "rpmds-py.h"
#include "rpmfi-py.h"
#include "rpmfiles-py.h"
#include "rpmte-py.h"
#include "rpmps-py.h"

/** \ingroup python
 * \name Class: Rpmte
 * \class Rpmte
 * \brief An python rpm.te object represents an element of a transaction set.
 *
 * Elements of a transaction set are accessible after being added. Each
 * element carries descriptive information about the added element as well
 * as a file info set and dependency sets for each of the 4 types of dependency.
 *
 * The rpmte class contains the following methods:
 *
 * - te.Type()	Return transaction element type (TR_ADDED|TR_REMOVED).
 * - te.N()	Return package name.
 * - te.E()	Return package epoch.
 * - te.V()	Return package version.
 * - te.R()	Return package release.
 * - te.A()	Return package architecture.
 * - te.O()	Return package operating system.
 * - te.NEVR()	Return package name-version-release.
 * - te.Color() Return package color bits.
 * - te.PkgFileSize() Return no. of bytes in package file (approx).
 * - te.Parent() Return the parent element index.
 * - te.Problems() Return problems associated with this element.
 * - te.DBOffset() Return the Packages database instance number
 * - te.Key()	Return the associated opaque key, i.e. 2nd arg ts.addInstall().
 * - te.DS(tag)	Return package dependency set.
 * @param tag	'Providename', 'Requirename', 'Obsoletename', 'Conflictname'
 * - te.FI(tag)	Return package file info set iterator (deprecated).
 * @param tag	'Basenames'
 * - te.Files()	Return package file info set.
 * - te.Verified() Return package verification status.
 */

struct rpmteObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmte	te;
};

static PyObject *
rpmte_TEType(rpmteObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmteType(s->te));
}

static PyObject *
rpmte_N(rpmteObject * s, PyObject * unused)
{
    return utf8FromString(rpmteN(s->te));
}

static PyObject *
rpmte_E(rpmteObject * s, PyObject * unused)
{
    return utf8FromString(rpmteE(s->te));
}

static PyObject *
rpmte_V(rpmteObject * s, PyObject * unused)
{
    return utf8FromString(rpmteV(s->te));
}

static PyObject *
rpmte_R(rpmteObject * s, PyObject * unused)
{
    return utf8FromString(rpmteR(s->te));
}

static PyObject *
rpmte_A(rpmteObject * s, PyObject * unused)
{
    return utf8FromString(rpmteA(s->te));
}

static PyObject *
rpmte_O(rpmteObject * s, PyObject * unused)
{
    return utf8FromString(rpmteO(s->te));
}

static PyObject *
rpmte_NEVR(rpmteObject * s, PyObject * unused)
{
    return utf8FromString(rpmteNEVR(s->te));
}

static PyObject *
rpmte_NEVRA(rpmteObject * s, PyObject * unused)
{
    return utf8FromString(rpmteNEVRA(s->te));
}

static PyObject *
rpmte_Color(rpmteObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmteColor(s->te));
}

static PyObject *
rpmte_PkgFileSize(rpmteObject * s, PyObject * unused)
{
    return Py_BuildValue("L", rpmtePkgFileSize(s->te));
}

static PyObject *
rpmte_Parent(rpmteObject * s, PyObject * unused)
{
    rpmte parent = rpmteParent(s->te);
    if (parent)
	return rpmte_Wrap(&rpmte_Type, parent);

    Py_RETURN_NONE;
}

static PyObject * rpmte_Failed(rpmteObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmteFailed(s->te));
}

static PyObject * rpmte_Problems(rpmteObject * s, PyObject * unused)
{
    rpmps ps = rpmteProblems(s->te);
    PyObject *problems = rpmps_AsList(ps);
    rpmpsFree(ps);
    return problems;
}

static PyObject *
rpmte_DBOffset(rpmteObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmteDBOffset(s->te));
}

static PyObject *
rpmte_Key(rpmteObject * s, PyObject * unused)
{
    /* XXX how to insure this is a PyObject??? */
    PyObject * Key = (PyObject *) rpmteKey(s->te);
    if (Key == NULL)
	Key = Py_None;
    Py_INCREF(Key);
    return Key;
}

static PyObject *
rpmte_SetUserdata(rpmteObject * s, PyObject *arg)
{
    /* XXX how to insure this is a PyObject??? */
    PyObject *o = rpmteUserdata(s->te);
    rpmteSetUserdata(s->te, arg);
    Py_INCREF(arg);
    Py_XDECREF(o);
    Py_RETURN_NONE;
}

static PyObject *
rpmte_Userdata(rpmteObject * s)
{
    PyObject *po = Py_None;
    void *data = rpmteUserdata(s->te);

    if (data)
	po = data;

    Py_INCREF(po);
    return po;
}

static PyObject *
rpmte_DS(rpmteObject * s, PyObject * args, PyObject * kwds)
{
    rpmds ds;
    rpmTagVal tag;
    char * kwlist[] = {"tag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&:DS", kwlist,
				tagNumFromPyObject, &tag))
	return NULL;

    ds = rpmteDS(s->te, tag);
    if (ds == NULL) {
	Py_RETURN_NONE;
    }
    return rpmds_Wrap(&rpmds_Type, rpmdsLink(ds));
}

static PyObject *
rpmte_FI(rpmteObject * s, PyObject * args, PyObject * kwds)
{
    rpmfi fi;

    DEPRECATED_METHOD("use .Files() instead");

    fi = rpmteFI(s->te);
    if (fi == NULL) {
	Py_RETURN_NONE;
    }
    return rpmfi_Wrap(&rpmfi_Type, rpmfiLink(fi));
}

static PyObject *
rpmte_Files(rpmteObject * s, PyObject * args, PyObject * kwds)
{
    rpmfiles files = rpmteFiles(s->te);
    if (files == NULL) {
	Py_RETURN_NONE;
    }
    return rpmfiles_Wrap(&rpmfiles_Type, files);
}

static PyObject *
rpmte_Verified(rpmteObject * s)
{
    return Py_BuildValue("i", rpmteVerified(s->te));
}


static struct PyMethodDef rpmte_methods[] = {
    {"Type",	(PyCFunction)rpmte_TEType,	METH_NOARGS,
     "te.Type() -- Return element type (rpm.TR_ADDED | rpm.TR_REMOVED).\n" },
    {"N",	(PyCFunction)rpmte_N,		METH_NOARGS,
     "te.N() -- Return element name.\n" },
    {"E",	(PyCFunction)rpmte_E,		METH_NOARGS,
     "te.E() -- Return element epoch.\n" },
    {"V",	(PyCFunction)rpmte_V,		METH_NOARGS,
     "te.V() -- Return element version.\n" },
    {"R",	(PyCFunction)rpmte_R,		METH_NOARGS,
     "te.R() -- Return element release.\n" },
    {"A",	(PyCFunction)rpmte_A,		METH_NOARGS,
     "te.A() -- Return element arch.\n" },
    {"O",	(PyCFunction)rpmte_O,		METH_NOARGS,
     "te.O() -- Return element os.\n" },
    {"NEVR",	(PyCFunction)rpmte_NEVR,	METH_NOARGS,
     "te.NEVR() -- Return element name-[epoch:]version-release.\n" },
    {"NEVRA",	(PyCFunction)rpmte_NEVRA,	METH_NOARGS,
     "te.NEVRA() -- Return element name-[epoch:]version-release.arch\n" },
    {"Color",(PyCFunction)rpmte_Color,		METH_NOARGS,
     "te.Color() -- Return package color bits."},
    {"PkgFileSize",(PyCFunction)rpmte_PkgFileSize,	METH_NOARGS,
     "te.PkgFileSize() -- Return no. of bytes in package file (approx)."},
    {"Parent",	(PyCFunction)rpmte_Parent,	METH_NOARGS,
     "te.Parent() -- Return the parent element index."},
    {"Problems",(PyCFunction)rpmte_Problems,	METH_NOARGS,
     "te.Problems() -- Return problems associated with this element."},
    {"DBOffset",(PyCFunction)rpmte_DBOffset,	METH_NOARGS,
     "te.DBOffset() -- Return the Package's database instance number.\n"},
    {"Failed",	(PyCFunction)rpmte_Failed,	METH_NOARGS,
     "te.Failed() -- Return if there are any related errors."},
    {"Key",	(PyCFunction)rpmte_Key,		METH_NOARGS,
     "te.Key() -- Return the associated opaque retrieval key\n\
	as passed e.g. as data arg ts.addInstall()"},
    {"Userdata",	(PyCFunction)rpmte_Userdata,	METH_NOARGS,
     "te.Userdata() -- Return associated user data (if any)\n"},
    {"SetUserdata",	(PyCFunction)rpmte_SetUserdata,	METH_O,
     "te.SetUserdata() -- Set associated user data (if any)\n"},
    {"DS",	(PyCFunction)rpmte_DS,		METH_VARARGS|METH_KEYWORDS,
"te.DS(TagN) -- Return the TagN dependency set (or None).\n\
	TagN is one of 'Providename', 'Requirename', 'Obsoletename',\n\
	'Conflictname', 'Triggername', 'Recommendname', 'Suggestname',\n\
	'Supplementname', 'Enhancename'" },
    {"FI",	(PyCFunction)rpmte_FI,		METH_VARARGS|METH_KEYWORDS,
"te.FI(TagN) -- Return file info iterator of element.\n\n DEPRECATED! Use .Files() instead.\n" },
    {"Files",	(PyCFunction)rpmte_Files,	METH_NOARGS,
"te.Files() -- Return file info set of element.\n" },
    {"Verified",(PyCFunction)rpmte_Verified,	METH_NOARGS,
"te.Verified() -- Return element verification status.\n" },
    {NULL,		NULL}		/* sentinel */
};

/* ---------- */

static char rpmte_doc[] =
"";

PyTypeObject rpmte_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.te",			/* tp_name */
	sizeof(rpmteObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor)0,		 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0,		 	/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
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
	rpmte_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmte_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

PyObject * rpmte_Wrap(PyTypeObject *subtype, rpmte te)
{
    rpmteObject *s = (rpmteObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->te = te;
    return (PyObject *) s;
}
