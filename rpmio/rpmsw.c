/** \ingroup rpmio
 * \file rpmio/rpmsw.c
 */

#include "system.h"
#include <rpmsw.h>
#include "debug.h"

/*@unchecked@*/
static rpmtime_t rpmsw_overhead = 0;

/*@unchecked@*/
static rpmtime_t rpmsw_cycles = 1;

/*@unchecked@*/
static int rpmsw_type = 0;

#if defined(__i386__)
static inline unsigned long long do_rdtsc ( void )
	/*@*/
{
   unsigned long long x;
   __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
   return x;
}
#endif

rpmsw rpmswNow(rpmsw sw)
{
    static int oneshot = 0;
    if (oneshot == 0) {
	oneshot = 1;
	rpmswInit();
    }
    if (sw == NULL)
	return NULL;
    switch (rpmsw_type) {
    case 0:
	if (gettimeofday(&sw->u.tv, NULL))
	    return NULL;
	break;
#if defined(__i386__)
    case 1:
	sw->u.ticks = do_rdtsc();
	break;
#endif
    }
    return sw;
}

/** \ingroup rpmio
 * Return difference of 2 timeval stamps in micro-seconds.
 * @param *etv		end timeval
 * @param *btv		begin timeval
 * @return		difference in milli-seconds
 */
/*@unused@*/ static inline
rpmtime_t tvsub(/*@null@*/ const struct timeval * etv,
		/*@null@*/ const struct timeval * btv)
	/*@*/
{
    rpmtime_t secs, usecs;
    if (etv == NULL  || btv == NULL) return 0;
    secs = etv->tv_sec - btv->tv_sec;
    for (usecs = etv->tv_usec - btv->tv_usec; usecs < 0; usecs += 1000000)
	secs++;
    return ((secs * 1000000) + usecs);
}

rpmtime_t rpmswDiff(rpmsw end, rpmsw begin)
{
    rpmtime_t diff = 0;

    if (end == NULL || begin == NULL)
	return 0;
    switch (rpmsw_type) {
    default:
    case 0:
	diff = tvsub(&end->u.tv, &begin->u.tv);
	break;
#if defined(__i386__)
    case 1:
	if (end->u.ticks > begin->u.ticks)
	    diff = end->u.ticks - begin->u.ticks;
	break;
#endif
    }
    if (diff >= rpmsw_overhead)
	diff -= rpmsw_overhead;
    if (rpmsw_cycles > 1)
	diff /= rpmsw_cycles;
    return diff;
}

static rpmtime_t rpmswCalibrate(void)
	/*@*/
{
    struct rpmsw_s begin, end;
    rpmtime_t ticks;
    struct timespec req, rem;
    int rc;
    int i;

    (void) rpmswNow(&begin);
    req.tv_sec = 0;
    req.tv_nsec = 20 * 1000 * 1000;
    for (i = 0; i < 100; i++) {
	rc = nanosleep(&req, &rem);
	if (rc == 0)
	    break;
	if (rem.tv_sec == 0 && rem.tv_nsec == 0)
	    break;
	req = rem;	/* structure assignment */
    }
    ticks = rpmswDiff(rpmswNow(&end), &begin);

    if (ticks < 1)
	ticks = 1;
    return ticks;
}

rpmtime_t rpmswInit(void)
{
    struct rpmsw_s begin, end;

    rpmsw_type = 0;
    rpmsw_overhead = 0;
    rpmsw_cycles = 1;

#if 0
    (void) rpmswNow(&begin);
#if defined(__i386)
    rpmsw_type = 1;
    rpmsw_cycles = rpmswCalibrate();
    rpmsw_type = 0;
#endif
    rpmsw_overhead = rpmswDiff(rpmswNow(&end), &begin);
#if defined(__i386)
    rpmsw_type = 1;
    if (rpmsw_overhead > 1)
	rpmsw_cycles /= rpmsw_overhead;
#endif
    if (rpmsw_cycles < 1)
	rpmsw_cycles = 1;
#endif

    rpmsw_overhead = 0;
    (void) rpmswNow(&begin);
    rpmsw_overhead = rpmswDiff(rpmswNow(&end), &begin);

    return rpmsw_overhead;
}
