/** \ingroup rpmio
 * \file rpmio/rpmsw.c
 */

#include "system.h"
#include <rpmsw.h>
#include "debug.h"

#if defined(__LCLINT__)
/*@-exportheader@*/
extern int nanosleep(const struct timespec *__requested_time,
		/*@out@*/ /*@null@*/ struct timespec *__remaining)
	/*@globals errno @*/
	/*@modifies *__remaining, errno @*/;
/*@=exportheader@*/
#endif

/*@unchecked@*/
static rpmtime_t rpmsw_overhead = 0;

/*@unchecked@*/
static rpmtime_t rpmsw_cycles = 1;

/*@unchecked@*/
static int rpmsw_initialized = 0;

rpmsw rpmswNow(rpmsw sw)
{
    if (!rpmsw_initialized)
	(void) rpmswInit();
    if (sw == NULL)
	return NULL;
    if (gettimeofday(&sw->u.tv, NULL))
    	return NULL;
    return sw;
}

/** \ingroup rpmio
 * Return difference of 2 timeval stamps in micro-seconds.
 * @param *etv		end timeval
 * @param *btv		begin timeval
 * @return		difference in milli-seconds
 */
static inline
rpmtime_t tvsub(/*@null@*/ const struct timeval * etv,
		/*@null@*/ const struct timeval * btv)
	/*@*/
{
    time_t secs, usecs;
    if (etv == NULL  || btv == NULL) return 0;
    secs = etv->tv_sec - btv->tv_sec;
    for (usecs = etv->tv_usec - btv->tv_usec; usecs < 0; usecs += 1000000)
	secs--;
    return ((secs * 1000000) + usecs);
}

rpmtime_t rpmswDiff(rpmsw end, rpmsw begin)
{
    unsigned long long ticks = 0;

    if (end == NULL || begin == NULL)
	return 0;
    ticks = tvsub(&end->u.tv, &begin->u.tv);
    if (ticks >= rpmsw_overhead)
	ticks -= rpmsw_overhead;
    if (rpmsw_cycles > 1)
	ticks /= rpmsw_cycles;
    return ticks;
}

rpmtime_t rpmswInit(void)
	/*@globals rpmsw_cycles, rpmsw_initialized, rpmsw_overhead,
		rpmsw_type @*/
	/*@modifies rpmsw_cycles, rpmsw_initialized, rpmsw_overhead,
		rpmsw_type @*/
{
    struct rpmsw_s begin, end;
    rpmtime_t sum_overhead = 0;
    rpmtime_t cycles;
    int i;

    rpmsw_initialized = 1;

    rpmsw_overhead = 0;
    rpmsw_cycles = 0;

    /* Convergence for simultaneous cycles and overhead is overkill ... */
    for (i = 0; i < 3; i++) {
	/* Calculate timing overhead in usecs. */
/*@-uniondef@*/
	(void) rpmswNow(&begin);
	sum_overhead += rpmswDiff(rpmswNow(&end), &begin);
/*@=uniondef@*/

	rpmsw_overhead = sum_overhead/(i+1);
    }

    return rpmsw_overhead;
}

int rpmswEnter(rpmop op, ssize_t rc)
{
    if (op == NULL)
	return 0;

    op->count++;
    if (rc < 0) {
	op->bytes = 0;
	op->usecs = 0;
    }
/*@-uniondef@*/
    (void) rpmswNow(&op->begin);
/*@=uniondef@*/
    return 0;
}

rpmtime_t rpmswExit(rpmop op, ssize_t rc)
{
    struct rpmsw_s end;

    if (op == NULL)
	return 0;

/*@-uniondef@*/
    op->usecs += rpmswDiff(rpmswNow(&end), &op->begin);
/*@=uniondef@*/
    if (rc > 0)
	op->bytes += rc;
    op->begin = end;	/* structure assignment */
    return op->usecs;
}

rpmtime_t rpmswAdd(rpmop to, rpmop from)
{
    rpmtime_t usecs = 0;
    if (to != NULL && from != NULL) {
	to->count += from->count;
	to->bytes += from->bytes;
	to->usecs += from->usecs;
	usecs = to->usecs;
    }
    return usecs;
}

rpmtime_t rpmswSub(rpmop to, rpmop from)
{
    rpmtime_t usecs = 0;
    if (to != NULL && from != NULL) {
	to->count -= from->count;
	to->bytes -= from->bytes;
	to->usecs -= from->usecs;
	usecs = to->usecs;
    }
    return usecs;
}
