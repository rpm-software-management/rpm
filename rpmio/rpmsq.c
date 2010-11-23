/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"

#include <signal.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <search.h>
#include <errno.h>
#include <stdio.h>

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
#define	ADD_REF(__tbl)	(__tbl)->active++
#define	SUB_REF(__tbl)	--(__tbl)->active

#define	ME()	((void *)pthread_self())

#define _RPMSQ_INTERNAL
#include <rpm/rpmsq.h>

#include "debug.h"

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
    } else {				/* Parent. */
	sq->child = pid;
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

    /* Remove processed SIGCHLD item from queue. */
    xx = rpmsqRemove(sq);

    /* Disable SIGCHLD handler on refcount == 0. */
    xx = rpmsqEnable(-SIGCHLD, NULL);

    return ret;
}

pid_t rpmsqWait(rpmsq sq)
{
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
    }

    return sq->reaped;
}
