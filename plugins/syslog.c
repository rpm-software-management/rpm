#include "system.h"

#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmts.h>
#include <rpm/rpmplugin.h>

typedef void (*loggerfunc)(int prio, const char *fmt, ...);

struct logstat {
    int logging;
    unsigned int scriptfail;
    unsigned int pkgfail;
    loggerfunc log;
};

static void errlog(int prio, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static rpmRC syslog_init(rpmPlugin plugin, rpmts ts)
{
    /* XXX make this configurable? */
    const char * log_ident = "rpm";
    struct logstat * state = rcalloc(1, sizeof(*state));
    char *target = rpmExpand("%{?__transaction_syslog_target}", NULL);

    if (rstreq(target, "stderr")) {
	state->log = errlog;
    } else {
	if (*target && !rstreq(target, "syslog")) {
	    rpmlog(RPMLOG_WARNING,
		_("unknown syslog target %s, using 'syslog'\n"), target);
	}
	state->log = syslog;
	openlog(log_ident, (LOG_PID), LOG_USER);
    }

    rpmPluginSetData(plugin, state);
    free(target);
    return RPMRC_OK;
}

static void syslog_cleanup(rpmPlugin plugin)
{
    struct logstat * state = rpmPluginGetData(plugin);
    if (state->log == syslog)
	closelog();
    free(state);
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
	state->log(LOG_NOTICE, "Transaction ID %x started", rpmtsGetTid(ts));
    }

    return RPMRC_OK;
}

static rpmRC syslog_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
    struct logstat * state = rpmPluginGetData(plugin);

    if (state->logging) {
	if (state->pkgfail || state->scriptfail) {
	    state->log(LOG_WARNING, "%u elements failed, %u scripts failed",
		   state->pkgfail, state->scriptfail);
	}
	state->log(LOG_NOTICE, "Transaction ID %x finished: %d",
		rpmtsGetTid(ts), res);
    }

    state->logging = 0;
    return RPMRC_OK;
}

static const char *getOp(rpmte te)
{
    switch (rpmteType(te)) {
    case TR_ADDED:	return "install";
    case TR_REMOVED:	return "erase";
    case TR_RPMDB:	return "rpmdb";
    case TR_RESTORED:	return "restore";
    }
    return "<unknown>";
}

static rpmRC syslog_psm_post(rpmPlugin plugin, rpmte te, int res)
{
    struct logstat * state = rpmPluginGetData(plugin);

    if (state->logging) {
	int lvl = LOG_NOTICE;
	const char *op = getOp(te);
	const char *outcome = "success";
	/* XXX: Permit configurable header queryformat? */
	const char *pkg = rpmteNEVRA(te);

	if (res != RPMRC_OK) {
	    lvl = LOG_WARNING;
	    outcome = "failure";
	    state->pkgfail++;
	}

	state->log(lvl, "%s %s: %s", op, pkg, outcome);
    }
    return RPMRC_OK;
}

static rpmRC syslog_scriptlet_post(rpmPlugin plugin,
					const char *s_name, int type, int res)
{
    struct logstat * state = rpmPluginGetData(plugin);

    if (state->logging && res) {
	state->log(LOG_WARNING, "scriptlet %s failure: %d\n", s_name, res);
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
