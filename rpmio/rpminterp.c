#include "system.h"

#include <dlfcn.h>
#include <rpm/rpmlog.h>

#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpminterp.h>

#include "debug.h"

int _rpminterp_debug = 0;

struct rpminterp_handle_s {
	rpminterp interp;
	void *h;
};

static void rpminterpFree(int argc, void *p) {
    struct rpminterp_handle_s *handle = p;

    if (handle->interp->free != NULL)
	handle->interp->free();
    if (handle->h && dlclose(handle->h))
	rpmlog(RPMLOG_WARNING, "Error closing rpminterp \"%s\": %s", handle->interp->name, dlerror());

    free(handle);

}

rpminterp rpminterpLoad(const char* name, const char *modpath) {
    char buf[64];
    struct rpminterp_s *interp = NULL;
    void *h = NULL;
    Dl_info info;
    ARGV_t files = NULL;
    rpmRC rc = RPMRC_FAIL;
    rpminterpFlag flags = RPMINTERP_DEFAULT;

    snprintf(buf, sizeof(buf), "%%_rpminterp_%s_flags", name);

    flags = rpmExpandNumeric(buf);

    snprintf(buf, sizeof(buf), "rpminterp_%s", name);

    if (!(interp = dlsym(RTLD_DEFAULT, buf))) {

	if (_rpminterp_debug)
	    rpmlog(RPMLOG_DEBUG, " %8s (modpath) %s\n", __func__, modpath);

	if (rpmGlob(modpath, NULL, &files) != 0) {
	    rpmlog(RPMLOG_ERR, "\"%s\" does not exist, "
		    "embedded %s will not be available\n",
		    modpath, name);
	} else if (!(h = dlopen((modpath = *files), RTLD_LAZY|RTLD_GLOBAL|RTLD_DEEPBIND))) {
	    rpmlog(RPMLOG_ERR, "Unable to open \"%s\" (%s), "
		    "embedded %s will not be available\n",
		    modpath, dlerror(), name);
	} else if (!(interp = dlsym(h, buf))) {
	    rpmlog(RPMLOG_ERR, "Opened library \"%s\" is incompatible (%s), "
		    "embedded %s will not be available\n",
		    modpath, dlerror(), name);
	} else if (dladdr(interp->init, &info) && strcmp(modpath, info.dli_fname)) {
	    rpmlog(RPMLOG_ERR, "\"%s\" lacks %s interpreter support, "
		    "embedding of will not be available\n",
		    modpath, name);
	}
    }

    if (interp != NULL) {
	if (interp->init != NULL) {
	    struct rpminterp_handle_s *handle = malloc(sizeof(*handle));
	    handle->interp = interp;
	    handle->h = h;
	    if ((rc = interp->init(NULL, flags)) != RPMRC_OK) {
		rpmlog(RPMLOG_ERR, "%s->init() != RPMRC_OK: %d\n",
			buf, rc);
		rpminterpFree(0, handle);
	    }
	    else
		on_exit(rpminterpFree, handle);
	} else
	    rc = RPMRC_OK;
    } else if (h && dlclose(h)) {
	    rpmlog(RPMLOG_WARNING, "Error closing library \"%s\": %s", modpath,
		    dlerror());
    }

    if (_rpminterp_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (_rpminterp_%s_modpath, %d) %s\n", __func__, name, rc, modpath);

    if (rc != RPMRC_OK)
	interp = NULL;

    argvFree(files);

    return interp;
}
