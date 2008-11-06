/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"

#include <signal.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <search.h>

#if defined(HAVE_PTHREAD_H)

#include <pthread.h>

/* XXX suggested in bugzilla #159024 */
#if PTHREAD_MUTEX_DEFAULT != PTHREAD_MUTEX_NORMAL
  #error RPM expects PTHREAD_MUTEX_DEFAULT == PTHREAD_MUTEX_NORMAL
#endif

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
static pthread_mutex_t rpmsigTbl_lock = PTHREAD_MUTEX_INITIALIZER;
#else
static pthread_mutex_t rpmsigTbl_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

#define	DO_LOCK()	pthread_mutex_lock(&rpmsigTbl_lock);
#define	DO_UNLOCK()	pthread_mutex_unlock(&rpmsigTbl_lock);
#define	INIT_LOCK()	\
    {	pthread_mutexattr_t attr; \
	(void) pthread_mutexattr_init(&attr); \
	(void) pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); \
	(void) pthread_mutex_init (&rpmsigTbl_lock, &attr); \
	(void) pthread_mutexattr_destroy(&attr); \
	rpmsigTbl_sigchld->active = 0; \
    }
#define	ADD_REF(__tbl)	(__tbl)->active++
#define	SUB_REF(__tbl)	--(__tbl)->active
#define	CLEANUP_HANDLER(__handler, __arg, __oldtypeptr) \
    (void) pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, (__oldtypeptr));\
	pthread_cleanup_push((__handler), (__arg));
#define	CLEANUP_RESET(__execute, __oldtype) \
    pthread_cleanup_pop(__execute); \
    (void) pthread_setcanceltype ((__oldtype), &(__oldtype));

#define	SAME_THREAD(_a, _b)	pthread_equal(((pthread_t)_a), ((pthread_t)_b))

#define	ME()	((void *)pthread_self())

#else

#define	DO_LOCK()
#define	DO_UNLOCK()
#define	INIT_LOCK()
#define	ADD_REF(__tbl)	(0)
#define	SUB_REF(__tbl)	(0)
#define	CLEANUP_HANDLER(__handler, __arg, __oldtypeptr)
#define	CLEANUP_RESET(__execute, __oldtype)

#define	SAME_THREAD(_a, _b)	(42)

#define	ME()	(((void *)getpid()))

#endif	/* HAVE_PTHREAD_H */

#define _RPMSQ_INTERNAL
#include <rpm/rpmsq.h>

#include "debug.h"

#define	_RPMSQ_DEBUG	0
int _rpmsq_debug = _RPMSQ_DEBUG;

static struct rpmsqElem rpmsqRock;

static rpmsq rpmsqQueue = &rpmsqRock;

/** \ingroup rpmsq
 * Insert node into from queue.
 * @param elem          node to link
 * @param prev          previous node from queue
 * @return              0 on success
 */
static int rpmsqInsert(void * elem, void * prev)
{
    rpmsq sq = (rpmsq) elem;
    int ret = -1;

    if (sq != NULL) {
#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "    Insert(%p): %p\n", ME(), sq);
#endif
	ret = sighold(SIGCHLD);
	if (ret == 0) {
	    sq->child = 0;
	    sq->reaped = 0;
	    sq->status = 0;
	    sq->reaper = 1;
	    sq->pipes[0] = sq->pipes[1] = -1;

	    sq->id = ME();
	    ret = pthread_mutex_init(&sq->mutex, NULL);
	    insque(elem, (prev != NULL ? prev : rpmsqQueue));
	    ret = sigrelse(SIGCHLD);
	}
    }
    return ret;
}

/** \ingroup rpmsq
 * Remove node from queue.
 * @param elem          node to link
 * @return              0 on success
 */
static int rpmsqRemove(void * elem)
{
    rpmsq sq = (rpmsq) elem;
    int ret = -1;

    if (elem != NULL) {

#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "    Remove(%p): %p\n", ME(), sq);
#endif
	ret = sighold (SIGCHLD);
	if (ret == 0) {
	    remque(elem);
	   
	    /* Unlock the mutex and then destroy it */ 
	    if((ret = pthread_mutex_unlock(&sq->mutex)) == 0)
		ret = pthread_mutex_destroy(&sq->mutex);

	    sq->id = NULL;
	    if (sq->pipes[1])	ret = close(sq->pipes[1]);
	    if (sq->pipes[0])	ret = close(sq->pipes[0]);
	    sq->pipes[0] = sq->pipes[1] = -1;
#ifdef	NOTYET	/* rpmpsmWait debugging message needs */
	    sq->reaper = 1;
	    sq->status = 0;
	    sq->reaped = 0;
	    sq->child = 0;
#endif
	    ret = sigrelse(SIGCHLD);
	}
    }
    return ret;
}

static sigset_t rpmsqCaught;

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
    { SIGCHLD,	rpmsqAction },
#define	rpmsigTbl_sigchld	(&rpmsigTbl[2])
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
void rpmsqAction(int signum,
		void * info, void * context)
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

	switch (signum) {
	case SIGCHLD:
	    while (1) {
		rpmsq sq;
		int status = 0;
		pid_t reaped = waitpid(0, &status, WNOHANG);

		/* XXX errno set to ECHILD/EINVAL/EINTR. */
		if (reaped <= 0)
		    break;

		/* XXX insque(3)/remque(3) are dequeue, not ring. */
		for (sq = rpmsqQueue->q_forw;
		     sq != NULL && sq != rpmsqQueue;
		     sq = sq->q_forw)
		{
		    int ret;

		    if (sq->child != reaped)
			continue;
		    sq->reaped = reaped;
		    sq->status = status;

		    /* Unlock the mutex.  The waiter will then be able to 
		     * aquire the lock.  
		     *
		     * XXX: jbj, wtd, if this fails? 
		     */
		    ret = pthread_mutex_unlock(&sq->mutex); 

		    break;
		}
	    }
	    break;
	default:
	    break;
	}
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

    (void) DO_LOCK ();
    if (rpmsqQueue->id == NULL)
	rpmsqQueue->id = ME();
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
		sa.sa_sigaction = (void*)(handler != NULL ? handler : tbl->handler);
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
    (void) DO_UNLOCK ();
    return ret;
}

pid_t rpmsqFork(rpmsq sq)
{
    pid_t pid;
    int xx;
    int nothreads = 0;   /* XXX: Shouldn't this be a global? */

    if (sq->reaper) {
	xx = rpmsqInsert(sq, NULL);
#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "    Enable(%p): %p\n", ME(), sq);
#endif
	xx = rpmsqEnable(SIGCHLD, NULL);
    }

    xx = pipe(sq->pipes);

    xx = sighold(SIGCHLD);

    /* 
     * Initialize the cond var mutex.   We have to aquire the lock we 
     * use for the condition before we fork.  Otherwise it is possible for
     * the child to exit, we get sigchild and the sig handler to send 
     * the condition signal before we are waiting on the condition.
     */
    if (!nothreads) {
	if(pthread_mutex_lock(&sq->mutex)) {
	    /* Yack we did not get the lock, lets just give up */
	    xx = close(sq->pipes[0]);
	    xx = close(sq->pipes[1]);
	    sq->pipes[0] = sq->pipes[1] = -1;
	    goto out;
	}
    }

    pid = fork();
    if (pid < (pid_t) 0) {		/* fork failed.  */
	sq->child = (pid_t)-1;
	xx = close(sq->pipes[0]);
	xx = close(sq->pipes[1]);
	sq->pipes[0] = sq->pipes[1] = -1;
	goto out;
    } else if (pid == (pid_t) 0) {	/* Child. */
	int yy;

	/* Block to permit parent time to wait. */
	xx = close(sq->pipes[1]);
	xx = read(sq->pipes[0], &yy, sizeof(yy));
	xx = close(sq->pipes[0]);
	sq->pipes[0] = sq->pipes[1] = -1;

#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "     Child(%p): %p child %d\n", ME(), sq, getpid());
#endif

    } else {				/* Parent. */

	sq->child = pid;

#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "    Parent(%p): %p child %d\n", ME(), sq, sq->child);
#endif

    }

out:
    xx = sigrelse(SIGCHLD);
    return sq->child;
}

/**
 * Wait for child process to be reaped, and unregister SIGCHLD handler.
 * @todo Rewrite to use waitpid on helper thread.
 * @param sq		scriptlet queue element
 * @return		0 on success
 */
static int rpmsqWaitUnregister(rpmsq sq)
{
    int nothreads = 0;
    int ret = 0;
    int xx;

    /* Protect sq->reaped from handler changes. */
    ret = sighold(SIGCHLD);

    /* Start the child, linux often runs child before parent. */
    if (sq->pipes[0] >= 0)
	xx = close(sq->pipes[0]);
    if (sq->pipes[1] >= 0)
	xx = close(sq->pipes[1]);
    sq->pipes[0] = sq->pipes[1] = -1;

    /* Put a stopwatch on the time spent waiting to measure performance gain. */
    (void) rpmswEnter(&sq->op, -1);

    /* Wait for handler to receive SIGCHLD. */
    while (ret == 0 && sq->reaped != sq->child) {
	if (nothreads)
	    /* Note that sigpause re-enables SIGCHLD. */
	    ret = sigpause(SIGCHLD);
	else {
	    xx = sigrelse(SIGCHLD);
	    
	    /* 
	     * We start before the fork with this mutex locked;
	     * The only one that unlocks this the signal handler.
	     * So if we get the lock the child has been reaped.
	     */
	    ret = pthread_mutex_lock(&sq->mutex);
	    xx = sighold(SIGCHLD);
	}
    }

    /* Accumulate stopwatch time spent waiting, potential performance gain. */
    sq->ms_scriptlets += rpmswExit(&sq->op, -1)/1000;

    xx = sigrelse(SIGCHLD);

#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "      Wake(%p): %p child %d reaper %d ret %d\n", ME(), sq, sq->child, sq->reaper, ret);
#endif

    /* Remove processed SIGCHLD item from queue. */
    xx = rpmsqRemove(sq);

    /* Disable SIGCHLD handler on refcount == 0. */
    xx = rpmsqEnable(-SIGCHLD, NULL);
#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "   Disable(%p): %p\n", ME(), sq);
#endif

    return ret;
}

pid_t rpmsqWait(rpmsq sq)
{

#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "      Wait(%p): %p child %d reaper %d\n", ME(), sq, sq->child, sq->reaper);
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
if (_rpmsq_debug)
fprintf(stderr, "   Waitpid(%p): %p child %d reaped %d\n", ME(), sq, sq->child, sq->reaped);
#endif
    }

#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "      Fini(%p): %p child %d status 0x%x\n", ME(), sq, sq->child, sq->status);
#endif

    return sq->reaped;
}

void * rpmsqThread(void * (*start) (void * arg), void * arg)
{
    pthread_t pth;
    int ret;

    ret = pthread_create(&pth, NULL, start, arg);
    return (ret == 0 ? (void *)pth : NULL);
}

int rpmsqJoin(void * thread)
{
    pthread_t pth = (pthread_t) thread;
    if (thread == NULL)
	return EINVAL;
    return pthread_join(pth, NULL);
}

int rpmsqThreadEqual(void * thread)
{
    pthread_t t1 = (pthread_t) thread;
    pthread_t t2 = pthread_self();
    return pthread_equal(t1, t2);
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

    (void) DO_LOCK ();
    if (SUB_REF (rpmsigTbl_sigchld) == 0) {
	(void) rpmsqEnable(-SIGQUIT, NULL);
	(void) rpmsqEnable(-SIGINT, NULL);
    }
    (void) DO_UNLOCK ();
}

/**
 * Execute a command, returning its status.
 */
int
rpmsqExecve (const char ** argv)
{
    int oldtype;
    int status = -1;
    pid_t pid = 0;
    pid_t result;
    sigset_t newMask, oldMask;

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
	INIT_LOCK ();
#endif

    (void) DO_LOCK ();
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
    (void) DO_UNLOCK ();

    (void) sigemptyset (&newMask);
    (void) sigaddset (&newMask, SIGCHLD);
    if (sigprocmask (SIG_BLOCK, &newMask, &oldMask) < 0) {
	(void) DO_LOCK ();
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

    (void) DO_LOCK ();
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
    (void) DO_UNLOCK ();
    return status;
}
