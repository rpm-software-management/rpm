#ifdef ENABLE_PYTHON
#include <Python.h>
#if PY_VERSION_HEX < 0x03050000 && PY_VERSION_HEX >= 0x03000000
#include <fileutils.h>
#define Py_DecodeLocale _Py_char2wchar
#endif

#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>

#include "rpmpython.h"

extern int _rpmpython_debug;
extern rpmpython _rpmpythonI;

struct rpmpython_s {
    void * I; 	/* (unused) */
};

rpmpython rpmpythonFree(rpmpython python)
{
    if (python == NULL) python = _rpmpythonI;

    Py_Finalize();
    /* free(python->I); */
    _rpmpythonI = NULL;
    free(python);
    return NULL;
}

#if PY_VERSION_HEX >= 0x03000000
static const char _rpmpythonI_init[] =	"from io import StringIO;"
					"sys.stdout = StringIO();";
#else
static const char _rpmpythonI_init[] =	"from cStringIO import StringIO;"
					"sys.stdout = StringIO();";
#endif

rpmpython rpmpythonNew(ARGV_t * argvp, uint32_t flags)
{
    rpmpython python = NULL;
    ARGV_t argv = argvp ? *argvp : NULL;

    int initialize = (_rpmpythonI == NULL);
    python = initialize
       ? (_rpmpythonI = calloc(1, sizeof(rpmpython))) : _rpmpythonI;

    if (!Py_IsInitialized()) {
	Py_Initialize();
    }

    if (!(flags & RPMPYTHON_NO_INIT)) {
	int ac = argvCount((ARGV_t)argv);
#if PY_VERSION_HEX >= 0x03000000
	wchar_t ** wav = NULL;
#endif
	static const char _pythonI_init[] = "%{?_pythonI_init}";
	char * s = rpmExpand("import sys;", (flags & RPMPYTHON_NO_IO_REDIR) ? "" : _rpmpythonI_init, _pythonI_init, NULL);

	if (ac) {
#if PY_VERSION_HEX >= 0x03000000
	    wav = malloc(ac * sizeof(wchar_t*));
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
	free(s);

#if PY_VERSION_HEX >= 0x03000000
	if(wav) {
	    for (int i = 0; i < ac; i++)
		free(wav[i]);
	    free(wav);
	}
#endif
    }
    return python;
}

rpmRC rpmpythonRunFile(rpmpython python, const char * fn, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
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
    return rc;
}

rpmRC rpmpythonRun(rpmpython python, const char * str, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
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
	char *val = strdup(str);
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
		    *resultp = strdup((PyUnicode_Check(o) ? PyUnicode_AsUTF8(o) : ""));
#else
		    *resultp = strdup((PyString_Check(o) ? PyString_AsString(o) : ""));
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

	free(val);
	rc = RPMRC_OK;
    }
    return rc;
}
#endif
