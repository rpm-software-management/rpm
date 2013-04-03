#include <syslog.h>

#include <rpm/rpmts.h>
#include "lib/rpmplugin.h"

struct logstat {
    int logging;
    unsigned int scriptfail;
    unsigned int pkgfail;
};

static rpmRC PLUGINHOOK_INIT_FUNC(rpmPlugin plugin,
				rpmts ts, const char * name, const char * opts)
{
    /* XXX make this configurable? */
    const char * log_ident = "[RPM]";
    struct logstat * state = rcalloc(1, sizeof(*state));

    rpmPluginSetData(plugin, state);
    openlog(log_ident, (LOG_PID), LOG_USER);
    return RPMRC_OK;
}

static void PLUGINHOOK_CLEANUP_FUNC(rpmPlugin plugin)
{
    struct logstat * state = rpmPluginGetData(plugin);
    free(state);
    closelog();
}

static rpmRC PLUGINHOOK_TSM_PRE_FUNC(rpmPlugin plugin, rpmts ts)
{
    struct logstat * state = rpmPluginGetData(plugin);
    
    /* Reset counters */
    state->scriptfail = 0;
    state->pkgfail = 0;

    /* Assume we are logging but... */
    state->logging = 1;

    /* ...don't log test transactions */
    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_BUILD_PROBS))
	state->logging = 0;

    /* ...don't log chroot transactions */
    if (!rstreq(rpmtsRootDir(ts), "/"))
	state->logging = 0;

    if (state->logging) {
	syslog(LOG_NOTICE, "Transaction ID %x started", rpmtsGetTid(ts));
    }

    return RPMRC_OK;
}

static rpmRC PLUGINHOOK_TSM_POST_FUNC(rpmPlugin plugin, rpmts ts, int res)
{
    struct logstat * state = rpmPluginGetData(plugin);

    if (state->logging) {
	if (state->pkgfail || state->scriptfail) {
	    syslog(LOG_WARNING, "%u elements failed, %u scripts failed",
		   state->pkgfail, state->scriptfail);
	}
	syslog(LOG_NOTICE, "Transaction ID %x finished: %d",
		rpmtsGetTid(ts), res);
    }

    state->logging = 0;
    return RPMRC_OK;
}

static rpmRC PLUGINHOOK_PSM_POST_FUNC(rpmPlugin plugin, rpmte te, int res)
{
    struct logstat * state = rpmPluginGetData(plugin);

    if (state->logging) {
	int lvl = LOG_NOTICE;
	const char *op = (rpmteType(te) == TR_ADDED) ? "install" : "erase";
	const char *outcome = "success";
	/* XXX: Permit configurable header queryformat? */
	const char *pkg = rpmteNEVRA(te);

	if (res != RPMRC_OK) {
	    lvl = LOG_WARNING;
	    outcome = "failure";
	    state->pkgfail++;
	}

	syslog(lvl, "%s %s: %s", op, pkg, outcome);
    }
    return RPMRC_OK;
}

static rpmRC PLUGINHOOK_SCRIPTLET_POST_FUNC(rpmPlugin plugin,
					const char *s_name, int type, int res)
{
    struct logstat * state = rpmPluginGetData(plugin);

    if (state->logging && res) {
	syslog(LOG_WARNING, "scriptlet %s failure: %d\n", s_name, res);
	state->scriptfail++;
    }
    return RPMRC_OK;
}

struct rpmPluginHooks_s syslog_hooks = {
    .init = PLUGINHOOK_INIT_FUNC,
    .cleanup = PLUGINHOOK_CLEANUP_FUNC,
    .tsm_pre = PLUGINHOOK_TSM_PRE_FUNC,
    .tsm_post = PLUGINHOOK_TSM_POST_FUNC,
    .psm_post = PLUGINHOOK_PSM_POST_FUNC,
    .scriptlet_post = PLUGINHOOK_SCRIPTLET_POST_FUNC,
};
