/** \ingroup python
 * \file python/rpmts-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include <rpmcli.h>
#include <rpmpgp.h>
#include <rpmdb.h>

#include "header-py.h"
#include "rpmds-py.h"	/* XXX for rpmdsNew */
#include "rpmfi-py.h"	/* XXX for rpmfiNew */
#include "rpmmi-py.h"
#include "rpmte-py.h"

#define	_RPMTS_INTERNAL	/* XXX for ts->rdb, ts->availablePackage */
#include "rpmts-py.h"

#include "debug.h"

static int _rpmts_debug = 0;

/*@access alKey @*/

/** \ingroup python
 * \name Class: Rpmts
 * \class Rpmts
 * \brief A python rpm.ts object represents an RPM transaction set.
 * 
 * The transaction set is the workhorse of RPM.  It performs the
 * installation and upgrade of packages.  The rpm.ts object is
 * instantiated by the TransactionSet function in the rpm module.
 *
 * The TransactionSet function takes two optional arguments. The first
 * argument is the root path. The second is the verify signature disable flags,
 * a set of the following bits:
 *
 * -    rpm.RPMVSF_NOHDRCHK	if set, don't check rpmdb headers
 * -    rpm.RPMVSF_NEEDPAYLOAD	if not set, check header+payload (if possible)
 * -	rpm.RPMVSF_NOSHA1HEADER	if set, don't check header SHA1 digest
 * -	rpm.RPMVSF_NODSAHEADER	if set, don't check header DSA signature
 * -	rpm.RPMVSF_NOMD5	if set, don't check header+payload MD5 digest
 * -	rpm.RPMVSF_NODSA	if set, don't check header+payload DSA signature
 * -	rpm.RPMVSF_NORSA	if set, don't check header+payload RSA signature
 *
 * For convenience, there are the following masks:
 * -    rpm._RPMVSF_NODIGESTS		if set, don't check digest(s).
 * -    rpm._RPMVSF_NOSIGNATURES	if set, don't check signature(s).
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
 * - ts.order()	Do a topological sort of added element relations.
 * @return	None
 *
 * - ts.setFlags(transFlags) Set transaction set flags.
 * @param transFlags - bit(s) to controll transaction operations. The
 *		following values can be logically OR'ed together:
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
 * @return	previous transFlags
 *
 * - run(problemSetFilter,callback,data) Attempt to execute a
 *	transaction set. After the transaction set has been populated
 *	with install and upgrade actions, it can be executed by invoking
 *	the run() method.
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
 */
struct rpmtsCallbackType_s {
    PyObject * cb;
    PyObject * data;
    rpmtsObject * tso;
    int pythonError;
    PyThreadState *_save;
};

/** \ingroup python
 */
static PyObject *
rpmts_Debug(/*@unused@*/ rpmtsObject * s, PyObject * args)
        /*@globals _Py_NoneStruct @*/
        /*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i:Debug", &_rpmts_debug)) return NULL;

if (_rpmts_debug < 0)
fprintf(stderr, "*** rpmts_Debug(%p) ts %p\n", s, s->ts);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
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
    rpmfi fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);

    /* XXX FIXME: return code RPMAL_NOMATCH is error */
    (void) rpmalAdd(&ts->availablePackages, RPMAL_NOMATCH, key,
		provides, fi);
    fi = rpmfiFree(fi);
    provides = rpmdsFree(provides);

if (_rpmts_debug < 0)
fprintf(stderr, "\tAddAvailable(%p) list %p\n", ts, ts->availablePackages);

}

/** \ingroup python
 */
static PyObject *
rpmts_AddInstall(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    hdrObject * h;
    PyObject * key;
    char * how = NULL;
    int isUpgrade = 0;

    if (!PyArg_ParseTuple(args, "O!O|s:AddInstall", &hdr_Type, &h, &key, &how))
	return NULL;

    {	PyObject * hObj = (PyObject *) h;
	if (hObj->ob_type != &hdr_Type) {
	    PyErr_SetString(PyExc_TypeError, "bad type for header argument");
	    return NULL;
	}
    }

if (_rpmts_debug < 0 || (_rpmts_debug > 0 && *how != 'a'))
fprintf(stderr, "*** rpmts_AddInstall(%p) ts %p\n", s, s->ts);

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
 * @todo Permit finer control (i.e. not just --allmatches) of deleted elments.
 */
static PyObject *
rpmts_AddErase(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * o;
    int count;
    rpmdbMatchIterator mi;
    
if (_rpmts_debug)
fprintf(stderr, "*** rpmts_AddErase(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, "O:AddErase", &o))
        return NULL;

    if (PyString_Check(o)) {
	char * name = PyString_AsString(o);

	mi = rpmtsInitIterator(s->ts, RPMDBI_LABEL, name, 0);
	count = rpmdbGetIteratorCount(mi);
	if (count <= 0) {
	    mi = rpmdbFreeIterator(mi);
	    PyErr_SetString(pyrpmError, "package not installed");
	    return NULL;
	} else { /* XXX: Note that we automatically choose to remove all matches */
	    Header h;
	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		unsigned int recOffset = rpmdbGetIteratorOffset(mi);
		if (recOffset)
		    rpmtsAddEraseElement(s->ts, h, recOffset);
	    }
	}
	mi = rpmdbFreeIterator(mi);
    } else
    if (PyInt_Check(o)) {
	uint_32 instance = PyInt_AsLong(o);

	mi = rpmtsInitIterator(s->ts, RPMDBI_PACKAGES, &instance, sizeof(instance));
	if (instance <= 0 || mi == NULL) {
	    mi = rpmdbFreeIterator(mi);
	    PyErr_SetString(pyrpmError, "package not installed");
	    return NULL;
	} else {
	    Header h;
	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		uint_32 recOffset = rpmdbGetIteratorOffset(mi);
		if (recOffset)
		    rpmtsAddEraseElement(s->ts, h, recOffset);
		break;
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static int
rpmts_SolveCallback(rpmts ts, rpmds ds, void * data)
	/*@*/
{
    struct rpmtsCallbackType_s * cbInfo = data;
    PyObject * args, * result;
    int res = 1;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_SolveCallback(%p,%p,%p) \"%s\"\n", ts, ds, data, rpmdsDNEVR(ds));

    if (cbInfo->tso == NULL) return res;
    if (cbInfo->pythonError) return res;
    if (cbInfo->cb == Py_None) return res;

    PyEval_RestoreThread(cbInfo->_save);

    args = Py_BuildValue("(Oissi)", cbInfo->tso,
		rpmdsTagN(ds), rpmdsN(ds), rpmdsEVR(ds), rpmdsFlags(ds));
    result = PyEval_CallObject(cbInfo->cb, args);
    Py_DECREF(args);

    if (!result) {
	cbInfo->pythonError = 1;
    } else {
	if (PyInt_Check(result))
	    res = PyInt_AsLong(result);
	Py_DECREF(result);
    }

    cbInfo->_save = PyEval_SaveThread();

    return res;
}

/** \ingroup python
 */
static PyObject *
rpmts_Check(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    rpmps ps;
    rpmProblem p;
    PyObject * list, * cf;
    struct rpmtsCallbackType_s cbInfo;
    int i;
    int xx;

    memset(&cbInfo, 0, sizeof(cbInfo));
    if (!PyArg_ParseTuple(args, "|O:Check", &cbInfo.cb))
	return NULL;

    if (cbInfo.cb != NULL) {
	if (!PyCallable_Check(cbInfo.cb)) {
	    PyErr_SetString(PyExc_TypeError, "expected a callable");
	    return NULL;
	}
	xx = rpmtsSetSolveCallback(s->ts, rpmts_SolveCallback, (void *)&cbInfo);
    }

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_Check(%p) ts %p cb %p\n", s, s->ts, cbInfo.cb);

    cbInfo.tso = s;
    cbInfo.pythonError = 0;
    cbInfo._save = PyEval_SaveThread();

    xx = rpmtsCheck(s->ts);
    ps = rpmtsProblems(s->ts);

    if (cbInfo.cb) {
	xx = rpmtsSetSolveCallback(s->ts, rpmtsSolve, NULL);
    }

    PyEval_RestoreThread(cbInfo._save);

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
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    int rc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_Order(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":Order")) return NULL;

    Py_BEGIN_ALLOW_THREADS
    rc = rpmtsOrder(s->ts);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
static PyObject *
rpmts_Clean(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
if (_rpmts_debug)
fprintf(stderr, "*** rpmts_Clean(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":Clean")) return NULL;

    rpmtsClean(s->ts);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject *
rpmts_IDTXload(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result = NULL;
    rpmTag tag = RPMTAG_INSTALLTID;
    IDTX idtx;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_IDTXload(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":IDTXload")) return NULL;

    Py_BEGIN_ALLOW_THREADS
    idtx = IDTXload(s->ts, tag);
    Py_END_ALLOW_THREADS

    if (idtx == NULL || idtx->nidt <= 0) {
	Py_INCREF(Py_None);
	result = Py_None;
    } else {
	PyObject * tuple;
	IDT idt;
	int i;

	result = PyTuple_New(idtx->nidt);
	for (i = 0; i < idtx->nidt; i++) {
	    idt = idtx->idt + i;
	    tuple = Py_BuildValue("(iOi)", idt->val.u32, hdr_Wrap(idt->h), idt->instance);
	    PyTuple_SET_ITEM(result,  i, tuple);
	}
    }

    idtx = IDTXfree(idtx);

    return result;
}

/** \ingroup python
 */
static PyObject *
rpmts_IDTXglob(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result = NULL;
    rpmTag tag = RPMTAG_REMOVETID;
    const char * globstr;
    IDTX idtx;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_IDTXglob(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":IDTXglob")) return NULL;

    Py_BEGIN_ALLOW_THREADS
    globstr = rpmExpand("%{_repackage_dir}/*.rpm", NULL);
    idtx = IDTXglob(s->ts, globstr, tag);
    globstr = _free(globstr);
    Py_END_ALLOW_THREADS

    if (idtx->nidt <= 0) {
	Py_INCREF(Py_None);
	result = Py_None;
    } else {
	PyObject * tuple;
	IDT idt;
	int i;

	result = PyTuple_New(idtx->nidt);
	for (i = 0; i < idtx->nidt; i++) {
	    idt = idtx->idt + i;
	    tuple = Py_BuildValue("(iOs)", idt->val.u32, hdr_Wrap(idt->h), idt->key);
	    PyTuple_SET_ITEM(result,  i, tuple);
	}
    }

    idtx = IDTXfree(idtx);

    return result;
}

/** \ingroup python
 */
static PyObject *
rpmts_Rollback(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    struct rpmInstallArguments_s * ia = alloca(sizeof(*ia));
    rpmtransFlags transFlags;
    const char ** av = NULL;
    uint_32 rbtid;
    int rc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_Rollback(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, "i:Rollback", &rbtid)) return NULL;

    Py_BEGIN_ALLOW_THREADS
    memset(ia, 0, sizeof(*ia));
    ia->qva_flags = (VERIFY_DIGEST|VERIFY_SIGNATURE|VERIFY_HDRCHK);
    ia->transFlags |= (INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL);
    ia->transFlags |= RPMTRANS_FLAG_NOMD5;
    ia->installInterfaceFlags = (INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL);
    ia->rbtid = rbtid;
    ia->relocations = NULL;
    ia->probFilter |= RPMPROB_FILTER_OLDPACKAGE;

    transFlags = rpmtsSetFlags(s->ts, ia->transFlags);
    rc = rpmRollback(s->ts, ia, av);
    transFlags = rpmtsSetFlags(s->ts, transFlags);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
static PyObject *
rpmts_OpenDB(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_OpenDB(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":OpenDB")) return NULL;

    if (s->ts->dbmode == -1)
	s->ts->dbmode = O_RDONLY;

    return Py_BuildValue("i", rpmtsOpenDB(s->ts, s->ts->dbmode));
}

/** \ingroup python
 */
static PyObject *
rpmts_CloseDB(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    int rc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_CloseDB(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":CloseDB")) return NULL;

    rc = rpmtsCloseDB(s->ts);
    s->ts->dbmode = -1;		/* XXX disable lazy opens */

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
static PyObject *
rpmts_InitDB(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    int rc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_InitDB(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":InitDB")) return NULL;

    rc = rpmtsInitDB(s->ts, O_RDONLY);
    if (rc == 0)
	rc = rpmtsCloseDB(s->ts);

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
static PyObject *
rpmts_RebuildDB(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    int rc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_RebuildDB(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":RebuildDB")) return NULL;

    Py_BEGIN_ALLOW_THREADS
    rc = rpmtsRebuildDB(s->ts);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
static PyObject *
rpmts_VerifyDB(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    int rc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_VerifyDB(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":VerifyDB")) return NULL;

    Py_BEGIN_ALLOW_THREADS
    rc = rpmtsVerifyDB(s->ts);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
static PyObject *
rpmts_HdrFromFdno(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct, fileSystem @*/
	/*@modifies s, _Py_NoneStruct, fileSystem @*/
{
    hdrObject * hdr;
    Header h;
    FD_t fd;
    int fdno;
    rpmRC rpmrc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_HdrFromFdno(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, "i:HdrFromFdno", &fdno)) return NULL;

    fd = fdDup(fdno);
    rpmrc = rpmReadPackageFile(s->ts, fd, "rpmts_HdrFromFdno", &h);
    Fclose(fd);

    switch (rpmrc) {
    case RPMRC_BADSIZE:
    case RPMRC_OK:
	hdr = hdr_Wrap(h);
	h = headerFree(h);	/* XXX ref held by hdr */
	break;

    case RPMRC_NOTFOUND:
	Py_INCREF(Py_None);
	hdr = (hdrObject *) Py_None;
	break;

    case RPMRC_FAIL:
    case RPMRC_SHORTREAD:
    default:
	PyErr_SetString(pyrpmError, "error reading package header");
	return NULL;
    }

    return Py_BuildValue("N", hdr);
}

/** \ingroup python
 */
static PyObject *
rpmts_HdrCheck(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * blob;
    PyObject * result = NULL;
    const char * msg = NULL;
    const void * uh;
    int uc;
    rpmRC rpmrc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_HdrCheck(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, "O:HdrCheck", &blob)) return NULL;
    if (blob == Py_None) {
	Py_INCREF(Py_None);
	return Py_None;
    }
    if (!PyString_Check(blob)) {
	PyErr_SetString(pyrpmError, "hdrCheck takes a string of octets");
	return result;
    }
    uh = PyString_AsString(blob);
    uc = PyString_Size(blob);

    rpmrc = headerCheck(s->ts, uh, uc, &msg);

    switch (rpmrc) {
    case RPMRC_OK:
	Py_INCREF(Py_None);
	result = Py_None;
	break;
    default:
    case RPMRC_FAIL:
	PyErr_SetString(pyrpmError, msg);
	break;
    }
    msg = _free(msg);

    return result;
}

/** \ingroup python
 */
static PyObject *
rpmts_SetVSFlags(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    rpmVSFlags vsflags;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_SetVSFlags(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, "i:SetVSFlags", &vsflags)) return NULL;

    /* XXX FIXME: value check on vsflags. */

    return Py_BuildValue("i", rpmtsSetVSFlags(s->ts, vsflags));
}

/** \ingroup python
 */
static PyObject *
rpmts_PgpPrtPkts(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    PyObject * blob;
    unsigned char * pkt;
    unsigned int pktlen;
    int rc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_PgpPrtPkts(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, "O:PgpPrtPkts", &blob)) return NULL;
    if (blob == Py_None) {
	Py_INCREF(Py_None);
	return Py_None;
    }
    if (!PyString_Check(blob)) {
	PyErr_SetString(pyrpmError, "pgpPrtPkts takes a string of octets");
	return NULL;
    }
    pkt = PyString_AsString(blob);
    pktlen = PyString_Size(blob);

    rc = pgpPrtPkts(pkt, pktlen, NULL, 1);

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
static PyObject *
rpmts_PgpImportPubkey(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    PyObject * blob;
    unsigned char * pkt;
    unsigned int pktlen;
    int rc;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_PgpImportPubkey(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, "O:PgpImportPubkey", &blob)) return NULL;
    if (blob == Py_None) {
	Py_INCREF(Py_None);
	return Py_None;
    }
    if (!PyString_Check(blob)) {
	PyErr_SetString(pyrpmError, "PgpImportPubkey takes a string of octets");
	return NULL;
    }
    pkt = PyString_AsString(blob);
    pktlen = PyString_Size(blob);

    rc = rpmcliImportPubkey(s->ts, pkt, pktlen);

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
static PyObject *
rpmts_GetKeys(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    const void **data = NULL;
    int num, i;
    PyObject *tuple;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_GetKeys(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, ":GetKeys")) return NULL;

    rpmtsGetKeys(s->ts, &data, &num);
    if (data == NULL || num <= 0) {
	data = _free(data);
	Py_INCREF(Py_None);
	return Py_None;
    }

    tuple = PyTuple_New(num);

    for (i = 0; i < num; i++) {
	PyObject *obj;
	obj = (data[i] ? (PyObject *) data[i] : Py_None);
	Py_INCREF(obj);
	PyTuple_SetItem(tuple, i, obj);
    }

    data = _free(data);

    return tuple;
}

/** \ingroup python
 */
static void *
rpmtsCallback(/*@unused@*/ const void * hd, const rpmCallbackType what,
		         const unsigned long amount, const unsigned long total,
	                 const void * pkgKey, rpmCallbackData data)
	/*@*/
{
    Header h = (Header) hd;
    struct rpmtsCallbackType_s * cbInfo = data;
    PyObject * pkgObj = (PyObject *) pkgKey;
    PyObject * args, * result;
    static FD_t fd;

    if (cbInfo->pythonError) return NULL;
    if (cbInfo->cb == Py_None) return NULL;

    /* Synthesize a python object for callback (if necessary). */
    if (pkgObj == NULL) {
	if (h) {
	    const char * n = NULL;
	    (void) headerNVR(h, &n, NULL, NULL);
	    pkgObj = Py_BuildValue("s", n);
	} else {
	    pkgObj = Py_None;
	    Py_INCREF(pkgObj);
	}
    } else
	Py_INCREF(pkgObj);

    PyEval_RestoreThread(cbInfo->_save);

    args = Py_BuildValue("(illOO)", what, amount, total, pkgObj, cbInfo->data);
    result = PyEval_CallObject(cbInfo->cb, args);
    Py_DECREF(args);
    Py_DECREF(pkgObj);

    if (!result) {
	cbInfo->pythonError = 1;
	cbInfo->_save = PyEval_SaveThread();
	return NULL;
    }

    if (what == RPMCALLBACK_INST_OPEN_FILE) {
	int fdno;

        if (!PyArg_Parse(result, "i", &fdno)) {
	    cbInfo->pythonError = 1;
	    cbInfo->_save = PyEval_SaveThread();
	    return NULL;
	}
	Py_DECREF(result);
	cbInfo->_save = PyEval_SaveThread();

	fd = fdDup(fdno);
if (_rpmts_debug)
fprintf(stderr, "\t%p = fdDup(%d)\n", fd, fdno);
	
	return fd;
    } else
    if (what == RPMCALLBACK_INST_CLOSE_FILE) {
if (_rpmts_debug)
fprintf(stderr, "\tFclose(%p)\n", fd);
	Fclose (fd);
    } else {
if (_rpmts_debug)
fprintf(stderr, "\t%ld:%ld key %p\n", amount, total, pkgKey);
    }

    Py_DECREF(result);
    cbInfo->_save = PyEval_SaveThread();

    return NULL;
}

/** \ingroup python
 */
static PyObject * rpmts_SetFlags(rpmtsObject * s, PyObject * args)
	/*@globals rpmGlobalMacroContext, _Py_NoneStruct @*/
	/*@modifies s, rpmGlobalMacroContext, _Py_NoneStruct @*/
{
    rpmtransFlags transFlags = 0;

    if (!PyArg_ParseTuple(args, "i:SetFlags", &transFlags))
	return NULL;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_SetFlags(%p) ts %p transFlags %x\n", s, s->ts, transFlags);

    return Py_BuildValue("i", rpmtsSetFlags(s->ts, transFlags));
}

/** \ingroup python
 */
static PyObject * rpmts_Run(rpmtsObject * s, PyObject * args)
	/*@globals rpmGlobalMacroContext, _Py_NoneStruct @*/
	/*@modifies s, rpmGlobalMacroContext, _Py_NoneStruct @*/
{
    int ignoreSet;
    int rc, i;
    PyObject * list;
    rpmps ps;
    struct rpmtsCallbackType_s cbInfo;

    if (!PyArg_ParseTuple(args, "iOO:Run", &ignoreSet, &cbInfo.cb,
			  &cbInfo.data))
	return NULL;

    cbInfo.tso = s;
    cbInfo.pythonError = 0;
    cbInfo._save = PyEval_SaveThread();

    if (cbInfo.cb != NULL) {
	if (!PyCallable_Check(cbInfo.cb)) {
	    PyErr_SetString(PyExc_TypeError, "expected a callable");
	    return NULL;
	}
	(void) rpmtsSetNotifyCallback(s->ts, rpmtsCallback, (void *) &cbInfo);
    }


if (_rpmts_debug)
fprintf(stderr, "*** rpmts_Run(%p) ts %p ignore %x\n", s, s->ts, ignoreSet);

    rc = rpmtsRun(s->ts, NULL, ignoreSet);
    ps = rpmtsProblems(s->ts);

    if (cbInfo.cb) {
	(void) rpmtsSetNotifyCallback(s->ts, NULL, NULL);
    }

    PyEval_RestoreThread(cbInfo._save);

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
rpmts_iter(rpmtsObject * s)
	/*@modifies s @*/
{
if (_rpmts_debug)
fprintf(stderr, "*** rpmts_iter(%p) ts %p\n", s, s->ts);

    Py_INCREF(s);
    return (PyObject *)s;
}
#endif

/**
 * @todo Add TR_ADDED filter to iterator.
 */
static PyObject *
rpmts_iternext(rpmtsObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result = NULL;
    rpmte te;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_iternext(%p) ts %p tsi %p %d\n", s, s->ts, s->tsi, s->tsiFilter);

    /* Reset iterator on 1st entry. */
    if (s->tsi == NULL) {
	s->tsi = rpmtsiInit(s->ts);
	if (s->tsi == NULL)
	    return NULL;
	s->tsiFilter = 0;
    }

    te = rpmtsiNext(s->tsi, s->tsiFilter);
    if (te != NULL) {
	result = (PyObject *) rpmte_Wrap(te);
    } else {
	s->tsi = rpmtsiFree(s->tsi);
	s->tsiFilter = 0;
    }

    return result;
}

/**
 * @todo Add TR_ADDED filter to iterator.
 */
static PyObject *
rpmts_Next(rpmtsObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result;

if (_rpmts_debug)
fprintf(stderr, "*** rpmts_Next(%p) ts %p\n", s, s->ts);

    result = rpmts_iternext(s);

    if (result == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    return result;
}

/**
 */
static rpmmiObject *
rpmts_Match(rpmtsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject *TagN = NULL;
    char *key = NULL;
    int len = 0;
    int tag = RPMDBI_PACKAGES;
    
if (_rpmts_debug)
fprintf(stderr, "*** rpmts_Match(%p) ts %p\n", s, s->ts);

    if (!PyArg_ParseTuple(args, "|Ozi", &TagN, &key, &len))
	return NULL;

    if (TagN && (tag = tagNumFromPyObject (TagN)) == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    /* XXX If not already opened, open the database O_RDONLY now. */
    if (s->ts->rdb == NULL) {
	int rc = rpmtsOpenDB(s->ts, O_RDONLY);
	if (rc || s->ts->rdb == NULL) {
	    PyErr_SetString(PyExc_TypeError, "rpmdb open failed");
	    return NULL;
	}
    }

    return rpmmi_Wrap( rpmtsInitIterator(s->ts, tag, key, len) );
}

/** \ingroup python
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmts_methods[] = {
 {"Debug",	(PyCFunction)rpmts_Debug,	METH_VARARGS,
        NULL},

 {"addInstall",	(PyCFunction) rpmts_AddInstall,	METH_VARARGS,
	NULL },
 {"addErase",	(PyCFunction) rpmts_AddErase,	METH_VARARGS,
	NULL },
 {"check",	(PyCFunction) rpmts_Check,	METH_VARARGS,
	NULL },
 {"order",	(PyCFunction) rpmts_Order,	METH_VARARGS,
	NULL },
 {"setFlags",	(PyCFunction) rpmts_SetFlags,	METH_VARARGS,
"ts.setFlags(transFlags) -> previous transFlags\n\
- Set control bit(s) for processing a transaction set.\n\
  Note: This method sets bit(s) passed as the first argument to ts.run()\n" },
 {"run",	(PyCFunction) rpmts_Run,	METH_VARARGS,
	NULL },
 {"clean",	(PyCFunction) rpmts_Clean,	METH_VARARGS,
	NULL },
 {"IDTXload",	(PyCFunction) rpmts_IDTXload,	METH_VARARGS,
	NULL },
 {"IDTXglob",	(PyCFunction) rpmts_IDTXglob,	METH_VARARGS,
	NULL },
 {"rollback",	(PyCFunction) rpmts_Rollback,	METH_VARARGS,
	NULL },
 {"openDB",	(PyCFunction) rpmts_OpenDB,	METH_VARARGS,
"ts.openDB() -> None\n\
- Open the default transaction rpmdb.\n\
  Note: The transaction rpmdb is lazily opened, so ts.openDB() is seldom needed.\n" },
 {"closeDB",	(PyCFunction) rpmts_CloseDB,	METH_VARARGS,
"ts.closeDB() -> None\n\
- Close the default transaction rpmdb.\n\
  Note: ts.closeDB() disables lazy opens, and should hardly ever be used.\n" },
 {"initDB",	(PyCFunction) rpmts_InitDB,	METH_VARARGS,
"ts.initDB() -> None\n\
- Initialize the default transaction rpmdb.\n\
 Note: ts.initDB() is seldom needed anymore.\n" },
 {"rebuildDB",	(PyCFunction) rpmts_RebuildDB,	METH_VARARGS,
"ts.rebuildDB() -> None\n\
- Rebuild the default transaction rpmdb.\n" },
 {"verifyDB",	(PyCFunction) rpmts_VerifyDB,	METH_VARARGS,
"ts.verifyDB() -> None\n\
- Verify the default transaction rpmdb.\n" },
 {"hdrFromFdno",(PyCFunction) rpmts_HdrFromFdno,METH_VARARGS,
"ts.hdrFromFdno(fdno) -> hdr\n\
- Read a package header from a file descriptor.\n" },
 {"hdrCheck",	(PyCFunction) rpmts_HdrCheck,	METH_VARARGS,
	NULL },
 {"setVSFlags",(PyCFunction) rpmts_SetVSFlags,	METH_VARARGS,
"ts.setVSFlags(vsflags) -> ovsflags\n\
- Set signature verification flags. Values for vsflags are:\n\
    rpm.RPMVSF_NOHDRCHK      if set, don't check rpmdb headers\n\
    rpm.RPMVSF_NEEDPAYLOAD   if not set, check header+payload (if possible)\n\
    rpm.RPMVSF_NOSHA1HEADER  if set, don't check header SHA1 digest\n\
    rpm.RPMVSF_NODSAHEADER   if set, don't check header DSA signature\n\
    rpm.RPMVSF_NOMD5         if set, don't check header+payload MD5 digest\n\
    rpm.RPMVSF_NODSA         if set, don't check header+payload DSA signature\n\
    rpm.RPMVSF_NORSA         if set, don't check header+payload RSA signature\n\
    rpm._RPMVSF_NODIGESTS    if set, don't check digest(s)\n\
    rpm._RPMVSF_NOSIGNATURES if set, don't check signature(s)\n" },
 {"pgpPrtPkts",	(PyCFunction) rpmts_PgpPrtPkts,	METH_VARARGS,
	NULL },
 {"pgpImportPubkey",	(PyCFunction) rpmts_PgpImportPubkey,	METH_VARARGS,
	NULL },
 {"getKeys",	(PyCFunction) rpmts_GetKeys,	METH_VARARGS,
	NULL },
 {"dbMatch",	(PyCFunction) rpmts_Match,	METH_VARARGS,
"ts.dbMatch([TagN, [key, [len]]]) -> mi\n\
- Create a match iterator for the default transaction rpmdb.\n" },
 {"next",		(PyCFunction)rpmts_Next,	METH_VARARGS,
"ts.next() -> te\n\
- Retrieve next transaction set element.\n" },
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/** \ingroup python
 */
static void rpmts_dealloc(/*@only@*/ PyObject * o)
	/*@modifies o @*/
{
    rpmtsObject * trans = (void *) o;

if (_rpmts_debug)
fprintf(stderr, "%p -- ts %p db %p\n", trans, trans->ts, trans->ts->rdb);
    rpmtsFree(trans->ts);

    if (trans->scriptFd) Fclose(trans->scriptFd);
    /* this will free the keyList, and decrement the ref count of all
       the items on the list as well :-) */
    Py_DECREF(trans->keyList);
    PyMem_DEL(o);
}

/** \ingroup python
 */
static PyObject * rpmts_getattr(rpmtsObject * o, char * name)
	/*@*/
{
    return Py_FindMethod(rpmts_methods, (PyObject *) o, name);
}

/** \ingroup python
 */
static int rpmts_setattr(rpmtsObject * o, char * name, PyObject * val)
	/*@modifies o @*/
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
/*@unchecked@*/ /*@observer@*/
static char rpmts_doc[] =
"";

/** \ingroup python
 */
/*@-fullinitblock@*/
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
	(getiterfunc)rpmts_iter,	/* tp_iter */
	(iternextfunc)rpmts_iternext,	/* tp_iternext */
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
/*@=fullinitblock@*/

/**
 */
rpmtsObject *
rpmts_Create(/*@unused@*/ PyObject * self, PyObject * args)
{
    rpmtsObject * o;
    char * rootDir = "/";
    int vsflags = rpmExpandNumeric("%{?_vsflags_up2date}");

    if (!PyArg_ParseTuple(args, "|si:Create", &rootDir, &vsflags))
	return NULL;

    o = (void *) PyObject_NEW(rpmtsObject, &rpmts_Type);

    o->ts = rpmtsCreate();
    (void) rpmtsSetRootDir(o->ts, rootDir);
    (void) rpmtsSetVSFlags(o->ts, vsflags);

    o->keyList = PyList_New(0);
    o->scriptFd = NULL;
    o->tsi = NULL;
    o->tsiFilter = 0;

if (_rpmts_debug)
fprintf(stderr, "%p ++ ts %p db %p\n", o, o->ts, o->ts->rdb);
    return o;
}
