#include "rpmsystem-py.h"

#include <fcntl.h>

#include <rpm/rpmlib.h>	/* rpmReadPackageFile, headerCheck */
#include <rpm/rpmtag.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmbuild.h>

#include "header-py.h"
#include "rpmds-py.h"	/* XXX for rpmdsNew */
#include "rpmfd-py.h"
#include "rpmkeyring-py.h"
#include "rpmfi-py.h"	/* XXX for rpmfiNew */
#include "rpmmi-py.h"
#include "rpmii-py.h"
#include "rpmps-py.h"
#include "rpmte-py.h"
#include "rpmts-py.h"

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
 * -    rpm.RPMVSF_MASK_NODIGESTS		if set, don't check digest(s).
 * -    rpm.RPMVSF_MASK_NOSIGNATURES	if set, don't check signature(s).
 *
 * A rpm.ts object has the following methods:
 *
 * - addInstall(hdr,data,mode)  Add an install element to a transaction set.
 * @param hdr	the header to be added
 * @param data	user data that will be passed to the transaction callback
 *		during transaction execution
 * @param mode 	optional argument that specifies if this package should
 *		be installed ('i'), upgraded ('u').
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
 * @param transFlags - bit(s) to control transaction operations. The
 *		following values can be logically OR'ed together:
 *	- rpm.RPMTRANS_FLAG_TEST - test mode, do not modify the RPM
 *		database, change any files, or run any package scripts
 *	- rpm.RPMTRANS_FLAG_BUILD_PROBS - only build a list of
 *		problems encountered when attempting to run this transaction
 *		set
 *	- rpm.RPMTRANS_FLAG_JUSTDB - only make changes to the rpm
 *		database, do not modify files.
 *	- rpm.RPMTRANS_FLAG_NOSCRIPTS - do not execute package scripts
 *	- rpm.RPMTRANS_FLAG_NOTRIGGERS - do not run trigger scripts
 *	- rpm.RPMTRANS_FLAG_NO* - disable specific scripts and triggers
 *	- rpm.RPMTRANS_FLAG_NODOCS - do not install files marked as %doc
 *	- rpm.RPMTRANS_FLAG_NOPLUGINS - do not run plugins
 *	- rpm.RPMTRANS_FLAG_NOFILEDIGEST - disable checking checksums
 *	- rpm.RPMTRANS_FLAG_ALLFILES - create all files, even if a
 *		file is marked %config(missingok) and an upgrade is
 *		being performed.
 *	- rpm.RPMTRANS_FLAG_NOCONFIGS - skip config files
 *	- rpm.RPMTRANS_FLAG_DEPLOOPS - enable debugging for dependency loops
 * @return	previous transFlags
 *
 * - ts.setProbFilter(ignoreSet) Set transaction set problem filter.
 * @param problemSetFilter - control bit(s) to ignore classes of problems,
 *		a logical or of one or more of the following bit(s):
 *	- rpm.RPMPROB_FILTER_IGNOREOS -
 *	- rpm.RPMPROB_FILTER_IGNOREARCH -
 *	- rpm.RPMPROB_FILTER_REPLACEPKG -
 *	- rpm.RPMPROB_FILTER_FORCERELOCATE -
 *	- rpm.RPMPROB_FILTER_REPLACENEWFILES -
 *	- rpm.RPMPROB_FILTER_REPLACEOLDFILES -
 *	- rpm.RPMPROB_FILTER_OLDPACKAGE -
 *	- rpm.RPMPROB_FILTER_DISKSPACE -
 * @return	previous ignoreSet
 *
 * - ts.run(callback,data) Attempt to execute a transaction set.
 *	After the transaction set has been populated with install/upgrade or
 *	erase actions, the transaction set can be executed by invoking
 *	the ts.run() method.
 */

struct rpmtsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmfdObject *scriptFd;
    PyObject *keyList;
    rpmts	ts;
    rpmtsi tsi;
};

struct rpmtsCallbackType_s {
    PyObject * cb;
    PyObject * data;
    rpmtsObject * tso;
    int style;
    PyThreadState *_save;
};

RPM_GNUC_NORETURN
static void die(PyObject *cb)
{
    char *pyfn = NULL;
    PyObject *r;

    if (PyErr_Occurred()) {
	PyErr_Print();
    }
    if ((r = PyObject_Repr(cb)) != NULL) { 
	pyfn = PyBytes_AsString(r);
    }
    fprintf(stderr, "FATAL ERROR: python callback %s failed, aborting!\n", 
	    	      pyfn ? pyfn : "???");
    exit(EXIT_FAILURE);
}

int rpmtsFromPyObject(PyObject *item, rpmts *ts)
{
    if (rpmtsObject_Check(item)) {
	*ts = ((rpmtsObject *) item)->ts;
	return 1;
    }
    PyErr_SetString(PyExc_TypeError, "TransactionSet object expected");
    return 0;
}


static PyObject *
rpmts_AddInstall(rpmtsObject * s, PyObject * args)
{
    Header h = NULL;
    PyObject * key;
    int how = 0;
    int rc;

    if (!PyArg_ParseTuple(args, "O&Oi:AddInstall", 
			  hdrFromPyObject, &h, &key, &how))
	return NULL;

    rc = rpmtsAddInstallElement(s->ts, h, key, how, NULL);
    if (key && rc == 0) {
	PyList_Append(s->keyList, key);
    }
    return PyBool_FromLong((rc == 0));
}

static PyObject *
rpmts_AddReinstall(rpmtsObject * s, PyObject * args)
{
    Header h = NULL;
    PyObject * key;
    int rc;

    if (!PyArg_ParseTuple(args, "O&O:AddReinstall", 
			  hdrFromPyObject, &h, &key))
	return NULL;

    rc = rpmtsAddReinstallElement(s->ts, h, key);
    if (key && rc == 0) {
	PyList_Append(s->keyList, key);
    }
    return PyBool_FromLong((rc == 0));
}

static PyObject *
rpmts_AddErase(rpmtsObject * s, PyObject * args)
{
    Header h;

    if (!PyArg_ParseTuple(args, "O&:AddErase", hdrFromPyObject, &h))
        return NULL;

    return PyBool_FromLong(rpmtsAddEraseElement(s->ts, h, -1) == 0);
}

static int
rpmts_SolveCallback(rpmts ts, rpmds ds, const void * data)
{
    struct rpmtsCallbackType_s * cbInfo = (struct rpmtsCallbackType_s *) data;
    PyObject * args, * result;
    int res = 1;

    if (cbInfo->tso == NULL) return res;
    if (cbInfo->cb == Py_None) return res;

    PyEval_RestoreThread(cbInfo->_save);

    args = Py_BuildValue("(OiNNi)", cbInfo->tso,
		rpmdsTagN(ds), utf8FromString(rpmdsN(ds)),
		utf8FromString(rpmdsEVR(ds)), rpmdsFlags(ds));
    result = PyObject_Call(cbInfo->cb, args, NULL);
    Py_DECREF(args);

    if (!result) {
	die(cbInfo->cb);
    } else {
	if (PyLong_Check(result))
	    res = PyLong_AsLong(result);
	Py_DECREF(result);
    }

    cbInfo->_save = PyEval_SaveThread();

    return res;
}

static PyObject *
rpmts_Check(rpmtsObject * s, PyObject * args, PyObject * kwds)
{
    struct rpmtsCallbackType_s cbInfo;
    int rc;
    char * kwlist[] = {"callback", NULL};

    memset(&cbInfo, 0, sizeof(cbInfo));
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O:Check", kwlist,
	    &cbInfo.cb))
	return NULL;

    if (cbInfo.cb != NULL) {
	if (!PyCallable_Check(cbInfo.cb)) {
	    PyErr_SetString(PyExc_TypeError, "expected a callable");
	    return NULL;
	}
	rc = rpmtsSetSolveCallback(s->ts, rpmts_SolveCallback, (void *)&cbInfo);
    }

    cbInfo.tso = s;
    cbInfo._save = PyEval_SaveThread();

    rc = rpmtsCheck(s->ts);

    PyEval_RestoreThread(cbInfo._save);

    return PyBool_FromLong((rc == 0));
}

static PyObject *
rpmts_Order(rpmtsObject * s)
{
    int rc;

    Py_BEGIN_ALLOW_THREADS
    rc = rpmtsOrder(s->ts);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmts_Clean(rpmtsObject * s)
{
    rpmtsClean(s->ts);

    Py_RETURN_NONE;
}

static PyObject *
rpmts_Clear(rpmtsObject * s)
{
    rpmtsEmpty(s->ts);

    Py_RETURN_NONE;
}

static PyObject *
rpmts_OpenDB(rpmtsObject * s)
{
    int dbmode;

    dbmode = rpmtsGetDBMode(s->ts);
    if (dbmode == -1)
	dbmode = O_RDONLY;

    return Py_BuildValue("i", rpmtsOpenDB(s->ts, dbmode));
}

static PyObject *
rpmts_CloseDB(rpmtsObject * s)
{
    int rc;

    rc = rpmtsCloseDB(s->ts);
    rpmtsSetDBMode(s->ts, -1);	/* XXX disable lazy opens */

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmts_InitDB(rpmtsObject * s)
{
    int rc;

    rc = rpmtsInitDB(s->ts, 0644);
    if (rc == 0)
	rc = rpmtsCloseDB(s->ts);

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmts_RebuildDB(rpmtsObject * s)
{
    int rc;

    Py_BEGIN_ALLOW_THREADS
    rc = rpmtsRebuildDB(s->ts);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmts_VerifyDB(rpmtsObject * s)
{
    int rc;

    Py_BEGIN_ALLOW_THREADS
    rc = rpmtsVerifyDB(s->ts);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmts_dbCookie(rpmtsObject * s)
{
    PyObject *ret = NULL;
    char *cookie = NULL;

    Py_BEGIN_ALLOW_THREADS
    cookie = rpmdbCookie(rpmtsGetRdb(s->ts));
    Py_END_ALLOW_THREADS

    ret = utf8FromString(cookie);
    free(cookie);
    return ret;
}

static PyObject *
rpmts_HdrFromFdno(rpmtsObject * s, PyObject *arg)
{
    PyObject *ho = NULL;
    rpmfdObject *fdo = NULL;
    Header h;
    rpmRC rpmrc;

    if (!PyArg_Parse(arg, "O&:HdrFromFdno", rpmfdFromPyObject, &fdo))
    	return NULL;

    Py_BEGIN_ALLOW_THREADS;
    rpmrc = rpmReadPackageFile(s->ts, rpmfdGetFd(fdo), NULL, &h);
    Py_END_ALLOW_THREADS;
    Py_XDECREF(fdo);

    if (rpmrc == RPMRC_OK) {
	ho = hdr_Wrap(&hdr_Type, h);
    } else {
	Py_INCREF(Py_None);
	ho = Py_None;
    }
    return Py_BuildValue("(iN)", rpmrc, ho);
}

static PyObject *
rpmts_HdrCheck(rpmtsObject * s, PyObject *obj)
{
    PyObject * blob;
    char * msg = NULL;
    const void * uh;
    int uc;
    rpmRC rpmrc;

    if (!PyArg_Parse(obj, "S:HdrCheck", &blob))
    	return NULL;

    uh = PyBytes_AsString(blob);
    uc = PyBytes_Size(blob);

    Py_BEGIN_ALLOW_THREADS;
    rpmrc = headerCheck(s->ts, uh, uc, &msg);
    Py_END_ALLOW_THREADS;

    return Py_BuildValue("(iN)", rpmrc, utf8FromString(msg));
}

static PyObject *
rpmts_PgpPrtPkts(rpmtsObject * s, PyObject * args, PyObject * kwds)
{
    PyObject * blob;
    unsigned char * pkt;
    unsigned int pktlen;
    int rc;
    char * kwlist[] = {"octets", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "S:PgpPrtPkts", kwlist, &blob))
    	return NULL;

    pkt = (unsigned char *)PyBytes_AsString(blob);
    pktlen = PyBytes_Size(blob);

    rc = pgpPrtPkts(pkt, pktlen, NULL, 1);

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmts_PgpImportPubkey(rpmtsObject * s, PyObject * args, PyObject * kwds)
{
    PyObject * blob;
    unsigned char * pkt;
    unsigned int pktlen;
    int rc;
    char * kwlist[] = {"pubkey", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "S:PgpImportPubkey",
    	    kwlist, &blob))
	return NULL;

    pkt = (unsigned char *)PyBytes_AsString(blob);
    pktlen = PyBytes_Size(blob);

    rc = rpmtsImportPubkey(s->ts, pkt, pktlen);

    return Py_BuildValue("i", rc);
}

static PyObject *rpmts_setKeyring(rpmtsObject *s, PyObject *arg)
{
    rpmKeyring keyring = NULL;
    if (arg == Py_None || rpmKeyringFromPyObject(arg, &keyring)) {
	return PyBool_FromLong(rpmtsSetKeyring(s->ts, keyring) == 0);
    } else {
	PyErr_SetString(PyExc_TypeError, "rpm.keyring or None expected");
	return NULL;
    }
}

static PyObject *rpmts_getKeyring(rpmtsObject *s, PyObject *args, PyObject *kwds)
{
    rpmKeyring keyring = NULL;
    int autoload = 1;
    char * kwlist[] = { "autoload", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i:getKeyring",
				     kwlist, &autoload))
	return NULL;

    keyring = rpmtsGetKeyring(s->ts, autoload);
    if (keyring) {
	return rpmKeyring_Wrap(&rpmKeyring_Type, keyring);
    } else {
	Py_RETURN_NONE;
    }
}

static void *
rpmtsCallback(const void * arg, const rpmCallbackType what,
		         const rpm_loff_t amount, const rpm_loff_t total,
	                 const void * pkgKey, rpmCallbackData data)
{
    struct rpmtsCallbackType_s * cbInfo = data;
    PyObject * pkgObj = (PyObject *) pkgKey;
    PyObject * args, * result;
    static FD_t fd;

    if (cbInfo->cb == Py_None) return NULL;

    PyEval_RestoreThread(cbInfo->_save);


    if (cbInfo->style == 0) {
	/* Synthesize a python object for callback (if necessary). */
	if (pkgObj == NULL) {
	    if (arg) {
		Header h = (Header) arg;
		pkgObj = utf8FromString(headerGetString(h, RPMTAG_NAME));
	    } else {
		pkgObj = Py_None;
		Py_INCREF(pkgObj);
	    }
	} else
	    Py_INCREF(pkgObj);

	args = Py_BuildValue("(iLLOO)", what, amount, total,
				pkgObj, cbInfo->data);
	Py_DECREF(pkgObj);
    } else {
	PyObject *o;
	if (arg) {
	    o = rpmte_Wrap(&rpmte_Type, (rpmte) arg);
	} else {
	    o = Py_None;
	    Py_INCREF(o);
	}
	args = Py_BuildValue("(OiLLO)", o, what, amount, total, cbInfo->data);
	Py_DECREF(o);
    }

    result = PyObject_Call(cbInfo->cb, args, NULL);
    Py_DECREF(args);

    if (!result) {
	die(cbInfo->cb);
    }

    if (what == RPMCALLBACK_INST_OPEN_FILE) {
	int fdno;

        if (!PyArg_Parse(result, "i", &fdno)) {
	    die(cbInfo->cb);
	}
	Py_DECREF(result);
	cbInfo->_save = PyEval_SaveThread();

	fd = fdDup(fdno);
	fcntl(Fileno(fd), F_SETFD, FD_CLOEXEC);

	return fd;
    } else
    if (what == RPMCALLBACK_INST_CLOSE_FILE) {
	Fclose (fd);
    }

    Py_DECREF(result);
    cbInfo->_save = PyEval_SaveThread();

    return NULL;
}

static PyObject *
rpmts_Problems(rpmtsObject * s)
{
    rpmps ps = rpmtsProblems(s->ts);
    PyObject *problems = rpmps_AsList(ps);
    rpmpsFree(ps);
    return problems;
}

static PyObject *
rpmts_Run(rpmtsObject * s, PyObject * args, PyObject * kwds)
{
    int rc;
    struct rpmtsCallbackType_s cbInfo;
    rpmprobFilterFlags ignoreSet;
    char * kwlist[] = {"callback", "data", "ignoreSet", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OOi:Run", kwlist,
	    &cbInfo.cb, &cbInfo.data, &ignoreSet))
	return NULL;

    cbInfo.tso = s;
    cbInfo.style = rpmtsGetNotifyStyle(s->ts);
    cbInfo._save = PyEval_SaveThread();

    if (cbInfo.cb != NULL) {
	if (!PyCallable_Check(cbInfo.cb)) {
	    PyErr_SetString(PyExc_TypeError, "expected a callable");
	    return NULL;
	}
	(void) rpmtsSetNotifyCallback(s->ts, rpmtsCallback, (void *) &cbInfo);
    }

    rc = rpmtsRun(s->ts, NULL, ignoreSet);

    if (cbInfo.cb)
	(void) rpmtsSetNotifyCallback(s->ts, NULL, NULL);

    PyEval_RestoreThread(cbInfo._save);

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmts_iternext(rpmtsObject * s)
{
    PyObject * result = NULL;
    rpmte te;

    /* Reset iterator on 1st entry. */
    if (s->tsi == NULL) {
	s->tsi = rpmtsiInit(s->ts);
	if (s->tsi == NULL)
	    return NULL;
    }

    te = rpmtsiNext(s->tsi, 0);
    if (te != NULL) {
	result = rpmte_Wrap(&rpmte_Type, te);
    } else {
	s->tsi = rpmtsiFree(s->tsi);
    }

    return result;
}

static PyObject *
rpmts_Match(rpmtsObject * s, PyObject * args, PyObject * kwds)
{
    PyObject *Key = NULL;
    PyObject *str = NULL;
    PyObject *mio = NULL;
    char *key = NULL;
/* XXX lkey *must* be a 32 bit integer, int "works" on all known platforms. */
    int lkey = 0;
    int len = 0;
    rpmDbiTagVal tag = RPMDBI_PACKAGES;
    char * kwlist[] = {"tagNumber", "key", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O&O:Match", kwlist,
	    tagNumFromPyObject, &tag, &Key))
	return NULL;

    if (Key) {
	if (PyLong_Check(Key)) {
	    lkey = PyLong_AsLong(Key);
	    key = (char *)&lkey;
	    len = sizeof(lkey);
	} else if (utf8FromPyObject(Key, &str)) {
	    key = PyBytes_AsString(str);
	    len = PyBytes_Size(str);
	} else {
	    PyErr_SetString(PyExc_TypeError, "unknown key type");
	    return NULL;
	}
	/* One of the conversions above failed, exception is set already */
	if (PyErr_Occurred()) goto exit;
    }

    /* XXX If not already opened, open the database O_RDONLY now. */
    /* XXX FIXME: lazy default rdonly open also done by rpmtsInitIterator(). */
    if (rpmtsGetRdb(s->ts) == NULL) {
	int rc = rpmtsOpenDB(s->ts, O_RDONLY);
	if (rc || rpmtsGetRdb(s->ts) == NULL) {
	    PyErr_SetString(pyrpmError, "rpmdb open failed");
	    goto exit;
	}
    }

    mio = rpmmi_Wrap(&rpmmi_Type, rpmtsInitIterator(s->ts, tag, key, len), (PyObject*)s);

exit:
    Py_XDECREF(str);
    return mio;
}
static PyObject *
rpmts_index(rpmtsObject * s, PyObject * args, PyObject * kwds)
{
    rpmDbiTagVal tag;
    PyObject *mio = NULL;
    char * kwlist[] = {"tag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&:Keys", kwlist,
              tagNumFromPyObject, &tag))
	return NULL;

    /* XXX If not already opened, open the database O_RDONLY now. */
    if (rpmtsGetRdb(s->ts) == NULL) {
	int rc = rpmtsOpenDB(s->ts, O_RDONLY);
	if (rc || rpmtsGetRdb(s->ts) == NULL) {
	    PyErr_SetString(pyrpmError, "rpmdb open failed");
	    goto exit;
	}
    }

    rpmdbIndexIterator ii = rpmdbIndexIteratorInit(rpmtsGetRdb(s->ts), tag);
    if (ii == NULL) {
        PyErr_SetString(PyExc_KeyError, "No index for this tag");
        return NULL;
    }
    mio = rpmii_Wrap(&rpmii_Type, ii, (PyObject*)s);

exit:
    return mio;
}

static struct PyMethodDef rpmts_methods[] = {
 {"addInstall",	(PyCFunction) rpmts_AddInstall,	METH_VARARGS,
  "ts.addInstall(hdr, data, mode) --  Add transaction element(s)\n"
  "representing an installation or update of a package.\n\n"
  "Args:\n"
  "  hdr : the header to be added\n"
  "  data : user data that will be passed to the transaction callback\n\t\tduring transaction execution\n"
  "  mode : optional argument that specifies if this package should be\n\t\tinstalled ('i'), upgraded ('u')"},
 {"addReinstall",	(PyCFunction) rpmts_AddReinstall,	METH_VARARGS,
  "ts.addReinstall(hdr, data) -- Adds transaction elements\nrepresenting a reinstall of an already installed package.\n\nSee addInstall for details."},
 {"addErase",	(PyCFunction) rpmts_AddErase,	METH_VARARGS|METH_KEYWORDS,
  "addErase(name) -- Add a transaction element representing an erase\nof an installed package.\n\n"
  "  name: the package name to be erased"},
 {"check",	(PyCFunction) rpmts_Check,	METH_VARARGS|METH_KEYWORDS,
  "ts.check( )-- Perform a dependency check on the transaction set.\n"
  "		After headers have been added to a transaction set,\n"
  "		a dependencycheck can be performed to make sure that\n"
  "		all package dependencies are satisfied.\n"
  "Return	None If there are no unresolved dependencies\n"
  "		Otherwise a list of complex tuples is returned,\n"
  "		one tuple per unresolved dependency, with\n"
  "The format of the dependency tuple is:\n"
  "    ((packageName, packageVersion, packageRelease),\n"
  "     (reqName, reqVersion),\n"
  "     needsFlags,\n"
  "     suggestedPackage,\n"
  "     sense)\n"
  "  packageName, packageVersion, packageRelease are the name,\n"
  "    version, and release of the package that has the unresolved\n"
  "    dependency or conflict.\n"
  "  The reqName and reqVersion are the name and version of the\n"
  "    requirement or conflict.\n"
  "  The needsFlags is a bitfield that describes the versioned\n"
  "    nature of a requirement or conflict.  The constants\n"
  "    rpm.RPMSENSE_LESS, rpm.RPMSENSE_GREATER, and\n"
  "    rpm.RPMSENSE_EQUAL can be logical ANDed with the needsFlags\n"
  "    to get versioned dependency information.\n"
  "  suggestedPackage is a tuple if the dependency check was aware\n"
  "    of a package that solves this dependency problem when the\n"
  "    dependency check was run.  Packages that are added to the\n"
  "    transaction set as \"available\" are examined during the\n"
  "    dependency check as possible dependency solvers. The tuple\n"
  "    contains two values, (header, suggestedName).  These are set to\n"
  "    the header of the suggested package and its name, respectively.\n"
  "    If there is no known package to solve the dependency problem,\n"
  "    suggestedPackage is None.\n"
  "  The constants rpm.RPMDEP_SENSE_CONFLICTS and\n"
  "    rpm.RPMDEP_SENSE_REQUIRES are set to show a dependency as a\n"
  "    requirement or a conflict.\n"},
 {"order",	(PyCFunction) rpmts_Order,	METH_NOARGS,
  "ts.order() Do a topological sort of added element relations." },
 {"problems",	(PyCFunction) rpmts_Problems,	METH_NOARGS,
"ts.problems() -> ps\n\
- Return current problem set.\n" },
 {"run",	(PyCFunction) rpmts_Run,	METH_VARARGS|METH_KEYWORDS,
"ts.run(callback, data) -> (problems)\n\
- Run a transaction set, returning list of problems found.\n\
  Note: The callback may not be None.\n" },
 {"clean",	(PyCFunction) rpmts_Clean,	METH_NOARGS,
  "ts.clean()-- Free memory needed only for dependency checks\nand ordering. Should not be needed in normal operation." },
 {"clear",	(PyCFunction) rpmts_Clear,	METH_NOARGS,
"ts.clear() -> None\n\
Remove all elements from the transaction set\n" },
 {"openDB",	(PyCFunction) rpmts_OpenDB,	METH_NOARGS,
"ts.openDB() -> None -- Open the default transaction rpmdb.\n\n\
  Note: The transaction rpmdb is lazily opened,\n  so ts.openDB() is seldom needed.\n" },
 {"closeDB",	(PyCFunction) rpmts_CloseDB,	METH_NOARGS,
"ts.closeDB() -> None\n\
- Close the default transaction rpmdb.\n\
  Note: ts.closeDB() disables lazy opens,\n\
  and should hardly ever be used.\n" },
 {"initDB",	(PyCFunction) rpmts_InitDB,	METH_NOARGS,
"ts.initDB() -> None\n\
- Initialize the default transaction rpmdb.\n\
 Note: ts.initDB() is seldom needed anymore.\n" },
 {"rebuildDB",	(PyCFunction) rpmts_RebuildDB,	METH_NOARGS,
"ts.rebuildDB() -> None\n\
- Rebuild the default transaction rpmdb.\n" },
 {"verifyDB",	(PyCFunction) rpmts_VerifyDB,	METH_NOARGS,
"ts.verifyDB() -> None\n\
- Verify the default transaction rpmdb.\n" },
 {"hdrFromFdno",(PyCFunction) rpmts_HdrFromFdno,METH_O,
"ts.hdrFromFdno(fdno) -> hdr\n\
- Read a package header from a file descriptor.\n" },
 {"hdrCheck",	(PyCFunction) rpmts_HdrCheck,	METH_O,
  "ts.hdrCheck(hdrblob) -- Check header consistency,\nperforming headerGetEntry() the hard way.\n\n"
  "Sanity checks on the header are performed while looking for a\n"
  "header-only digest or signature to verify the blob. If found,\n"
  "the digest or signature is verified.\n\n"
  "\thdrblob : unloaded header blob\n"
  "Return tuple (int status, message string)"},
 {"pgpPrtPkts",	(PyCFunction) rpmts_PgpPrtPkts,	METH_VARARGS|METH_KEYWORDS,
  "pgpPrtPkts(octets) -- Print/parse a OpenPGP packet(s).\n\nReturn 0 on success." },
 {"pgpImportPubkey",	(PyCFunction) rpmts_PgpImportPubkey,	METH_VARARGS|METH_KEYWORDS,
  "pgpImportPubkey(pubkey) -- Import public key packet." },
 {"getKeyring",	(PyCFunction) rpmts_getKeyring,	METH_VARARGS|METH_KEYWORDS, 
  "ts.getKeyring(autoload=False) -- Return key ring object." },
 {"setKeyring",	(PyCFunction) rpmts_setKeyring,	METH_O, 
  "ts.setKeyring(keyring) -- Set key ring used for checking signatures\n\n"
  "Pass None for an empty key ring." },
 {"dbMatch",	(PyCFunction) rpmts_Match,	METH_VARARGS|METH_KEYWORDS,
"ts.dbMatch([TagN, [key]]) -> mi\n\
- Create a match iterator for the default transaction rpmdb.\n" },
 {"dbIndex",     (PyCFunction) rpmts_index,	METH_VARARGS|METH_KEYWORDS,
"ts.dbIndex(TagN) -> ii\n\
- Create a key iterator for the default transaction rpmdb.\n" },
 {"dbCookie",	(PyCFunction) rpmts_dbCookie, 	METH_NOARGS,
"dbCookie -> cookie\n\
- Return a cookie string for determining if database has changed\n" },
    {NULL,		NULL}		/* sentinel */
};

static void rpmts_dealloc(rpmtsObject * s)
{

    s->ts = rpmtsFree(s->ts);
    Py_XDECREF(s->scriptFd);
    Py_XDECREF(s->keyList);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject * rpmts_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
{
    rpmtsObject * s = (rpmtsObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->ts = rpmtsCreate();
    s->scriptFd = NULL;
    s->tsi = NULL;
    s->keyList = PyList_New(0);
    return (PyObject *) s;
}

static int rpmts_init(rpmtsObject *s, PyObject *args, PyObject *kwds)
{
    const char * rootDir = "/";
    rpmVSFlags vsflags = rpmExpandNumeric("%{?__vsflags}");
    char * kwlist[] = {"rootdir", "vsflags", 0};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|si:rpmts_new", kwlist,
	    &rootDir, &vsflags))
	return -1;

    (void) rpmtsSetRootDir(s->ts, rootDir);
    /* XXX: make this use common code with rpmts_SetVSFlags() to check the
     *      python objects */
    (void) rpmtsSetVSFlags(s->ts, vsflags);

    return 0;
}

static PyObject *rpmts_get_tid(rpmtsObject *s, void *closure)
{
    return Py_BuildValue("i", rpmtsGetTid(s->ts));
}

static PyObject *rpmts_get_rootDir(rpmtsObject *s, void *closure)
{
    return utf8FromString(rpmtsRootDir(s->ts));
}

static PyObject *rpmts_get_cbstyle(rpmtsObject *s, void *closure)
{
    return Py_BuildValue("i", rpmtsGetNotifyStyle(s->ts));
}

static int rpmts_set_cbstyle(rpmtsObject *s, PyObject *value, void *closure)
{
    int cbstyle;
    int rc = -1;
    if (PyArg_Parse(value, "i", &cbstyle))
	rc = rpmtsSetNotifyStyle(s->ts, cbstyle);
    return rc;
}

static int rpmts_set_scriptFd(rpmtsObject *s, PyObject *value, void *closure)
{
    rpmfdObject *fdo = NULL;
    int rc = 0;
    if (PyArg_Parse(value, "O&", rpmfdFromPyObject, &fdo)) {
	Py_XDECREF(s->scriptFd);
	s->scriptFd = fdo;
	rpmtsSetScriptFd(s->ts, rpmfdGetFd(s->scriptFd));
    } else if (value == Py_None) {
	Py_XDECREF(s->scriptFd);
	s->scriptFd = NULL;
	rpmtsSetScriptFd(s->ts, NULL);
    } else {
	rc = -1;
    }
    return rc;
}

static PyObject *rpmts_get_color(rpmtsObject *s, void *closure)
{
    return Py_BuildValue("i", rpmtsColor(s->ts));
}

static PyObject *rpmts_get_prefcolor(rpmtsObject *s, void *closure)
{
    return Py_BuildValue("i", rpmtsPrefColor(s->ts));
}

static int rpmts_set_color(rpmtsObject *s, PyObject *value, void *closure)
{
    rpm_color_t color;
    if (!PyArg_Parse(value, "i", &color)) return -1;

    /* TODO: validate the bits */
    rpmtsSetColor(s->ts, color);
    return 0;
}

static int rpmts_set_prefcolor(rpmtsObject *s, PyObject *value, void *closure)
{
    rpm_color_t color;
    if (!PyArg_Parse(value, "i", &color)) return -1;

    /* TODO: validate the bits */
    rpmtsSetPrefColor(s->ts, color);
    return 0;
}

static int rpmts_set_flags(rpmtsObject *s, PyObject *value, void *closure)
{
    rpmtransFlags flags;
    if (!PyArg_Parse(value, "i", &flags)) return -1;

    /* TODO: validate the bits */
    rpmtsSetFlags(s->ts, flags);
    return 0;
}

static int rpmts_set_vsflags(rpmtsObject *s, PyObject *value, void *closure)
{
    rpmVSFlags flags;
    if (!PyArg_Parse(value, "i", &flags)) return -1;

    /* TODO: validate the bits */
    rpmtsSetVSFlags(s->ts, flags);
    return 0;
}

static int rpmts_set_vfyflags(rpmtsObject *s, PyObject *value, void *closure)
{
    rpmVSFlags flags;
    if (!PyArg_Parse(value, "i", &flags)) return -1;

    /* TODO: validate the bits */
    rpmtsSetVfyFlags(s->ts, flags);
    return 0;
}

static int rpmts_set_vfylevel(rpmtsObject *s, PyObject *value, void *closure)
{
    int vfylevel;
    if (!PyArg_Parse(value, "i", &vfylevel)) return -1;
    rpmtsSetVfyLevel(s->ts, vfylevel);
    return 0;
}

static PyObject *rpmts_get_flags(rpmtsObject *s, void *closure)
{
    return Py_BuildValue("i", rpmtsFlags(s->ts));
}

static PyObject *rpmts_get_vsflags(rpmtsObject *s, void *closure)
{
    return Py_BuildValue("i", rpmtsVSFlags(s->ts));
}

static PyObject *rpmts_get_vfyflags(rpmtsObject *s, void *closure)
{
    return Py_BuildValue("i", rpmtsVfyFlags(s->ts));
}

static PyObject *rpmts_get_vfylevel(rpmtsObject *s, void *closure)
{
    return Py_BuildValue("i", rpmtsVfyLevel(s->ts));
}

static char rpmts_doc[] =
  "A python rpm.ts object represents an RPM transaction set.\n"
  "\n"
  "The transaction set is the workhorse of RPM. It performs the\n"
  "installation and upgrade of packages. The rpm.ts object is\n"
  "instantiated by the TransactionSet function in the rpm module.\n"
  "\n"
  "The TransactionSet function takes two optional arguments. The first\n"
  "argument is the root path. The second is the verify signature disable\n"
  "flags, a set of the following bits:\n"
  "\n"
  "-    rpm.RPMVSF_NOHDRCHK	if set, don't check rpmdb headers\n"
  "-    rpm.RPMVSF_NEEDPAYLOAD	if not set, check header+payload\n"
  "				(if possible)\n"
  "-    rpm.RPMVSF_NOSHA1HEADER	if set, don't check header SHA1 digest\n"
  "-    rpm.RPMVSF_NODSAHEADER	if set, don't check header DSA signature\n"
  "-    rpm.RPMVSF_NOMD5	if set, don't check header+payload MD5 digest\n"
  "-    rpm.RPMVSF_NODSA	if set, don't check header+payload DSA signature\n"
  "-    rpm.RPMVSF_NORSA	if set, don't check header+payload RSA signature\n"
  "\n"
  "For convenience, there are the following masks:\n"
  "-    rpm.RPMVSF_MASK_NODIGESTS	if set, don't check digest(s).\n"
  "-    rpm.RPMVSF_MASK_NOSIGNATURES	if set, don't check signature(s).\n\n"
  "The transaction set offers an read only iterable interface for the\ntransaction elements added by the .addInstall(), .addErase() and\n.addReinstall() methods.";

static PyGetSetDef rpmts_getseters[] = {
	{"cbStyle",	(getter)rpmts_get_cbstyle, (setter)rpmts_set_cbstyle,
	 "progress callback style" },
	/* only provide a setter until we have rpmfd wrappings */
	{"scriptFd",	NULL,	(setter)rpmts_set_scriptFd,
	 "write only, file descriptor the output of script gets written to." },
	{"tid",		(getter)rpmts_get_tid, NULL,
	 "read only, current transaction id, i.e. transaction time stamp."},
	{"rootDir",	(getter)rpmts_get_rootDir, NULL,
	 "read only, directory rpm treats as root of the file system." },
	{"_color",	(getter)rpmts_get_color, (setter)rpmts_set_color, NULL},
	{"_prefcolor",	(getter)rpmts_get_prefcolor, (setter)rpmts_set_prefcolor, NULL},
	{"_flags",	(getter)rpmts_get_flags, (setter)rpmts_set_flags, NULL},
	{"_vsflags",	(getter)rpmts_get_vsflags, (setter)rpmts_set_vsflags, NULL},
	{"_vfyflags",	(getter)rpmts_get_vfyflags, (setter)rpmts_set_vfyflags, NULL},
	{"_vfylevel",	(getter)rpmts_get_vfylevel, (setter)rpmts_set_vfylevel, NULL},
	{ NULL }
};

PyTypeObject rpmts_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.ts",			/* tp_name */
	sizeof(rpmtsObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmts_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr, 	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmts_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc) rpmts_iternext,	/* tp_iternext */
	rpmts_methods,			/* tp_methods */
	0,				/* tp_members */
	rpmts_getseters,		/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmts_init,		/* tp_init */
	0,				/* tp_alloc */
	(newfunc) rpmts_new,		/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};
