#include "system.h"

#ifdef ENABLE_PYTHON

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
    char *pattern = rpmExpand("%{?rpmpython_modpath}%{?!rpmpython_modpath:%{python_sitearch}/rpm}/_rpm*.so", NULL);
    const char *librpmpython = pattern;
    void *h = NULL;
    Dl_info info;
    ARGV_t files = NULL;

    if (_rpmpython_debug)
        rpmlog(RPMLOG_DEBUG, " %8s (pattern) %s\n", __func__, pattern);


    /* Python 3 has additional suffix in front of just .so for extensions
     * if built with distutils (setup.py), while using .so if built with
     * make, so need to support both */
    if (rpmGlob(librpmpython, NULL, &files) != 0) {
	rpmlog(RPMLOG_WARNING, "\"%s\" does not exist, "
		    "embedded python will not be available\n",
		librpmpython);
    } else if (!(h = dlopen((librpmpython = *files), RTLD_NOW|RTLD_GLOBAL|RTLD_DEEPBIND))) {
	rpmlog(RPMLOG_WARNING, "Unable to open \"%s\" (%s), "
		    "embedded python will not be available\n",
		librpmpython, dlerror());
    } else if (!((rpmpythonNew_p = dlsym(h, "rpmpythonNew"))
		&& (rpmpythonFree_p = dlsym(h, "rpmpythonFree"))
		&& (rpmpythonRunFile_p = dlsym(h, "rpmpythonRunFile"))
		&& (rpmpythonRun_p = dlsym(h, "rpmpythonRun")))) {
	rpmlog(RPMLOG_WARNING, "Opened library \"%s\" is incompatible (%s), "
		    "embedded python will not be available\n",
		librpmpython, dlerror());
    } else if (dladdr(rpmpythonNew_p, &info) && strcmp(librpmpython, info.dli_fname)) {
	rpmlog(RPMLOG_WARNING, "\"%s\" lacks rpmpython interpreter support, "
		    "embedded python will not be available\n",
		librpmpython);
    } else
	_dlopened = 1;

    if (_rpmpython_debug)
        rpmlog(RPMLOG_DEBUG, " %8s (librpmpython, %d) %s\n", __func__, _dlopened, librpmpython);

    if (h && !_dlopened && dlclose (h))
	    rpmlog(RPMLOG_WARNING, "Error closing library \"%s\": %s", librpmpython,
		    dlerror());

    argvFree(files);
    free(pattern);
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
#endif
