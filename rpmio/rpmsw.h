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
struct rpmsw_s {
    union {
	struct timeval tv;
	unsigned long long int ticks;
	unsigned long int tocks[2];
    } u;
};

#define	RPMSW_SCALE	(1000 * 1000)

#ifdef __cplusplus
extern "C" {
#endif

/** Return benchmark time stamp.
 * @param *sw		time stamp
 * @return		0 on success
 */
/*@null@*/
rpmsw rpmswNow(/*@returned@*/ rpmsw sw)
	/*@modifies sw @*/;

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
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO_INTERNAL */
