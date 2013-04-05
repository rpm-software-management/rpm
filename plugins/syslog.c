#include <syslog.h>

#include <rpm/rpmts.h>
#include "lib/rpmplugin.h"

struct logstat {
    int logging;
    unsigned int scriptfail;
    unsigned int pkgfail;
};

static rpmRC syslog_init(rpmPlugin plugin, rpmts ts)
{
    /* XXX make this configurable? */
    const char * log_ident = "[RPM]";
    struct logstat * state = rcalloc(1, sizeof(*state));

    rpmPluginSetData(plugin, state);
    openlog(log_ident, (LOG_PID), LOG_USER);
    return RPMRC_OK;
}

static void syslog_cleanup(rpmPlugin plugin)
{
    struct logstat * state = rpmPluginGetData(plugin);
    free(state);
    closelog();
}

static rpmRC syslog_tsm_pre(rpmPlugin plugin, rpmts ts)
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

static rpmRC syslog_tsm_post(rpmPlugin plugin, rpmts ts, int res)
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

static rpmRC syslog_psm_post(rpmPlugin plugin, rpmte te, int res)
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

static rpmRC syslog_scriptlet_post(rpmPlugin plugin,
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
    .init = syslog_init,
    .cleanup = syslog_cleanup,
    .tsm_pre = syslog_tsm_pre,
    .tsm_post = syslog_tsm_post,
    .psm_post = syslog_psm_post,
    .scriptlet_post = syslog_scriptlet_post,
};
