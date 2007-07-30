/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: mut_failchk.c,v 12.5 2007/05/17 15:15:45 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/mutex_int.h"

/*
 * __mut_failchk --
 *	Check for mutexes held by dead processes.
 *
 * PUBLIC: int __mut_failchk __P((DB_ENV *));
 */
int
__mut_failchk(dbenv)
	DB_ENV *dbenv;
{
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	DB_MUTEX *mutexp;
	db_mutex_t i;
	int ret;
	char buf[DB_THREADID_STRLEN];

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	ret = 0;

	MUTEX_SYSTEM_LOCK(dbenv);
	for (i = 1; i <= mtxregion->stat.st_mutex_cnt; ++i, ++mutexp) {
		mutexp = MUTEXP_SET(i);

		/*
		 * We're looking for per-process mutexes where the process
		 * has died.
		 */
		if (!F_ISSET(mutexp, DB_MUTEX_ALLOCATED) ||
		    !F_ISSET(mutexp, DB_MUTEX_PROCESS_ONLY))
			continue;

		/*
		 * The thread that allocated the mutex may have exited, but
		 * we cannot reclaim the mutex if the process is still alive.
		 */
		if (dbenv->is_alive(
		    dbenv, mutexp->pid, 0, DB_MUTEX_PROCESS_ONLY))
			continue;

		__db_msg(dbenv, "Freeing mutex for process: %s",
		    dbenv->thread_id_string(dbenv, mutexp->pid, 0, buf));

		/* Unlock and free the mutex. */
		if (F_ISSET(mutexp, DB_MUTEX_LOCKED))
			MUTEX_UNLOCK(dbenv, i);

		if ((ret = __mutex_free_int(dbenv, 0, &i)) != 0)
			break;
	}
	MUTEX_SYSTEM_UNLOCK(dbenv);

	return (ret);
}
