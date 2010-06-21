#include "collection.h"

#include <sys/wait.h>

rpmCollHook COLLECTION_HOOKS = COLLHOOK_POST_ANY;

rpmRC COLLHOOK_POST_ANY_FUNC(rpmts ts, const char *collname,
			     const char *options)
{
    int rc = RPMRC_FAIL;

    if (rpmChrootIn()) {
	goto exit;
    }

    if (options) {
	int status = system(options);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	    rpmlog(RPMLOG_ERR, "%s collection action failed\n", collname);
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
