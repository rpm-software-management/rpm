/** \ingroup py_c
 * \file python/rpmte-py.c
 */

#include "system.h"

#include <rpmlib.h>

#include "header-py.h"	/* XXX tagNumFromPyObject */
#include "rpmds-py.h"
#include "rpmfi-py.h"
#include "rpmte-py.h"

#include "debug.h"

/*@access rpmte @*/
/*@access fnpyKey @*/

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
 * - te.Depth()	Return the level in the dependency tree (after ordering).
 * - te.Npreds() Return the number of package prerequisites (after ordering).
 * - te.Degree() Return the parent's degree + 1.
 * - te.Parent() Return the parent element index.
 * - te.Tree()	Return the root dependency tree index.
 * - te.AddedKey() Return the added package index (TR_ADDED).
 * - te.DependsOnKey() Return the package index for the added package (TR_REMOVED).
 * - te.DBOffset() Return the Packages database instance number (TR_REMOVED)
 * - te.Key()	Return the associated opaque key, i.e. 2nd arg ts.addInstall().
 * - te.DS(tag)	Return package dependency set.
 * @param tag	'Providename', 'Requirename', 'Obsoletename', 'Conflictname'
 * - te.FI(tag)	Return package file info set.
 * @param tag	'Basenames'
 */

/*@null@*/
static PyObject *
rpmte_Debug(/*@unused@*/ rpmteObject * s, PyObject * args, PyObject * kwds)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    char * kwlist[] = {"debugLevel", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &_rpmte_debug))
	return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

/*@null@*/
static PyObject *
rpmte_TEType(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteType(s->te));
}

/*@null@*/
static PyObject *
rpmte_N(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmteN(s->te));
}

/*@null@*/
static PyObject *
rpmte_E(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmteE(s->te));
}

/*@null@*/
static PyObject *
rpmte_V(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmteV(s->te));
}

/*@null@*/
static PyObject *
rpmte_R(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmteR(s->te));
}

/*@null@*/
static PyObject *
rpmte_A(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmteA(s->te));
}

/*@null@*/
static PyObject *
rpmte_O(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmteO(s->te));
}

/*@null@*/
static PyObject *
rpmte_NEVR(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmteNEVR(s->te));
}

/*@null@*/
static PyObject *
rpmte_Color(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteColor(s->te));
}

/*@null@*/
static PyObject *
rpmte_PkgFileSize(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmtePkgFileSize(s->te));
}

/*@null@*/
static PyObject *
rpmte_Depth(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteDepth(s->te));
}

/*@null@*/
static PyObject *
rpmte_Npreds(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteNpreds(s->te));
}

/*@null@*/
static PyObject *
rpmte_Degree(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteDegree(s->te));
}

/*@null@*/
static PyObject *
rpmte_Parent(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteParent(s->te));
}

/*@null@*/
static PyObject *
rpmte_Tree(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteTree(s->te));
}

/*@null@*/
static PyObject *
rpmte_AddedKey(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteAddedKey(s->te));
}

/*@null@*/
static PyObject *
rpmte_DependsOnKey(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteDependsOnKey(s->te));
}

/*@null@*/
static PyObject *
rpmte_DBOffset(rpmteObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmteDBOffset(s->te));
}

/*@null@*/
static PyObject *
rpmte_Key(rpmteObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    PyObject * Key;

    /* XXX how to insure this is a PyObject??? */
    Key = (PyObject *) rpmteKey(s->te);
    if (Key == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }
    return Key;
}

/*@null@*/
static PyObject *
rpmte_DS(rpmteObject * s, PyObject * args, PyObject * kwds)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    PyObject * TagN = NULL;
    rpmds ds;
    rpmTag tag;
    char * kwlist[] = {"tag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:DS", kwlist, &TagN))
	return NULL;

    tag = tagNumFromPyObject(TagN);
    if (tag == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    ds = rpmteDS(s->te, tag);
    if (ds == NULL) {
#ifdef	DYING
	PyErr_SetString(PyExc_TypeError, "invalid ds tag");
	return NULL;
#else
	Py_INCREF(Py_None);
	return Py_None;
#endif
    }
    return (PyObject *) rpmds_Wrap(rpmdsLink(ds, "rpmte_DS"));
}

/*@null@*/
static PyObject *
rpmte_FI(rpmteObject * s, PyObject * args, PyObject * kwds)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    PyObject * TagN = NULL;
    rpmfi fi;
    rpmTag tag;
    char * kwlist[] = {"tag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:FI", kwlist, &TagN))
	return NULL;

    tag = tagNumFromPyObject(TagN);
    if (tag == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    fi = rpmteFI(s->te, tag);
    if (fi == NULL) {
#ifdef	DYING
	PyErr_SetString(PyExc_TypeError, "invalid fi tag");
	return NULL;
#else
	Py_INCREF(Py_None);
	return Py_None;
#endif
    }
    return (PyObject *) rpmfi_Wrap(rpmfiLink(fi, "rpmte_FI"));
}

/** \ingroup py_c
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmte_methods[] = {
    {"Debug",	(PyCFunction)rpmte_Debug,	METH_VARARGS|METH_KEYWORDS,
	NULL},
    {"Type",	(PyCFunction)rpmte_TEType,	METH_NOARGS,
"te.Type() -> Type\n\
- Return element type (rpm.TR_ADDED | rpm.TR_REMOVED).\n" },
    {"N",	(PyCFunction)rpmte_N,		METH_NOARGS,
"te.N() -> N\n\
- Return element name.\n" },
    {"E",	(PyCFunction)rpmte_E,		METH_NOARGS,
"te.E() -> E\n\
- Return element epoch.\n" },
    {"V",	(PyCFunction)rpmte_V,		METH_NOARGS,
"te.V() -> V\n\
- Return element version.\n" },
    {"R",	(PyCFunction)rpmte_R,		METH_NOARGS,
"te.R() -> R\n\
- Return element release.\n" },
    {"A",	(PyCFunction)rpmte_A,		METH_NOARGS,
"te.A() -> A\n\
- Return element arch.\n" },
    {"O",	(PyCFunction)rpmte_O,		METH_NOARGS,
"te.O() -> O\n\
- Return element os.\n" },
    {"NEVR",	(PyCFunction)rpmte_NEVR,	METH_NOARGS,
"te.NEVR() -> NEVR\n\
- Return element name-version-release.\n" },
    {"Color",(PyCFunction)rpmte_Color,		METH_NOARGS,
	NULL},
    {"PkgFileSize",(PyCFunction)rpmte_PkgFileSize,	METH_NOARGS,
	NULL},
    {"Depth",	(PyCFunction)rpmte_Depth,	METH_NOARGS,
	NULL},
    {"Npreds",	(PyCFunction)rpmte_Npreds,	METH_NOARGS,
	NULL},
    {"Degree",	(PyCFunction)rpmte_Degree,	METH_NOARGS,
	NULL},
    {"Parent",	(PyCFunction)rpmte_Parent,	METH_NOARGS,
	NULL},
    {"Tree",	(PyCFunction)rpmte_Tree,	METH_NOARGS,
	NULL},
    {"AddedKey",(PyCFunction)rpmte_AddedKey,	METH_NOARGS,
	NULL},
    {"DependsOnKey",(PyCFunction)rpmte_DependsOnKey,	METH_NOARGS,
	NULL},
    {"DBOffset",(PyCFunction)rpmte_DBOffset,	METH_NOARGS,
	NULL},
    {"Key",	(PyCFunction)rpmte_Key,		METH_NOARGS,
	NULL},
    {"DS",	(PyCFunction)rpmte_DS,		METH_VARARGS|METH_KEYWORDS,
"te.DS(TagN) -> DS\n\
- Return the TagN dependency set (or None). TagN is one of\n\
	'Providename', 'Requirename', 'Obsoletename', 'Conflictname'\n" },
    {"FI",	(PyCFunction)rpmte_FI,		METH_VARARGS|METH_KEYWORDS,
"te.FI(TagN) -> FI\n\
- Return the TagN dependency set (or None). TagN must be 'Basenames'.\n" },
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

static int
rpmte_print(rpmteObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies fp, fileSystem @*/
{
    const char * tstr;
    if (!(s && s->te))
	return -1;
    switch (rpmteType(s->te)) {
    case TR_ADDED:	tstr = "++";	break;
    case TR_REMOVED:	tstr = "--";	break;
    default:		tstr = "??";	break;
    }
    fprintf(fp, "%s %s %s", tstr, rpmteNEVR(s->te), rpmteA(s->te));
    return 0;
}

static PyObject * rpmte_getattro(PyObject * o, PyObject * n)
	/*@*/
{
    return PyObject_GenericGetAttr(o, n);
}

static int rpmte_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr(o, n, v);
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmte_doc[] =
"";

/** \ingroup py_c
 */
/*@-fullinitblock@*/
PyTypeObject rpmte_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.te",			/* tp_name */
	sizeof(rpmteObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor)0,		 	/* tp_dealloc */
	(printfunc) rpmte_print,	/* tp_print */
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
	(getattrofunc) rpmte_getattro,	/* tp_getattro */
	(setattrofunc) rpmte_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmte_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
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
#endif
};
/*@=fullinitblock@*/

rpmteObject * rpmte_Wrap(rpmte te)
{
    rpmteObject *s = PyObject_New(rpmteObject, &rpmte_Type);
    if (s == NULL)
	return NULL;
    s->te = te;
    return s;
}
