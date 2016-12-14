#ifndef	H_RPMSQ
#define	H_RPMSQ

/** \ingroup rpmio
 * \file rpmio/rpmsq.h
 *
 * Signal Queue API
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
typedef void (*rpmsqAction_t) (int signum, siginfo_t * info, void * context);

/** \ingroup rpmsq
 * Test if given signal has been caught (while signals blocked).
 * Similar to sigismember() but operates on internal signal queue.
 * @param signum	signal to test for
 * @return		1 if caught, 0 if not and -1 on error
 */
int rpmsqIsCaught(int signum);

/** \ingroup rpmsq
 * Enable or disable a signal handler.
 * @param signum	signal to enable (or disable if negative)
 * @param handler	signal handler (or NULL to use default)
 * @return		no. of refs, -1 on error
 */
int rpmsqEnable(int signum, rpmsqAction_t handler);

/** \ingroup rpmsq
 * Poll for caught signals, executing their handlers.
 * @return		no. active signals found
 */
int rpmsqPoll(void);

void rpmsqSetInterruptSafety(int on);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSQ */
