/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"

#include <signal.h>
#include <sys/signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <rpm/rpmsq.h>

#include "debug.h"

static int disableInterruptSafety;
static sigset_t rpmsqCaught;
static sigset_t rpmsqActive;

typedef struct rpmsig_s * rpmsig;

static struct rpmsig_s {
    int signum;
    rpmsqAction_t handler;
    struct sigaction oact;
} rpmsigTbl[] = {
    { SIGINT,	rpmsqAction },
    { SIGQUIT,	rpmsqAction },
    { SIGHUP,	rpmsqAction },
    { SIGTERM,	rpmsqAction },
    { SIGPIPE,	rpmsqAction },
    { -1,	NULL },
};

int rpmsqIsCaught(int signum)
{
    return sigismember(&rpmsqCaught, signum);
}

void rpmsqAction(int signum, siginfo_t * info, void * context)
{
    int save = errno;

    if (sigismember(&rpmsqActive, signum))
	(void) sigaddset(&rpmsqCaught, signum);

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
		sa.sa_sigaction = (handler != NULL ? handler : tbl->handler);
		if (sigaction(tbl->signum, &sa, &tbl->oact) < 0)
		    break;
		sigaddset(&rpmsqActive, tblsignum);
		if (handler != NULL)
		    tbl->handler = handler;
	    }
	} else {				/* Disable. */
	    if (sigismember(&rpmsqActive, tblsignum)) {
		if (sigaction(tbl->signum, &tbl->oact, NULL) < 0)
		    break;
		sigdelset(&rpmsqActive, tblsignum);
		tbl->handler = (handler != NULL ? handler : rpmsqAction);
	    }
	}
	ret = sigismember(&rpmsqActive, tblsignum);
	break;
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
