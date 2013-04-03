#include "system.h"
#include <sys/wait.h>
#include <rpm/rpmlog.h>
#include "lib/rpmplugin.h"
#include "lib/rpmchroot.h"
#include "debug.h"

static char * options;
static char * name;

static rpmRC PLUGINHOOK_INIT_FUNC(rpmts ts, const char *name, const char *opts)
{
    options = strdup(opts);
    name = strdup(name);
    return RPMRC_OK;
}

static void PLUGINHOOK_CLEANUP_FUNC(void)
{
    options = _free(options);
    name = _free(name);
}

static rpmRC PLUGINHOOK_COLL_POST_ANY_FUNC(void)
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

struct rpmPluginHooks_s exec_hooks = {
    .init = PLUGINHOOK_INIT_FUNC,
    .cleanup = PLUGINHOOK_CLEANUP_FUNC,
    .coll_post_any = PLUGINHOOK_COLL_POST_ANY_FUNC,
};
