/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"
                                                                                
#if defined(HAVE_PTHREAD_H) && !defined(__LCLINT__)
#include <pthread.h>
#endif

#include <rpmsq.h>

#include "debug.h"

/*@unchecked@*/
static struct rpmsqElem rpmsqRock;
/*@unchecked@*/
rpmsq rpmsqQueue = &rpmsqRock;

void Insque(void * elem, void * prev)
{
    if (elem != NULL)
	insque(elem, (prev ? prev : rpmsqQueue));
}

void Remque(void * elem)
{
    if (elem != NULL)
	remque(elem);
}

/*@unchecked@*/
sigset_t rpmsqCaught;

/*@unchecked@*/
static pthread_mutex_t rpmsigTbl_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/*@unchecked@*/
/*@-fullinitblock@*/
static struct rpmsig_s {
    int signum;
    void (*handler) (int signum);
    int active;
    struct sigaction oact;
} rpmsigTbl[] = {
    { SIGINT,	rpmsqHandler },
#define	rpmsigTbl_sigint	(&rpmsigTbl[0])
    { SIGQUIT,	rpmsqHandler },
#define	rpmsigTbl_sigquit	(&rpmsigTbl[1])
    { SIGCHLD,	rpmsqHandler },
#define	rpmsigTbl_sigchld	(&rpmsigTbl[2])

#define	DO_LOCK()	pthread_mutex_lock(&rpmsigTbl_lock);
#define	DO_UNLOCK()	pthread_mutex_unlock(&rpmsigTbl_lock);
#define	INIT_LOCK()	\
     {	pthread_mutexattr_t attr; \
	pthread_mutexattr_init(&attr); \
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); \
	pthread_mutex_init (&rpmsigTbl_lock, &attr); \
	pthread_mutexattr_destroy(&attr); \
	rpmsigTbl_sigchld->active = 0; \
     }
#define	ADD_REF(__tbl)	(__tbl)->active++
#define	SUB_REF(__tbl)	--(__tbl)->active

#define	CLEANUP_HANDLER(__handler, __arg, __oldtypeptr) \
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, (__oldtypeptr)); \
	pthread_cleanup_push((__handler), (__arg));
#define	CLEANUP_RESET(__execute, __oldtype) \
	pthread_cleanup_pop(__execute); \
	pthread_setcanceltype ((__oldtype), &(__oldtype));

    { SIGHUP,	rpmsqHandler },
#define	rpmsigTbl_sighup	(&rpmsigTbl[3])
    { SIGTERM,	rpmsqHandler },
#define	rpmsigTbl_sigterm	(&rpmsigTbl[4])
    { SIGPIPE,	rpmsqHandler },
#define	rpmsigTbl_sigpipe	(&rpmsigTbl[5])
    { -1,	NULL },
};
/*@=fullinitblock@*/

/**
 */
/*@-incondefs@*/
void rpmsqHandler(int signum)
{
    rpmsig tbl;

    for (tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (tbl->signum != signum)
	    continue;

	(void) sigaddset(&rpmsqCaught, signum);

	switch (signum) {
	case SIGCHLD:
	    while (1) {
		rpmsq sq;
		int status = 0;
		pid_t reaped = waitpid(0, &status, WNOHANG);

		if (reaped <= 0)
		    /*@innerbreak@*/ break;

		for (sq = rpmsqQueue->q_forw;
		     sq != NULL && sq != rpmsqQueue;
		     sq = sq->q_forw)
		{
		    if (sq->child != reaped)
			/*@innercontinue@*/ continue;
		    sq->reaped = reaped;
		    sq->status = status;
		    /*@innerbreak@*/ break;
		}
	    }
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
	break;
    }
}
/*@=incondefs@*/

/**
 * Enable or disable a signal handler.
 * @param signum	signal to enable (or disable if negative)
 * @param handler	signal handler (or NULL to use rpmsqHandler())
 * @return		no. of refs, -1 on error
 */
int rpmsqEnable(int signum, /*@null@*/ sighandler_t handler)
	/*@globals rpmsqCaught, rpmsigTbl @*/
	/*@modifies rpmsqCaught, rpmsigTbl @*/
{
    int tblsignum = (signum >= 0 ? signum : -signum);
    struct sigaction sa;
    rpmsig tbl;
    int ret = -1;

    DO_LOCK ();
    for (tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (tblsignum != tbl->signum)
	    continue;

	if (signum >= 0) {			/* Enable. */
	    if (ADD_REF(tbl) <= 0) {
		tbl->active = 1;		/* XXX just in case */
		(void) sigdelset(&rpmsqCaught, tbl->signum);
		sa.sa_flags = 0;
		sigemptyset (&sa.sa_mask);
		sa.sa_handler = (handler != NULL ? handler : tbl->handler);
		if (sigaction(tbl->signum, &sa, &tbl->oact) < 0) {
		    SUB_REF(tbl);
		    break;
		}
	    }
	} else {				/* Disable. */
	    if (SUB_REF(tbl) <= 0) {
		tbl->active = 0;		/* XXX just in case */
		if (sigaction(tbl->signum, &tbl->oact, NULL) < 0)
		    break;
	    }
	}
	ret = tbl->active;
	break;
    }
    DO_UNLOCK ();
    return ret;
}

/**
 * SIGCHLD cancellation handler.
 */
static void
sigchld_cancel (void *arg)
{
    pid_t child = *(pid_t *) arg;
    pid_t result;

    (void) kill(child, SIGKILL);

    do {
	result = waitpid(child, NULL, 0);
    } while (result == (pid_t)-1 && errno == EINTR);

    DO_LOCK ();
    if (SUB_REF (rpmsigTbl_sigchld) == 0) {
	(void) rpmsqEnable(-SIGQUIT, NULL);
	(void) rpmsqEnable(-SIGINT, NULL);
    }
    DO_UNLOCK ();
}

/**
 * Execute a command, returning its status.
 */
int
rpmsqExecve (const char ** argv)
{
    int oldtype;
    int status = -1;
    pid_t pid;
    pid_t result;
    sigset_t newMask, oldMask;

    DO_LOCK ();
    if (ADD_REF (rpmsigTbl_sigchld) == 0) {
	if (rpmsqEnable(SIGINT, NULL) < 0) {
	    SUB_REF (rpmsigTbl_sigchld);
	    goto out;
	}
	if (rpmsqEnable(SIGQUIT, NULL) < 0) {
	    SUB_REF (rpmsigTbl_sigchld);
	    goto out_restore_sigint;
	}
    }
    DO_UNLOCK ();

    sigemptyset (&newMask);
    sigaddset (&newMask, SIGCHLD);
    if (sigprocmask (SIG_BLOCK, &newMask, &oldMask) < 0) {
	DO_LOCK ();
	if (SUB_REF (rpmsigTbl_sigchld) == 0)
	    goto out_restore_sigquit_and_sigint;
	goto out;
    }

    CLEANUP_HANDLER(sigchld_cancel, &pid, &oldtype);

    pid = fork ();
    if (pid < (pid_t) 0) {		/* fork failed.  */
	goto out;
    } else if (pid == (pid_t) 0) {	/* Child. */

	/* Restore the signals.  */
	(void) sigaction (SIGINT, &rpmsigTbl_sigint->oact, NULL);
	(void) sigaction (SIGQUIT, &rpmsigTbl_sigquit->oact, NULL);
	(void) sigprocmask (SIG_SETMASK, &oldMask, NULL);

	/* Reset rpmsigTbl lock and refcnt. */
	INIT_LOCK ();

	(void) execve (argv[0], (char *const *) argv, environ);
	_exit (127);
    } else {				/* Parent. */
	do {
	    result = waitpid(pid, &status, 0);
	} while (result == (pid_t)-1 && errno == EINTR);
	if (result != pid)
	    status = -1;
    }

    CLEANUP_RESET(0, oldtype);

    DO_LOCK ();
    if ((SUB_REF (rpmsigTbl_sigchld) == 0 &&
        (rpmsqEnable(-SIGINT, NULL) < 0 || rpmsqEnable (-SIGQUIT, NULL) < 0))
      || sigprocmask (SIG_SETMASK, &oldMask, (sigset_t *) NULL) != 0)
    {
	status = -1;
    }
    goto out;

out_restore_sigquit_and_sigint:
    (void) rpmsqEnable(-SIGQUIT, NULL);
out_restore_sigint:
    (void) rpmsqEnable(-SIGINT, NULL);
out:
    DO_UNLOCK ();
    return status;
}
