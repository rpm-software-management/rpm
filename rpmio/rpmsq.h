#ifndef	H_RPMSQ
#define	H_RPMSQ

/** \ingroup rpmio
 * \file rpmio/rpmsq.h
 *
 */

#include <rpmsw.h>

/**
 */
typedef struct rpmsig_s * rpmsig;

/**
 */
typedef struct rpmsqElem * rpmsq;

/**
 * Default signal handler prototype.
 * @param signum	signal number
 * @param info		(siginfo_t) signal info
 * @param context	signal context
 */
typedef void (*rpmsqAction_t) (int signum, void * info, void * context)
	/*@*/;

/*@-redecl@*/
/*@unchecked@*/
extern int _rpmsq_debug;
/*@=redecl@*/

/**
 * SIGCHLD queue element.
 */
struct rpmsqElem {
    struct rpmsqElem * q_forw;	/*!< for use by insque(3)/remque(3). */
    struct rpmsqElem * q_back;
    pid_t child;		/*!< Currently running child. */
    volatile pid_t reaped;	/*!< Reaped waitpid(3) return. */
    volatile int status;	/*!< Reaped waitpid(3) status. */
    struct rpmop_s op;		/*!< Scriptlet operation timestamp; */
    rpmtime_t ms_scriptlets;	/*!< Accumulated script duration (msecs). */
    int reaper;			/*!< Register SIGCHLD handler? */
    int pipes[2];		/*!< Parent/child interlock. */
    void * id;			/*!< Blocking thread id (pthread_t). */
    pthread_mutex_t mutex;	/*!< Signal delivery to thread condvar. */
    pthread_cond_t cond;
};

/*@-exportlocal@*/
/*@unchecked@*/
extern rpmsq rpmsqQueue;
/*@=exportlocal@*/

/*@unchecked@*/
extern sigset_t rpmsqCaught;

#ifdef __cplusplus
{
#endif

/**
 * Insert node into from queue.
 * @param elem		node to link
 * @param prev		previous node from queue
 * @return		0 on success
 */
/*@-exportlocal@*/
int rpmsqInsert(/*@null@*/ void * elem, /*@null@*/ void * prev)
	/*@modifies elem @*/;
/*@=exportlocal@*/

/**
 * Remove node from queue.
 * @param elem		node to link
 * @return		0 on success
 */
/*@-exportlocal@*/
int rpmsqRemove(/*@null@*/ void * elem)
	/*@globals fileSystem, internalState @*/
	/*@modifies elem, fileSystem, internalState @*/;
/*@=exportlocal@*/

/**
 * Default signal handler.
 * @param signum	signal number
 * @param info		(siginfo_t) signal info
 * @param context	signal context
 */
/*@-exportlocal@*/
void rpmsqAction(int signum, void * info, void * context)
	/*@globals rpmsqCaught, errno, fileSystem @*/
	/*@modifies rpmsqCaught, errno, fileSystem @*/;
/*@=exportlocal@*/

/**
 * Enable or disable a signal handler.
 * @param signum	signal to enable (or disable if negative)
 * @param handler	sa_sigaction handler (or NULL to use rpmsqHandler())
 * @return		no. of refs, -1 on error
 */
int rpmsqEnable(int signum, /*@null@*/ rpmsqAction_t handler)
	/*@globals rpmsqCaught, fileSystem, internalState @*/
	/*@modifies rpmsqCaught, fileSystem, internalState @*/;

/**
 * Fork a child process.
 * @param sq		scriptlet queue element
 * @return		fork(2) pid
 */
pid_t rpmsqFork(rpmsq sq)
	/*@globals fileSystem, internalState @*/
	/*@modifies sq, fileSystem, internalState @*/;

/**
 * Wait for child process to be reaped.
 * @param sq		scriptlet queue element
 * @return		reaped child pid
 */
pid_t rpmsqWait(rpmsq sq)
	/*@globals fileSystem, internalState @*/
	/*@modifies sq, fileSystem, internalState @*/;

/**
 * Call a function in a thread.
 * @param start		function
 * @param arg		function argument
 * @return		thread pointer (NULL on error)
 */
void * rpmsqThread(void * (*start) (void * arg), void * arg)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/**
 * Wait for thread to terminate.
 * @param thread	thread
 * @return		0 on success
 */
int rpmsqJoin(/*@null@*/ void * thread)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/**
 * Compare thread with current thread.
 * @param thread	thread
 * @return		0 if not equal
 */
int rpmsqThreadEqual(/*@null@*/ void * thread)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/**
 * Execute a command, returning its status.
 */
int rpmsqExecve (const char ** argv)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSQ */
