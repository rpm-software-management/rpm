/** \ingroup python
 * \file python/rpmmodule.c
 */

#include "system.h"

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

#include "rpmdb.h"
#include "rpmps.h"

#define	_RPMTS_INTERNAL	/* XXX for ts->rdb */
#include "rpmts.h"

#include "legacy.h"
#include "misc.h"
#include "header_internal.h"
#include "upgrade.h"

#include "db-py.h"
#include "header-py.h"
#include "rpmds-py.h"
#include "rpmfi-py.h"
#include "rpmts-py.h"

#include "debug.h"

extern int _rpmio_debug;

#ifdef __LCLINT__
#undef	PyObject_HEAD
#define	PyObject_HEAD	int _PyObjectHead
#endif

/* from lib/misc.c */
int rpmvercmp(const char * one, const char * two);

/** \ingroup python
 * \name Module: rpm
 */
/*@{*/

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
	if (((PyObject *) hdr)->ob_type != &hdr_Type) {
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
	rpmts ts;
	const char * av[2];
	QVA_t ka = memset(alloca(sizeof(*ka)), 0, sizeof(*ka));

	av[0] = filename;
	av[1] = NULL;
	ka->qva_mode = 'K';
	ka->qva_flags = (VERIFY_DIGEST|VERIFY_SIGNATURE);
	ka->sign = 0;
	ka->passPhrase = NULL;
	ts = rpmtsCreate();
	rc = rpmcliSign(ts, ka, av);
	rpmtsFree(ts);
    }
    return Py_BuildValue("i", rc);
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
    { "TransactionSet", (PyCFunction) rpmts_Create, METH_VARARGS, NULL },
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
    { "Fopen", (PyCFunction) doFopen, METH_VARARGS, NULL },
    { "setVerbosity", (PyCFunction) setVerbosity, METH_VARARGS, NULL },
    { "rpmdsSingle", (PyCFunction) rpmds_Single, METH_VARARGS, NULL },
    { NULL }
} ;

void initrpm(void);	/* XXX eliminate gcc warning */
/**
 */
void initrpm(void) {
    PyObject * d, *o, * tag = NULL, * dict;
    int i;
    const struct headerSprintfExtension_s * extensions = rpmHeaderFormats;
    struct headerSprintfExtension_s * ext;
    PyObject * m = Py_InitModule("rpm", rpmModuleMethods);

    hdr_Type.ob_type = &PyType_Type;
    rpmds_Type.ob_type = &PyType_Type;
    rpmfi_Type.ob_type = &PyType_Type;

    rpmdbMIType.ob_type = &PyType_Type;
    rpmdbType.ob_type = &PyType_Type;

    rpmts_Type.ob_type = &PyType_Type;

    if (m == NULL)
	return;

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
