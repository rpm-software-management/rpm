/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"

#if defined(__LCLINT__)
#define	_BITS_SIGTHREAD_H	/* XXX avoid __sigset_t heartburn. */

/*@-exportheader@*/
/*@constant int SA_SIGINFO@*/
extern int sighold(int sig)
	/*@globals errno, systemState @*/;
extern int sigignore(int sig)
	/*@globals errno, systemState @*/;
extern int sigpause(int sig)
	/*@globals errno, systemState @*/;
extern int sigrelse(int sig)
	/*@globals errno, systemState @*/;
extern void (*sigset(int sig, void (*disp)(int)))(int)
	/*@globals errno, systemState @*/;

struct qelem;
extern	void insque(struct qelem * __elem, struct qelem * __prev)
	/*@modifies  __elem, __prev @*/;
extern	void remque(struct qelem * __elem)
	/*@modifies  __elem @*/;

extern pthread_t pthread_self(void)
	/*@*/;
extern int pthread_equal(pthread_t t1, pthread_t t2)
	/*@*/;

extern int pthread_create(/*@out@*/ pthread_t *restrict thread,
		const pthread_attr_t *restrict attr,
		void *(*start_routine)(void*), void *restrict arg)
	/*@modifies *thread @*/;
extern int pthread_join(pthread_t thread, /*@out@*/ void **value_ptr)
	/*@modifies *value_ptr @*/;

extern int pthread_setcancelstate(int state, /*@out@*/ int *oldstate)
	/*@globals internalState @*/
	/*@modifies *oldstate, internalState @*/;
extern int pthread_setcanceltype(int type, /*@out@*/ int *oldtype)
	/*@globals internalState @*/
	/*@modifies *oldtype, internalState @*/;
extern void pthread_testcancel(void)
	/*@globals internalState @*/
	/*@modifies internalState @*/;
extern void pthread_cleanup_pop(int execute)
	/*@globals internalState @*/
	/*@modifies internalState @*/;
extern void pthread_cleanup_push(void (*routine)(void*), void *arg)
	/*@globals internalState @*/
	/*@modifies internalState @*/;
extern void _pthread_cleanup_pop(/*@out@*/ struct _pthread_cleanup_buffer *__buffer, int execute)
	/*@globals internalState @*/
	/*@modifies internalState @*/;
extern void _pthread_cleanup_push(/*@out@*/ struct _pthread_cleanup_buffer *__buffer, void (*routine)(void*), /*@out@*/ void *arg)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

extern int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
	/*@modifies *attr @*/;
extern int pthread_mutexattr_init(/*@out@*/ pthread_mutexattr_t *attr)
	/*@modifies *attr @*/;

int pthread_mutexattr_gettype(const pthread_mutexattr_t *restrict attr,
		/*@out@*/ int *restrict type)
	/*@modifies *type @*/;
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
	/*@modifies *attr @*/;

extern int pthread_mutex_destroy(pthread_mutex_t *mutex)
	/*@modifies *mutex @*/;
extern int pthread_mutex_init(/*@out@*/ pthread_mutex_t *restrict mutex,
		/*@null@*/ const pthread_mutexattr_t *restrict attr)
	/*@modifies *mutex @*/;

extern int pthread_mutex_lock(pthread_mutex_t *mutex)
	/*@modifies *mutex @*/;
extern int pthread_mutex_trylock(pthread_mutex_t *mutex)
	/*@modifies *mutex @*/;
extern int pthread_mutex_unlock(pthread_mutex_t *mutex)
	/*@modifies *mutex @*/;

extern int pthread_cond_destroy(pthread_cond_t *cond)
	/*@modifies *cond @*/;
extern int pthread_cond_init(/*@out@*/ pthread_cond_t *restrict cond,
		const pthread_condattr_t *restrict attr)
	/*@modifies *cond @*/;

extern int pthread_cond_timedwait(pthread_cond_t *restrict cond,
		pthread_mutex_t *restrict mutex,
		const struct timespec *restrict abstime)
	/*@modifies *cond, *mutex @*/;
extern int pthread_cond_wait(pthread_cond_t *restrict cond,
		pthread_mutex_t *restrict mutex)
	/*@modifies *cond, *mutex @*/;
extern int pthread_cond_broadcast(pthread_cond_t *cond)
	/*@modifies *cond @*/;
extern int pthread_cond_signal(pthread_cond_t *cond)
	/*@modifies *cond @*/;

/*@=exportheader@*/
#endif

#include <signal.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <search.h>

#if defined(HAVE_PTHREAD_H)

#include <pthread.h>

/*@unchecked@*/
/*@-type@*/
static pthread_mutex_t rpmsigTbl_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
/*@=type@*/

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
    (void) pthread_cleanup_pop(__execute); \
    (void) pthread_setcanceltype ((__oldtype), &(__oldtype));

#define	SAME_THREAD(_a, _b)	pthread_equal(((pthread_t)_a), ((pthread_t)_b))

#define	ME()	((void *)pthread_self())

#else

#define	DO_LOCK()
#define	DO_UNLOCK()
#define	INIT_LOCK()
#define	ADD_REF(__tbl)	/*@-noeffect@*/ (0) /*@=noeffect@*/
#define	SUB_REF(__tbl)	/*@-noeffect@*/ (0) /*@=noeffect@*/
#define	CLEANUP_HANDLER(__handler, __arg, __oldtypeptr)
#define	CLEANUP_RESET(__execute, __oldtype)

#define	SAME_THREAD(_a, _b)	(42)

#define	ME()	(((void *)getpid()))

#endif	/* HAVE_PTHREAD_H */

#include <rpmsq.h>

#include "debug.h"

#define	_RPMSQ_DEBUG	0
/*@unchecked@*/
int _rpmsq_debug = _RPMSQ_DEBUG;

/*@unchecked@*/
static struct rpmsqElem rpmsqRock;

/*@-compmempass@*/
/*@unchecked@*/
rpmsq rpmsqQueue = &rpmsqRock;
/*@=compmempass@*/

int rpmsqInsert(void * elem, void * prev)
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
/*@-bounds@*/
	    sq->pipes[0] = sq->pipes[1] = -1;
/*@=bounds@*/

	    sq->id = ME();
	    ret = pthread_mutex_init(&sq->mutex, NULL);
	    ret = pthread_cond_init(&sq->cond, NULL);
	    insque(elem, (prev != NULL ? prev : rpmsqQueue));
	    ret = sigrelse(SIGCHLD);
	}
    }
    return ret;
}

int rpmsqRemove(void * elem)
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
	    ret = pthread_cond_destroy(&sq->cond);
	    ret = pthread_mutex_destroy(&sq->mutex);
	    sq->id = NULL;
/*@-bounds@*/
	    if (sq->pipes[1])	ret = close(sq->pipes[1]);
	    if (sq->pipes[0])	ret = close(sq->pipes[0]);
	    sq->pipes[0] = sq->pipes[1] = -1;
/*@=bounds@*/
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

/*@unchecked@*/
sigset_t rpmsqCaught;

/*@unchecked@*/
/*@-fullinitblock@*/
static struct rpmsig_s {
    int signum;
    void (*handler) (int signum, void * info, void * context);
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
/*@=fullinitblock@*/

void rpmsqAction(int signum,
		/*@unused@*/ void * info, /*@unused@*/ void * context)
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
		    if (sq->child != reaped)
			/*@innercontinue@*/ continue;
		    sq->reaped = reaped;
		    sq->status = status;
		    (void) pthread_cond_signal(&sq->cond);
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

int rpmsqEnable(int signum, /*@null@*/ rpmsqAction_t handler)
	/*@globals rpmsigTbl @*/
	/*@modifies rpmsigTbl @*/
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
		(void) sigemptyset (&sa.sa_mask);
/*@-compdef -type @*/
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = (handler != NULL ? handler : tbl->handler);
		if (sigaction(tbl->signum, &sa, &tbl->oact) < 0) {
		    SUB_REF(tbl);
		    break;
		}
/*@=compdef =type @*/
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

    pid = fork();
    if (pid < (pid_t) 0) {		/* fork failed.  */
/*@-bounds@*/
	xx = close(sq->pipes[0]);
	xx = close(sq->pipes[1]);
	sq->pipes[0] = sq->pipes[1] = -1;
/*@=bounds@*/
	goto out;
    } else if (pid == (pid_t) 0) {	/* Child. */
	int yy;

	/* Block to permit parent to wait. */
/*@-bounds@*/
	xx = close(sq->pipes[1]);
	xx = read(sq->pipes[0], &yy, sizeof(yy));
	xx = close(sq->pipes[0]);
	sq->pipes[0] = sq->pipes[1] = -1;
/*@=bounds@*/

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
 * @param sq		scriptlet queue element
 * @return		0 on success
 */
static int rpmsqWaitUnregister(rpmsq sq)
	/*@globals fileSystem, internalState @*/
	/*@modifies sq, fileSystem, internalState @*/
{
    int same_thread = 0;
    int ret = 0;
    int xx;

    if (same_thread)
	ret = sighold(SIGCHLD);
    else
	ret = pthread_mutex_lock(&sq->mutex);

    /* Start the child. */
/*@-bounds@*/
    if (sq->pipes[0] >= 0)
	xx = close(sq->pipes[0]);
    if (sq->pipes[1] >= 0)
	xx = close(sq->pipes[1]);
    sq->pipes[0] = sq->pipes[1] = -1;
/*@=bounds@*/

    (void) rpmswEnter(&sq->op, -1);

    /*@-infloops@*/
    while (ret == 0 && sq->reaped != sq->child) {
	if (same_thread)
	    ret = sigpause(SIGCHLD);
	else
	    ret = pthread_cond_wait(&sq->cond, &sq->mutex);
    }
    /*@=infloops@*/

    sq->ms_scriptlets += rpmswExit(&sq->op, -1)/1000;

    if (same_thread)
	xx = sigrelse(SIGCHLD);
    else
	xx = pthread_mutex_unlock(&sq->mutex);

#ifdef _RPMSQ_DEBUG
if (_rpmsq_debug)
fprintf(stderr, "      Wake(%p): %p child %d reaper %d ret %d\n", ME(), sq, sq->child, sq->reaper, ret);
#endif

    xx = rpmsqRemove(sq);
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
	/*@globals rpmsigTbl, fileSystem, internalState @*/
	/*@modifies rpmsigTbl, fileSystem, internalState @*/
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
	/*@globals rpmsigTbl @*/
	/*@modifies rpmsigTbl @*/
{
    int oldtype;
    int status = -1;
    pid_t pid = 0;
    pid_t result;
    sigset_t newMask, oldMask;
    rpmsq sq = memset(alloca(sizeof(*sq)), 0, sizeof(*sq));

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
