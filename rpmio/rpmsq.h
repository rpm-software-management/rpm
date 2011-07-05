#ifndef	H_RPMSQ
#define	H_RPMSQ

/** \ingroup rpmio
 * \file rpmio/rpmsq.h
 *
 */
#include <rpm/rpmsw.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmsq
 * Default signal handler prototype.
 * @param signum	signal number
 * @param info		(siginfo_t) signal info
 * @param context	signal context
 */
#ifdef SA_SIGINFO
typedef void (*rpmsqAction_t) (int signum, siginfo_t * info, void * context);
#else
typedef void (*rpmsqAction_t) (int signum);
#endif

/** \ingroup rpmsq
 * Test if given signal has been caught (while signals blocked).
 * Similar to sigismember() but operates on internal signal queue.
 * @param signum	signal to test for
 * @return		1 if caught, 0 if not and -1 on error
 */
int rpmsqIsCaught(int signum);

/** \ingroup rpmsq
 * Default signal handler.
 * @param signum	signal number
 * @param info		(siginfo_t) signal info
 * @param context	signal context
 */
#ifdef SA_SIGINFO
void rpmsqAction(int signum, siginfo_t * info, void * context);
#else
void rpmsqAction(int signum);
#endif

/** \ingroup rpmsq
 * Enable or disable a signal handler.
 * @param signum	signal to enable (or disable if negative)
 * @param handler	sa_sigaction handler (or NULL to use rpmsqHandler())
 * @return		no. of refs, -1 on error
 */
int rpmsqEnable(int signum, rpmsqAction_t handler);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSQ */
