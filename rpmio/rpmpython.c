#include "system.h"

#define _RPMPYTHON_INTERNAL
#include "rpmpython.h"

#if defined(MODULE_EMBED)
#include <Python.h>
#if PY_VERSION_HEX < 0x03050000
#include <fileutils.h>
#define Py_DecodeLocale _Py_char2wchar
#endif
#undef WITH_PYTHONEMBED
#endif

#if defined(WITH_PYTHONEMBED)
#include <dlfcn.h>
#include <rpm/rpmlog.h>
#endif

#include <rpm/rpmsw.h>
#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>
#include "rpmio_internal.h"
#include "argv.h"

#include "debug.h"

int _rpmpython_debug = 0;

rpmpython _rpmpythonI = NULL;

#if defined(WITH_PYTHONEMBED)
static int _dlopened = 0;
static rpmpython (*rpmpythonNew_p) (char ** av);
static rpmRC (*rpmpythonRunFile_p) (rpmpython python, const char * fn, char ** resultp);
static rpmRC (*rpmpythonRun_p) (rpmpython python, const char * str, char ** resultp);
#endif

rpmpython rpmpythonFree(rpmpython python)
{
#if defined(MODULE_EMBED)
    Py_Finalize();
#endif
    //python->I = _free(python->I);
    python = _free(python);
    return python;
}

#if defined(MODULE_EMBED)
static const char _rpmpythonI_init[] =	"import sys;"
					"from io import StringIO;"
					"sys.stdout = StringIO();\n";
#endif

#if defined(WITH_PYTHONEMBED)
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

rpmpython rpmpythonNew(char ** av)
{
    rpmpython python = NULL;
#if defined(WITH_PYTHONEMBED)
    if (!_dlopened) loadModule();
    if (_dlopened) python = rpmpythonNew_p(av);
#else
    static const wchar_t *_wav[] = { L"python", NULL };
    int initialize = (_rpmpythonI == NULL);
    python = initialize
	? (_rpmpythonI = xcalloc(1, sizeof(rpmpython))) : _rpmpythonI;

    if (!Py_IsInitialized()) {
	Py_SetProgramName((wchar_t*)_wav[0]);
	Py_Initialize();
    }

    if (initialize) {
	int ac = argvCount((ARGV_t)av);
	wchar_t ** wav = NULL;
	static const char _pythonI_init[] = "%{?_pythonI_init}";
	char * s = rpmExpand(_rpmpythonI_init, _pythonI_init, NULL);

	if (av) {
	    wav = xmalloc(ac * sizeof(wchar_t*));
	    for (int i = 0; i < ac; i++)
		wav[i] = Py_DecodeLocale(av[i], NULL);
	}
	(void) PySys_SetArgvEx(ac, wav ? wav : (wchar_t**)_wav, 0);
	if (_rpmpython_debug)
	    fprintf(stderr, "==========\n%s\n==========\n", s);
	(void) rpmpythonRun(python, s, NULL);
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
