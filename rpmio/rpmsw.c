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
static int rpmsw_type = 0;

/*@unchecked@*/
static int rpmsw_initialized = 0;

#if 1	/* XXX defined(__i386__) */
/* Swiped from glibc-2.3.2 sysdeps/i386/i686/hp-timing.h */

#define	HP_TIMING_ZERO(Var)	(Var) = (0)
#define	HP_TIMING_NOW(Var)	__asm__ __volatile__ ("rdtsc" : "=A" (Var))

/* It's simple arithmetic for us.  */
#define	HP_TIMING_DIFF(Diff, Start, End)	(Diff) = ((End) - (Start))

/* We have to jump through hoops to get this correctly implemented.  */
#define HP_TIMING_ACCUM(Sum, Diff) \
  do {									      \
    char __not_done;							      \
    hp_timing_t __oldval = (Sum);					      \
    hp_timing_t __diff = (Diff) - GL(dl_hp_timing_overhead);		      \
    do									      \
      {									      \
	hp_timing_t __newval = __oldval + __diff;			      \
	int __temp0, __temp1;						      \
	__asm__ __volatile__ ("xchgl %4, %%ebx\n\t"			      \
			      "lock; cmpxchg8b %1\n\t"			      \
			      "sete %0\n\t"				      \
			      "movl %4, %%ebx"				      \
			      : "=q" (__not_done), "=m" (Sum),		      \
				"=A" (__oldval), "=c" (__temp0),	      \
				"=SD" (__temp1)				      \
			      : "1" (Sum), "2" (__oldval),		      \
				"3" (__newval >> 32),			      \
				"4" (__newval & 0xffffffff)		      \
			      : "memory");				      \
      }									      \
    while (__not_done);							      \
  } while (0)

/* No threads, no extra work.  */
#define HP_TIMING_ACCUM_NT(Sum, Diff)	(Sum) += (Diff)

/* Print the time value.  */
#define HP_TIMING_PRINT(Buf, Len, Val) \
  do {									      \
    char __buf[20];							      \
    char *__cp = _itoa (Val, __buf + sizeof (__buf), 10, 0);		      \
    int __len = (Len);							      \
    char *__dest = (Buf);						      \
    while (__len-- > 0 && __cp < __buf + sizeof (__buf))		      \
      *__dest++ = *__cp++;						      \
    memcpy (__dest, " clock cycles", MIN (__len, sizeof (" clock cycles")));  \
  } while (0)
#endif	/* __i386__ */

rpmsw rpmswNow(rpmsw sw)
{
/*@-noeffect@*/
    if (!rpmsw_initialized)
	(void) rpmswInit();
/*@=noeffect@*/
    if (sw == NULL)
	return NULL;
    switch (rpmsw_type) {
    case 0:
	if (gettimeofday(&sw->u.tv, NULL))
	    return NULL;
	break;
#if defined(HP_TIMING_NOW)
    case 1:
	HP_TIMING_NOW(sw->u.ticks);
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
static inline
rpmtime_t tvsub(/*@null@*/ const struct timeval * etv,
		/*@null@*/ const struct timeval * btv)
	/*@*/
{
    time_t secs, usecs;
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
#if defined(HP_TIMING_NOW)
    case 1:
	if (end->u.ticks > begin->u.ticks)
	    HP_TIMING_DIFF(diff, begin->u.ticks, end->u.ticks);
	break;
#endif
    }
    if (diff >= rpmsw_overhead)
	diff -= rpmsw_overhead;
    if (rpmsw_cycles > 1)
	diff /= rpmsw_cycles;
    return diff;
}

#if defined(HP_TIMING_NOW)
/*@-type@*/
static rpmtime_t rpmswCalibrate(void)
	/*@globals internalState @*/
	/*@modifies internalState @*/
{
    struct rpmsw_s begin, end;
    rpmtime_t ticks;
    struct timespec req, rem;
    int rc;
    int i;

/*@-uniondef@*/
    (void) rpmswNow(&begin);
/*@=uniondef@*/
    req.tv_sec = 0;
    req.tv_nsec = 20 * 1000 * 1000;
    for (i = 0; i < 100; i++) {
/*@-compdef@*/
	rc = nanosleep(&req, &rem);
/*@=compdef@*/
	if (rc == 0)
	    break;
	if (rem.tv_sec == 0 && rem.tv_nsec == 0)
	    break;
	req = rem;	/* structure assignment */
    }
/*@-uniondef@*/
    ticks = rpmswDiff(rpmswNow(&end), &begin);
/*@=uniondef@*/

    return ticks;
}
/*@=type@*/
#endif

/*@-incondefs@*/
rpmtime_t rpmswInit(void)
	/*@globals rpmsw_cycles, rpmsw_initialized, rpmsw_overhead,
		rpmsw_type @*/
	/*@modifies rpmsw_cycles, rpmsw_initialized, rpmsw_overhead,
		rpmsw_type @*/
{
    struct rpmsw_s begin, end;
    rpmtime_t cycles, usecs;
    int i;

    rpmsw_initialized = 1;

    rpmsw_overhead = 0;
    rpmsw_cycles = 0;

    /* Convergence is futile overkill ... */
    for (i = 0; i < 1; i++) {
#if defined(HP_TIMING_NOW)
	rpmtime_t save_cycles = rpmsw_cycles;

	/* We want cycles, not cycles/usec, here. */
	rpmsw_cycles = 1;

	/* Start wall clock. */
	rpmsw_type = 0;
/*@-uniondef@*/
	(void) rpmswNow(&begin);
/*@=uniondef@*/

	/* Get no. of cycles in 20ms nanosleep */
	rpmsw_type = 1;
	cycles = rpmswCalibrate();
	if (i)
	    cycles -= (save_cycles * rpmsw_overhead);

	/* Compute wall clock delta in usecs. */
	rpmsw_type = 0;
/*@-uniondef@*/
	usecs = rpmswDiff(rpmswNow(&end), &begin);
/*@=uniondef@*/

	rpmsw_type = 1;

	/* Compute cycles/usec */
	if (usecs > 1)
	    cycles /= usecs;

	rpmsw_cycles = save_cycles;
	rpmsw_cycles *= i;
	rpmsw_cycles += cycles;
	rpmsw_cycles /= (i+1);
#endif

	/* Calculate timing overhead in usecs. */
/*@-uniondef@*/
	(void) rpmswNow(&begin);
	usecs = rpmswDiff(rpmswNow(&end), &begin);
/*@=uniondef@*/

	rpmsw_overhead *= i;
	rpmsw_overhead += usecs;
	rpmsw_overhead /= (i+1);

    }

    return rpmsw_overhead;
}
/*@=incondefs@*/

/*@-mods@*/
int rpmswEnter(rpmop op, ssize_t rc)
{
    op->count++;
    if (rc < 0)
	op->usecs = 0;
/*@-uniondef@*/
    (void) rpmswNow(&op->begin);
/*@=uniondef@*/
    return 0;
}

rpmtime_t rpmswExit(rpmop op, ssize_t rc)
{
    struct rpmsw_s end;

/*@-uniondef@*/
    op->usecs += rpmswDiff(rpmswNow(&end), &op->begin);
/*@=uniondef@*/
    if (rc > 0)
	op->bytes += rc;
    return op->usecs;
}
/*@=mods@*/
