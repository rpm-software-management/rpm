/** \ingroup python
 * \file python/rpmmodule.c
 */

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <glob.h>	/* XXX rpmio.h */
#include <dirent.h>	/* XXX rpmio.h */
#include <locale.h>
#include <time.h>

#include "Python.h"
#include "rpmio_internal.h"
#include "rpmcli.h"	/* XXX for rpmCheckSig */
#include "rpmts.h"	/* XXX for ts->rpmdb */
#include "legacy.h"
#include "misc.h"
#include "header_internal.h"
#include "upgrade.h"

#include "db-py.h"
#include "header-py.h"

extern int _rpmio_debug;

/* XXX These names/constants have been removed from the rpmlib API. */
enum {
   RPMDEP_SENSE_REQUIRES,		/*!< requirement not satisfied. */
   RPMDEP_SENSE_CONFLICTS		/*!< conflict was found. */
};

#ifdef __LCLINT__
#undef	PyObject_HEAD
#define	PyObject_HEAD	int _PyObjectHead
#endif

void initrpm(void);

/* from lib/misc.c */
int rpmvercmp(const char * one, const char * two);

/** \ingroup python
 */
typedef struct rpmtransObject_s rpmtransObject;

/** \ingroup python
 * \name Class: rpmtrans
 * \class rpmtrans
 * \brief A python rpmtrans object represents an RPM transaction set.
 * 
 * The transaction set is the workhorse of RPM.  It performs the
 * installation and upgrade of packages.  The rpmtrans object is
 * instantiated by the TransactionSet function in the rpm module.
 *
 * The TransactionSet function takes two optional arguments.  The first
 * argument is the root path, the second is an open database to perform
 * the transaction set upon.
 *
 * A rpmtrans object has the following methods:
 *
 * - add(header,data,mode)	Add a binary package to a transaction set.
 * @param header the header to be added
 * @param data	user data that will be passed to the transaction callback
 *		during transaction execution
 * @param mode 	optional argument that specifies if this package should
 *		be installed ('i'), upgraded ('u'), or if it is just
 *		available to the transaction when computing
 *		dependencies but no action should be performed with it
 *		('a').
 *
 * - remove
 *
 * - depcheck()	Perform a dependency and conflict check on the
 *		transaction set. After headers have been added to a
 *		transaction set, a dependency check can be performed
 *		to make sure that all package dependencies are
 *		satisfied.
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

/** \ingroup python
 * \name Class: rpmtrans
 */
/*@{*/

/** \ingroup python
 */
struct rpmtransObject_s {
    PyObject_HEAD;
    rpmdbObject * dbo;
    rpmTransactionSet ts;
    PyObject * keyList;			/* keeps reference counts correct */
    FD_t scriptFd;
} ;

/** \ingroup python
 */
static PyObject * rpmtransAdd(rpmtransObject * s, PyObject * args) {
    hdrObject * h;
    PyObject * key;
    char * how = NULL;
    int isUpgrade = 0;
    PyObject * hObj;

    if (!PyArg_ParseTuple(args, "OO|s", &h, &key, &how)) return NULL;

    hObj = (PyObject *) h;
    if (hObj->ob_type != &hdrType) {
	PyErr_SetString(PyExc_TypeError, "bad type for header argument");
	return NULL;
    }

    if (how && strcmp(how, "a") && strcmp(how, "u") && strcmp(how, "i")) {
	PyErr_SetString(PyExc_TypeError, "how argument must be \"u\", \"a\", or \"i\"");
	return NULL;
    } else if (how && !strcmp(how, "u"))
    	isUpgrade = 1;

    if (how && !strcmp(how, "a"))
	rpmtransAvailablePackage(s->ts, hdrGetHeader(h), key);
    else
	rpmtransAddPackage(s->ts, hdrGetHeader(h), key, isUpgrade, NULL);

    /* This should increment the usage count for me */
    if (key) {
	PyList_Append(s->keyList, key);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * rpmtransRemove(rpmtransObject * s, PyObject * args) {
    char * name;
    int count;
    rpmdbMatchIterator mi;
    
    if (!PyArg_ParseTuple(args, "s", &name))
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
	        rpmtransRemovePackage(s->ts, h, recOffset);
	    }
	}
    }
    rpmdbFreeIterator(mi);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * rpmtransDepCheck(rpmtransObject * s, PyObject * args) {
    rpmProblem conflicts, c;
    int numConflicts;
    PyObject * list, * cf;
    int i;
    int allSuggestions = 0;

    if (!PyArg_ParseTuple(args, "|i", &allSuggestions)) return NULL;

    rpmdepCheck(s->ts, &conflicts, &numConflicts);
    if (numConflicts) {
	list = PyList_New(0);

	/* XXX TODO: rpmlib >= 4.0.3 can return multiple suggested keys. */
	for (i = 0; i < numConflicts; i++) {
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
	    
	    c = conflicts + i;

	    byName = c->pkgNEVR;
	    if ((byRelease = strrchr(byName, '-')) != NULL)
		*byRelease++ = '\0';
	    if ((byVersion = strrchr(byName, '-')) != NULL)
		*byVersion++ = '\0';

	    key = c->key;

	    needsName = c->altNEVR;
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

	conflicts = rpmdepFreeConflicts(conflicts, numConflicts);

	return list;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * rpmtransOrder(rpmtransObject * s, PyObject * args) {
    if (!PyArg_ParseTuple(args, "")) return NULL;

    rpmdepOrder(s->ts);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * py_rpmtransGetKeys(rpmtransObject * s, PyObject * args) {
    const void **data = NULL;
    int num, i;
    PyObject *tuple;

    rpmtransGetKeys(s->ts, &data, &num);
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
struct tsCallbackType {
    PyObject * cb;
    PyObject * data;
    int pythonError;
};

/** \ingroup python
 * @todo Remove, there's no headerLink refcount on the pointer.
 */
static Header transactionSetHeader = NULL;

/** \ingroup python
 */
static void * tsCallback(const void * hd, const rpmCallbackType what,
		         const unsigned long amount, const unsigned long total,
	                 const void * pkgKey, rpmCallbackData data) {
    struct tsCallbackType * cbInfo = data;
    PyObject * args, * result;
    int fd;
    static FD_t fdt;
    const Header h = (Header) hd;

    if (cbInfo->pythonError) return NULL;
    if (cbInfo->cb == Py_None) return NULL;

    if (!pkgKey) pkgKey = Py_None;
    transactionSetHeader = h;    

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
static PyObject * rpmtransRun(rpmtransObject * s, PyObject * args) {
    int flags, ignoreSet;
    int rc, i;
    PyObject * list, * prob;
    rpmProblemSet probs;
    struct tsCallbackType cbInfo;

    if (!PyArg_ParseTuple(args, "iiOO", &flags, &ignoreSet, &cbInfo.cb,
			  &cbInfo.data))
	return NULL;

    cbInfo.pythonError = 0;

    (void) rpmtsSetNotifyCallback(s->ts, tsCallback, (void *) &cbInfo);
    (void) rpmtsSetFlags(s->ts, flags);

    rc = rpmRunTransactions(s->ts, NULL, &probs, ignoreSet);

    if (cbInfo.pythonError) {
	if (rc > 0)
	    rpmProblemSetFree(probs);
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
    for (i = 0; i < probs->numProblems; i++) {
	rpmProblem myprob = probs->probs + i;
	prob = Py_BuildValue("s(isN)", rpmProblemString(myprob),
			     myprob->type,
			     myprob->str1,
			     PyLong_FromLongLong(myprob->ulong1));
	PyList_Append(list, prob);
	Py_DECREF(prob);
    }

    rpmProblemSetFree(probs);

    return list;
}

/** \ingroup python
 */
static struct PyMethodDef rpmtransMethods[] = {
	{"add",		(PyCFunction) rpmtransAdd,	1 },
	{"remove",	(PyCFunction) rpmtransRemove,	1 },
	{"depcheck",	(PyCFunction) rpmtransDepCheck,	1 },
	{"order",	(PyCFunction) rpmtransOrder,	1 },
	{"getKeys",	(PyCFunction) py_rpmtransGetKeys, 1 },
	{"run",		(PyCFunction) rpmtransRun, 1 },
	{NULL,		NULL}		/* sentinel */
};

/** \ingroup python
 */
static PyObject * rpmtransGetAttr(rpmtransObject * o, char * name) {
    return Py_FindMethod(rpmtransMethods, (PyObject *) o, name);
}

/** \ingroup python
 */
static void rpmtransDealloc(PyObject * o) {
    rpmtransObject * trans = (void *) o;

    trans->ts->rpmdb = NULL;	/* XXX HACK: avoid rpmdb close/free */
    rpmtransFree(trans->ts);
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
static int rpmtransSetAttr(rpmtransObject * o, char * name,
			   PyObject * val) {
    int i;

    if (!strcmp(name, "scriptFd")) {
	if (!PyArg_Parse(val, "i", &i)) return 0;
	if (i < 0) {
	    PyErr_SetString(PyExc_TypeError, "bad file descriptor");
	    return -1;
	} else {
	    o->scriptFd = fdDup(i);
	    rpmtransSetScriptFd(o->ts, o->scriptFd);
	}
    } else {
	PyErr_SetString(PyExc_AttributeError, name);
	return -1;
    }

    return 0;
}

/** \ingroup python
 */
static PyTypeObject rpmtransType = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"rpmtrans",			/* tp_name */
	sizeof(rpmtransObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmtransDealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmtransGetAttr, 	/* tp_getattr */
	(setattrfunc) rpmtransSetAttr,	/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
};

/*@}*/

/** \ingroup python
 * \name Module: rpm
 */
/*@{*/

/**
 */
static PyObject * rpmtransCreate(PyObject * self, PyObject * args) {
    rpmtransObject * o;
    rpmdbObject * db = NULL;
    char * rootPath = "/";

    if (!PyArg_ParseTuple(args, "|sO", &rootPath, &db)) return NULL;
    if (db && ((PyObject *) db)->ob_type != &rpmdbType) {
	PyErr_SetString(PyExc_TypeError, "bad type for database argument");
	return NULL;
    }

    o = (void *) PyObject_NEW(rpmtransObject, &rpmtransType);

    Py_XINCREF(db);
    o->dbo = db;
    o->scriptFd = NULL;
    o->ts = rpmtransCreateSet(NULL, rootPath);
    o->ts->rpmdb = (db ? dbFromDb(db) : NULL);
    o->keyList = PyList_New(0);

    return (void *) o;
}

/**
 */
static PyObject * doAddMacro(PyObject * self, PyObject * args) {
    char * name, * val;

    if (!PyArg_ParseTuple(args, "ss", &name, &val))
	return NULL;

    addMacro(NULL, name, NULL, val, RMIL_DEFAULT);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject * doDelMacro(PyObject * self, PyObject * args) {
    char * name;

    if (!PyArg_ParseTuple(args, "s", &name))
	return NULL;

    delMacro(NULL, name);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject * archScore(PyObject * self, PyObject * args) {
    char * arch;
    int score;

    if (!PyArg_ParseTuple(args, "s", &arch))
	return NULL;

    score = rpmMachineScore(RPM_MACHTABLE_INSTARCH, arch);

    return Py_BuildValue("i", score);
}

/**
 */
static int psGetArchScore(Header h) {
    void * pkgArch;
    int type, count;

    if (!headerGetEntry(h, RPMTAG_ARCH, &type, (void **) &pkgArch, &count) ||
        type == RPM_INT8_TYPE)
       return 150;
    else
        return rpmMachineScore(RPM_MACHTABLE_INSTARCH, pkgArch);
}

/**
 */
static int pkgCompareVer(void * first, void * second) {
    struct packageInfo ** a = first;
    struct packageInfo ** b = second;
    int ret, score1, score2;

    /* put packages w/o names at the end */
    if (!(*a)->name) return 1;
    if (!(*b)->name) return -1;

    ret = xstrcasecmp((*a)->name, (*b)->name);
    if (ret) return ret;
    score1 = psGetArchScore((*a)->h);
    if (!score1) return 1;
    score2 = psGetArchScore((*b)->h);
    if (!score2) return -1;
    if (score1 < score2) return -1;
    if (score1 > score2) return 1;
    return rpmVersionCompare((*b)->h, (*a)->h);
}

/**
 */
static void pkgSort(struct pkgSet * psp) {
    int i;
    char *name;

    if (psp->numPackages <= 0)
	return;

    qsort(psp->packages, psp->numPackages, sizeof(*psp->packages),
	 (void *) pkgCompareVer);

    name = psp->packages[0]->name;
    if (!name) {
       psp->numPackages = 0;
       return;
    }
    for (i = 1; i < psp->numPackages; i++) {
       if (!psp->packages[i]->name) break;
       if (!strcmp(psp->packages[i]->name, name))
	   psp->packages[i]->name = NULL;
       else
	   name = psp->packages[i]->name;
    }

    qsort(psp->packages, psp->numPackages, sizeof(*psp->packages),
	 (void *) pkgCompareVer);

    for (i = 0; i < psp->numPackages; i++)
       if (!psp->packages[i]->name) break;
    psp->numPackages = i;
}

/**
 */
static PyObject * findUpgradeSet(PyObject * self, PyObject * args) {
    PyObject * hdrList, * result;
    char * root = "/";
    int i;
    struct pkgSet list;
    hdrObject * hdr;

    if (!PyArg_ParseTuple(args, "O|s", &hdrList, &root)) return NULL;

    if (!PyList_Check(hdrList)) {
	PyErr_SetString(PyExc_TypeError, "list of headers expected");
	return NULL;
    }

    list.numPackages = PyList_Size(hdrList);
    list.packages = alloca(sizeof(list.packages) * list.numPackages);
    for (i = 0; i < list.numPackages; i++) {
	hdr = (hdrObject *) PyList_GetItem(hdrList, i);
	if (((PyObject *) hdr)->ob_type != &hdrType) {
	    PyErr_SetString(PyExc_TypeError, "list of headers expected");
	    return NULL;
	}
	list.packages[i] = alloca(sizeof(struct packageInfo));
	list.packages[i]->h = hdrGetHeader(hdr);
	list.packages[i]->selected = 0;
	list.packages[i]->data = hdr;

	headerGetEntry(list.packages[i]->h, RPMTAG_NAME, NULL,
		      (void **) &list.packages[i]->name, NULL);
    }

    pkgSort (&list);

    if (ugFindUpgradePackages(&list, root)) {
	PyErr_SetString(pyrpmError, "error during upgrade check");
	return NULL;
    }

    result = PyList_New(0);
    for (i = 0; i < list.numPackages; i++) {
	if (list.packages[i]->selected) {
	    PyList_Append(result, list.packages[i]->data);
/*  	    Py_DECREF(list.packages[i]->data); */
	}
    }

    return result;
}

/**
 */
static PyObject * rpmInitDB(PyObject * self, PyObject * args) {
    char *root;
    int forWrite = 0;

    if (!PyArg_ParseTuple(args, "i|s", &forWrite, &root)) return NULL;

    if (rpmdbInit(root, forWrite ? O_RDWR | O_CREAT: O_RDONLY)) {
	char * errmsg = "cannot initialize database in %s";
	char * errstr = NULL;
	int errsize;

	errsize = strlen(errmsg) + strlen(root);
	errstr = alloca(errsize);
	snprintf(errstr, errsize, errmsg, root);
	PyErr_SetString(pyrpmError, errstr);
	return NULL;
    }

    Py_INCREF(Py_None);
    return(Py_None);
}

/**
 */
static PyObject * errorCB = NULL, * errorData = NULL;

/**
 */
static void errorcb (void)
{
    PyObject * result, * args = NULL;

    if (errorData)
	args = Py_BuildValue("(O)", errorData);

    result = PyEval_CallObject(errorCB, args);
    Py_XDECREF(args);

    if (result == NULL) {
	PyErr_Print();
	PyErr_Clear();
    }
    Py_DECREF (result);
}

/**
 */
static PyObject * errorSetCallback (PyObject * self, PyObject * args) {
    PyObject *newCB = NULL, *newData = NULL;

    if (!PyArg_ParseTuple(args, "O|O", &newCB, &newData)) return NULL;

    /* if we're getting a void*, set the error callback to this. */
    /* also, we can possibly decref any python callbacks we had  */
    /* and set them to NULL.                                     */
    if (PyCObject_Check (newCB)) {
	rpmErrorSetCallback (PyCObject_AsVoidPtr(newCB));

	Py_XDECREF (errorCB);
	Py_XDECREF (errorData);

	errorCB   = NULL;
	errorData = NULL;
	
	Py_INCREF(Py_None);
	return Py_None;
    }
    
    if (!PyCallable_Check (newCB)) {
	PyErr_SetString(PyExc_TypeError, "parameter must be callable");
	return NULL;
    }

    Py_XDECREF(errorCB);
    Py_XDECREF(errorData);

    errorCB = newCB;
    errorData = newData;
    
    Py_INCREF (errorCB);
    Py_XINCREF (errorData);

    return PyCObject_FromVoidPtr(rpmErrorSetCallback (errorcb), NULL);
}

/**
 */
static PyObject * errorString (PyObject * self, PyObject * args) {
    return PyString_FromString(rpmErrorString ());
}

/**
 */
static PyObject * checkSig (PyObject * self, PyObject * args) {
    char * filename;
    int flags;
    int rc = 255;

    if (PyArg_ParseTuple(args, "si", &filename, &flags)) {
	rpmTransactionSet ts;
	const char * av[2];
	QVA_t ka = memset(alloca(sizeof(*ka)), 0, sizeof(*ka));

	av[0] = filename;
	av[1] = NULL;
	ka->qva_mode = 'K';
	ka->qva_flags = (VERIFY_DIGEST|VERIFY_SIGNATURE);
	ka->sign = 0;
	ka->passPhrase = NULL;
	ts = rpmtransCreateSet(NULL, NULL);
	rc = rpmcliSign(ts, ka, av);
	rpmtransFree(ts);
    }
    return Py_BuildValue("i", rc);
}

/* hack to get the current header that's in the transaction set */
/**
 */
static PyObject * getTsHeader (PyObject * self, PyObject * args) {

    if (!PyArg_ParseTuple(args, ""))
	return NULL;
    
    if (transactionSetHeader) {
	return (PyObject *) createHeaderObject(transactionSetHeader);
    }
    Py_INCREF(Py_None);
    return (PyObject *) Py_None;
}

static PyObject * setVerbosity (PyObject * self, PyObject * args) {
    int level;

    if (!PyArg_ParseTuple(args, "i", &level))
	return NULL;

    rpmSetVerbosity(level);

    Py_INCREF(Py_None);
    return (PyObject *) Py_None;
}

/**
 */
typedef struct FDlist_t FDlist;

/**
 */
struct FDlist_t {
    FILE *f;
    FD_t fd;
    char *note;
    FDlist *next;
} ;

/**
 */
static FDlist *fdhead = NULL;

/**
 */
static FDlist *fdtail = NULL;

/**
 */
static int closeCallback(FILE * f) {
    FDlist *node, *last;

    printf ("close callback on %p\n", f);
    
    node = fdhead;
    last = NULL;
    while (node) {
        if (node->f == f)
            break;
        last = node;
        node = node->next;
    }
    if (node) {
        if (last)
            last->next = node->next;
        else
            fdhead = node->next;
        printf ("closing %s %p\n", node->note, node->fd);
	free (node->note);
        node->fd = fdLink(node->fd, "closeCallback");
        Fclose (node->fd);
        while (node->fd)
            node->fd = fdFree(node->fd, "closeCallback");
        free (node);
    }
    return 0; 
}

/**
 */
static PyObject * doFopen(PyObject * self, PyObject * args) {
    char * path, * mode;
    FDlist *node;
    
    if (!PyArg_ParseTuple(args, "ss", &path, &mode))
	return NULL;
    
    node = malloc (sizeof(FDlist));
    
    node->fd = Fopen(path, mode);
    node->fd = fdLink(node->fd, "doFopen");
    node->note = strdup (path);

    if (!node->fd) {
	PyErr_SetFromErrno(pyrpmError);
        free (node);
	return NULL;
    }
    
    if (Ferror(node->fd)) {
	const char *err = Fstrerror(node->fd);
        free(node);
	if (err) {
	    PyErr_SetString(pyrpmError, err);
	    return NULL;
	}
    }
    node->f = fdGetFp(node->fd);
    printf ("opening %s fd = %p f = %p\n", node->note, node->fd, node->f);
    if (!node->f) {
	PyErr_SetString(pyrpmError, "FD_t has no FILE*");
        free(node);
	return NULL;
    }

    node->next = NULL;
    if (!fdhead) {
	fdhead = fdtail = node;
    } else if (fdtail) {
        fdtail->next = node;
    } else {
        fdhead = node;
    }
    fdtail = node;
    
    return PyFile_FromFile (node->f, path, mode, closeCallback);
}

/**
 */
static PyMethodDef rpmModuleMethods[] = {
    { "TransactionSet", (PyCFunction) rpmtransCreate, METH_VARARGS, NULL },
    { "addMacro", (PyCFunction) doAddMacro, METH_VARARGS, NULL },
    { "delMacro", (PyCFunction) doDelMacro, METH_VARARGS, NULL },
    { "archscore", (PyCFunction) archScore, METH_VARARGS, NULL },
    { "findUpgradeSet", (PyCFunction) findUpgradeSet, METH_VARARGS, NULL },
    { "headerFromPackage", (PyCFunction) rpmHeaderFromPackage, METH_VARARGS, NULL },
    { "headerLoad", (PyCFunction) hdrLoad, METH_VARARGS, NULL },
    { "rhnLoad", (PyCFunction) rhnLoad, METH_VARARGS, NULL },
    { "initdb", (PyCFunction) rpmInitDB, METH_VARARGS, NULL },
    { "opendb", (PyCFunction) rpmOpenDB, METH_VARARGS, NULL },
    { "rebuilddb", (PyCFunction) rebuildDB, METH_VARARGS, NULL },
    { "mergeHeaderListFromFD", (PyCFunction) rpmMergeHeadersFromFD, METH_VARARGS, NULL },
    { "readHeaderListFromFD", (PyCFunction) rpmHeaderFromFD, METH_VARARGS, NULL },
    { "readHeaderListFromFile", (PyCFunction) rpmHeaderFromFile, METH_VARARGS, NULL },
    { "errorSetCallback", (PyCFunction) errorSetCallback, METH_VARARGS, NULL },
    { "errorString", (PyCFunction) errorString, METH_VARARGS, NULL },
    { "versionCompare", (PyCFunction) versionCompare, METH_VARARGS, NULL },
    { "labelCompare", (PyCFunction) labelCompare, METH_VARARGS, NULL },
    { "checksig", (PyCFunction) checkSig, METH_VARARGS, NULL },
    { "getTransactionCallbackHeader", (PyCFunction) getTsHeader, METH_VARARGS, NULL },
    { "Fopen", (PyCFunction) doFopen, METH_VARARGS, NULL },
    { "setVerbosity", (PyCFunction) setVerbosity, METH_VARARGS, NULL },
    { NULL }
} ;

/**
 */
void initrpm(void) {
    PyObject * m, * d, *o, * tag = NULL, * dict;
    int i;
    const struct headerSprintfExtension_s * extensions = rpmHeaderFormats;
    struct headerSprintfExtension_s * ext;
    m = Py_InitModule("rpm", rpmModuleMethods);

    hdrType.ob_type = &PyType_Type;
    rpmdbMIType.ob_type = &PyType_Type;
    rpmdbType.ob_type = &PyType_Type;
    rpmtransType.ob_type = &PyType_Type;

    if(!m)
	return;

/*      _rpmio_debug = -1; */
    rpmReadConfigFiles(NULL, NULL);

    d = PyModule_GetDict(m);

    pyrpmError = PyString_FromString("rpm.error");
    PyDict_SetItemString(d, "error", pyrpmError);
    Py_DECREF(pyrpmError);

    dict = PyDict_New();

    for (i = 0; i < rpmTagTableSize; i++) {
	tag = PyInt_FromLong(rpmTagTable[i].val);
	PyDict_SetItemString(d, (char *) rpmTagTable[i].name, tag);
	Py_DECREF(tag);
        PyDict_SetItem(dict, tag, o=PyString_FromString(rpmTagTable[i].name + 7));
	Py_DECREF(o);
    }

    while (extensions->name) {
	if (extensions->type == HEADER_EXT_TAG) {
            (const struct headerSprintfExtension *) ext = extensions;
            PyDict_SetItemString(d, (char *) extensions->name, o=PyCObject_FromVoidPtr(ext, NULL));
	    Py_DECREF(o);
            PyDict_SetItem(dict, tag, o=PyString_FromString(ext->name + 7));
	    Py_DECREF(o);    
        }
        extensions++;
    }

    PyDict_SetItemString(d, "tagnames", dict);
    Py_DECREF(dict);


#define REGISTER_ENUM(val) \
    PyDict_SetItemString(d, #val, o=PyInt_FromLong( val )); \
    Py_DECREF(o);
    
    REGISTER_ENUM(RPMFILE_STATE_NORMAL);
    REGISTER_ENUM(RPMFILE_STATE_REPLACED);
    REGISTER_ENUM(RPMFILE_STATE_NOTINSTALLED);
    REGISTER_ENUM(RPMFILE_STATE_NETSHARED);

    REGISTER_ENUM(RPMFILE_CONFIG);
    REGISTER_ENUM(RPMFILE_DOC);
    REGISTER_ENUM(RPMFILE_MISSINGOK);
    REGISTER_ENUM(RPMFILE_NOREPLACE);
    REGISTER_ENUM(RPMFILE_GHOST);
    REGISTER_ENUM(RPMFILE_LICENSE);
    REGISTER_ENUM(RPMFILE_README);

    REGISTER_ENUM(RPMDEP_SENSE_REQUIRES);
    REGISTER_ENUM(RPMDEP_SENSE_CONFLICTS);

    REGISTER_ENUM(RPMSENSE_SERIAL);
    REGISTER_ENUM(RPMSENSE_LESS);
    REGISTER_ENUM(RPMSENSE_GREATER);
    REGISTER_ENUM(RPMSENSE_EQUAL);
    REGISTER_ENUM(RPMSENSE_PREREQ);
    REGISTER_ENUM(RPMSENSE_INTERP);
    REGISTER_ENUM(RPMSENSE_SCRIPT_PRE);
    REGISTER_ENUM(RPMSENSE_SCRIPT_POST);
    REGISTER_ENUM(RPMSENSE_SCRIPT_PREUN);
    REGISTER_ENUM(RPMSENSE_SCRIPT_POSTUN);
    REGISTER_ENUM(RPMSENSE_SCRIPT_VERIFY);
    REGISTER_ENUM(RPMSENSE_FIND_REQUIRES);
    REGISTER_ENUM(RPMSENSE_FIND_PROVIDES);
    REGISTER_ENUM(RPMSENSE_TRIGGERIN);
    REGISTER_ENUM(RPMSENSE_TRIGGERUN);
    REGISTER_ENUM(RPMSENSE_TRIGGERPOSTUN);
    REGISTER_ENUM(RPMSENSE_MULTILIB);
    REGISTER_ENUM(RPMSENSE_SCRIPT_PREP);
    REGISTER_ENUM(RPMSENSE_SCRIPT_BUILD);
    REGISTER_ENUM(RPMSENSE_SCRIPT_INSTALL);
    REGISTER_ENUM(RPMSENSE_SCRIPT_CLEAN);
    REGISTER_ENUM(RPMSENSE_RPMLIB);
    REGISTER_ENUM(RPMSENSE_TRIGGERPREIN);

    REGISTER_ENUM(RPMTRANS_FLAG_TEST);
    REGISTER_ENUM(RPMTRANS_FLAG_BUILD_PROBS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSCRIPTS);
    REGISTER_ENUM(RPMTRANS_FLAG_JUSTDB);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERS);
    REGISTER_ENUM(RPMTRANS_FLAG_NODOCS);
    REGISTER_ENUM(RPMTRANS_FLAG_ALLFILES);
    REGISTER_ENUM(RPMTRANS_FLAG_KEEPOBSOLETE);
    REGISTER_ENUM(RPMTRANS_FLAG_MULTILIB);

    REGISTER_ENUM(RPMPROB_FILTER_IGNOREOS);
    REGISTER_ENUM(RPMPROB_FILTER_IGNOREARCH);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEPKG);
    REGISTER_ENUM(RPMPROB_FILTER_FORCERELOCATE);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACENEWFILES);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEOLDFILES);
    REGISTER_ENUM(RPMPROB_FILTER_OLDPACKAGE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKSPACE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKNODES);

    REGISTER_ENUM(RPMCALLBACK_INST_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_INST_START);
    REGISTER_ENUM(RPMCALLBACK_INST_OPEN_FILE);
    REGISTER_ENUM(RPMCALLBACK_INST_CLOSE_FILE);
    REGISTER_ENUM(RPMCALLBACK_TRANS_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_TRANS_START);
    REGISTER_ENUM(RPMCALLBACK_TRANS_STOP);
    REGISTER_ENUM(RPMCALLBACK_UNINST_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_UNINST_START);
    REGISTER_ENUM(RPMCALLBACK_UNINST_STOP);
    REGISTER_ENUM(RPMCALLBACK_UNPACK_ERROR);
    REGISTER_ENUM(RPMCALLBACK_CPIO_ERROR);

    REGISTER_ENUM(RPMPROB_BADARCH);
    REGISTER_ENUM(RPMPROB_BADOS);
    REGISTER_ENUM(RPMPROB_PKG_INSTALLED);
    REGISTER_ENUM(RPMPROB_BADRELOCATE);
    REGISTER_ENUM(RPMPROB_REQUIRES);
    REGISTER_ENUM(RPMPROB_CONFLICT);
    REGISTER_ENUM(RPMPROB_NEW_FILE_CONFLICT);
    REGISTER_ENUM(RPMPROB_FILE_CONFLICT);
    REGISTER_ENUM(RPMPROB_OLDPACKAGE);
    REGISTER_ENUM(RPMPROB_DISKSPACE);
    REGISTER_ENUM(RPMPROB_DISKNODES);
    REGISTER_ENUM(RPMPROB_BADPRETRANS);

#ifdef	DEAD
    REGISTER_ENUM(CHECKSIG_PGP);	/* XXX use VERIFY_SIGNATURE */
    REGISTER_ENUM(CHECKSIG_GPG);	/* XXX use VERIFY_SIGNATURE */
    REGISTER_ENUM(CHECKSIG_MD5);	/* XXX use VERIFY_DIGEST */
#else
    REGISTER_ENUM(VERIFY_DIGEST);
    REGISTER_ENUM(VERIFY_SIGNATURE);
#endif

    REGISTER_ENUM(RPMLOG_EMERG);
    REGISTER_ENUM(RPMLOG_ALERT);
    REGISTER_ENUM(RPMLOG_CRIT);
    REGISTER_ENUM(RPMLOG_ERR);
    REGISTER_ENUM(RPMLOG_WARNING);
    REGISTER_ENUM(RPMLOG_NOTICE);
    REGISTER_ENUM(RPMLOG_INFO);
    REGISTER_ENUM(RPMLOG_DEBUG);

    REGISTER_ENUM(RPMMIRE_DEFAULT);
    REGISTER_ENUM(RPMMIRE_STRCMP);
    REGISTER_ENUM(RPMMIRE_REGEX);
    REGISTER_ENUM(RPMMIRE_GLOB);

}

/*@}*/
