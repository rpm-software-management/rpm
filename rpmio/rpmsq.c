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

static void rpmsqIgn(int signum, siginfo_t *info, void *context)
{
}

static void rpmsqTerm(int signum, siginfo_t *info, void *context)
{
    if (info->si_pid == 0) {
	rpmlog(RPMLOG_DEBUG,
		"exiting on signal %d (killed by death, eh?)\n", signum);
    } else {
	int lvl = (signum == SIGPIPE) ? RPMLOG_DEBUG : RPMLOG_WARNING;
	rpmlog(lvl,
		_("exiting on signal %d from pid %d\n"), signum, info->si_pid);
    }
    /* exit 128 + signum for compatibility with bash(1) */
    exit(128 + signum);
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
	if (!sigismember(&rpmsqCaught, signum)) {
	    rpmsig sig = NULL;
	    if (rpmsigGet(signum, &sig)) {
		(void) sigaddset(&rpmsqCaught, signum);
		memcpy(&sig->siginfo, info, sizeof(*info));
	    }
	}
    }

    errno = save;
}

rpmsqAction_t rpmsqSetAction(int signum, rpmsqAction_t handler)
{
    rpmsig sig = NULL;
    rpmsqAction_t oh = RPMSQ_ERR;

    if (rpmsigGet(signum, &sig)) {
	oh = sig->handler;
	sig->handler = (handler == RPMSQ_IGN) ? rpmsqIgn : handler;
    }
    return oh;
}

int rpmsqActivate(int state)
{
    sigset_t newMask, oldMask;

    if (disableInterruptSafety)
      return 0;

    (void) sigfillset(&newMask);
    (void) pthread_sigmask(SIG_BLOCK, &newMask, &oldMask);

    if (state) {
	struct sigaction sa;
	for (rpmsig tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	    sigdelset(&rpmsqCaught, tbl->signum);
	    memset(&tbl->siginfo, 0, sizeof(tbl->siginfo));

	    /* XXX Don't set a signal handler if already SIG_IGN */
	    sigaction(tbl->signum, NULL, &tbl->oact);
	    if (tbl->oact.sa_handler == SIG_IGN)
		continue;

	    sigemptyset (&sa.sa_mask);
	    sa.sa_flags = SA_SIGINFO;
	    sa.sa_sigaction = rpmsqHandler;
	    if (sigaction(tbl->signum, &sa, &tbl->oact) == 0)
		sigaddset(&rpmsqActive, tbl->signum);
	}
    } else {
	for (rpmsig tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	    if (!sigismember(&rpmsqActive, tbl->signum))
		continue;
	    if (sigaction(tbl->signum, &tbl->oact, NULL) == 0) {
		sigdelset(&rpmsqActive, tbl->signum);
		sigdelset(&rpmsqCaught, tbl->signum);
		memset(&tbl->siginfo, 0, sizeof(tbl->siginfo));
	    }
	}
    }
    pthread_sigmask(SIG_SETMASK, &oldMask, NULL);
    return 0;
}

int rpmsqPoll(void)
{
    sigset_t newMask, oldMask;
    int n = 0;

    /* block all signals while processing the queue */
    (void) sigfillset(&newMask);
    (void) pthread_sigmask(SIG_BLOCK, &newMask, &oldMask);

    for (rpmsig tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	/* honor blocked signals in polling too */
	if (sigismember(&oldMask, tbl->signum))
	    continue;
	if (sigismember(&rpmsqCaught, tbl->signum)) {
	    rpmsqAction_t handler = (tbl->handler != NULL) ? tbl->handler :
							     tbl->defhandler;
	    /* delete signal before running handler to prevent recursing */
	    sigdelset(&rpmsqCaught, tbl->signum);
	    handler(tbl->signum, &tbl->siginfo, NULL);
	    memset(&tbl->siginfo, 0, sizeof(tbl->siginfo));
	    n++;
	}
    }
    pthread_sigmask(SIG_SETMASK, &oldMask, NULL);
    return n;
}

int rpmsqBlock(int op)
{
    static sigset_t oldMask;
    static int blocked = 0;
    sigset_t newMask;
    int ret = 0;

    if (op == SIG_BLOCK) {
	blocked++;
	if (blocked == 1) {
	    sigfillset(&newMask);
	    sigdelset(&newMask, SIGABRT);
	    sigdelset(&newMask, SIGBUS);
	    sigdelset(&newMask, SIGFPE);
	    sigdelset(&newMask, SIGILL);
	    sigdelset(&newMask, SIGSEGV);
	    sigdelset(&newMask, SIGTSTP);
	    ret = pthread_sigmask(SIG_BLOCK, &newMask, &oldMask);
	}
    } else if (op == SIG_UNBLOCK) {
	blocked--;
	if (blocked == 0) {
	    ret = pthread_sigmask(SIG_SETMASK, &oldMask, NULL);
	    rpmsqPoll();
	} else if (blocked < 0) {
	    blocked = 0;
	    ret = -1;
	}
    }

    return ret;
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
