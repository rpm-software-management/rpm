/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: rep_region.c,v 1.14 2001/10/25 14:08:49 bostic Exp ";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#endif

#include <string.h>

#include "db_int.h"
#include "rep.h"
#include "log.h"

/*
 * __rep_region_init --
 *	Initialize the shared memory state for the replication system.
 *
 * PUBLIC: int __rep_region_init __P((DB_ENV *));
 */
int
__rep_region_init(dbenv)
	DB_ENV *dbenv;
{
	REGENV *renv;
	REGINFO *infop;
	DB_REP *db_rep;
	REP *rep;
	int ret;

	db_rep = dbenv->rep_handle;
	infop = dbenv->reginfo;
	renv = infop->primary;
	ret = 0;

	MUTEX_LOCK(dbenv, &renv->mutex, dbenv->lockfhp);
	if (renv->rep_off == INVALID_ROFF) {
		/* Must create the region. */
		if ((ret = __db_shalloc(infop->addr,
		    sizeof(REP), MUTEX_ALIGN, &rep)) != 0)
			goto err;
		memset(rep, 0, sizeof(*rep));
		rep->tally_off = INVALID_ROFF;
		renv->rep_off = R_OFFSET(infop, rep);
		if ((ret = __db_mutex_init(dbenv,
		    &rep->mutex, renv->rep_off, 0)) != 0)
			goto err;

		/* We have the region; fill in the values. */
		rep->eid = DB_EID_INVALID;
		rep->master_id = DB_EID_INVALID;
		rep->gen = 0;
	} else
		rep = R_ADDR(infop, renv->rep_off);
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	db_rep->mutexp = &rep->mutex;
	db_rep->region = rep;

	return (0);

err:	MUTEX_UNLOCK(dbenv, &renv->mutex);
	return (ret);
}

/*
 * __rep_region_destroy --
 *	Destroy any system resources allocated in the replication region.
 *
 * PUBLIC: int __rep_region_destroy __P((DB_ENV *));
 */
int
__rep_region_destroy(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	int ret;

	ret = 0;
	db_rep = (DB_REP *)dbenv->rep_handle;

	if (db_rep != NULL && db_rep->mutexp != NULL)
		ret = __db_mutex_destroy(db_rep->mutexp);

	return (ret);
}

/*
 * __rep_dbenv_close --
 *	Replication-specific destruction of the DB_ENV structure.
 *
 * PUBLIC: int __rep_dbenv_close __P((DB_ENV *));
 */
int
__rep_dbenv_close(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;

	db_rep = (DB_REP *)dbenv->rep_handle;

	if (db_rep != NULL) {
		__os_free(dbenv, db_rep, sizeof(DB_REP));
		dbenv->rep_handle = NULL;
	}

	return (0);
}

/*
 * __rep_preclose --
 * If we are a client, shut down our client database.  Remember that the
 * client database was opened in its own environment, not the environment
 * for which it keeps track of information.  Also, if we have a client
 * database (i.e., rep_handle->rep_db) that means we were a client and
 * could have applied file opens that need to be closed now.  This could
 * also mask errors where dbp's that weren't opened by us are still open,
 * but we have no way of distingushing the two.
 *
 * PUBLIC: int __rep_preclose __P((DB_ENV *));
 */
int
__rep_preclose(dbenv)
	DB_ENV *dbenv;
{
	DB *dbp;
	DB_REP *db_rep;
	int ret;

	ret = 0;
	db_rep = (DB_REP *)dbenv->rep_handle;

	if (db_rep != NULL && (dbp = db_rep->rep_db) != NULL) {
		__log_close_files(dbenv);
		ret = dbp->close(dbp, 0);
		db_rep->rep_db = NULL;
	}

	return (ret);
}
