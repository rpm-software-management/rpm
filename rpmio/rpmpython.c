#include "system.h"

#include <dlfcn.h>
#include <rpm/rpmlog.h>

#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>

#include "debug.h"

#include "python/rpmpython.h"

int _rpmpython_debug = 0;

rpmpython _rpmpythonI = NULL;

static int _dlopened = 0;
static rpmpython (*rpmpythonFree_p) (rpmpython python);
static rpmpython (*rpmpythonNew_p) (ARGV_t * av, uint32_t flags);
static rpmRC (*rpmpythonRunFile_p) (rpmpython python, const char * fn, char ** resultp);
static rpmRC (*rpmpythonRun_p) (rpmpython python, const char * str, char ** resultp);

static void loadModule(void) {
    char *rpmpython = rpmExpand("%{?rpmpython_modpath}%{?!rpmpython_modpath:%{python_sitearch}/rpm}/_rpm.so", NULL);
    void *h;

    h = dlopen (rpmpython, RTLD_NOW|RTLD_GLOBAL);
    if (!h)
    {
	rpmlog(RPMLOG_WARNING, "Unable to open \"%s\" (%s), "
		    "embedded python will not be available\n",
		rpmpython, dlerror());
    }
    else if(!((rpmpythonNew_p = dlsym(h, "rpmpythonNew"))
		&& (rpmpythonFree_p = dlsym(h, "rpmpythonFree"))
		&& (rpmpythonRunFile_p = dlsym(h, "rpmpythonRunFile"))
		&& (rpmpythonRun_p = dlsym(h, "rpmpythonRun")))) {
	rpmlog(RPMLOG_WARNING, "Opened library \"%s\" is incompatible (%s), "
		    "embedded python will not be available\n",
		rpmpython, dlerror());
	if (dlclose (h))
	    rpmlog(RPMLOG_WARNING, "Error closing library \"%s\": %s", rpmpython,
		    dlerror());
    } else
	_dlopened = 1;
    free(rpmpython);
}

rpmpython rpmpythonFree(rpmpython python)
{
    if (_dlopened) return rpmpythonFree_p(python);
    else return NULL;
}


rpmpython rpmpythonNew(ARGV_t * argvp, uint32_t flags)
{
    rpmpython python = NULL;
    if (!_dlopened) loadModule();
    if (_dlopened) python = rpmpythonNew_p(argvp, flags);
    return python;
}

rpmRC rpmpythonRunFile(rpmpython python, const char * fn, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
    if (_dlopened) rc = rpmpythonRunFile_p(python, fn, resultp);
    return rc;
}

rpmRC rpmpythonRun(rpmpython python, const char * str, char **resultp)
{
    rpmRC rc = RPMRC_FAIL;
    if (_dlopened) return rpmpythonRun_p(python, str, resultp);
    return rc;
}

