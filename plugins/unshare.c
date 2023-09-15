#include "system.h"

#include <sched.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>

#include <rpm/rpmts.h>
#include <rpm/rpmplugin.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>

#include "debug.h"

static ARGV_t private_mounts = NULL;
static int unshare_flags = 0;

static rpmRC unshare_init(rpmPlugin plugin, rpmts ts)
{
    char *paths = rpmExpand("%{?__transaction_unshare_paths}", NULL);
    private_mounts = argvSplitString(paths, ":", ARGV_SKIPEMPTY);
    if (private_mounts)
	unshare_flags |= CLONE_NEWNS;
    free(paths);

    if (rpmExpandNumeric("%{?__transaction_unshare_nonet}"))
	unshare_flags |= CLONE_NEWNET;

    return RPMRC_OK;
}

static void unshare_cleanup(rpmPlugin plugin)
{
    /* ensure clean state for possible next transaction */
    private_mounts = argvFree(private_mounts);
    unshare_flags = 0;
}

static rpmRC unshare_scriptlet_fork_post(rpmPlugin plugin,
						 const char *path, int type)
{
    rpmRC rc = RPMRC_FAIL;

    if (unshare_flags && (unshare(unshare_flags) == -1)) {
	rpmlog(RPMLOG_ERR, _("unshare with flags x%x failed: %s\n"),
		unshare_flags, strerror(errno));
	goto exit;
    }

    if (private_mounts) {
	if (mount("/", "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
	    rpmlog(RPMLOG_ERR, _("failed to mount private %s: %s\n"),
		    "/", strerror(errno));
	    goto exit;
	}
	for (ARGV_t mnt = private_mounts; mnt && *mnt; mnt++) {
	    if (mount("none", *mnt, "tmpfs", 0, NULL) == -1) {
		rpmlog(RPMLOG_ERR, _("failed to mount private %s: %s\n"),
			*mnt, strerror(errno));
		goto exit;
	    }
	}
    }
    rc = RPMRC_OK;

exit:
    return rc;
}

struct rpmPluginHooks_s unshare_hooks = {
    .init = unshare_init,
    .cleanup = unshare_cleanup,
    .scriptlet_fork_post = unshare_scriptlet_fork_post,
};
