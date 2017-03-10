#include "system.h"

#include <errno.h>
#include <sys/resource.h>
#if defined(__linux__)
#include <sys/syscall.h>        /* For ionice */
#endif

#include <rpm/rpmlog.h>
#include "lib/rpmplugin.h"

#include "debug.h"

/*
 * In general we want scriptlets to run with the same priority as rpm
 * itself. However on legacy SysV init systems, properties of the
 * parent process can be inherited by the actual daemons on restart,
 * so you can end up with eg nice/ionice'd mysql or httpd, ouch.
 * This plugin resets the scriptlet process priorities after forking, and
 * can be used to counter that effect. Should not be used with systemd
 * because the it's not needed there, and the effect is counter-productive.
 */

static rpmRC prioreset_scriptlet_fork_post(rpmPlugin plugin, const char *path, int type)
{
        /* Call for resetting nice priority. */
        int ret = setpriority(PRIO_PROCESS, 0, 0);
        if (ret == -1) {
            rpmlog(RPMLOG_WARNING, _("Unable to reset nice value: %s"),
                strerror(errno));
        }

        /* Call for resetting IO priority. */
        #if defined(__linux__)
        /* Defined at include/linux/ioprio.h */
        const int _IOPRIO_WHO_PROCESS = 1;
        const int _IOPRIO_CLASS_NONE = 0;
        ret = syscall(SYS_ioprio_set, _IOPRIO_WHO_PROCESS, 0, _IOPRIO_CLASS_NONE);
        if (ret == -1) {
            rpmlog(RPMLOG_WARNING, _("Unable to reset I/O priority: %s"),
                strerror(errno));
        }
        #endif

    return RPMRC_OK;
}

struct rpmPluginHooks_s prioreset_hooks = {
    .scriptlet_fork_post = prioreset_scriptlet_fork_post,
};
