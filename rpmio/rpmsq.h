#ifndef	H_RPMSQ
#define	H_RPMSQ

/** \ingroup rpmio
 * \file rpmio/rpmsq.h
 *
 */

#include <signal.h>
#include <sys/signal.h>
#include <search.h>

typedef struct rpmsig_s * rpmsig;

typedef struct rpmsqElem * rpmsq;

/**
 * SIGCHLD queue element.
 */
struct rpmsqElem {
    struct rpmsqElem * q_forw;	/*!< for use by insque(3)/remque(3). */
    struct rpmsqElem * q_back;
    pid_t child;		/*!< Currently running child. */
    pid_t reaped;		/*!< Reaped waitpid(3) return. */
    int status;			/*!< Reaped waitpid(3) status. */
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
void Insque(/*@null@*/ void * elem, /*@null@*/ void * prev)
	/*@globals rpmsqQueue @*/
	/*@modifies elem, rpmsqQueue @*/;

/**
 */
void Remque(/*@null@*/ void * elem)
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
 * Execute a command, returning its status.
 */
int
rpmsqExecve (const char ** argv)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSQ */
