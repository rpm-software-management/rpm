#include "rpmsystem-py.h"

#include <rpm/rpmlib.h>		/* rpmMachineScore, rpmReadConfigFiles */
#include <rpm/rpmtag.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmsq.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmsign.h>

#include "header-py.h"
#include "rpmarchive-py.h"
#include "rpmds-py.h"
#include "rpmfd-py.h"
#include "rpmfiles-py.h"
#include "rpmkeyring-py.h"
#include "rpmmi-py.h"
#include "rpmii-py.h"
#include "rpmps-py.h"
#include "rpmmacro-py.h"
#include "rpmstrpool-py.h"
#include "rpmtd-py.h"
#include "rpmte-py.h"
#include "rpmts-py.h"
#include "rpmver-py.h"
#include "spec-py.h"

/** \ingroup python
 * \name Module: rpm
 */

unsigned long python_version = 0;

static PyObject * archScore(PyObject * self, PyObject * arg)
{
    const char * arch;

    if (!PyArg_Parse(arg, "s", &arch))
	return NULL;

    return Py_BuildValue("i", rpmMachineScore(RPM_MACHTABLE_INSTARCH, arch));
}

static PyObject * blockSignals(PyObject * self, PyObject *arg)
{
    int block;
    if (!PyArg_Parse(arg, "p", &block)) return NULL;

    return Py_BuildValue("i", rpmsqBlock(block ? SIG_BLOCK : SIG_UNBLOCK));
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

static int parseSignArgs(PyObject * args, PyObject *kwds,
			const char **path, struct rpmSignArgs *sargs)
{
    char * kwlist[] = { "path", "keyid", "hashalgo", NULL };

    memset(sargs, 0, sizeof(*sargs));
    return PyArg_ParseTupleAndKeywords(args, kwds, "s|si", kwlist,
				    path, &sargs->keyid, &sargs->hashalgo);
}

static PyObject * addSign(PyObject * self, PyObject * args, PyObject *kwds)
{
    const char *path = NULL;
    struct rpmSignArgs sargs;

    if (!parseSignArgs(args, kwds, &path, &sargs))
	return NULL;

    return PyBool_FromLong(rpmPkgSign(path, &sargs) == 0);
}

static PyObject * delSign(PyObject * self, PyObject * args, PyObject *kwds)
{
    const char *path = NULL;
    struct rpmSignArgs sargs;

    if (!parseSignArgs(args, kwds, &path, &sargs))
	return NULL;

    return PyBool_FromLong(rpmPkgDelSign(path, &sargs) == 0);
}

static PyMethodDef rpmModuleMethods[] = {
    { "addMacro", (PyCFunction) rpmmacro_AddMacro, METH_VARARGS|METH_KEYWORDS,
      "rpmPushMacro(macro, value)\n"
    },
    { "delMacro", (PyCFunction) rpmmacro_DelMacro, METH_VARARGS|METH_KEYWORDS,
      "rpmPopMacro(macro)\n"
    },
    { "expandMacro", (PyCFunction) rpmmacro_ExpandMacro, METH_VARARGS|METH_KEYWORDS,
      "expandMacro(string, numeric=False) -- expands a string containing macros\n\n"
      "Returns an int if numeric is True. 'Y' or 'y' returns 1,\n'N' or 'n' returns 0\nAn undefined macro returns 0."},

    { "archscore", (PyCFunction) archScore, METH_O,
      "archscore(archname) -- How well does an architecture fit on this machine\n\n"
      "0 for non matching arch names\n1 for best arch\nhigher numbers for less fitting arches\n(e.g. 2 for \"i586\" on an i686 machine)" },

    { "blockSignals", (PyCFunction) blockSignals, METH_O,
      "blocksignals(True/False) -- Block/unblock signals, refcounted."},

    { "log",		(PyCFunction) doLog, METH_VARARGS|METH_KEYWORDS,
	"log(level, msg) -- Write msg to log if level is selected to be logged.\n\n"
    "level must be one of the RPMLOG_* constants."},
    { "setLogFile", (PyCFunction) setLogFile, METH_O,
	"setLogFile(file) -- set file to write log messages to or None." },

    { "versionCompare", (PyCFunction) versionCompare, METH_VARARGS|METH_KEYWORDS,
      "versionCompare(version0, version1) -- compares two version strings\n\n"
      "Returns 1 if version0 > version1\n"
      "Returns 0 if version0 == version1\n"
      "Returns -1 if version0 < version1\n"},
    { "labelCompare", (PyCFunction) labelCompare, METH_VARARGS|METH_KEYWORDS,
      "labelCompare(version0, version1) -- as versionCompare()\n\n"
      "but arguments are tuples of of strings for (epoch, version, release)"},
    { "setVerbosity", (PyCFunction) setVerbosity, METH_O,
      "setVerbosity(level) -- Set log level. See RPMLOG_* constants." },
    { "setStats", (PyCFunction) setStats, METH_O,
      "setStats(bool) -- Set if timing stats are printed after a transaction."},
    { "reloadConfig", (PyCFunction) reloadConfig, METH_VARARGS|METH_KEYWORDS,
      "reloadConfig(target=None) -- Reload config from files.\n\n"
      "Set all macros and settings accordingly."},
    { "addSign", (PyCFunction) addSign, METH_VARARGS|METH_KEYWORDS, NULL },
    { "delSign", (PyCFunction) delSign, METH_VARARGS|METH_KEYWORDS, NULL },
    { NULL }
} ;

static char rpm__doc__[] = "";

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
	pyval = PyLong_FromLong(tagval);
	pyname = utf8FromString(shortname);
	PyDict_SetItem(dict, pyval, pyname);
	Py_DECREF(pyval);
	Py_DECREF(pyname);
    }
    PyModule_AddObject(module, "tagnames", dict);
    rpmtdFree(names);
}

static int initModule(PyObject *m);

static int rpmModuleTraverse(PyObject *m, visitproc visit, void *arg) {
    rpmmodule_state_t *modstate = PyModule_GetState(m);
    Py_VISIT(modstate->hdr_Type);
    Py_VISIT(modstate->rpmarchive_Type);
    Py_VISIT(modstate->rpmds_Type);
    Py_VISIT(modstate->rpmfd_Type);
    Py_VISIT(modstate->rpmfile_Type);
    Py_VISIT(modstate->rpmfiles_Type);
    Py_VISIT(modstate->rpmii_Type);
    Py_VISIT(modstate->rpmKeyring_Type);
    Py_VISIT(modstate->rpmPubkey_Type);
    Py_VISIT(modstate->rpmmi_Type);
    Py_VISIT(modstate->rpmProblem_Type);
    Py_VISIT(modstate->rpmstrPool_Type);
    Py_VISIT(modstate->rpmte_Type);
    Py_VISIT(modstate->rpmts_Type);
    Py_VISIT(modstate->rpmver_Type);
    Py_VISIT(modstate->spec_Type);
    Py_VISIT(modstate->specPkg_Type);
    Py_VISIT(modstate->pyrpmError);
    return 0;
}

static int rpmModuleClear(PyObject *m) {
    rpmmodule_state_t *modstate = PyModule_GetState(m);
    Py_CLEAR(modstate->hdr_Type);
    Py_CLEAR(modstate->rpmarchive_Type);
    Py_CLEAR(modstate->rpmds_Type);
    Py_CLEAR(modstate->rpmfd_Type);
    Py_CLEAR(modstate->rpmfile_Type);
    Py_CLEAR(modstate->rpmfiles_Type);
    Py_CLEAR(modstate->rpmii_Type);
    Py_CLEAR(modstate->rpmKeyring_Type);
    Py_CLEAR(modstate->rpmPubkey_Type);
    Py_CLEAR(modstate->rpmmi_Type);
    Py_CLEAR(modstate->rpmProblem_Type);
    Py_CLEAR(modstate->rpmstrPool_Type);
    Py_CLEAR(modstate->rpmte_Type);
    Py_CLEAR(modstate->rpmts_Type);
    Py_CLEAR(modstate->rpmver_Type);
    Py_CLEAR(modstate->spec_Type);
    Py_CLEAR(modstate->specPkg_Type);
    Py_CLEAR(modstate->pyrpmError);
    return 0;
}

static void rpmModuleFree(void *m) {
    (void)rpmModuleClear(m);
}

static PyModuleDef_Slot module_slots[] = {
    {Py_mod_exec, initModule},
    {0}
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_rpm",			/* m_name */
    rpm__doc__,			/* m_doc */
    sizeof(rpmmodule_state_t),	/* m_size */
    rpmModuleMethods,
    module_slots,      /* m_slots */
    rpmModuleTraverse,
    rpmModuleClear,
    rpmModuleFree
};

/* The init function must be exported as it's called by Python, but it doesn't
 * need a declaration in a header file.
 * Provide a declaration to avoid -Wmissing-prototypes warnings.
 */
PyObject *
PyInit__rpm(void);

PyObject *
PyInit__rpm(void)
{
    return PyModuleDef_Init(&moduledef);
}

/* Create a type object based on a Spec, and add it to the module. */
static int initAndAddType(PyObject *m, PyTypeObject **type, PyType_Spec *spec,
                          char *name)
{
    if (!*type) {
        *type = (PyTypeObject *)PyType_FromModuleAndSpec(m, spec, NULL);
        if (!*type) return 0;
    }
    /* We intentionally leak a reference to `type` (only once per type per
     * process).
     */
    Py_INCREF(*type);
    /* Reference counting for PyModule_AddObject is tricky (see
     * PyModule_AddObject docs). But let's do it right, as if we haven't just
     * leaked.
     * (Simpler API, `PyModule_AddObjectRef`, is only in Python 3.10+.)
     */
    if (PyModule_AddObject(m, name, (PyObject *) *type) < 0) {
        Py_DECREF(*type);
        return 0;
    }
    return 1;
}

static unsigned long _get_python_version(void) {
    PyObject *python_version_tuple = PySys_GetObject("version_info");
    if (python_version_tuple == NULL) {
        PyErr_SetString(PyExc_SystemError, "could not get Python version");
	return 0;
    }
    unsigned int major, minor, micro, serial;
    PyObject *releaselevel;
    if (!PyArg_ParseTuple(
	    python_version_tuple, "IIIOI",
	    &major, &minor, &micro, &releaselevel, &serial)) {
	return 0;
    }
    return (major << 24) | (minor << 16) | (micro << 8);
}

/* Module initialization: */
static int initModule(PyObject *m)
{
    PyObject * d;

    /* failure to initialize rpm (crypto and all) is rather fatal too... */
    if (rpmReadConfigFiles(NULL, NULL) == -1)
	return -1;

    d = PyModule_GetDict(m);

    if (python_version == 0) {
	python_version = _get_python_version();
	if (python_version == 0) {
	return -1;
	}
    }

    rpmmodule_state_t *modstate = PyModule_GetState(m);
    if (!modstate) {
	return -1;
    }

    modstate->pyrpmError = PyErr_NewException("_rpm.error", NULL, NULL);
    if (modstate->pyrpmError != NULL)
	PyDict_SetItemString(d, "error", modstate->pyrpmError);

    if (!initAndAddType(m, &modstate->hdr_Type, &hdr_Type_Spec, "hdr")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmarchive_Type, &rpmarchive_Type_Spec, "archive")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmds_Type, &rpmds_Type_Spec, "ds")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmfd_Type, &rpmfd_Type_Spec, "fd")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmfile_Type, &rpmfile_Type_Spec, "file")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmfiles_Type, &rpmfiles_Type_Spec, "files")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmKeyring_Type, &rpmKeyring_Type_Spec, "keyring")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmmi_Type, &rpmmi_Type_Spec, "mi")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmii_Type, &rpmii_Type_Spec, "ii")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmProblem_Type, &rpmProblem_Type_Spec, "prob")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmPubkey_Type, &rpmPubkey_Type_Spec, "pubkey")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmstrPool_Type, &rpmstrPool_Type_Spec, "strpool")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmte_Type, &rpmte_Type_Spec, "te")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmts_Type, &rpmts_Type_Spec, "ts")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->rpmver_Type, &rpmver_Type_Spec, "ver")) {
	return -1;
    }

    if (!initAndAddType(m, &modstate->spec_Type, &spec_Type_Spec, "spec")) {
	return -1;
    }
    if (!initAndAddType(m, &modstate->specPkg_Type, &specPkg_Type_Spec, "specPkg")) {
	return -1;
    }

    addRpmTags(m);

    PyModule_AddStringConstant(m, "__version__", RPMVERSION);

    PyModule_AddObject(m, "header_magic",
		PyBytes_FromStringAndSize((const char *)rpm_header_magic, 8));

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
    REGISTER_ENUM(RPMFILE_ICON);
    REGISTER_ENUM(RPMFILE_MISSINGOK);
    REGISTER_ENUM(RPMFILE_NOREPLACE);
    REGISTER_ENUM(RPMFILE_SPECFILE);
    REGISTER_ENUM(RPMFILE_GHOST);
    REGISTER_ENUM(RPMFILE_LICENSE);
    REGISTER_ENUM(RPMFILE_README);
    REGISTER_ENUM(RPMFILE_PUBKEY);
    REGISTER_ENUM(RPMFILE_ARTIFACT);

    REGISTER_ENUM(RPMFI_FLAGS_ERASE);
    REGISTER_ENUM(RPMFI_FLAGS_INSTALL);
    REGISTER_ENUM(RPMFI_FLAGS_VERIFY);
    REGISTER_ENUM(RPMFI_FLAGS_QUERY);
    REGISTER_ENUM(RPMFI_FLAGS_FILETRIGGER);
    REGISTER_ENUM(RPMFI_FLAGS_ONLY_FILENAMES);

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
    REGISTER_ENUM(RPMSENSE_MISSINGOK);
    REGISTER_ENUM(RPMSENSE_PREUNTRANS);
    REGISTER_ENUM(RPMSENSE_POSTUNTRANS);

    REGISTER_ENUM(RPMTRANS_FLAG_TEST);
    REGISTER_ENUM(RPMTRANS_FLAG_BUILD_PROBS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSCRIPTS);
    REGISTER_ENUM(RPMTRANS_FLAG_JUSTDB);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERS);
    REGISTER_ENUM(RPMTRANS_FLAG_NODOCS);
    REGISTER_ENUM(RPMTRANS_FLAG_ALLFILES);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPLUGINS);
    REGISTER_ENUM(RPMTRANS_FLAG_KEEPOBSOLETE);
    REGISTER_ENUM(RPMTRANS_FLAG_NOCONTEXTS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOCAPS);
    REGISTER_ENUM(RPMTRANS_FLAG_NODB);
    REGISTER_ENUM(RPMTRANS_FLAG_REPACKAGE);
    REGISTER_ENUM(RPMTRANS_FLAG_REVERSE);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPRE);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOST);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPREUNTRANS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOSTUNTRANS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERPREIN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERIN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPREUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOSTUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERPOSTUN);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPRETRANS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOPOSTTRANS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOMD5);
    REGISTER_ENUM(RPMTRANS_FLAG_NOFILEDIGEST);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSUGGEST);
    REGISTER_ENUM(RPMTRANS_FLAG_ADDINDEPS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOCONFIGS);
    REGISTER_ENUM(RPMTRANS_FLAG_DEPLOOPS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOARTIFACTS);

    REGISTER_ENUM(RPMPROB_FILTER_IGNOREOS);
    REGISTER_ENUM(RPMPROB_FILTER_IGNOREARCH);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEPKG);
    REGISTER_ENUM(RPMPROB_FILTER_FORCERELOCATE);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACENEWFILES);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEOLDFILES);
    REGISTER_ENUM(RPMPROB_FILTER_OLDPACKAGE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKSPACE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKNODES);
    REGISTER_ENUM(RPMPROB_FILTER_VERIFY);

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
    REGISTER_ENUM(RPMCALLBACK_ELEM_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_VERIFY_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_VERIFY_START);
    REGISTER_ENUM(RPMCALLBACK_VERIFY_STOP);

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
    REGISTER_ENUM(RPMPROB_VERIFY);

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
    REGISTER_ENUM(RPMVSF_NOSHA256HEADER);
    REGISTER_ENUM(RPMVSF_NOSHA3_256HEADER);
    REGISTER_ENUM(RPMVSF_NODSAHEADER);
    REGISTER_ENUM(RPMVSF_NORSAHEADER);
    REGISTER_ENUM(RPMVSF_NOSHA512PAYLOAD);
    REGISTER_ENUM(RPMVSF_NOSHA3_256PAYLOAD);
    REGISTER_ENUM(RPMVSF_NOSHA256PAYLOAD);
    REGISTER_ENUM(RPMVSF_NOPAYLOAD);
    REGISTER_ENUM(RPMVSF_NOMD5);
    REGISTER_ENUM(RPMVSF_NODSA);
    REGISTER_ENUM(RPMVSF_NORSA);
    REGISTER_ENUM(_RPMVSF_NODIGESTS);
    REGISTER_ENUM(_RPMVSF_NOSIGNATURES);
    REGISTER_ENUM(_RPMVSF_NOHEADER);
    REGISTER_ENUM(_RPMVSF_NOPAYLOAD);
    REGISTER_ENUM(RPMVSF_MASK_NODIGESTS);
    REGISTER_ENUM(RPMVSF_MASK_NOSIGNATURES);
    REGISTER_ENUM(RPMVSF_MASK_NOHEADER);
    REGISTER_ENUM(RPMVSF_MASK_NOPAYLOAD);

    REGISTER_ENUM(RPMSIG_NONE_TYPE);
    REGISTER_ENUM(RPMSIG_DIGEST_TYPE);
    REGISTER_ENUM(RPMSIG_SIGNATURE_TYPE);
    REGISTER_ENUM(RPMSIG_VERIFIABLE_TYPE);
    REGISTER_ENUM(RPMSIG_UNVERIFIED_TYPE);

    REGISTER_ENUM(TR_ADDED);
    REGISTER_ENUM(TR_REMOVED);
    REGISTER_ENUM(TR_RPMDB);

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

    REGISTER_ENUM(RPMVERIFY_NONE);
    REGISTER_ENUM(RPMVERIFY_FILEDIGEST);
    REGISTER_ENUM(RPMVERIFY_FILESIZE);
    REGISTER_ENUM(RPMVERIFY_LINKTO);
    REGISTER_ENUM(RPMVERIFY_USER);
    REGISTER_ENUM(RPMVERIFY_GROUP);
    REGISTER_ENUM(RPMVERIFY_MTIME);
    REGISTER_ENUM(RPMVERIFY_MODE);
    REGISTER_ENUM(RPMVERIFY_RDEV);
    REGISTER_ENUM(RPMVERIFY_CAPS);
    REGISTER_ENUM(RPMVERIFY_READLINKFAIL);
    REGISTER_ENUM(RPMVERIFY_READFAIL);
    REGISTER_ENUM(RPMVERIFY_LSTATFAIL);

    REGISTER_ENUM(RPMBUILD_ISSOURCE);
    REGISTER_ENUM(RPMBUILD_ISPATCH);
    REGISTER_ENUM(RPMBUILD_ISICON);
    REGISTER_ENUM(RPMBUILD_ISNO);

    REGISTER_ENUM(RPMBUILD_NONE);
    REGISTER_ENUM(RPMBUILD_PREP);
    REGISTER_ENUM(RPMBUILD_BUILD);
    REGISTER_ENUM(RPMBUILD_INSTALL);
    REGISTER_ENUM(RPMBUILD_CHECK);
    REGISTER_ENUM(RPMBUILD_CLEAN);
    REGISTER_ENUM(RPMBUILD_FILECHECK);
    REGISTER_ENUM(RPMBUILD_PACKAGESOURCE);
    REGISTER_ENUM(RPMBUILD_PACKAGEBINARY);
    REGISTER_ENUM(RPMBUILD_RMSOURCE);
    REGISTER_ENUM(RPMBUILD_RMBUILD);
    REGISTER_ENUM(RPMBUILD_RMSPEC);

    REGISTER_ENUM(RPMBUILD_PKG_NONE);
    REGISTER_ENUM(RPMBUILD_PKG_NODIRTOKENS);

    REGISTER_ENUM(RPMSPEC_NONE);
    REGISTER_ENUM(RPMSPEC_ANYARCH);
    REGISTER_ENUM(RPMSPEC_FORCE);
    REGISTER_ENUM(RPMSPEC_NOLANG);

    return 0;
}

rpmmodule_state_t *rpmModState_FromModule(PyObject *m) {
    assert(PyModule_GetDef(m) == &moduledef);
    rpmmodule_state_t *state = PyModule_GetState(m);
    if (state == NULL && !PyErr_Occurred()) {
	PyErr_SetString(PyExc_SystemError, "could not get _rpm module state");
    }
    return state;
}

/* PyType_GetModuleByDef is only in Python 3.11+ */
static PyObject *
MyType_GetModuleByDef(PyTypeObject *type, PyModuleDef *def)
{
    assert(PyType_Check(type));
    PyObject *m = PyType_GetModule(type);
    if (m && PyModule_GetDef(m) == def) {
	return m;
    }
    if (!m && PyErr_Occurred()) {
	if (PyErr_ExceptionMatches(PyExc_TypeError)) {
	    PyErr_Clear();
	} else {
	    return NULL;
	}
    }

    PyObject *mro = PyObject_CallMethod((PyObject*)type, "mro", "");
    if (!mro) {
	return NULL;
    }
    size_t size = PyList_Size(mro);
    for (Py_ssize_t i = 1; i < size; i++) {
        PyObject *super = PyList_GetItem(mro, i); // borrowed reference
	if (!super) {
	    Py_DECREF(mro);
	    return NULL;
	}
	m = PyType_GetModule((PyTypeObject*)super);
	if (m && PyModule_GetDef(m) == def) {
	    Py_DECREF(mro);
	    return m;
	}
	if (!m && PyErr_Occurred()) {
	    if (PyErr_ExceptionMatches(PyExc_TypeError)) {
		PyErr_Clear();
	    } else {
		Py_DECREF(mro);
		return NULL;
	    }
	}
    }

    PyErr_SetString(PyExc_SystemError, "could not get _rpm module state");
    Py_DECREF(mro);
    return NULL;
}

rpmmodule_state_t *rpmModState_FromType(PyTypeObject *t) {
    PyObject *mod = MyType_GetModuleByDef(t, &moduledef);
    if (!mod) {
	return NULL;
    }
    return rpmModState_FromModule(mod);
}

rpmmodule_state_t *rpmModState_FromObject(PyObject *s) {
    return rpmModState_FromType(Py_TYPE(s));
}
