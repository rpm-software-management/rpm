/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_sleep.c,v 12.9 2006/09/06 20:22:12 bostic Exp $
 */

#include "db_config.h"

#define	__INCLUDE_SELECT_H	1
#include "db_int.h"

/*
 * __os_sleep --
 *	Yield the processor for a period of time.
 *
 * PUBLIC: void __os_sleep __P((DB_ENV *, u_long, u_long));
 */
void
__os_sleep(dbenv, secs, usecs)
	DB_ENV *dbenv;
	u_long secs, usecs;		/* Seconds and microseconds. */
{
	struct timeval t;
	int ret;

	/* Don't require that the values be normalized. */
	for (; usecs >= 1000000; usecs -= 1000000)
		++secs;

	if (DB_GLOBAL(j_sleep) != NULL) {
		(void)DB_GLOBAL(j_sleep)(secs, usecs);
		return;
	}

	/*
	 * It's important that we yield the processor here so that other
	 * processes or threads are permitted to run.
	 *
	 * XXX
	 * VxWorks doesn't yield the processor on select.  This isn't really
	 * an infinite loop, even though __os_yield can call __os_sleep, and
	 * we'll fix this when the tree isn't frozen. [#15037]
	 */
#ifdef HAVE_VXWORKS
	__os_yield(dbenv);
#endif

	/*
	 * Sheer raving paranoia -- don't select for 0 time.
	 */
	t.tv_sec = (long)secs;
	if (secs == 0 && usecs == 0)
		t.tv_usec = 1;
	else
		t.tv_usec = (long)usecs;

	/*
	 * We don't catch interrupts and restart the system call here, unlike
	 * other Berkeley DB system calls.  This may be a user attempting to
	 * interrupt a sleeping DB utility (for example, db_checkpoint), and
	 * we want the utility to see the signal and quit.  This assumes it's
	 * always OK for DB to sleep for less time than originally scheduled.
	 */
	if (select(0, NULL, NULL, NULL, &t) == -1) {
		ret = __os_get_syserr();
		if (__os_posix_err(ret) != EINTR)
			__db_syserr(dbenv, ret, "select");
	}
}
