/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"
                                                                                
#if defined(HAVE_PTHREAD_H) && !defined(__LCLINT__)

#include <pthread.h>

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

#define	SAME_THREAD(_a, _b)	pthread_equal(((pthread_t)_a), ((pthread_t)_b))

#define	ME()	((void *)pthread_self())

#else

#define	DO_LOCK()
#define	DO_UNLOCK()
#define	INIT_LOCK()
#define	ADD_REF(__tbl)
#define	SUB_REF(__tbl)
#define	CLEANUP_HANDLER(__handler, __arg, __oldtypeptr)
#define	CLEANUP_RESET(__execute, __oldtype)

#define	SAME_THREAD(_a, _b)	(42)

#define	ME()	(((void *))getpid())

#endif	/* HAVE_PTHREAD_H */

#include <rpmsq.h>

#include "debug.h"

#define	_RPMSQ_DEBUG	0
/*@unchecked@*/
int _rpmsq_debug = _RPMSQ_DEBUG;

/*@unchecked@*/
static struct rpmsqElem rpmsqRock;
/*@unchecked@*/
rpmsq rpmsqQueue = &rpmsqRock;

int rpmsqInsert(void * elem, void * prev)
{
    sigset_t newMask, oldMask;
    rpmsq sq = (rpmsq) elem;
    int ret = -1;

    if (sq != NULL) {
#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "    Insert(%p): %p\n", ME(), sq);
/*@=modfilesys@*/
#endif
	ret = sigemptyset (&newMask);
	ret = sigaddset (&newMask, SIGCHLD);
	ret = sigprocmask(SIG_BLOCK, &newMask, &oldMask);
	if (ret == 0) {
	    sq->child = 0;
	    sq->reaped = 0;
	    sq->status = 0;

	    sq->id = ME();
	    (void) pthread_mutex_init(&sq->mutex, NULL);
	    (void) pthread_cond_init(&sq->cond, NULL);
	    insque(elem, (prev ? prev : rpmsqQueue));
	    ret = sigprocmask(SIG_SETMASK, &oldMask, NULL);
	}
    }
    return 0;
}

int rpmsqRemove(void * elem)
{
    sigset_t newMask, oldMask;
    rpmsq sq = (rpmsq) elem;
    int ret = -1;

    if (elem != NULL) {

#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "    Remove(%p): %p\n", ME(), sq);
/*@=modfilesys@*/
#endif
	ret = sigemptyset (&newMask);
	ret = sigaddset (&newMask, SIGCHLD);
	ret = sigprocmask(SIG_BLOCK, &newMask, &oldMask);
	if (ret == 0) {
	    remque(elem);
	    (void) pthread_cond_destroy(&sq->cond);
	    (void) pthread_mutex_destroy(&sq->mutex);
	    sq->id = NULL;
	    sq->child = 0;
	    sq->reaped = 0;
	    sq->status = 0;
	    ret = sigprocmask(SIG_SETMASK, &oldMask, NULL);
	}
    }
    return ret;
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
    { SIGHUP,	rpmsqHandler },
#define	rpmsigTbl_sighup	(&rpmsigTbl[3])
    { SIGTERM,	rpmsqHandler },
#define	rpmsigTbl_sigterm	(&rpmsigTbl[4])
    { SIGPIPE,	rpmsqHandler },
#define	rpmsigTbl_sigpipe	(&rpmsigTbl[5])
    { -1,	NULL },
};
/*@=fullinitblock@*/

/*@-incondefs@*/
void rpmsqHandler(int signum)
{
    int save = errno;
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

		/* XXX errno set to ECHILD/EINVAL/EINTR. */
		if (reaped <= 0)
		    /*@innerbreak@*/ break;

		/* XXX insque(3)/remque(3) are dequeue, not ring. */
		for (sq = rpmsqQueue->q_forw;
		     sq != NULL && sq != rpmsqQueue;
		     sq = sq->q_forw)
		{
		    int same_thread;
		    if (sq->child != reaped)
			/*@innercontinue@*/ continue;
		    same_thread = SAME_THREAD(ME(), rpmsqQueue->id);
#ifdef _RPMSQ_DEBUG_XXX
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "      Reap(%p): %p child %d id %p same %d\n", ME(), sq, sq->child, sq->id, same_thread);
/*@=modfilesys@*/
#endif
		    sq->reaped = reaped;
		    sq->status = status;

#ifdef	HACK
		    if (!SAME_THREAD(ME(), sq->id))
#endif
		    {

#ifdef _RPMSQ_DEBUG_XXX
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "    Signal(%p): %p child %d id %p\n", ME(), sq, sq->child, sq->id);
/*@=modfilesys@*/
#endif
			(void) pthread_cond_signal(&sq->cond);
		    }

		    /*@innerbreak@*/ break;
		}
	    }
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
	break;
    }
    errno = save;
}
/*@=incondefs@*/

int rpmsqEnable(int signum, /*@null@*/ sighandler_t handler)
{
    int tblsignum = (signum >= 0 ? signum : -signum);
    struct sigaction sa;
    rpmsig tbl;
    int ret = -1;

    DO_LOCK ();
    if (rpmsqQueue->id == NULL)
	rpmsqQueue->id = ME();
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

pid_t rpmsqFork(rpmsq sq)
{
    sigset_t newMask, oldMask;
    pid_t pid;
    int pipes[2];
    int xx;

    if (sq->reaper) {
	xx = rpmsqInsert(sq, NULL);
#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "    Enable(%p): %p\n", ME(), sq);
/*@=modfilesys@*/
#endif
	xx = rpmsqEnable(SIGCHLD, NULL);
    }

    xx = pipe(pipes);

    xx = sigemptyset (&newMask);
    xx = sigaddset (&newMask, SIGCHLD);
    xx = sigprocmask (SIG_BLOCK, &newMask, &oldMask);

    pid = fork();
    if (pid < (pid_t) 0) {		/* fork failed.  */
	close(pipes[0]);
	close(pipes[1]);
	goto out;
    } else if (pid == (pid_t) 0) {	/* Child. */
	int yy;

	/* Block to permit parent to wait. */
	close(pipes[1]);
	xx = read(pipes[0], &yy, sizeof(yy));
	close(pipes[0]);

#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "     Child(%p): %p child %d\n", ME(), sq, getpid());
/*@=modfilesys@*/
#endif

    } else {				/* Parent. */

	sq->child = pid;

#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "    Parent(%p): %p child %d\n", ME(), sq, sq->child);
/*@=modfilesys@*/
#endif

	/* Unblock child. */
	close(pipes[0]);
	close(pipes[1]);

    }

out:
    xx = sigprocmask (SIG_SETMASK, &oldMask, NULL);
    return sq->child;
}

/**
 * Wait for child process to be reaped, and unregister SIGCHLD handler.
 * @param sq		scriptlet queue element
 * @return		0 on success
 */
static int rpmsqWaitUnregister(rpmsq sq)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    sigset_t newMask, oldMask;
#ifdef	HACK
    int same_thread = SAME_THREAD(ME(), rpmsqQueue->id);
#else
    int same_thread = 0;
#endif
    int ret = 0;
    int xx;

    if (same_thread) {
	ret = sigemptyset (&newMask);
	ret = sigaddset (&newMask, SIGCHLD);
	ret = sigprocmask(SIG_BLOCK, &newMask, &oldMask);
    } else {
    }

    /*@-infloops@*/
    while (ret == 0 && sq->reaped != sq->child) {
	if (same_thread) {
	    ret = sigsuspend(&oldMask);
	} else {
	    ret = pthread_mutex_lock(&sq->mutex);
	    ret = pthread_cond_wait(&sq->cond, &sq->mutex);
	    xx = pthread_mutex_unlock(&sq->mutex);
	}
    }
    /*@=infloops@*/

    if (same_thread) {
	xx = sigprocmask(SIG_SETMASK, &oldMask, NULL);
    } else {
    }

#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "      Wake(%p): %p child %d reaper %d ret %d\n", ME(), sq, sq->child, sq->reaper, ret);
/*@=modfilesys@*/
#endif

    xx = rpmsqRemove(sq);
    xx = rpmsqEnable(-SIGCHLD, NULL);
#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "   Disable(%p): %p\n", ME(), sq);
/*@=modfilesys@*/
#endif

    return ret;
}

pid_t rpmsqWait(rpmsq sq)
{
    int same_thread = SAME_THREAD(ME(), rpmsqQueue->id);

#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "      Wait(%p): %p child %d reaper %d same %d\n", ME(), sq, sq->child, sq->reaper, same_thread);
/*@=modfilesys@*/
#endif

    if (sq->reaper) {
	(void) rpmsqWaitUnregister(sq);
    } else {
	pid_t reaped;
	int status;
	do {
	    reaped = waitpid(sq->child, &status, 0);
	} while (reaped >= 0 && reaped != sq->child);
	sq->reaped = reaped;
	sq->status = status;
#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "   Waitpid(%p): %p child %d reaped %d\n", ME(), sq, sq->child, sq->reaped);
/*@=modfilesys@*/
#endif
    }

#ifdef _RPMSQ_DEBUG
/*@-modfilesys@*/
if (_rpmsq_debug)
fprintf(stderr, "      Fini(%p): %p child %d status 0x%x\n", ME(), sq, sq->child, sq->status);
/*@=modfilesys@*/
#endif

    return sq->reaped;
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
    rpmsq sq = memset(alloca(sizeof(*sq)), 0, sizeof(*sq));

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
      || sigprocmask (SIG_SETMASK, &oldMask, NULL) != 0)
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
