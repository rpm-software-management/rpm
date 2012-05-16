#include "rpmsystem-py.h"

#include <rpm/rpmlib.h>		/* rpmMachineScore, rpmReadConfigFiles */
#include <rpm/rpmtag.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmsq.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>

#include "header-py.h"
#include "rpmds-py.h"
#include "rpmfd-py.h"
#include "rpmfi-py.h"
#include "rpmkeyring-py.h"
#include "rpmmi-py.h"
#include "rpmii-py.h"
#include "rpmps-py.h"
#include "rpmmacro-py.h"
#include "rpmtd-py.h"
#include "rpmte-py.h"
#include "rpmts-py.h"

/** \ingroup python
 * \name Module: rpm
 */

PyObject * pyrpmError;

static PyObject * archScore(PyObject * self, PyObject * arg)
{
    const char * arch;

    if (!PyArg_Parse(arg, "s", &arch))
	return NULL;

    return Py_BuildValue("i", rpmMachineScore(RPM_MACHTABLE_INSTARCH, arch));
}

static PyObject * signalCaught(PyObject *self, PyObject *o)
{
    int signo;
    if (!PyArg_Parse(o, "i", &signo)) return NULL;

    return PyBool_FromLong(rpmsqIsCaught(signo));
}

static PyObject * checkSignals(PyObject * self)
{
    rpmdbCheckSignals();
    Py_RETURN_NONE;
}

static PyObject * setLogFile (PyObject * self, PyObject *arg)
{
    FILE *fp;
    int fdno = PyObject_AsFileDescriptor(arg);

    if (fdno >= 0) {
	/* XXX we dont know the mode here.. guessing append for now */
	fp = fdopen(fdno, "a");
	if (fp == NULL) {
	    PyErr_SetFromErrno(PyExc_IOError);
	    return NULL;
	}
    } else if (arg == Py_None) {
	fp = NULL;
    } else {
	PyErr_SetString(PyExc_TypeError, "file object or None expected");
	return NULL;
    }

    (void) rpmlogSetFile(fp);
    Py_RETURN_NONE;
}

static PyObject *
setVerbosity (PyObject * self, PyObject * arg)
{
    int level;

    if (!PyArg_Parse(arg, "i", &level))
	return NULL;

    rpmSetVerbosity(level);

    Py_RETURN_NONE;
}

static PyObject *
setEpochPromote (PyObject * self, PyObject * arg)
{
    if (!PyArg_Parse(arg, "i", &_rpmds_nopromote))
	return NULL;

    Py_RETURN_NONE;
}

static PyObject * setStats (PyObject * self, PyObject * arg)
{
    if (!PyArg_Parse(arg, "i", &_rpmts_stats))
	return NULL;

    Py_RETURN_NONE;
}

static PyObject * doLog(PyObject * self, PyObject * args, PyObject *kwds)
{
    int code;
    const char *msg;
    char * kwlist[] = {"code", "msg", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "is", kwlist, &code, &msg))
	return NULL;

    rpmlog(code, "%s", msg);
    Py_RETURN_NONE;
}

static PyObject * reloadConfig(PyObject * self, PyObject * args, PyObject *kwds)
{
    const char * target = NULL;
    char * kwlist[] = { "target", NULL };
    int rc;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &target))
        return NULL;

    rpmFreeMacros(NULL);
    rpmFreeRpmrc();
    rc = rpmReadConfigFiles(NULL, target) ;

    return PyBool_FromLong(rc == 0);
}

static PyMethodDef rpmModuleMethods[] = {
    { "addMacro", (PyCFunction) rpmmacro_AddMacro, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "delMacro", (PyCFunction) rpmmacro_DelMacro, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "expandMacro", (PyCFunction) rpmmacro_ExpandMacro, METH_VARARGS|METH_KEYWORDS,
	NULL },

    { "archscore", (PyCFunction) archScore, METH_O,
	NULL },

    { "signalCaught", (PyCFunction) signalCaught, METH_O, 
	NULL },
    { "checkSignals", (PyCFunction) checkSignals, METH_NOARGS,
        NULL },

    { "mergeHeaderListFromFD", (PyCFunction) rpmMergeHeadersFromFD, METH_VARARGS|METH_KEYWORDS,
	NULL },

    { "log",		(PyCFunction) doLog, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "setLogFile", (PyCFunction) setLogFile, METH_O,
	NULL },

    { "versionCompare", (PyCFunction) versionCompare, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "labelCompare", (PyCFunction) labelCompare, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "setVerbosity", (PyCFunction) setVerbosity, METH_O,
	NULL },
    { "setEpochPromote", (PyCFunction) setEpochPromote, METH_O,
	NULL },
    { "setStats", (PyCFunction) setStats, METH_O,
	NULL },
    { "reloadConfig", (PyCFunction) reloadConfig, METH_VARARGS|METH_KEYWORDS,
	NULL },

    { NULL }
} ;

/*
* Force clean up of open iterators and dbs on exit.
*/
static void rpm_exithook(void)
{
   rpmdbCheckTerminate(1);
}

static char rpm__doc__[] =
"";

/*
 * Add rpm tag dictionaries to the module
 */
static void addRpmTags(PyObject *module)
{
    PyObject *pyval, *pyname, *dict = PyDict_New();
    rpmtd names = rpmtdNew();
    rpmTagGetNames(names, 1);
    const char *tagname, *shortname;
    rpmTagVal tagval;

    while ((tagname = rpmtdNextString(names))) {
	shortname = tagname + strlen("RPMTAG_");
	tagval = rpmTagGetValue(shortname);

	PyModule_AddIntConstant(module, tagname, tagval);
	pyval = PyInt_FromLong(tagval);
	pyname = Py_BuildValue("s", shortname);
	PyDict_SetItem(dict, pyval, pyname);
	Py_DECREF(pyval);
	Py_DECREF(pyname);
    }
    PyModule_AddObject(module, "tagnames", dict);
    rpmtdFreeData(names);
    rpmtdFree(names);
}

/*
  Do any common preliminary work before python 2 vs python 3 module creation:
*/
static int prepareInitModule(void)
{
    if (PyType_Ready(&hdr_Type) < 0) return 0;
    if (PyType_Ready(&rpmds_Type) < 0) return 0;
    if (PyType_Ready(&rpmfd_Type) < 0) return 0;
    if (PyType_Ready(&rpmfi_Type) < 0) return 0;
    if (PyType_Ready(&rpmKeyring_Type) < 0) return 0;
    if (PyType_Ready(&rpmmi_Type) < 0) return 0;
    if (PyType_Ready(&rpmii_Type) < 0) return 0;
    if (PyType_Ready(&rpmProblem_Type) < 0) return 0;
    if (PyType_Ready(&rpmPubkey_Type) < 0) return 0;
#if 0
    if (PyType_Ready(&rpmtd_Type) < 0) return 0;
#endif
    if (PyType_Ready(&rpmte_Type) < 0) return 0;
    if (PyType_Ready(&rpmts_Type) < 0) return 0;

    return 1;
}
static int initModule(PyObject *m);

#if PY_MAJOR_VERSION >= 3
static int rpmModuleTraverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(pyrpmError);
    return 0;
}

static int rpmModuleClear(PyObject *m) {
    Py_CLEAR(pyrpmError);
    return 0;
}

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_rpm",            /* m_name */
    rpm__doc__,        /* m_doc */
    0,                 /* m_size */
    rpmModuleMethods,
    NULL,              /* m_reload */
    rpmModuleTraverse,
    rpmModuleClear,
    NULL               /* m_free */
};

PyObject *
PyInit__rpm(void);

PyObject *
PyInit__rpm(void)
{
    PyObject * m;
    if (!prepareInitModule()) return NULL;
    m = PyModule_Create(&moduledef);
    initModule(m);
    return m;
}
#else
void init_rpm(void);	/* XXX eliminate gcc warning */
void init_rpm(void)
{
    PyObject * m;

    if (!prepareInitModule()) return;
    m = Py_InitModule3("_rpm", rpmModuleMethods, rpm__doc__);
    if (m == NULL)
	return;
    initModule(m);
}
#endif

/* Shared python2/3 module initialization: */
static int initModule(PyObject *m)
{
    PyObject * d;

    /* 
     * treat error to register rpm cleanup hook as fatal, tracebacks
     * can and will leave stale locks around if we can't clean up
     */
    if (Py_AtExit(rpm_exithook) == -1)
        return 0;

    rpmReadConfigFiles(NULL, NULL);

    d = PyModule_GetDict(m);

    pyrpmError = PyErr_NewException("_rpm.error", NULL, NULL);
    if (pyrpmError != NULL)
	PyDict_SetItemString(d, "error", pyrpmError);

    Py_INCREF(&hdr_Type);
    PyModule_AddObject(m, "hdr", (PyObject *) &hdr_Type);

    Py_INCREF(&rpmds_Type);
    PyModule_AddObject(m, "ds", (PyObject *) &rpmds_Type);

    Py_INCREF(&rpmfd_Type);
    PyModule_AddObject(m, "fd", (PyObject *) &rpmfd_Type);

    Py_INCREF(&rpmfi_Type);
    PyModule_AddObject(m, "fi", (PyObject *) &rpmfi_Type);

    Py_INCREF(&rpmKeyring_Type);
    PyModule_AddObject(m, "keyring", (PyObject *) &rpmKeyring_Type);

    Py_INCREF(&rpmmi_Type);
    PyModule_AddObject(m, "mi", (PyObject *) &rpmmi_Type);

    Py_INCREF(&rpmii_Type);
    PyModule_AddObject(m, "ii", (PyObject *) &rpmii_Type);

    Py_INCREF(&rpmProblem_Type);
    PyModule_AddObject(m, "prob", (PyObject *) &rpmProblem_Type);

    Py_INCREF(&rpmPubkey_Type);
    PyModule_AddObject(m, "pubkey", (PyObject *) &rpmPubkey_Type);

#if 0
    Py_INCREF(&rpmtd_Type);
    PyModule_AddObject(m, "td", (PyObject *) &rpmtd_Type);
#endif

    Py_INCREF(&rpmte_Type);
    PyModule_AddObject(m, "te", (PyObject *) &rpmte_Type);

    Py_INCREF(&rpmts_Type);
    PyModule_AddObject(m, "ts", (PyObject *) &rpmts_Type);

    addRpmTags(m);

    PyModule_AddStringConstant(m, "__version__", RPMVERSION);

#define REGISTER_ENUM(val) PyModule_AddIntConstant(m, #val, val)

    REGISTER_ENUM(RPMTAG_NOT_FOUND);

    REGISTER_ENUM(RPMRC_OK);
    REGISTER_ENUM(RPMRC_NOTFOUND);
    REGISTER_ENUM(RPMRC_FAIL);
    REGISTER_ENUM(RPMRC_NOTTRUSTED);
    REGISTER_ENUM(RPMRC_NOKEY);

    REGISTER_ENUM(RPMFILE_STATE_NORMAL);
    REGISTER_ENUM(RPMFILE_STATE_REPLACED);
    REGISTER_ENUM(RPMFILE_STATE_NOTINSTALLED);
    REGISTER_ENUM(RPMFILE_STATE_NETSHARED);
    REGISTER_ENUM(RPMFILE_STATE_WRONGCOLOR);

    REGISTER_ENUM(RPMFILE_CONFIG);
    REGISTER_ENUM(RPMFILE_DOC);
    REGISTER_ENUM(RPMFILE_MISSINGOK);
    REGISTER_ENUM(RPMFILE_NOREPLACE);
    REGISTER_ENUM(RPMFILE_GHOST);
    REGISTER_ENUM(RPMFILE_LICENSE);
    REGISTER_ENUM(RPMFILE_README);
    REGISTER_ENUM(RPMFILE_PUBKEY);

    REGISTER_ENUM(RPMDEP_SENSE_REQUIRES);
    REGISTER_ENUM(RPMDEP_SENSE_CONFLICTS);

    REGISTER_ENUM(RPMSENSE_ANY);
    REGISTER_ENUM(RPMSENSE_LESS);
    REGISTER_ENUM(RPMSENSE_GREATER);
    REGISTER_ENUM(RPMSENSE_EQUAL);
    REGISTER_ENUM(RPMSENSE_POSTTRANS);
    REGISTER_ENUM(RPMSENSE_PREREQ);
    REGISTER_ENUM(RPMSENSE_PRETRANS);
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
    REGISTER_ENUM(RPMSENSE_RPMLIB);
    REGISTER_ENUM(RPMSENSE_TRIGGERPREIN);
    REGISTER_ENUM(RPMSENSE_KEYRING);
    REGISTER_ENUM(RPMSENSE_CONFIG);

    REGISTER_ENUM(RPMTRANS_FLAG_TEST);
    REGISTER_ENUM(RPMTRANS_FLAG_BUILD_PROBS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSCRIPTS);
    REGISTER_ENUM(RPMTRANS_FLAG_JUSTDB);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERS);
    REGISTER_ENUM(RPMTRANS_FLAG_NODOCS);
    REGISTER_ENUM(RPMTRANS_FLAG_ALLFILES);
    REGISTER_ENUM(RPMTRANS_FLAG_KEEPOBSOLETE);
    REGISTER_ENUM(RPMTRANS_FLAG_NOCONTEXTS);
    REGISTER_ENUM(RPMTRANS_FLAG_REPACKAGE);
    REGISTER_ENUM(RPMTRANS_FLAG_REVERSE);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPRE);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOST);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERPREIN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERIN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPREUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOSTUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERPOSTUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOMD5);
    REGISTER_ENUM(RPMTRANS_FLAG_NOFILEDIGEST);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSUGGEST);
    REGISTER_ENUM(RPMTRANS_FLAG_ADDINDEPS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOCONFIGS);

    REGISTER_ENUM(RPMPROB_FILTER_IGNOREOS);
    REGISTER_ENUM(RPMPROB_FILTER_IGNOREARCH);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEPKG);
    REGISTER_ENUM(RPMPROB_FILTER_FORCERELOCATE);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACENEWFILES);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEOLDFILES);
    REGISTER_ENUM(RPMPROB_FILTER_OLDPACKAGE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKSPACE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKNODES);

    REGISTER_ENUM(RPMCALLBACK_UNKNOWN);
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
    REGISTER_ENUM(RPMCALLBACK_REPACKAGE_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_REPACKAGE_START);
    REGISTER_ENUM(RPMCALLBACK_REPACKAGE_STOP);
    REGISTER_ENUM(RPMCALLBACK_UNPACK_ERROR);
    REGISTER_ENUM(RPMCALLBACK_CPIO_ERROR);
    REGISTER_ENUM(RPMCALLBACK_SCRIPT_ERROR);
    REGISTER_ENUM(RPMCALLBACK_SCRIPT_START);
    REGISTER_ENUM(RPMCALLBACK_SCRIPT_STOP);
    REGISTER_ENUM(RPMCALLBACK_INST_STOP);

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
    REGISTER_ENUM(RPMPROB_OBSOLETES);

    REGISTER_ENUM(VERIFY_DIGEST);
    REGISTER_ENUM(VERIFY_SIGNATURE);

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

    REGISTER_ENUM(RPMVSF_DEFAULT);
    REGISTER_ENUM(RPMVSF_NOHDRCHK);
    REGISTER_ENUM(RPMVSF_NEEDPAYLOAD);
    REGISTER_ENUM(RPMVSF_NOSHA1HEADER);
    REGISTER_ENUM(RPMVSF_NOMD5HEADER);
    REGISTER_ENUM(RPMVSF_NODSAHEADER);
    REGISTER_ENUM(RPMVSF_NORSAHEADER);
    REGISTER_ENUM(RPMVSF_NOSHA1);
    REGISTER_ENUM(RPMVSF_NOMD5);
    REGISTER_ENUM(RPMVSF_NODSA);
    REGISTER_ENUM(RPMVSF_NORSA);
    REGISTER_ENUM(_RPMVSF_NODIGESTS);
    REGISTER_ENUM(_RPMVSF_NOSIGNATURES);
    REGISTER_ENUM(_RPMVSF_NOHEADER);
    REGISTER_ENUM(_RPMVSF_NOPAYLOAD);

    REGISTER_ENUM(TR_ADDED);
    REGISTER_ENUM(TR_REMOVED);

    REGISTER_ENUM(RPMDBI_PACKAGES);
    REGISTER_ENUM(RPMDBI_LABEL);
    REGISTER_ENUM(RPMDBI_INSTFILENAMES);
    REGISTER_ENUM(RPMDBI_NAME);
    REGISTER_ENUM(RPMDBI_BASENAMES);
    REGISTER_ENUM(RPMDBI_GROUP);
    REGISTER_ENUM(RPMDBI_REQUIRENAME);
    REGISTER_ENUM(RPMDBI_PROVIDENAME);
    REGISTER_ENUM(RPMDBI_CONFLICTNAME);
    REGISTER_ENUM(RPMDBI_OBSOLETENAME);
    REGISTER_ENUM(RPMDBI_TRIGGERNAME);
    REGISTER_ENUM(RPMDBI_DIRNAMES);
    REGISTER_ENUM(RPMDBI_INSTALLTID);
    REGISTER_ENUM(RPMDBI_SIGMD5);
    REGISTER_ENUM(RPMDBI_SHA1HEADER);

    REGISTER_ENUM(HEADERCONV_EXPANDFILELIST);
    REGISTER_ENUM(HEADERCONV_COMPRESSFILELIST);
    REGISTER_ENUM(HEADERCONV_RETROFIT_V3);

    return 1;
}

