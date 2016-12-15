/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"

#include <signal.h>
#include <sys/signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rpm/rpmsq.h>
#include <rpm/rpmlog.h>

#include "debug.h"

static int disableInterruptSafety;
static sigset_t rpmsqCaught;
static sigset_t rpmsqActive;

typedef struct rpmsig_s * rpmsig;

static void rpmsqTerm(int signum, siginfo_t *info, void *context)
{
    if (info->si_pid == 0) {
	rpmlog(RPMLOG_DEBUG,
		"exiting on signal %d (killed by death, eh?)\n", signum);
    } else {
	rpmlog(RPMLOG_WARNING,
		_("exiting on signal %d from pid %d\n"), signum, info->si_pid);
    }
    exit(EXIT_FAILURE);
}

static struct rpmsig_s {
    int signum;
    rpmsqAction_t defhandler;
    rpmsqAction_t handler;
    siginfo_t siginfo;
    struct sigaction oact;
} rpmsigTbl[] = {
    { SIGINT,	rpmsqTerm,	NULL },
    { SIGQUIT,	rpmsqTerm,	NULL },
    { SIGHUP,	rpmsqTerm,	NULL },
    { SIGTERM,	rpmsqTerm,	NULL },
    { SIGPIPE,	rpmsqTerm,	NULL },
    { -1,	NULL,		NULL },
};

static int rpmsigGet(int signum, struct rpmsig_s **sig)
{
    for (rpmsig tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (tbl->signum == signum) {
	    *sig = tbl;
	    return 1;
	}
    }
    return 0;
}

int rpmsqIsCaught(int signum)
{
    return sigismember(&rpmsqCaught, signum);
}

static void rpmsqHandler(int signum, siginfo_t * info, void * context)
{
    int save = errno;

    if (sigismember(&rpmsqActive, signum)) {
	rpmsig sig = NULL;
	(void) sigaddset(&rpmsqCaught, signum);
	if (rpmsigGet(signum, &sig))
	    memcpy(&sig->siginfo, info, sizeof(*info));
    }

    errno = save;
}

int rpmsqEnable(int signum, rpmsqAction_t handler)
{
    int tblsignum = abs(signum);
    struct sigaction sa;
    rpmsig tbl;
    int ret = -1;

    if (disableInterruptSafety)
      return 0;

    for (tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (tblsignum != tbl->signum)
	    continue;

	if (signum >= 0) {			/* Enable. */
	    if (!sigismember(&rpmsqActive, tblsignum)) {
		(void) sigdelset(&rpmsqCaught, tbl->signum);

		/* XXX Don't set a signal handler if already SIG_IGN */
		(void) sigaction(tbl->signum, NULL, &tbl->oact);
		if (tbl->oact.sa_handler == SIG_IGN)
		    continue;

		(void) sigemptyset (&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = rpmsqHandler;
		if (sigaction(tbl->signum, &sa, &tbl->oact) < 0)
		    break;
		sigaddset(&rpmsqActive, tblsignum);
		tbl->handler = (handler != NULL) ? handler : tbl->defhandler;
	    }
	} else {				/* Disable. */
	    if (sigismember(&rpmsqActive, tblsignum)) {
		if (sigaction(tbl->signum, &tbl->oact, NULL) < 0)
		    break;
		sigdelset(&rpmsqActive, tblsignum);
		tbl->handler = NULL;
	    }
	}
	ret = sigismember(&rpmsqActive, tblsignum);
	break;
    }
    return ret;
}

int rpmsqPoll(void)
{
    sigset_t newMask, oldMask;
    int n = 0;

    /* block all signals while processing the queue */
    (void) sigfillset(&newMask);
    (void) sigprocmask(SIG_BLOCK, &newMask, &oldMask);

    for (rpmsig tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (sigismember(&rpmsqCaught, tbl->signum)) {
	    n++;
	    /* delete signal before running handler to prevent recursing */
	    sigdelset(&rpmsqCaught, tbl->signum);
	    if (tbl->handler) {
		tbl->handler(tbl->signum, &tbl->siginfo, NULL);
		memset(&tbl->siginfo, 0, sizeof(tbl->siginfo));
	    }
	}
    }
    sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return n;
}

/** \ingroup rpmio
 * 
 * By default, librpm will trap various unix signals such as SIGINT and SIGTERM,
 * in order to avoid process exit while locks are held or a transaction is being
 * performed.  However, there exist tools that operate on non-running roots (traditionally
 * build systems such as mock), as well as deployment tools such as rpm-ostree.
 *
 * These tools are more robust against interruption - typically they
 * will just throw away the partially constructed root.  This function
 * is designed for use by those tools, so an operator can happily
 * press Control-C.
 *
 * It's recommended to call this once only at process startup if this
 * behavior is desired (and to then avoid using librpm against "live"
 * databases), because currently signal handlers will not be retroactively
 * applied if a database is open.
 */
void rpmsqSetInterruptSafety(int on)
{
  disableInterruptSafety = !on;
}
