#include "system.h"

#if defined(MODULE_EMBED)
#include <Python.h>
#if PY_VERSION_HEX < 0x03050000 && PY_VERSION_HEX >= 0x03000000
#include <fileutils.h>
#define Py_DecodeLocale _Py_char2wchar
#endif
#endif

#if defined(WITH_PYTHONEMBED) && !defined(MODULE_EMBED)
#include <dlfcn.h>
#include <rpm/rpmlog.h>
#endif

#include <rpm/rpmsw.h>
#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>
#include "rpmio_internal.h"

#include "debug.h"

#include "rpmpython.h"

#if defined(MODULE_EMBED)
struct rpmpython_s {
    PyThreadState * I;
    char *(*nextFileFunc)(void *);
    void *nextFileFuncParam;
};
#endif

int _rpmpython_debug = 0;

rpmpython _rpmpythonI = NULL;

#if defined(WITH_PYTHONEMBED)
#if !defined(MODULE_EMBED)
static int _dlopened = 0;
static rpmpython (*rpmpythonFree_p) (rpmpython python);
static rpmpython (*rpmpythonNew_p) (ARGV_t * av, uint32_t flags);
static rpmRC (*rpmpythonRunFile_p) (rpmpython python, const char * fn, char ** resultp);
static rpmRC (*rpmpythonRun_p) (rpmpython python, const char * str, char ** resultp);
static void (*rpmpythonSetNextFileFunc_p) (rpmpython python, char *(*func)(void *), void *funcParam);

static void loadModule(void) {
    static const char librpmpython[] = "rpmpython.so";
    void *h;

    h = dlopen (librpmpython, RTLD_NOW|RTLD_GLOBAL);
    if (!h)
    {
	rpmlog(RPMLOG_WARNING, "Unable to open \"%s\" (%s), "
		    "embedded python will not be available\n",
		librpmpython, dlerror());
    }
    else if(!((rpmpythonNew_p = dlsym(h, "rpmpythonNew"))
		&& (rpmpythonFree_p = dlsym(h, "rpmpythonFree"))
		&& (rpmpythonRunFile_p = dlsym(h, "rpmpythonRunFile"))
		&& (rpmpythonSetNextFileFunc_p = dlsym(h, "rpmpythonSetNextFileFunc"))
		&& (rpmpythonRun_p = dlsym(h, "rpmpythonRun")))) {
	rpmlog(RPMLOG_WARNING, "Opened library \"%s\" is incompatible (%s), "
		    "embedded python will not be available\n",
		librpmpython, dlerror());
	if (dlclose (h))
	    rpmlog(RPMLOG_WARNING, "Error closing library \"%s\": %s", librpmpython,
		    dlerror());
    } else
	_dlopened = 1;
}
#endif
#endif

rpmpython rpmpythonFree(rpmpython python)
{
#if defined(WITH_PYTHONEMBED)
#if !defined(MODULE_EMBED)
    if (_dlopened) return rpmpythonFree_p(python);
    else return NULL;
#else
    if (python == NULL) python = _rpmpythonI;

    if (python->I != _rpmpythonI->I) {
	Py_EndInterpreter(python->I);
    } else {
	Py_Finalize();
	_rpmpythonI = NULL;
    }
    python = _free(python);
#endif
#endif
    return python;
}

#if defined(MODULE_EMBED)
#if PY_VERSION_HEX >= 0x03000000
static const char _rpmpythonI_init[] =	"from io import StringIO;"
					"sys.stdout = StringIO();";
#else
static const char _rpmpythonI_init[] =	"from cStringIO import StringIO;"
					"sys.stdout = StringIO();";
#endif
#endif

rpmpython rpmpythonNew(ARGV_t * argvp, uint32_t flags)
{
    rpmpython python = NULL;
#if defined(WITH_PYTHONEMBED)
#if !defined(MODULE_EMBED)
    if (!_dlopened) loadModule();
    if (_dlopened) python = rpmpythonNew_p(argvp, flags);
#else
    ARGV_t argv = argvp ? *argvp : NULL;

    if (_rpmpythonI == NULL)
	_rpmpythonI = xcalloc(1, sizeof(rpmpython));

    python = (flags & RPMPYTHON_GLOBAL_INTERP)
	? _rpmpythonI : xcalloc(1, sizeof(rpmpython));

    if (!Py_IsInitialized()) {
	Py_Initialize();
	_rpmpythonI->I = PyThreadState_Get();
    }

    if (!(flags & RPMPYTHON_GLOBAL_INTERP))
	python->I = Py_NewInterpreter();

    PyThreadState_Swap(python->I);

    if (!(flags & RPMPYTHON_NO_INIT)) {
	int ac = argvCount((ARGV_t)argv);
#if PY_VERSION_HEX >= 0x03000000
	wchar_t ** wav = NULL;
#endif
	static const char _pythonI_init[] = "%{?_pythonI_init}";
	char * s = rpmExpand("import sys;", (flags & RPMPYTHON_NO_IO_REDIR) ? "" : _rpmpythonI_init, _pythonI_init, NULL);

	if (ac) {
#if PY_VERSION_HEX >= 0x03000000
	    wav = xmalloc(ac * sizeof(wchar_t*));
	    for (int i = 0; i < ac; i++)
		wav[i] = Py_DecodeLocale(argv[i], NULL);
	    PySys_SetArgvEx(ac, wav, 0);
#else
	    PySys_SetArgvEx(ac, (char **)argv, 0);
#endif
	}
	if (_rpmpython_debug)
	    fprintf(stderr, "==========\n%s\n==========\n", s);
	rpmpythonRun(python, s, NULL);
	s = _free(s);

#if PY_VERSION_HEX >= 0x03000000
	if(wav) {
	    for (int i = 0; i < ac; i++)
		free(wav[i]);
	    wav = _free(wav);
	}
#endif
    }
#endif
#endif
    return python;
}

rpmRC rpmpythonRunFile(rpmpython python, const char * fn, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
#if defined(WITH_PYTHONEMBED)
#if !defined(MODULE_EMBED)
    if (_dlopened) rc = rpmpythonRunFile_p(python, fn, resultp);
#else
if (_rpmpython_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, python, fn);

    if (python == NULL) python = _rpmpythonI;
    PyThreadState_Swap(python->I);

    if (fn != NULL) {
	const char * pyfn = ((fn == NULL || !strcmp(fn, "-")) ? "<stdin>" : fn);
	FILE * pyfp = (!strcmp(pyfn, "<stdin>") ? stdin : fopen(fn, "rb"));
	int closeit = (pyfp != stdin);
	PyCompilerFlags cf = { 0 };

	if (pyfp != NULL) {
	    PyRun_AnyFileExFlags(pyfp, pyfn, closeit, &cf);
	    rc = RPMRC_OK;
	}
    }
#endif
#endif
    return rc;
}

rpmRC rpmpythonRun(rpmpython python, const char * str, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
#if defined(WITH_PYTHONEMBED)
#if !defined(MODULE_EMBED)
    if (_dlopened) return rpmpythonRun_p(python, str, resultp);
#else
    struct stat sb;
    PyObject *fileList = NULL;
    const char *line = NULL;

if (_rpmpython_debug)
fprintf(stderr, "==> %s(%p,%s,%p)\n", __FUNCTION__, python, str, resultp);

    if (python == NULL) python = _rpmpythonI;
    PyThreadState_Swap(python->I);

    if (python->nextFileFunc) {
	fileList = PyList_New(0);
	while ((line = python->nextFileFunc(python->nextFileFuncParam)) != NULL) {
	    size_t size = strlen(line);
	    PyObject * fileName = PyUnicode_FromStringAndSize(line, size);
	    PyList_Append(fileList, fileName);
	    Py_XDECREF(fileName);
	}
	PyObject * modDict = PyImport_GetModuleDict();
	/* XXX: not really optimal, depends on rpm module being loaded... */
	PyObject * rpmString = PyUnicode_FromString("rpm");
	if (PyDict_Contains(modDict, rpmString)) {
	    PyObject * module = PyDict_GetItemString(modDict, "rpm");
	    PyModule_AddObject(module, "files", fileList);

	    Py_XDECREF(module);
	}
	Py_XDECREF(rpmString);
    }

    if (str != NULL) {
	if ((!strcmp(str, "-")) /* Macros from stdin arg. */
		|| ((str[0] == '/' || strchr(str, ' ') == NULL)
		    && !stat(str, &sb) && S_ISREG(sb.st_mode))) /* Macros from a file arg. */
	{
	    return rpmpythonRunFile(python, str, resultp);
	}
	char *val = xstrdup(str);
	PyCompilerFlags cf = { 0 };
	PyObject * m = PyImport_AddModule("__main__");
	PyObject * d = (m ? PyModule_GetDict(m) : NULL);
	PyObject * v = (m ? PyRun_StringFlags(val, Py_file_input, d, d, &cf) : NULL);

	if (v == NULL) {
	    PyErr_Print();
	} else {
	    PyObject * sys_stdout = PySys_GetObject("stdout");
	    if (sys_stdout != NULL) {
		if (resultp != NULL) {
		    PyObject * o = PyObject_CallMethod(sys_stdout, "getvalue", NULL);
#if PY_VERSION_HEX >= 0x03000000
		    *resultp = xstrdup((PyUnicode_Check(o) ? PyUnicode_AsUTF8(o) : ""));
#else
		    *resultp = xstrdup((PyString_Check(o) ? PyString_AsString(o) : ""));
#endif
		    PyObject_CallMethod(sys_stdout, "seek", "i",0);
		    PyObject_CallMethod(sys_stdout, "truncate", NULL);

		    Py_XDECREF(o);
		} 
	    }
	    if (!PyFile_WriteString("", sys_stdout))
		PyErr_Clear();
	}

	Py_XDECREF(v);
	Py_XDECREF(fileList);

	val = _free(val);
	rc = RPMRC_OK;
    }
#endif
#endif
    return rc;
}

void rpmpythonSetNextFileFunc(rpmpython python, char *(*func)(void *), void *funcParam)
{
#if defined(WITH_PYTHONEMBED)
#if !defined(MODULE_EMBED)
    rpmpythonSetNextFileFunc_p(python, func, funcParam);
#else
    python->nextFileFunc = func;
    python->nextFileFuncParam = funcParam;
#endif
#endif
}
