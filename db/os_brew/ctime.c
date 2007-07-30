/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: ctime.c,v 1.6 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

static void  __os_brew_ct_numb __P((char *, int));

/*
 * __db_ctime --
 *	Format a time-stamp.
 */
char *
__db_ctime(tod, time_buf)
	const time_t *tod;
	char *time_buf;
{
	JulianType jt;
	time_t tt;
	char *ncp;

	strcpy(time_buf, "Thu Jan 01 00:00:00 1970");
	time_buf[CTIME_BUFLEN - 1] = '\0';

	/*
	 * Berkeley DB uses POSIX time values internally, convert to a BREW
	 * time value.
	 */
	tt = *tod - BREW_EPOCH_OFFSET + LOCALTIMEOFFSET(NULL);
	GETJULIANDATE(tt, &jt);

	/*
	 * wWeekDay : Day of the week 0-6 (0=Monday, 6=Sunday)
	 */
	ncp = &"MonTueWedThuFriSatSun"[jt.wWeekDay*3];
	time_buf[0] = *ncp++;
	time_buf[1] = *ncp++;
	time_buf[2] = *ncp;
	ncp = &"JanFebMarAprMayJunJulAugSepOctNovDec"[(jt.wMonth - 1) * 3];
	time_buf[4] = *ncp++;
	time_buf[5] = *ncp++;
	time_buf[6] = *ncp;

	__os_brew_ct_numb(time_buf + 8, jt.wDay);
					/* Add 100 to keep the leading zero. */
	__os_brew_ct_numb(time_buf + 11, jt.wHour + 100);
	__os_brew_ct_numb(time_buf + 14, jt.wMinute + 100);
	__os_brew_ct_numb(time_buf + 17, jt.wSecond + 100);

	if (jt.wYear < 100) {		/* 9 99 */
		time_buf[20] = ' ';
		time_buf[21] = ' ';
		__os_brew_ct_numb(time_buf + 22, jt.wYear);
	} else {			/* 99 1999 */
		__os_brew_ct_numb(time_buf + 20, jt.wYear / 100);
		__os_brew_ct_numb(time_buf + 22, jt.wYear % 100 + 100);
	}

	return (time_buf);
}

/*
 * ct_numb --
 *	Used by asctime.
 */
static void
__os_brew_ct_numb(cp, n)
	char *cp;
	int n;
{
	cp[0] = ' ';
	if (n >= 10)
		cp[0] = (n / 10) % 10 + '0';
	cp[1] = n % 10 + '0';
}
