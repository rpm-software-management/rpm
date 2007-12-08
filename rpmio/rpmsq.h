#ifndef	H_RPMSQ
#define	H_RPMSQ

/** \ingroup rpmio
 * \file rpmio/rpmsq.h
 *
 */

#include <rpm/rpmsw.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmsq
 */
typedef struct rpmsig_s * rpmsig;

/** \ingroup rpmsq
 */
typedef struct rpmsqElem * rpmsq;

/** \ingroup rpmsq
 * Default signal handler prototype.
 * @param signum	signal number
 * @param info		(siginfo_t) signal info
 * @param context	signal context
 */
typedef void (*rpmsqAction_t) (int signum, void * info, void * context);

extern int _rpmsq_debug;

/* XXX make this fully opaque? */
#if defined(_RPMSQ_INTERNAL)
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
#endif /* _RPMSQ_INTERNAL */

extern rpmsq rpmsqQueue;

extern sigset_t rpmsqCaught;

/** \ingroup rpmsq
 * Insert node into from queue.
 * @param elem		node to link
 * @param prev		previous node from queue
 * @return		0 on success
 */
int rpmsqInsert(void * elem, void * prev);

/** \ingroup rpmsq
 * Remove node from queue.
 * @param elem		node to link
 * @return		0 on success
 */
int rpmsqRemove(void * elem);

/** \ingroup rpmsq
 * Default signal handler.
 * @param signum	signal number
 * @param info		(siginfo_t) signal info
 * @param context	signal context
 */
void rpmsqAction(int signum, void * info, void * context);

/** \ingroup rpmsq
 * Enable or disable a signal handler.
 * @param signum	signal to enable (or disable if negative)
 * @param handler	sa_sigaction handler (or NULL to use rpmsqHandler())
 * @return		no. of refs, -1 on error
 */
int rpmsqEnable(int signum, rpmsqAction_t handler);

/** \ingroup rpmsq
 * Fork a child process.
 * @param sq		scriptlet queue element
 * @return		fork(2) pid
 */
pid_t rpmsqFork(rpmsq sq);

/** \ingroup rpmsq
 * Wait for child process to be reaped.
 * @param sq		scriptlet queue element
 * @return		reaped child pid
 */
pid_t rpmsqWait(rpmsq sq);

/** \ingroup rpmsq
 * Call a function in a thread.
 * @param start		function
 * @param arg		function argument
 * @return		thread pointer (NULL on error)
 */
void * rpmsqThread(void * (*start) (void * arg), void * arg);

/** \ingroup rpmsq
 * Wait for thread to terminate.
 * @param thread	thread
 * @return		0 on success
 */
int rpmsqJoin(void * thread);

/** \ingroup rpmsq
 * Compare thread with current thread.
 * @param thread	thread
 * @return		0 if not equal
 */
int rpmsqThreadEqual(void * thread);

/** \ingroup rpmsq
 * Execute a command, returning its status.
 */
int rpmsqExecve (const char ** argv);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSQ */
