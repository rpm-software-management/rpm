/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: rep_region.c,v 12.29 2006/08/24 14:46:25 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"

static int __rep_egen_init  __P((DB_ENV *, REP *));

/*
 * __rep_open --
 *	Initialize the shared memory state for the replication system.
 *
 * PUBLIC: int __rep_open __P((DB_ENV *));
 */
int
__rep_open(dbenv)
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

	if (renv->rep_off == INVALID_ROFF) {
		/* Must create the region. */
		if ((ret = __db_shalloc(infop, sizeof(REP), 0, &rep)) != 0)
			return (ret);
		memset(rep, 0, sizeof(*rep));

		/*
		 * We have the region; fill in the values.  Some values may
		 * have been configured before we open the region, and those
		 * are taken from the DB_REP structure.
		 */
		if ((ret = __mutex_alloc(
		    dbenv, MTX_REP_REGION, 0, &rep->mtx_region)) != 0)
			return (ret);
		/*
		 * Because we have no way to prevent deadlocks and cannot log
		 * changes made to it, we single-thread access to the client
		 * bookkeeping database.  This is suboptimal, but it only gets
		 * accessed when messages arrive out-of-order, so it should
		 * stay small and not be used in a high-performance app.
		 */
		if ((ret = __mutex_alloc(
		    dbenv, MTX_REP_DATABASE, 0, &rep->mtx_clientdb)) != 0)
			return (ret);

		rep->tally_off = INVALID_ROFF;
		rep->v2tally_off = INVALID_ROFF;
		rep->eid = db_rep->eid;
		rep->master_id = DB_EID_INVALID;
		rep->gen = 0;
		rep->version = DB_REPVERSION;
		if ((ret = __rep_egen_init(dbenv, rep)) != 0)
			return (ret);
		rep->gen = 0;
		rep->gbytes = db_rep->gbytes;
		rep->bytes = db_rep->bytes;
		rep->request_gap = db_rep->request_gap;
		rep->max_gap = db_rep->max_gap;
		rep->config_nsites = db_rep->config_nsites;
		rep->config = db_rep->config;
		rep->elect_timeout = db_rep->elect_timeout;
		rep->priority = db_rep->my_priority;

		F_SET(rep, REP_F_NOARCHIVE);

		/* Initialize encapsulating region. */
		renv->rep_off = R_OFFSET(infop, rep);
		(void)time(&renv->rep_timestamp);
		renv->op_timestamp = 0;
		F_CLR(renv, DB_REGENV_REPLOCKED);
	} else
		rep = R_ADDR(infop, renv->rep_off);

	db_rep->region = rep;

	return (0);
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
	REGENV *renv;
	REGINFO *infop;
	int ret, t_ret;

	if (!REP_ON(dbenv))
		return (0);

	ret = 0;

	db_rep = dbenv->rep_handle;
	if (db_rep->region != NULL) {
		ret = __mutex_free(dbenv, &db_rep->region->mtx_region);
		if ((t_ret = __mutex_free(
		    dbenv, &db_rep->region->mtx_clientdb)) != 0 && ret == 0)
			ret = t_ret;
	}

	infop = dbenv->reginfo;
	renv = infop->primary;
	if (renv->rep_off != INVALID_ROFF)
		__db_shalloc_free(infop, R_ADDR(infop, renv->rep_off));

	return (ret);
}

/*
 * __rep_dbenv_refresh --
 *	Replication-specific refresh of the DB_ENV structure.
 *
 * PUBLIC: void __rep_dbenv_refresh __P((DB_ENV *));
 */
void
__rep_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	dbenv->rep_handle->region = NULL;
}

/*
 * __rep_close --
 *      Shut down all of replication.
 *
 * PUBLIC: int __rep_close __P((DB_ENV *));
 */
int
__rep_close(dbenv)
	DB_ENV *dbenv;
{
	int ret, t_ret;

	ret = __rep_preclose(dbenv);
	if ((t_ret = __rep_closefiles(dbenv)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __rep_preclose --
 *	If we are a client, shut down our client database and send
 * any outstanding bulk buffers.
 *
 * PUBLIC: int __rep_preclose __P((DB_ENV *));
 */
int
__rep_preclose(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	DB_REP *db_rep;
	LOG *lp;
	REP_BULK bulk;
	int ret;

	ret = 0;

	db_rep = dbenv->rep_handle;
	dblp = dbenv->lg_handle;

	/*
	 * If we have a rep region, we can preclose.  Otherwise, return.
	 * If we're on an error path from env open, we may not have
	 * a region, even though we have a handle.
	 */
	if (db_rep == NULL || db_rep->region == NULL)
		return (ret);
	MUTEX_LOCK(dbenv, db_rep->region->mtx_clientdb);
	if (db_rep->rep_db != NULL) {
		ret = __db_close(db_rep->rep_db, NULL, DB_NOSYNC);
		db_rep->rep_db = NULL;
	}
	/*
	 * We could be called early in an env_open error path, so
	 * only do this if we have a log region set up.
	 */
	if (dblp == NULL)
		goto out;
	lp = dblp->reginfo.primary;
	/*
	 * If we have something in the bulk buffer, send anything in it
	 * if we are able to.
	 */
	if (lp->bulk_off != 0 && db_rep->send != NULL) {
		memset(&bulk, 0, sizeof(bulk));
		bulk.addr = R_ADDR(&dblp->reginfo, lp->bulk_buf);
		bulk.offp = &lp->bulk_off;
		bulk.len = lp->bulk_len;
		bulk.type = REP_BULK_LOG;
		bulk.eid = DB_EID_BROADCAST;
		bulk.flagsp = &lp->bulk_flags;
		/*
		 * Ignore send errors here.  This can be called on the
		 * env->close path - make a best attempt to send.
		 */
		(void)__rep_send_bulk(dbenv, &bulk, 0);
	}
out:	MUTEX_UNLOCK(dbenv, db_rep->region->mtx_clientdb);
	return (ret);
}

/*
 * __rep_closefiles --
 *	If we were a client and are now a master, close all databases
 *	we've opened while applying messages as a client.  This can
 *	be called from __env_close and we need to check if the env,
 *	handles and regions are set up, or not.
 *
 * PUBLIC: int __rep_closefiles __P((DB_ENV *));
 */
int
__rep_closefiles(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	DB_REP *db_rep;
	int ret;

	ret = 0;

	db_rep = dbenv->rep_handle;
	dblp = dbenv->lg_handle;

	if (db_rep == NULL)
		return (ret);
	if (dblp == NULL)
		return (ret);
	if ((ret = __dbreg_close_files(dbenv)) == 0)
		F_CLR(db_rep, DBREP_OPENFILES);

	return (ret);
}

/*
 * __rep_egen_init --
 *	Initialize the value of egen in the region.  Called only from
 *	__rep_region_init, which is guaranteed to be single-threaded
 *	as we create the rep region.  We set the rep->egen field which
 *	is normally protected by db_rep->region->mutex.
 */
static int
__rep_egen_init(dbenv, rep)
	DB_ENV *dbenv;
	REP *rep;
{
	DB_FH *fhp;
	int ret;
	size_t cnt;
	char *p;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	if ((ret =
	    __db_appname(dbenv, DB_APP_NONE, REP_EGENNAME, 0, NULL, &p)) != 0)
		return (ret);
	/*
	 * If the file doesn't exist, create it now and initialize with 1.
	 */
	if (__os_exists(dbenv, p, NULL) != 0) {
		rep->egen = rep->gen + 1;
		if ((ret = __rep_write_egen(dbenv, rep->egen)) != 0)
			goto err;
	} else {
		/*
		 * File exists, open it and read in our egen.
		 */
		if ((ret = __os_open(dbenv, p, DB_OSO_RDONLY,
		    __db_omode(OWNER_RW), &fhp)) != 0)
			goto err;
		if ((ret = __os_read(dbenv, fhp, &rep->egen, sizeof(u_int32_t),
		    &cnt)) < 0 || cnt == 0)
			goto err1;
		RPRINT(dbenv, (dbenv, &mb, "Read in egen %lu",
		    (u_long)rep->egen));
err1:		 (void)__os_closehandle(dbenv, fhp);
	}
err:	__os_free(dbenv, p);
	return (ret);
}

/*
 * __rep_write_egen --
 *	Write out the egen into the env file.
 *
 * PUBLIC: int __rep_write_egen __P((DB_ENV *, u_int32_t));
 */
int
__rep_write_egen(dbenv, egen)
	DB_ENV *dbenv;
	u_int32_t egen;
{
	DB_FH *fhp;
	int ret;
	size_t cnt;
	char *p;

	if ((ret =
	    __db_appname(dbenv, DB_APP_NONE, REP_EGENNAME, 0, NULL, &p)) != 0)
		return (ret);
	if ((ret = __os_open(dbenv, p, DB_OSO_CREATE | DB_OSO_TRUNC,
	    __db_omode(OWNER_RW), &fhp)) == 0) {
		if ((ret = __os_write(dbenv, fhp, &egen, sizeof(u_int32_t),
		    &cnt)) != 0 || ((ret = __os_fsync(dbenv, fhp)) != 0))
			__db_err(dbenv, ret, "%s", p);
		(void)__os_closehandle(dbenv, fhp);
	}
	__os_free(dbenv, p);
	return (ret);
}
