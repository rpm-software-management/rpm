#include "system.h"
#include <sys/wait.h>
#include <rpm/rpmlog.h>
#include "lib/rpmplugin.h"
#include "lib/rpmchroot.h"
#include "debug.h"

static rpmRC PLUGINHOOK_COLL_POST_ANY_FUNC(rpmPlugin plugin)
{
    rpmRC rc = RPMRC_FAIL;
    const char *options = rpmPluginOpts(plugin);

    if (rpmChrootIn()) {
	goto exit;
    }

    if (options) {
	int status = system(options);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	    rpmlog(RPMLOG_ERR, "%s collection action failed\n",
		   rpmPluginName(plugin));
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
    .coll_post_any = PLUGINHOOK_COLL_POST_ANY_FUNC,
};
