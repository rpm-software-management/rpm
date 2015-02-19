/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"

#include <signal.h>
#include <sys/signal.h>
#include <errno.h>
#include <stdio.h>

#define	ADD_REF(__tbl)	(__tbl)->active++
#define	SUB_REF(__tbl)	--(__tbl)->active

#include <rpm/rpmsq.h>

#include "debug.h"

static int disableInterruptSafety;
static sigset_t rpmsqCaught;

typedef struct rpmsig_s * rpmsig;

static struct rpmsig_s {
    int signum;
    rpmsqAction_t handler;
    int active;
    struct sigaction oact;
} rpmsigTbl[] = {
    { SIGINT,	rpmsqAction },
#define	rpmsigTbl_sigint	(&rpmsigTbl[0])
    { SIGQUIT,	rpmsqAction },
#define	rpmsigTbl_sigquit	(&rpmsigTbl[1])
    { SIGHUP,	rpmsqAction },
#define	rpmsigTbl_sighup	(&rpmsigTbl[3])
    { SIGTERM,	rpmsqAction },
#define	rpmsigTbl_sigterm	(&rpmsigTbl[4])
    { SIGPIPE,	rpmsqAction },
#define	rpmsigTbl_sigpipe	(&rpmsigTbl[5])
    { -1,	NULL },
};

int rpmsqIsCaught(int signum)
{
    return sigismember(&rpmsqCaught, signum);
}

#ifdef SA_SIGINFO
void rpmsqAction(int signum, siginfo_t * info, void * context)
#else
void rpmsqAction(int signum)
#endif
{
    int save = errno;
    rpmsig tbl;

    for (tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (tbl->signum != signum)
	    continue;

	(void) sigaddset(&rpmsqCaught, signum);
	break;
    }
    errno = save;
}

int rpmsqEnable(int signum, rpmsqAction_t handler)
{
    int tblsignum = (signum >= 0 ? signum : -signum);
    struct sigaction sa;
    rpmsig tbl;
    int ret = -1;

    if (disableInterruptSafety)
      return 0;

    for (tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (tblsignum != tbl->signum)
	    continue;

	if (signum >= 0) {			/* Enable. */
	    if (ADD_REF(tbl) <= 0) {
		(void) sigdelset(&rpmsqCaught, tbl->signum);

		/* XXX Don't set a signal handler if already SIG_IGN */
		(void) sigaction(tbl->signum, NULL, &tbl->oact);
		if (tbl->oact.sa_handler == SIG_IGN)
		    continue;

		(void) sigemptyset (&sa.sa_mask);
#ifdef SA_SIGINFO
		sa.sa_flags = SA_SIGINFO;
#else
		sa.sa_flags = 0;
#endif
		sa.sa_sigaction = (handler != NULL ? handler : tbl->handler);
		if (sigaction(tbl->signum, &sa, &tbl->oact) < 0) {
		    SUB_REF(tbl);
		    break;
		}
		tbl->active = 1;		/* XXX just in case */
		if (handler != NULL)
		    tbl->handler = handler;
	    }
	} else {				/* Disable. */
	    if (SUB_REF(tbl) <= 0) {
		if (sigaction(tbl->signum, &tbl->oact, NULL) < 0)
		    break;
		tbl->active = 0;		/* XXX just in case */
		tbl->handler = (handler != NULL ? handler : rpmsqAction);
	    }
	}
	ret = tbl->active;
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
