/** \ingroup python
 * \file python/rpmts-py.c
 */

#include "system.h"

#include "Python.h"

#include <rpmlib.h>
#include "rpmdb.h"
#include "rpmps.h"

#include "rpmal.h"
#include "rpmds.h"
#include "rpmfi.h"
#include "rpmte.h"

#define	_RPMTS_INTERNAL	/* XXX for ts->rdb, ts->availablePackage */
#include "rpmts.h"

#include "db-py.h"
#include "header-py.h"
#include "rpmte-py.h"
#include "rpmts-py.h"

#include "debug.h"

/** \ingroup python
 * \name Class: rpm.ts
 * \class rpm.ts
 * \brief A python rpm.ts object represents an RPM transaction set.
 * 
 * The transaction set is the workhorse of RPM.  It performs the
 * installation and upgrade of packages.  The rpm.ts object is
 * instantiated by the TransactionSet function in the rpm module.
 *
 * The TransactionSet function takes two optional arguments.  The first
 * argument is the root path, the second is an open database to perform
 * the transaction set upon.
 *
 * A rpm.ts object has the following methods:
 *
 * - addInstall(hdr,data,mode)  Add an install element to a transaction set.
 * @param hdr	the header to be added
 * @param data	user data that will be passed to the transaction callback
 *		during transaction execution
 * @param mode 	optional argument that specifies if this package should
 *		be installed ('i'), upgraded ('u'), or if it is just
 *		available to the transaction when computing
 *		dependencies but no action should be performed with it
 *		('a').
 *
 * - addErase(name) Add an erase element to a transaction set.
 * @param name	the package name to be erased
 *
 * - check()	Perform a dependency check on the transaction set. After
 *		headers have been added to a transaction set, a dependency
 *		check can be performed to make sure that all package
 *		dependencies are satisfied.
 * @return	None If there are no unresolved dependencies
 *		Otherwise a list of complex tuples is returned, one tuple per
 *		unresolved dependency, with
 * The format of the dependency tuple is:
 *     ((packageName, packageVersion, packageRelease),
 *      (reqName, reqVersion),
 *      needsFlags,
 *      suggestedPackage,
 *      sense)
 *     packageName, packageVersion, packageRelease are the name,
 *     version, and release of the package that has the unresolved
 *     dependency or conflict.
 *     The reqName and reqVersion are the name and version of the
 *     requirement or conflict.
 *     The needsFlags is a bitfield that describes the versioned
 *     nature of a requirement or conflict.  The constants
 *     rpm.RPMSENSE_LESS, rpm.RPMSENSE_GREATER, and
 *     rpm.RPMSENSE_EQUAL can be logical ANDed with the needsFlags
 *     to get versioned dependency information.
 *     suggestedPackage is a tuple if the dependency check was aware
 *     of a package that solves this dependency problem when the
 *     dependency check was run.  Packages that are added to the
 *     transaction set as "available" are examined during the
 *     dependency check as possible dependency solvers. The tuple
 *     contains two values, (header, suggestedName).  These are set to
 *     the header of the suggested package and its name, respectively.
 *     If there is no known package to solve the dependency problem,
 *     suggestedPackage is None.
 *     The constants rpm.RPMDEP_SENSE_CONFLICTS and
 *     rpm.RPMDEP_SENSE_REQUIRES are set to show a dependency as a
 *     requirement or a conflict.
 *
 * - order()	Do a topological sort of added element relations.
 * @return	None
 *
 * - run(flags,problemSetFilter,callback,data) Attempt to execute a
 *	transaction set. After the transaction set has been populated
 *	with install and upgrade actions, it can be executed by invoking
 *	the run() method.
 * @param flags - modifies the behavior of the transaction set as it is
 *		processed.  The following values can be locical ORed
 *		together:
 *	- rpm.RPMTRANS_FLAG_TEST - test mode, do not modify the RPM
 *		database, change any files, or run any package scripts
 *	- rpm.RPMTRANS_FLAG_BUILD_PROBS - only build a list of
 *		problems encountered when attempting to run this transaction
 *		set
 *	- rpm.RPMTRANS_FLAG_NOSCRIPTS - do not execute package scripts
 *	- rpm.RPMTRANS_FLAG_JUSTDB - only make changes to the rpm
 *		database, do not modify files.
 *	- rpm.RPMTRANS_FLAG_NOTRIGGERS - do not run trigger scripts
 *	- rpm.RPMTRANS_FLAG_NODOCS - do not install files marked as %doc
 *	- rpm.RPMTRANS_FLAG_ALLFILES - create all files, even if a
 *		file is marked %config(missingok) and an upgrade is
 *		being performed.
 *	- rpm.RPMTRANS_FLAG_KEEPOBSOLETE - do not remove obsoleted
 *		packages.
 * @param problemSetFilter - control bit(s) to ignore classes of problems,
 *		any of
 *	- rpm.RPMPROB_FILTER_IGNOREOS - 
 *	- rpm.RPMPROB_FILTER_IGNOREARCH - 
 *	- rpm.RPMPROB_FILTER_REPLACEPKG - 
 *	- rpm.RPMPROB_FILTER_FORCERELOCATE - 
 *	- rpm.RPMPROB_FILTER_REPLACENEWFILES - 
 *	- rpm.RPMPROB_FILTER_REPLACEOLDFILES - 
 *	- rpm.RPMPROB_FILTER_OLDPACKAGE - 
 *	- rpm.RPMPROB_FILTER_DISKSPACE - 
 */

/**
 * Add package to universe of possible packages to install in transaction set.
 * @param ts		transaction set
 * @param h		header
 * @param key		package private data
 */
static void rpmtsAddAvailableElement(rpmts ts, Header h,
		/*@exposed@*/ /*@null@*/ fnpyKey key)
	/*@modifies h, ts @*/
{
    int scareMem = 0;
    rpmds provides = rpmdsNew(h, RPMTAG_PROVIDENAME, scareMem);
    rpmfi fi = rpmfiNew(ts, NULL, h, RPMTAG_BASENAMES, scareMem);

    /* XXX FIXME: return code RPMAL_NOMATCH is error */
    (void) rpmalAdd(&ts->availablePackages, RPMAL_NOMATCH, key,
		provides, fi);
    fi = rpmfiFree(fi, 1);
    provides = rpmdsFree(provides);
}

/** \ingroup python
 */
static PyObject *
rpmts_AddInstall(rpmtsObject * s, PyObject * args)
{
    hdrObject * h;
    PyObject * key;
    char * how = NULL;
    int isUpgrade = 0;

    if (!PyArg_ParseTuple(args, "O!O|s:Add", &hdr_Type, &h, &key, &how))
	return NULL;

#ifdef	DYING
    {	PyObject * hObj = (PyObject *) h;
	if (hObj->ob_type != &hdr_Type) {
	    PyErr_SetString(PyExc_TypeError, "bad type for header argument");
	    return NULL;
	}
    }
#endif

    if (how && strcmp(how, "a") && strcmp(how, "u") && strcmp(how, "i")) {
	PyErr_SetString(PyExc_TypeError, "how argument must be \"u\", \"a\", or \"i\"");
	return NULL;
    } else if (how && !strcmp(how, "u"))
    	isUpgrade = 1;

    if (how && !strcmp(how, "a"))
	rpmtsAddAvailableElement(s->ts, hdrGetHeader(h), key);
    else
	rpmtsAddInstallElement(s->ts, hdrGetHeader(h), key, isUpgrade, NULL);

    /* This should increment the usage count for me */
    if (key) {
	PyList_Append(s->keyList, key);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject *
rpmts_AddErase(rpmtsObject * s, PyObject * args)
{
    char * name;
    int count;
    rpmdbMatchIterator mi;
    
    if (!PyArg_ParseTuple(args, "s:Remove", &name))
        return NULL;

    /* XXX: Copied hack from ../lib/rpminstall.c, rpmErase() */
    mi = rpmdbInitIterator(dbFromDb(s->dbo), RPMDBI_LABEL, name, 0);
    count = rpmdbGetIteratorCount(mi);
    if (count <= 0) {
        PyErr_SetString(pyrpmError, "package not installed");
        return NULL;
    } else { /* XXX: Note that we automatically choose to remove all matches */
        Header h;
        while ((h = rpmdbNextIterator(mi)) != NULL) {
	    unsigned int recOffset = rpmdbGetIteratorOffset(mi);
	    if (recOffset) {
	        rpmtsAddEraseElement(s->ts, h, recOffset);
	    }
	}
    }
    rpmdbFreeIterator(mi);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject *
rpmts_Check(rpmtsObject * s, PyObject * args)
{
    rpmps ps;
    rpmProblem p;
    PyObject * list, * cf;
    int i;
    int allSuggestions = 0;
    int xx;

    if (!PyArg_ParseTuple(args, "|i:Check", &allSuggestions)) return NULL;

    xx = rpmtsCheck(s->ts);
    ps = rpmtsProblems(s->ts);
    if (ps) {
	list = PyList_New(0);

	/* XXX TODO: rpmlib >= 4.0.3 can return multiple suggested keys. */
	for (i = 0; i < ps->numProblems; i++) {
#ifdef	DYING
	    cf = Py_BuildValue("((sss)(ss)iOi)", conflicts[i].byName,
			       conflicts[i].byVersion, conflicts[i].byRelease,

			       conflicts[i].needsName,
			       conflicts[i].needsVersion,

			       conflicts[i].needsFlags,
			       conflicts[i].suggestedPkgs ?
				   conflicts[i].suggestedPkgs[0] : Py_None,
			       conflicts[i].sense);
#else
	    char * byName, * byVersion, * byRelease;
	    char * needsName, * needsOP, * needsVersion;
	    int needsFlags, sense;
	    fnpyKey key;
	    
	    p = ps->probs + i;

	    byName = p->pkgNEVR;
	    if ((byRelease = strrchr(byName, '-')) != NULL)
		*byRelease++ = '\0';
	    if ((byVersion = strrchr(byName, '-')) != NULL)
		*byVersion++ = '\0';

	    key = p->key;

	    needsName = p->altNEVR;
	    if (needsName[1] == ' ') {
		sense = (needsName[0] == 'C')
			? RPMDEP_SENSE_CONFLICTS : RPMDEP_SENSE_REQUIRES;
		needsName += 2;
	    } else
		sense = RPMDEP_SENSE_REQUIRES;
	    if ((needsVersion = strrchr(needsName, ' ')) != NULL)
		*needsVersion++ = '\0';

	    needsFlags = 0;
	    if ((needsOP = strrchr(needsName, ' ')) != NULL) {
		for (*needsOP++ = '\0'; *needsOP != '\0'; needsOP++) {
		    if (*needsOP == '<')	needsFlags |= RPMSENSE_LESS;
		    else if (*needsOP == '>')	needsFlags |= RPMSENSE_GREATER;
		    else if (*needsOP == '=')	needsFlags |= RPMSENSE_EQUAL;
		}
	    }
	    
	    cf = Py_BuildValue("((sss)(ss)iOi)", byName, byVersion, byRelease,
			       needsName, needsVersion, needsFlags,
			       (key != NULL ? key : Py_None),
			       sense);
#endif
	    PyList_Append(list, (PyObject *) cf);
	    Py_DECREF(cf);
	}

	ps = rpmpsFree(ps);

	return list;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject *
rpmts_Order(rpmtsObject * s, PyObject * args)
{
    int xx;

    if (!PyArg_ParseTuple(args, ":Order")) return NULL;

    xx = rpmtsOrder(s->ts);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject *
rpmts_GetKeys(rpmtsObject * s, PyObject * args)
{
    const void **data = NULL;
    int num, i;
    PyObject *tuple;

    if (!PyArg_ParseTuple(args, ":GetKeys")) return NULL;

    rpmtsGetKeys(s->ts, &data, &num);
    if (data == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    tuple = PyTuple_New(num);

    for (i = 0; i < num; i++) {
	PyObject *obj = (PyObject *) data[i];
	Py_INCREF(obj);
	PyTuple_SetItem(tuple, i, obj);
    }

    free (data);

    return tuple;
}

/** \ingroup python
 */
struct rpmtsCallbackType_s {
    PyObject * cb;
    PyObject * data;
    int pythonError;
};

/** \ingroup python
 */
static void *
rpmtsCallback(const void * hd, const rpmCallbackType what,
		         const unsigned long amount, const unsigned long total,
	                 const void * pkgKey, rpmCallbackData data)
{
    struct rpmtsCallbackType_s * cbInfo = data;
    PyObject * args, * result;
    int fd;
    static FD_t fdt;

    if (cbInfo->pythonError) return NULL;
    if (cbInfo->cb == Py_None) return NULL;

    if (!pkgKey) pkgKey = Py_None;

    args = Py_BuildValue("(illOO)", what, amount, total, pkgKey, cbInfo->data);
    result = PyEval_CallObject(cbInfo->cb, args);
    Py_DECREF(args);

    if (!result) {
	cbInfo->pythonError = 1;
	return NULL;
    }

    if (what == RPMCALLBACK_INST_OPEN_FILE) {
        if (!PyArg_Parse(result, "i", &fd)) {
	    cbInfo->pythonError = 1;
	    return NULL;
	}
	fdt = fdDup(fd);
	
	Py_DECREF(result);
	return fdt;
    }

    if (what == RPMCALLBACK_INST_CLOSE_FILE) {
	Fclose (fdt);
    }

    Py_DECREF(result);

    return NULL;
}

/** \ingroup python
 */
static PyObject * rpmts_Run(rpmtsObject * s, PyObject * args)
{
    int flags, ignoreSet;
    int rc, i;
    PyObject * list;
    rpmps ps;
    struct rpmtsCallbackType_s cbInfo;

    if (!PyArg_ParseTuple(args, "iiOO:Run", &flags, &ignoreSet, &cbInfo.cb,
			  &cbInfo.data))
	return NULL;

    cbInfo.pythonError = 0;

    (void) rpmtsSetNotifyCallback(s->ts, rpmtsCallback, (void *) &cbInfo);
    (void) rpmtsSetFlags(s->ts, flags);

    rc = rpmtsRun(s->ts, NULL, ignoreSet);
    ps = rpmtsProblems(s->ts);

    if (cbInfo.pythonError) {
	ps = rpmpsFree(ps);
	return NULL;
    }

    if (rc < 0) {
	list = PyList_New(0);
	return list;
    } else if (!rc) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    list = PyList_New(0);
    for (i = 0; i < ps->numProblems; i++) {
	rpmProblem p = ps->probs + i;
	PyObject * prob = Py_BuildValue("s(isN)", rpmProblemString(p),
			     p->type,
			     p->str1,
			     PyLong_FromLongLong(p->ulong1));
	PyList_Append(list, prob);
	Py_DECREF(prob);
    }

    ps = rpmpsFree(ps);

    return list;
}

#if Py_TPFLAGS_HAVE_ITER
static PyObject *
rpmts_Next(rpmtsObject * s)
{
    rpmte te;

    if (s == NULL || s->tsi == NULL)
	return NULL;

    te = rpmtsiNext(s->tsi, s->tsiFilter);
    if (te == NULL) {
	s->tsi = rpmtsiFree(s->tsi);
	s->tsiFilter = 0;
	Py_INCREF(Py_None);
	return Py_None;
    }

    return (PyObject *) rpmte_Wrap(te);
}

static PyObject *
rpmts_Iter(rpmtsObject * s)
{
    s->tsi = rpmtsiInit(s->ts);
    s->tsiFilter = 0;
    Py_INCREF(s);
    return (PyObject *) s;
}
#endif

/** \ingroup python
 */
static struct PyMethodDef rpmts_methods[] = {
    {"addInstall",	(PyCFunction) rpmts_AddInstall,	METH_VARARGS,
	NULL },
    {"addErase",	(PyCFunction) rpmts_AddErase,	METH_VARARGS,
	NULL },
    {"check",		(PyCFunction) rpmts_Check,	METH_VARARGS,
	NULL },
    {"order",		(PyCFunction) rpmts_Order,	METH_VARARGS,
	NULL },
    {"getKeys",		(PyCFunction) rpmts_GetKeys,	METH_VARARGS,
	NULL },
    {"run",		(PyCFunction) rpmts_Run,	METH_VARARGS,
	NULL },
#if Py_TPFLAGS_HAVE_ITER
    {"next",		(PyCFunction)rpmts_Next,	METH_VARARGS,
	NULL},
    {"iter",		(PyCFunction)rpmts_Iter,	METH_VARARGS,
	NULL},
#endif
    {NULL,		NULL}		/* sentinel */
};

/** \ingroup python
 */
static void rpmts_dealloc(PyObject * o)
{
    rpmtsObject * trans = (void *) o;

    trans->ts->rdb = NULL;	/* XXX HACK: avoid rpmdb close/free */
    rpmtsFree(trans->ts);
    if (trans->dbo) {
	Py_DECREF(trans->dbo);
    }
    if (trans->scriptFd) Fclose(trans->scriptFd);
    /* this will free the keyList, and decrement the ref count of all
       the items on the list as well :-) */
    Py_DECREF(trans->keyList);
    PyMem_DEL(o);
}

/** \ingroup python
 */
static PyObject * rpmts_getattr(rpmtsObject * o, char * name)
{
    return Py_FindMethod(rpmts_methods, (PyObject *) o, name);
}

/** \ingroup python
 */
static int rpmts_setattr(rpmtsObject * o, char * name, PyObject * val)
{
    int i;

    if (!strcmp(name, "scriptFd")) {
	if (!PyArg_Parse(val, "i", &i)) return 0;
	if (i < 0) {
	    PyErr_SetString(PyExc_TypeError, "bad file descriptor");
	    return -1;
	} else {
	    o->scriptFd = fdDup(i);
	    rpmtsSetScriptFd(o->ts, o->scriptFd);
	}
    } else {
	PyErr_SetString(PyExc_AttributeError, name);
	return -1;
    }

    return 0;
}

/**
 */
static char rpmts_doc[] =
"";

/** \ingroup python
 */
PyTypeObject rpmts_Type = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"rpm.ts",			/* tp_name */
	sizeof(rpmtsObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmts_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmts_getattr, 	/* tp_getattr */
	(setattrfunc) rpmts_setattr,	/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmts_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc)rpmts_Iter,	/* tp_iter */
	(iternextfunc)rpmts_Next,	/* tp_iternext */
	rpmts_methods,			/* tp_methods */
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

/**
 */
rpmtsObject *
rpmts_Create(PyObject * self, PyObject * args)
{
    rpmtsObject * o;
    rpmdbObject * db = NULL;
    char * rootDir = "/";

    if (!PyArg_ParseTuple(args, "|sO!:Create", &rootDir, &rpmdb_Type, &db))
	return NULL;
#ifdef	DYING
    if (db && ((PyObject *) db)->ob_type != &rpmdb_Type) {
	PyErr_SetString(PyExc_TypeError, "bad type for database argument");
	return NULL;
    }
#endif

    o = (void *) PyObject_NEW(rpmtsObject, &rpmts_Type);

    Py_XINCREF(db);
    o->dbo = db;
    o->scriptFd = NULL;
    o->ts = rpmtsCreate();
    (void) rpmtsSetRootDir(o->ts, rootDir);
    /* XXX this will be fun to fix */
    o->ts->rdb = (db ? dbFromDb(db) : NULL);
    o->keyList = PyList_New(0);

    return o;
}
