/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: os_uid.c,v 12.27 2007/05/17 15:15:46 bostic Exp $
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
	db_timespec v;
	pid_t pid;
	u_int32_t id;

	*idp = 0;

	/*
	 * Our randomized value is comprised of our process ID, the current
	 * time of day and a stack address, all XOR'd together.
	 */
	__os_id(dbenv, &pid, NULL);
	__os_gettime(dbenv, &v);

	id = (u_int32_t)pid ^
	    (u_int32_t)v.tv_sec ^ (u_int32_t)v.tv_nsec ^ P_TO_UINT32(&pid);

	/*
	 * We could try and find a reasonable random-number generator, but
	 * that's not all that easy to do.  Seed and use srand()/rand(), if
	 * we can find them.
	 */
	if (DB_GLOBAL(uid_init) == 0) {
		DB_GLOBAL(uid_init) = 1;
		srand((u_int)id);
	}
	id ^= (u_int)rand();

	*idp = id;
}
