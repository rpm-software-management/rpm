#ifndef	H_RPMSW
#define	H_RPMSW

/** \ingroup rpmio
 * \file rpmio/rpmsw.h
 */

/** \ingroup rpmio
 */
typedef unsigned long int rpmtime_t;

/** \ingroup rpmio
 */
typedef struct rpmsw_s * rpmsw;

/** \ingroup rpmio
 */
typedef struct rpmop_s * rpmop;

/** \ingroup rpmio
 */
struct rpmsw_s {
    union {
	struct timeval tv;
	unsigned long long int ticks;
	unsigned long int tocks[2];
    } u;
};

/** \ingroup rpmio
 * Cumulative statistics for an operation.
 */
struct rpmop_s {
    struct rpmsw_s	begin;	/*!< Starting time stamp. */
    int			count;	/*!< Number of operations. */
    size_t		bytes;	/*!< Number of bytes transferred. */
    rpmtime_t		usecs;	/*!< Number of ticks. */
};

#ifdef __cplusplus
extern "C" {
#endif

/** Return benchmark time stamp.
 * @param *sw		time stamp
 * @return		0 on success
 */
/*@null@*/
rpmsw rpmswNow(/*@returned@*/ rpmsw sw)
	/*@globals internalState @*/
	/*@modifies sw, internalState @*/;

/** Return benchmark time stamp difference.
 * @param *end		end time stamp
 * @param *begin	begin time stamp
 * @return		difference in micro-seconds
 */
rpmtime_t rpmswDiff(/*@null@*/ rpmsw end, /*@null@*/ rpmsw begin)
	/*@*/;

/** Return benchmark time stamp overhead.
 * @return		overhead in micro-seconds
 */
rpmtime_t rpmswInit(void)
	/*@globals internalState @*/
	/*@modifes internalState @*/;

/** \ingroup rpmio
 * Enter timed operation.
 * @param op			operation statistics
 * @return			0 always
 */
int rpmswEnter(rpmop op)
	/*@globals internalState @*/
	/*@modifes op->count, op->begin, internalState @*/;

/** \ingroup rpmio
 * Exit timed operation.
 * @param op			operation statistics
 * @param rc			per-operation data (e.g. bytes transferred)
 * @return			0 always
 */
int rpmswExit(rpmop op, ssize_t rc)
	/*@globals internalState @*/
	/*@modifes op-bytes, op->usecs, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO_INTERNAL */
