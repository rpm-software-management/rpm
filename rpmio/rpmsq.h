#ifndef	H_RPMSQ
#define	H_RPMSQ

/** \ingroup rpmio
 * \file rpmio/rpmsq.h
 *
 */

#include <pthread.h>
#include <signal.h>
#include <sys/signal.h>
#include <search.h>		/* XXX insque(3)/remque(3) protos. */

typedef struct rpmsig_s * rpmsig;

typedef struct rpmsqElem * rpmsq;

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
    int reaper;			/*!< Register SIGCHLD handler? */
    void * id;			/*!< Blocking thread id (pthread_t). */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

/*@unchecked@*/
extern rpmsq rpmsqQueue;

/*@unchecked@*/
extern sigset_t rpmsqCaught;

#ifdef __cplusplus
{
#endif

/**
 */
int rpmsqInsert(/*@null@*/ void * elem, /*@null@*/ void * prev)
	/*@globals rpmsqQueue @*/
	/*@modifies elem, rpmsqQueue @*/;

/**
 */
int rpmsqRemove(/*@null@*/ void * elem)
	/*@modifies elem @*/;

/**
 */
void rpmsqHandler(int signum)
	/*@globals rpmsqCaught, fileSystem @*/
	/*@modifies rpmsqCaught, fileSystem @*/;

/**
 * Enable or disable a signal handler.
 * @param signum	signal to enable (or disable if negative)
 * @param handler	signal handler (or NULL to use rpmsqHandler())
 * @return		no. of refs, -1 on error
 */
int rpmsqEnable(int signum, /*@null@*/ sighandler_t handler)
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
 * Execute a command, returning its status.
 */
int rpmsqExecve (const char ** argv)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSQ */
