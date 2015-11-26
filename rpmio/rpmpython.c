#include "system.h"

#if defined(MODULE_EMBED)
#include <Python.h>
#if PY_VERSION_HEX < 0x03050000
#include <fileutils.h>
#define Py_DecodeLocale _Py_char2wchar
#endif
#undef WITH_PYTHONEMBED
#endif

#if defined(MODULE_EMBED)
#define _RPMPYTHON_INTERNAL
#endif
#include "rpmpython.h"


#if defined(WITH_PYTHONEMBED)
#include <dlfcn.h>
#include <rpm/rpmlog.h>
#endif

#include <rpm/rpmsw.h>
#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>
#include "rpmio_internal.h"

#include "debug.h"

int _rpmpython_debug = 0;

rpmpython _rpmpythonI = NULL;

#if defined(WITH_PYTHONEMBED)
static int _dlopened = 0;
static rpmpython (*rpmpythonFree_p) (rpmpython python);
static rpmpython (*rpmpythonNew_p) (ARGV_t * av, uint32_t flags);
static rpmRC (*rpmpythonRunFile_p) (rpmpython python, const char * fn, char ** resultp);
static rpmRC (*rpmpythonRun_p) (rpmpython python, const char * str, char ** resultp);

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

rpmpython rpmpythonFree(rpmpython python)
{
#if defined(WITH_PYTHONEMBED)
    if (_dlopened) return rpmpythonFree_p(python);
    else return NULL;
#endif
#if defined(MODULE_EMBED)
    if (python == NULL) python = _rpmpythonI;

    if (python->I != _rpmpythonI->I) {
	Py_EndInterpreter(python->I);
    } else {
	Py_Finalize();
	_rpmpythonI = NULL;
    }
    python = _free(python);
#endif
    return python;
}

#if defined(MODULE_EMBED)
static const char _rpmpythonI_init[] =	"from io import StringIO;"
					"sys.stdout = StringIO();\n";

#endif

#if defined(WITH_PYTHONEMBED)

#endif

rpmpython rpmpythonNew(ARGV_t * argvp, uint32_t flags)
{
    rpmpython python = NULL;
#if defined(WITH_PYTHONEMBED)
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
	wchar_t ** wav = NULL;
	static const char _pythonI_init[] = "%{?_pythonI_init}";
	char * s = rpmExpand("import sys;", (flags & RPMPYTHON_NO_IO_REDIR) ? "" : _rpmpythonI_init, _pythonI_init, NULL);

	if (ac) {
	    wav = xmalloc(ac * sizeof(wchar_t*));
	    for (int i = 0; i < ac; i++)
		wav[i] = Py_DecodeLocale(argv[i], NULL);
	    PySys_SetArgvEx(ac, wav, 0);
	}
	if (_rpmpython_debug)
	    fprintf(stderr, "==========\n%s\n==========\n", s);
	rpmpythonRun(python, s, NULL);
	s = _free(s);

	if(wav) {
	    for (int i = 0; i < ac; i++)
		free(wav[i]);
	    wav = _free(wav);
	}
    }

#endif
    return python;
}

rpmRC rpmpythonRunFile(rpmpython python, const char * fn, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
#if defined(WITH_PYTHONEMBED)
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
    return rc;
}

rpmRC rpmpythonRun(rpmpython python, const char * str, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
#if defined(WITH_PYTHONEMBED)
    if (_dlopened) return rpmpythonRun_p(python, str, resultp);
#else
    struct stat sb;

if (_rpmpython_debug)
fprintf(stderr, "==> %s(%p,%s,%p)\n", __FUNCTION__, python, str, resultp);

    if (python == NULL) python = _rpmpythonI;
    PyThreadState_Swap(python->I);

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
	PyObject * v = (m ? PyRun_StringFlags(val, Py_single_input, d, d, &cf) : NULL);

	if (v == NULL) {
	    PyErr_Print();
	} else {
	    PyObject * sys_stdout = PySys_GetObject("stdout");
	    if (sys_stdout != NULL) {
		if (resultp != NULL) {
		    PyObject * o = PyObject_CallMethod(sys_stdout, "getvalue", NULL);

		    *resultp = xstrdup((PyUnicode_Check(o) ? PyUnicode_AsUTF8(o) : ""));
		    PyObject_CallMethod(sys_stdout, "seek", "i",0);
		    PyObject_CallMethod(sys_stdout, "truncate", NULL);

		    Py_XDECREF(o);
		} 
	    }
	    if (!PyFile_WriteString("", sys_stdout))
		PyErr_Clear();
	}

	Py_XDECREF(v);

	val = _free(val);
	rc = RPMRC_OK;
    }
#endif
    return rc;
}
