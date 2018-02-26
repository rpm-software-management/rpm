#include <stdbool.h>
#include <Python.h>
#if PY_VERSION_HEX < 0x03050000 && PY_VERSION_HEX >= 0x03000000
#include <fileutils.h>
#define Py_DecodeLocale _Py_char2wchar
#endif

#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpminterp.h>

extern int _rpminterp_debug;

#if PY_VERSION_HEX >= 0x03000000
static const char _rpmpythonI_init[] =	"from io import StringIO;"
					"sys.stdout = stdout = StringIO();";
#else
static const char _rpmpythonI_init[] =	"from cStringIO import StringIO;"
					"sys.stdout = stdout = StringIO();";
#endif

static bool initialized = false;
static bool underrpm = false;

static void rpmpythonFree(void);
static rpmRC rpmpythonInit(ARGV_t * av, uint32_t flags);
static rpmRC rpmpythonRunFile(const char * fn, char **resultp);
static rpmRC rpmpythonRun(const char * str, char **resultp);

static void rpmpythonFree(void)
{
    Py_Finalize();
}

static rpmRC rpmpythonInit(ARGV_t * argvp, rpminterpFlag flags)
{
    ARGV_t argv = argvp ? *argvp : NULL;
    rpmRC rc = RPMRC_OK;

if (_rpminterp_debug)
fprintf(stderr, "==> %s(initialized:%d, flags: 0x%.10x)\n", __FUNCTION__, initialized, flags);

    if (initialized)
	return rc;

    if (!Py_IsInitialized()) {
#if PY_VERSION_HEX >= 0x03000000
	Py_SetStandardStreamEncoding("UTF-8", "strict");
#endif
	Py_Initialize();
	underrpm = true;
    }

    if (!(flags & RPMINTERP_NO_INIT)) {
	int ac = argvCount((ARGV_t)argv);
#if PY_VERSION_HEX >= 0x03000000
	wchar_t ** wav = NULL;
#endif
	static const char _pythonI_init[] = "%{?_rpminterp_python_init}";
	char * s = rpmExpand("import sys;", (flags & RPMINTERP_NO_IO_REDIR) ? "" : _rpmpythonI_init, _pythonI_init, NULL);

	if (ac) {
#if PY_VERSION_HEX >= 0x03000000
	    wav = alloca(ac * sizeof(wchar_t*));
	    for (int i = 0; i < ac; i++)
		wav[i] = Py_DecodeLocale(argv[i], NULL);
	    PySys_SetArgvEx(ac, wav, 0);
#else
	    PySys_SetArgvEx(ac, (char **)argv, 0);
#endif
	}
	if (_rpminterp_debug)
	    fprintf(stderr, "==========\n%s\n==========\n", s);
	rc = rpmpythonRun(s, NULL);
	free(s);
    }

    initialized = true;

    return rc;
}

static rpmRC rpmpythonRunFile(const char * fn, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
if (_rpminterp_debug)
fprintf(stderr, "==> %s(%s)\n", __FUNCTION__, fn);

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

static rpmRC rpmpythonRun(const char * str, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
    struct stat sb;

if (_rpminterp_debug)
fprintf(stderr, "==> %s(%s,%p)\n", __FUNCTION__, str, resultp);

    if (str != NULL) {
	if ((!strcmp(str, "-")) /* Macros from stdin arg. */
		|| ((str[0] == '/' || strchr(str, ' ') == NULL)
		    && !stat(str, &sb) && S_ISREG(sb.st_mode))) /* Macros from a file arg. */
	{
	    return rpmpythonRunFile(str, resultp);
	}
	PyCompilerFlags cf = { 0 };
	PyObject * m = PyImport_AddModule("__main__");
	PyObject * d = (m ? PyModule_GetDict(m) : NULL);

	/* in order for %{python:...} to work from python itself, we need to
	 * switch back and forth between output redirection to StringIO buffer
	 * and stdout.
	 */
	if (!underrpm && resultp != NULL) {
	    PyObject * pre = (m ? PyRun_StringFlags("sys.stdout = stdout", Py_file_input, d, d, &cf) : NULL);
	    if (pre == NULL)
		PyErr_Print();
	    Py_XDECREF(pre);
	}
	PyObject * v = (m ? PyRun_StringFlags(str, Py_file_input, d, d, &cf) : NULL);

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

	if (!underrpm && resultp != NULL) {
	    PyObject * post = (m ? PyRun_StringFlags("sys.stdout = sys.__stdout__", Py_file_input, d, d, &cf) : NULL);
	    if (post == NULL)
		PyErr_Print();
	    Py_XDECREF(post);
	}
	Py_XDECREF(v);
	rc = RPMRC_OK;
    }

    return rc;
}

rpminterpInit(python, rpmpythonInit, rpmpythonFree, rpmpythonRun);
