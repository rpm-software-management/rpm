/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: txn_region.c,v 12.31 2007/05/17 15:16:00 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __txn_init __P((DB_ENV *, DB_TXNMGR *));
static size_t __txn_region_size __P((DB_ENV *));

/*
 * __txn_open --
 *	Open a transaction region.
 *
 * PUBLIC: int __txn_open __P((DB_ENV *, int));
 */
int
__txn_open(dbenv, create_ok)
	DB_ENV *dbenv;
	int create_ok;
{
	DB_TXNMGR *mgr;
	int ret;

	/* Create/initialize the transaction manager structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXNMGR), &mgr)) != 0)
		return (ret);
	TAILQ_INIT(&mgr->txn_chain);
	mgr->dbenv = dbenv;

	/* Join/create the txn region. */
	mgr->reginfo.dbenv = dbenv;
	mgr->reginfo.type = REGION_TYPE_TXN;
	mgr->reginfo.id = INVALID_REGION_ID;
	mgr->reginfo.flags = REGION_JOIN_OK;
	if (create_ok)
		F_SET(&mgr->reginfo, REGION_CREATE_OK);
	if ((ret = __env_region_attach(dbenv,
	    &mgr->reginfo, __txn_region_size(dbenv))) != 0)
		goto err;

	/* If we created the region, initialize it. */
	if (F_ISSET(&mgr->reginfo, REGION_CREATE))
		if ((ret = __txn_init(dbenv, mgr)) != 0)
			goto err;

	/* Set the local addresses. */
	mgr->reginfo.primary =
	    R_ADDR(&mgr->reginfo, mgr->reginfo.rp->primary);

	/* If threaded, acquire a mutex to protect the active TXN list. */
	if ((ret = __mutex_alloc(
	    dbenv, MTX_TXN_ACTIVE, DB_MUTEX_PROCESS_ONLY, &mgr->mutex)) != 0)
		goto err;

	dbenv->tx_handle = mgr;
	return (0);

err:	dbenv->tx_handle = NULL;
	if (mgr->reginfo.addr != NULL)
		(void)__env_region_detach(dbenv, &mgr->reginfo, 0);

	(void)__mutex_free(dbenv, &mgr->mutex);
	__os_free(dbenv, mgr);
	return (ret);
}

/*
 * __txn_init --
 *	Initialize a transaction region in shared memory.
 */
static int
__txn_init(dbenv, mgr)
	DB_ENV *dbenv;
	DB_TXNMGR *mgr;
{
	DB_LSN last_ckp;
	DB_TXNREGION *region;
	int ret;

	/*
	 * Find the last checkpoint in the log.
	 */
	ZERO_LSN(last_ckp);
	if (LOGGING_ON(dbenv)) {
		/*
		 * The log system has already walked through the last
		 * file.  Get the LSN of a checkpoint it may have found.
		 */
		if ((ret = __log_get_cached_ckp_lsn(dbenv, &last_ckp)) != 0)
			return (ret);

		/*
		 * If that didn't work, look backwards from the beginning of
		 * the last log file until we find the last checkpoint.
		 */
		if (IS_ZERO_LSN(last_ckp) &&
		    (ret = __txn_findlastckp(dbenv, &last_ckp, NULL)) != 0)
			return (ret);
	}

	if ((ret = __env_alloc(&mgr->reginfo,
	    sizeof(DB_TXNREGION), &mgr->reginfo.primary)) != 0) {
		__db_errx(dbenv,
		    "Unable to allocate memory for the transaction region");
		return (ret);
	}
	mgr->reginfo.rp->primary =
	    R_OFFSET(&mgr->reginfo, mgr->reginfo.primary);
	region = mgr->reginfo.primary;
	memset(region, 0, sizeof(*region));

	if ((ret = __mutex_alloc(
	    dbenv, MTX_TXN_REGION, 0, &region->mtx_region)) != 0)
		return (ret);

	region->maxtxns = dbenv->tx_max;
	region->last_txnid = TXN_MINIMUM;
	region->cur_maxid = TXN_MAXIMUM;

	if ((ret = __mutex_alloc(
	    dbenv, MTX_TXN_CHKPT, 0, &region->mtx_ckp)) != 0)
		return (ret);
	region->last_ckp = last_ckp;
	region->time_ckp = time(NULL);

	memset(&region->stat, 0, sizeof(region->stat));
#ifdef HAVE_STATISTICS
	region->stat.st_maxtxns = region->maxtxns;
#endif

	SH_TAILQ_INIT(&region->active_txn);
	SH_TAILQ_INIT(&region->mvcc_txn);
	return (ret);
}

/*
 * __txn_findlastckp --
 *	Find the last checkpoint in the log, walking backwards from the
 *	max_lsn given or the beginning of the last log file.  (The
 *	log system looked through the last log file when it started up.)
 *
 * PUBLIC: int __txn_findlastckp __P((DB_ENV *, DB_LSN *, DB_LSN *));
 */
int
__txn_findlastckp(dbenv, lsnp, max_lsn)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	DB_LSN *max_lsn;
{
	DB_LOGC *logc;
	DB_LSN lsn;
	DBT dbt;
	int ret, t_ret;
	u_int32_t rectype;

	ZERO_LSN(*lsnp);

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	/* Get the last LSN. */
	memset(&dbt, 0, sizeof(dbt));
	if (max_lsn != NULL) {
		lsn = *max_lsn;
		if ((ret = __logc_get(logc, &lsn, &dbt, DB_SET)) != 0)
			goto err;
	} else {
		if ((ret = __logc_get(logc, &lsn, &dbt, DB_LAST)) != 0)
			goto err;
		/*
		 * Twiddle the last LSN so it points to the beginning of the
		 * last file; we know there's no checkpoint after that, since
		 * the log system already looked there.
		 */
		lsn.offset = 0;
	}

	/* Read backwards, looking for checkpoints. */
	while ((ret = __logc_get(logc, &lsn, &dbt, DB_PREV)) == 0) {
		if (dbt.size < sizeof(u_int32_t))
			continue;
		memcpy(&rectype, dbt.data, sizeof(u_int32_t));
		if (rectype == DB___txn_ckp) {
			*lsnp = lsn;
			break;
		}
	}

err:	if ((t_ret = __logc_close(logc)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Not finding a checkpoint is not an error;  there may not exist
	 * one in the log.
	 */
	return ((ret == 0 || ret == DB_NOTFOUND) ? 0 : ret);
}

/*
 * __txn_env_refresh --
 *	Clean up after the transaction system on a close or failed open.
 *
 * PUBLIC: int __txn_env_refresh __P((DB_ENV *));
 */
int
__txn_env_refresh(dbenv)
	DB_ENV *dbenv;
{
	DB_TXN *txn;
	DB_TXNMGR *mgr;
	REGINFO *reginfo;
	u_int32_t txnid;
	int aborted, ret, t_ret;

	ret = 0;
	mgr = dbenv->tx_handle;
	reginfo = &mgr->reginfo;

	/*
	 * This function can only be called once per process (i.e., not
	 * once per thread), so no synchronization is required.
	 *
	 * The caller is probably doing something wrong if close is called with
	 * active transactions.  Try and abort any active transactions that are
	 * not prepared, but it's quite likely the aborts will fail because
	 * recovery won't find open files.  If we can't abort any of the
	 * unprepared transaction, panic, we have to run recovery to get back
	 * to a known state.
	 */
	aborted = 0;
	if (TAILQ_FIRST(&mgr->txn_chain) != NULL) {
		while ((txn = TAILQ_FIRST(&mgr->txn_chain)) != NULL) {
			/* Prepared transactions are OK. */
			txnid = txn->txnid;
			if (((TXN_DETAIL *)txn->td)->status == TXN_PREPARED) {
				if ((ret = __txn_discard_int(txn, 0)) != 0) {
					__db_err(dbenv, ret,
					    "unable to discard txn %#lx",
					    (u_long)txnid);
					break;
				}
				continue;
			}
			aborted = 1;
			if ((t_ret = __txn_abort(txn)) != 0) {
				__db_err(dbenv, t_ret,
				    "unable to abort transaction %#lx",
				    (u_long)txnid);
				ret = __db_panic(dbenv, t_ret);
				break;
			}
		}
		if (aborted) {
			__db_errx(dbenv,
	"Error: closing the transaction region with active transactions");
			if (ret == 0)
				ret = EINVAL;
		}
	}

	/* Discard the per-thread lock. */
	if ((t_ret = __mutex_free(dbenv, &mgr->mutex)) != 0 && ret == 0)
		ret = t_ret;

	/* Detach from the region. */
	if ((t_ret = __env_region_detach(dbenv, reginfo, 0)) != 0 && ret == 0)
		ret = t_ret;

	__os_free(dbenv, mgr);

	dbenv->tx_handle = NULL;
	return (ret);
}

/*
 * __txn_region_mutex_count --
 *	Return the number of mutexes the txn region will need.
 *
 * PUBLIC: u_int32_t __txn_region_mutex_count __P((DB_ENV *));
 */
u_int32_t
__txn_region_mutex_count(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * We need a MVCC mutex for each TXN_DETAIL structure, a mutex for
	 * DB_TXNMGR structure, two mutexes for the DB_TXNREGION structure.
	 */
	return (dbenv->tx_max + 1 + 2);
}

/*
 * __txn_region_size --
 *	 Return the amount of space needed for the txn region.
 */
static size_t
__txn_region_size(dbenv)
	DB_ENV *dbenv;
{
	size_t s;

	/*
	 * Make the region large enough to hold the primary transaction region
	 * structure, txn_max transaction detail structures, txn_max chunks of
	 * overhead required by the underlying shared region allocator for each
	 * chunk of memory, txn_max transaction names, at an average of 20
	 * bytes each, and 10KB for safety.
	 */
	s = sizeof(DB_TXNREGION) +
	    dbenv->tx_max * (sizeof(TXN_DETAIL) + __env_alloc_overhead() + 20) +
	    10 * 1024;
	return (s);
}

/*
 * __txn_id_set --
 *	Set the current transaction ID and current maximum unused ID (for
 *	testing purposes only).
 *
 * PUBLIC: int __txn_id_set __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__txn_id_set(dbenv, cur_txnid, max_txnid)
	DB_ENV *dbenv;
	u_int32_t cur_txnid, max_txnid;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	int ret;

	ENV_REQUIRES_CONFIG(dbenv, dbenv->tx_handle, "txn_id_set", DB_INIT_TXN);

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;
	region->last_txnid = cur_txnid;
	region->cur_maxid = max_txnid;

	ret = 0;
	if (cur_txnid < TXN_MINIMUM) {
		__db_errx(dbenv, "Current ID value %lu below minimum",
		    (u_long)cur_txnid);
		ret = EINVAL;
	}
	if (max_txnid < TXN_MINIMUM) {
		__db_errx(dbenv, "Maximum ID value %lu below minimum",
		    (u_long)max_txnid);
		ret = EINVAL;
	}
	return (ret);
}

/*
 * __txn_oldest_reader --
 *	 Find the oldest "read LSN" of any active transaction'
 *	 MVCC changes older than this can safely be discarded from the cache.
 *
 * PUBLIC: int __txn_oldest_reader __P((DB_ENV *, DB_LSN *));
 */
int
__txn_oldest_reader(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_LSN old_lsn;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *td;
	int ret;

	if ((mgr = dbenv->tx_handle) == NULL)
		return (0);
	region = mgr->reginfo.primary;

	if ((ret = __log_current_lsn(dbenv, &old_lsn, NULL, NULL)) != 0)
		return (ret);

	TXN_SYSTEM_LOCK(dbenv);
	SH_TAILQ_FOREACH(td, &region->active_txn, links, __txn_detail)
		if (LOG_COMPARE(&td->read_lsn, &old_lsn) < 0)
			old_lsn = td->read_lsn;
	TXN_SYSTEM_UNLOCK(dbenv);

	DB_ASSERT(dbenv, LOG_COMPARE(&old_lsn, lsnp) >= 0);
	*lsnp = old_lsn;

	return (0);
}

/*
 * __txn_add_buffer --
 *	Add to the count of buffers created by the given transaction.
 *
 * PUBLIC: int __txn_add_buffer __P((DB_ENV *, TXN_DETAIL *));
 */
int
__txn_add_buffer(dbenv, td)
	DB_ENV *dbenv;
	TXN_DETAIL *td;
{
	DB_ASSERT(dbenv, td != NULL);

	MUTEX_LOCK(dbenv, td->mvcc_mtx);
	DB_ASSERT(dbenv, td->mvcc_ref < UINT32_MAX);
	++td->mvcc_ref;
	MUTEX_UNLOCK(dbenv, td->mvcc_mtx);

	COMPQUIET(dbenv, NULL);
	return (0);
}

/*
 * __txn_remove_buffer --
 *	Remove a buffer from a transaction -- free the transaction if necessary.
 *
 * PUBLIC: int __txn_remove_buffer __P((DB_ENV *, TXN_DETAIL *, db_mutex_t));
 */
int
__txn_remove_buffer(dbenv, td, hash_mtx)
	DB_ENV *dbenv;
	TXN_DETAIL *td;
	db_mutex_t hash_mtx;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	int need_free, ret;

	DB_ASSERT(dbenv, td != NULL);
	ret = 0;
	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	MUTEX_LOCK(dbenv, td->mvcc_mtx);
	DB_ASSERT(dbenv, td->mvcc_ref > 0);
	need_free = (--td->mvcc_ref == 0);
	MUTEX_UNLOCK(dbenv, td->mvcc_mtx);

	if (need_free &&
	    (td->status == TXN_COMMITTED || td->status == TXN_ABORTED)) {
		MUTEX_UNLOCK(dbenv, hash_mtx);

		ret = __mutex_free(dbenv, &td->mvcc_mtx);
		td->mvcc_mtx = MUTEX_INVALID;

		TXN_SYSTEM_LOCK(dbenv);
		SH_TAILQ_REMOVE(&region->mvcc_txn, td, links, __txn_detail);
#ifdef HAVE_STATISTICS
		--region->stat.st_nsnapshot;
#endif
		__env_alloc_free(&mgr->reginfo, td);
		TXN_SYSTEM_UNLOCK(dbenv);

		MUTEX_LOCK(dbenv, hash_mtx);
	}

	return (ret);
}
