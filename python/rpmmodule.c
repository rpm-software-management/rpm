/** \ingroup py_c
 * \file python/rpmmodule.c
 */

#include "system.h"

#include <rpmio_internal.h>
#include <rpmcli.h>	/* XXX for rpmCheckSig */
#include <rpmdb.h>

#include "legacy.h"
#include "misc.h"
#include "header_internal.h"

#include "header-py.h"
#include "rpmal-py.h"
#include "rpmds-py.h"
#include "rpmfd-py.h"
#include "rpmfts-py.h"
#include "rpmfi-py.h"
#include "rpmmi-py.h"
#include "rpmrc-py.h"
#include "rpmte-py.h"
#include "rpmts-py.h"
#include "spec-py.h"

#include "debug.h"

#ifdef __LCLINT__
#undef	PyObject_HEAD
#define	PyObject_HEAD	int _PyObjectHead
#endif

/** \ingroup python
 * \name Module: rpm
 */
/*@{*/

/**
 */
static PyObject * archScore(PyObject * self, PyObject * args)
{
    char * arch;
    int score;

    if (!PyArg_ParseTuple(args, "s", &arch))
	return NULL;

    score = rpmMachineScore(RPM_MACHTABLE_INSTARCH, arch);

    return Py_BuildValue("i", score);
}

/**
 */
static PyObject * setLogFile (PyObject * self, PyObject * args)
{
    PyObject * fop = NULL;
    FILE * fp = NULL;

    if (!PyArg_ParseTuple(args, "|O:logSetFile", &fop))
	return NULL;

    if (fop) {
	if (!PyFile_Check(fop)) {
	    PyErr_SetString(pyrpmError, "requires file object");
	    return NULL;
	}
	fp = PyFile_AsFile(fop);
    }

    (void) rpmlogSetFile(fp);

    Py_INCREF(Py_None);
    return (PyObject *) Py_None;
}

/**
 */
static PyObject * setVerbosity (PyObject * self, PyObject * args)
{
    int level;

    if (!PyArg_ParseTuple(args, "i", &level))
	return NULL;

    rpmSetVerbosity(level);

    Py_INCREF(Py_None);
    return (PyObject *) Py_None;
}

/**
 */
static PyObject * setEpochPromote (PyObject * self, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "i", &_rpmds_nopromote))
	return NULL;
    Py_INCREF(Py_None);
    return (PyObject *) Py_None;
}

/**
 */
static PyObject * setStats (PyObject * self, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "i", &_rpmts_stats))
	return NULL;
    Py_INCREF(Py_None);
    return (PyObject *) Py_None;
}

/**
 */
static PyMethodDef rpmModuleMethods[] = {
    { "TransactionSet", (PyCFunction) rpmts_Create, METH_VARARGS,
"rpm.TransactionSet([rootDir, [db]]) -> ts\n\
- Create a transaction set.\n" },

#if Py_TPFLAGS_HAVE_ITER        /* XXX backport to python-1.5.2 */
    { "newrc", (PyCFunction) rpmrc_Create, METH_VARARGS|METH_KEYWORDS,
	NULL },
#endif
    { "addMacro", (PyCFunction) rpmrc_AddMacro, METH_VARARGS,
	NULL },
    { "delMacro", (PyCFunction) rpmrc_DelMacro, METH_VARARGS,
	NULL },

    { "archscore", (PyCFunction) archScore, METH_VARARGS,
	NULL },

    { "headerLoad", (PyCFunction) hdrLoad, METH_VARARGS,
	NULL },
    { "rhnLoad", (PyCFunction) rhnLoad, METH_VARARGS,
	NULL },
    { "mergeHeaderListFromFD", (PyCFunction) rpmMergeHeadersFromFD, METH_VARARGS,
	NULL },
    { "readHeaderListFromFD", (PyCFunction) rpmHeaderFromFD, METH_VARARGS,
	NULL },
    { "readHeaderListFromFile", (PyCFunction) rpmHeaderFromFile, METH_VARARGS,
	NULL },
    { "readHeaderFromFD", (PyCFunction) rpmSingleHeaderFromFD, METH_VARARGS,
	NULL },

    { "setLogFile", (PyCFunction) setLogFile, METH_VARARGS,
	NULL },

    { "versionCompare", (PyCFunction) versionCompare, METH_VARARGS,
	NULL },
    { "labelCompare", (PyCFunction) labelCompare, METH_VARARGS,
	NULL },
    { "setVerbosity", (PyCFunction) setVerbosity, METH_VARARGS,
	NULL },
    { "setEpochPromote", (PyCFunction) setEpochPromote, METH_VARARGS,
	NULL },
    { "setStats", (PyCFunction) setStats, METH_VARARGS,
	NULL },

    { "dsSingle", (PyCFunction) rpmds_Single, METH_VARARGS,
"rpm.dsSingle(TagN, N, [EVR, [Flags]] -> ds\n\
- Create a single element dependency set.\n" },
    { NULL }
} ;

/**
 */
static char rpm__doc__[] =
"";

void initrpm(void);	/* XXX eliminate gcc warning */
/**
 */
void initrpm(void)
{
    PyObject * d, *o, * tag = NULL, * dict;
    int i;
    const struct headerSprintfExtension_s * extensions = rpmHeaderFormats;
    struct headerSprintfExtension_s * ext;
    PyObject * m;

#if Py_TPFLAGS_HAVE_ITER        /* XXX backport to python-1.5.2 */
    if (PyType_Ready(&hdr_Type) < 0) return;
    if (PyType_Ready(&rpmal_Type) < 0) return;
    if (PyType_Ready(&rpmds_Type) < 0) return;
    if (PyType_Ready(&rpmfd_Type) < 0) return;
    if (PyType_Ready(&rpmfts_Type) < 0) return;
    if (PyType_Ready(&rpmfi_Type) < 0) return;
    if (PyType_Ready(&rpmmi_Type) < 0) return;

    rpmrc_Type.tp_base = &PyDict_Type;
    if (PyType_Ready(&rpmrc_Type) < 0) return;

    if (PyType_Ready(&rpmte_Type) < 0) return;
    if (PyType_Ready(&rpmts_Type) < 0) return;
    if (PyType_Ready(&spec_Type) < 0) return;
#endif

    m = Py_InitModule3("rpm", rpmModuleMethods, rpm__doc__);
    if (m == NULL)
	return;

    rpmReadConfigFiles(NULL, NULL);

    d = PyModule_GetDict(m);

#ifdef	HACK
    pyrpmError = PyString_FromString("rpm.error");
    PyDict_SetItemString(d, "error", pyrpmError);
    Py_DECREF(pyrpmError);
#else
    pyrpmError = PyErr_NewException("rpm.error", NULL, NULL);
    if (pyrpmError != NULL)
	PyDict_SetItemString(d, "error", pyrpmError);
#endif

#if Py_TPFLAGS_HAVE_ITER        /* XXX backport to python-1.5.2 */
    Py_INCREF(&hdr_Type);
    PyModule_AddObject(m, "hdr", (PyObject *) &hdr_Type);

    Py_INCREF(&rpmal_Type);
    PyModule_AddObject(m, "al", (PyObject *) &rpmal_Type);

    Py_INCREF(&rpmds_Type);
    PyModule_AddObject(m, "ds", (PyObject *) &rpmds_Type);

    Py_INCREF(&rpmfd_Type);
    PyModule_AddObject(m, "fd", (PyObject *) &rpmfd_Type);

    Py_INCREF(&rpmfts_Type);
    PyModule_AddObject(m, "fts", (PyObject *) &rpmfts_Type);

    Py_INCREF(&rpmfi_Type);
    PyModule_AddObject(m, "fi", (PyObject *) &rpmfi_Type);

    Py_INCREF(&rpmmi_Type);
    PyModule_AddObject(m, "mi", (PyObject *) &rpmmi_Type);

    Py_INCREF(&rpmrc_Type);
    PyModule_AddObject(m, "rc", (PyObject *) &rpmrc_Type);

    Py_INCREF(&rpmte_Type);
    PyModule_AddObject(m, "te", (PyObject *) &rpmte_Type);

    Py_INCREF(&rpmts_Type);
    PyModule_AddObject(m, "ts", (PyObject *) &rpmts_Type);

    Py_INCREF(&spec_Type);
    PyModule_AddObject(m, "spec", (PyObject *) &spec_Type);
#else
    hdr_Type.ob_type = &PyType_Type;
    rpmal_Type.ob_type = &PyType_Type;
    rpmds_Type.ob_type = &PyType_Type;
    rpmfd_Type.ob_type = &PyType_Type;
    rpmfts_Type.ob_type = &PyType_Type;
    rpmfi_Type.ob_type = &PyType_Type;
    rpmmi_Type.ob_type = &PyType_Type;
    rpmte_Type.ob_type = &PyType_Type;
    rpmts_Type.ob_type = &PyType_Type;
    spec_Type.ob_type =  &PyType_Type;
#endif

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
    REGISTER_ENUM(RPMFILE_STATE_WRONGCOLOR);

    REGISTER_ENUM(RPMFILE_CONFIG);
    REGISTER_ENUM(RPMFILE_DOC);
    REGISTER_ENUM(RPMFILE_ICON);
    REGISTER_ENUM(RPMFILE_MISSINGOK);
    REGISTER_ENUM(RPMFILE_NOREPLACE);
    REGISTER_ENUM(RPMFILE_GHOST);
    REGISTER_ENUM(RPMFILE_LICENSE);
    REGISTER_ENUM(RPMFILE_README);
    REGISTER_ENUM(RPMFILE_EXCLUDE);
    REGISTER_ENUM(RPMFILE_UNPATCHED);
    REGISTER_ENUM(RPMFILE_PUBKEY);

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
    REGISTER_ENUM(RPMSENSE_SCRIPT_PREP);
    REGISTER_ENUM(RPMSENSE_SCRIPT_BUILD);
    REGISTER_ENUM(RPMSENSE_SCRIPT_INSTALL);
    REGISTER_ENUM(RPMSENSE_SCRIPT_CLEAN);
    REGISTER_ENUM(RPMSENSE_RPMLIB);
    REGISTER_ENUM(RPMSENSE_TRIGGERPREIN);
    REGISTER_ENUM(RPMSENSE_KEYRING);
    REGISTER_ENUM(RPMSENSE_PATCHES);
    REGISTER_ENUM(RPMSENSE_CONFIG);

    REGISTER_ENUM(RPMTRANS_FLAG_TEST);
    REGISTER_ENUM(RPMTRANS_FLAG_BUILD_PROBS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSCRIPTS);
    REGISTER_ENUM(RPMTRANS_FLAG_JUSTDB);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERS);
    REGISTER_ENUM(RPMTRANS_FLAG_NODOCS);
    REGISTER_ENUM(RPMTRANS_FLAG_ALLFILES);
    REGISTER_ENUM(RPMTRANS_FLAG_KEEPOBSOLETE);
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
    REGISTER_ENUM(RPMTRANS_FLAG_ANACONDA);
    REGISTER_ENUM(RPMTRANS_FLAG_NOMD5);
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

    REGISTER_ENUM(RPMAL_NOMATCH);
}

/*@}*/
