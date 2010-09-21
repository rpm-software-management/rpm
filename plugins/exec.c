#include "plugin.h"

#include <sys/wait.h>

static char * options;
static char * name;

rpmPluginHook PLUGIN_HOOKS = \
	PLUGINHOOK_INIT | \
	PLUGINHOOK_CLEANUP | \
	PLUGINHOOK_COLL_POST_ANY;

rpmRC PLUGINHOOK_INIT_FUNC(rpmts ts, const char *name, const char *opts)
{
    options = strdup(opts);
    name = strdup(name);
    return RPMRC_OK;
}

rpmRC PLUGINHOOK_CLEANUP_FUNC(void)
{
    options = _free(options);
    name = _free(name);
    return RPMRC_OK;
}

rpmRC PLUGINHOOK_COLL_POST_ANY_FUNC(void)
{
    rpmRC rc = RPMRC_FAIL;

    if (rpmChrootIn()) {
	goto exit;
    }

    if (options) {
	int status = system(options);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	    rpmlog(RPMLOG_ERR, "%s collection action failed\n", name);
	    goto exit;
	}
    }

    rc = RPMRC_OK;

  exit:
    if (rpmChrootOut()) {
	rc = RPMRC_FAIL;
    }

    return rc;
}
