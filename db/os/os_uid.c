/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_uid.c,v 12.23 2006/09/15 19:24:50 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_unique_id --
 *	Return a unique 32-bit value.
 *
 * PUBLIC: void __os_unique_id __P((DB_ENV *, u_int32_t *));
 */
void
__os_unique_id(dbenv, idp)
	DB_ENV *dbenv;
	u_int32_t *idp;
{
	static int first = 1;
	pid_t pid;
	u_int32_t id, sec, usec;

	*idp = 0;

	/*
	 * Our randomized value is comprised of our process ID, the current
	 * time of day and a stack address, all XOR'd together.
	 */
	__os_id(dbenv, &pid, NULL);
	__os_clock(dbenv, &sec, &usec);

	id = (u_int32_t)pid ^ sec ^ usec ^ P_TO_UINT32(&pid);

	/*
	 * We could try and find a reasonable random-number generator, but
	 * that's not all that easy to do.  Seed and use srand()/rand(), if
	 * we can find them.
	 */
	if (first == 1) {
		srand((u_int)id);
		first = 0;
	}
	id ^= (u_int)rand();

	*idp = id;
}
