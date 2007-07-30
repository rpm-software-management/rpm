/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: ctime.c,v 12.12 2007/05/17 15:14:54 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __db_ctime --
 *	Format a time-stamp.
 *
 * PUBLIC: char *__db_ctime __P((const time_t *, char *));
 */
char *
__db_ctime(tod, time_buf)
	const time_t *tod;
	char *time_buf;
{
	time_buf[CTIME_BUFLEN - 1] = '\0';

	/*
	 * The ctime_r interface is the POSIX standard, thread-safe version of
	 * ctime.  However, it was implemented in two different ways (with and
	 * without a buffer length argument), and you can't depend on a return
	 * value of (char *), the version in HPUX 10.XX returned an int.
	 */
#if defined(HAVE_CTIME_R_3ARG)
	(void)ctime_r(tod, time_buf, CTIME_BUFLEN);
#elif defined(HAVE_CTIME_R)
	(void)ctime_r(tod, time_buf);
#else
	(void)strncpy(time_buf, ctime(tod), CTIME_BUFLEN - 1);
#endif
	return (time_buf);
}
