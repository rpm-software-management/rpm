#ifndef	H_RPMSQ
#define	H_RPMSQ

/** \ingroup rpmio
 * \file rpmsq.h
 *
 * Signal Queue API (obsolete, do not use)
 */
#include <rpm/rpmsw.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmsq
 * @deprecated		Obsolete, do not use.
 *
 * Default signal handler prototype.
 * @param signum	signal number
 * @param info		(siginfo_t) signal info
 * @param context	signal context
 */
typedef void (*rpmsqAction_t) (int signum, siginfo_t * info, void * context);

/** \ingroup rpmsq
 *  SIG_DFL, SIG_IGN and SIG_ERR counterparts
 */
#define RPMSQ_DFL	((rpmsqAction_t)0)
#define RPMSQ_IGN	((rpmsqAction_t)1)
#define RPMSQ_ERR	((rpmsqAction_t)-1)

/** \ingroup rpmsq
 * @deprecated		Obsolete, do not use.
 *
 * Test if given signal has been caught (while signals blocked).
 * Similar to sigismember() but operates on internal signal queue.
 * @param signum	signal to test for
 * @return		1 if caught, 0 if not and -1 on error
 */
RPM_GNUC_DEPRECATED
int rpmsqIsCaught(int signum);

/** \ingroup rpmsq
 * Activate (or disable) the signal queue.
 * @param state		1 to enable, 0 to disable
 * @return		0 on success, negative on error
 */
RPM_GNUC_DEPRECATED
int rpmsqActivate(int state);

/** \ingroup rpmsq
 * @deprecated		Obsolete, do not use.
 *
 * Set or delete a signal handler for a signal.
 * @param signum	signal number
 * @param handler	signal handler or NULL to delete
 * @return		previous handler, RPMSQ_ERR on error
 */
RPM_GNUC_DEPRECATED
rpmsqAction_t rpmsqSetAction(int signum, rpmsqAction_t handler);

/** \ingroup rpmsq
 * Block or unblock (almost) all signals.
 * The operation is "reference counted" so the calls can be nested,
 * and signals are only unblocked when the reference count falls to zero.
 * @param op		SIG_BLOCK/SIG_UNBLOCK
 * @return		0 on success, -1 on error
 */
int rpmsqBlock(int op);

/** \ingroup rpmsq
 * @deprecated		Obsolete, do not use.
 *
 * Poll for caught signals, executing their handlers.
 * @return		no. active signals found
 */
RPM_GNUC_DEPRECATED
int rpmsqPoll(void);

/** \ingroup rpmsq
 * @deprecated		Obsolete, do not use.
 */
RPM_GNUC_DEPRECATED
void rpmsqSetInterruptSafety(int on);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSQ */
