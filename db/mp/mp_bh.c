/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: mp_bh.c,v 12.31 2006/09/07 19:11:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"		/* Required for diagnostic code. */
#include "dbinc/mp.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __memp_pgwrite
	       __P((DB_ENV *, DB_MPOOLFILE *, DB_MPOOL_HASH *, BH *));

/*
 * __memp_bhwrite --
 *	Write the page associated with a given buffer header.
 *
 * PUBLIC: int __memp_bhwrite __P((DB_MPOOL *,
 * PUBLIC:      DB_MPOOL_HASH *, MPOOLFILE *, BH *, int));
 */
int
__memp_bhwrite(dbmp, hp, mfp, bhp, open_extents)
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	MPOOLFILE *mfp;
	BH *bhp;
	int open_extents;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *dbmfp;
	DB_MPREG *mpreg;
	int ret;

	dbenv = dbmp->dbenv;

	/*
	 * If the file has been removed or is a closed temporary file, we're
	 * done -- the page-write function knows how to handle the fact that
	 * we don't have (or need!) any real file descriptor information.
	 */
	if (mfp->deadfile)
		return (__memp_pgwrite(dbenv, NULL, hp, bhp));

	/*
	 * Walk the process' DB_MPOOLFILE list and find a file descriptor for
	 * the file.  We also check that the descriptor is open for writing.
	 */
	MUTEX_LOCK(dbenv, dbmp->mutex);
	TAILQ_FOREACH(dbmfp, &dbmp->dbmfq, q)
		if (dbmfp->mfp == mfp && !F_ISSET(dbmfp, MP_READONLY)) {
			++dbmfp->ref;
			break;
		}
	MUTEX_UNLOCK(dbenv, dbmp->mutex);

	if (dbmfp != NULL) {
		/*
		 * Temporary files may not have been created.  We only handle
		 * temporary files in this path, because only the process that
		 * created a temporary file will ever flush buffers to it.
		 */
		if (dbmfp->fhp == NULL) {
			/* We may not be allowed to create backing files. */
			if (mfp->no_backing_file) {
				--dbmfp->ref;
				return (EPERM);
			}

			MUTEX_LOCK(dbenv, dbmp->mutex);
			if (dbmfp->fhp == NULL)
				ret = __db_appname(dbenv, DB_APP_TMP, NULL,
				    F_ISSET(dbenv, DB_ENV_DIRECT_DB) ?
				    DB_OSO_DIRECT : 0, &dbmfp->fhp, NULL);
			else
				ret = 0;
			MUTEX_UNLOCK(dbenv, dbmp->mutex);
			if (ret != 0) {
				__db_errx(dbenv,
				    "unable to create temporary backing file");
				--dbmfp->ref;
				return (ret);
			}
		}

		goto pgwrite;
	}

	/*
	 * There's no file handle for this file in our process.
	 *
	 * !!!
	 * It's the caller's choice if we're going to open extent files.
	 */
	if (!open_extents && F_ISSET(mfp, MP_EXTENT))
		return (EPERM);

	/*
	 * !!!
	 * Don't try to attach to temporary files.  There are two problems in
	 * trying to do that.  First, if we have different privileges than the
	 * process that "owns" the temporary file, we might create the backing
	 * disk file such that the owning process couldn't read/write its own
	 * buffers, e.g., memp_trickle running as root creating a file owned
	 * as root, mode 600.  Second, if the temporary file has already been
	 * created, we don't have any way of finding out what its real name is,
	 * and, even if we did, it was already unlinked (so that it won't be
	 * left if the process dies horribly).  This decision causes a problem,
	 * however: if the temporary file consumes the entire buffer cache,
	 * and the owner doesn't flush the buffers to disk, we could end up
	 * with resource starvation, and the memp_trickle thread couldn't do
	 * anything about it.  That's a pretty unlikely scenario, though.
	 *
	 * Note we should never get here when the temporary file in question
	 * has already been closed in another process, in which case it should
	 * be marked dead.
	 */
	if (F_ISSET(mfp, MP_TEMP) || mfp->no_backing_file)
		return (EPERM);

	/*
	 * It's not a page from a file we've opened.  If the file requires
	 * application-specific input/output processing, see if this process
	 * has ever registered information as to how to write this type of
	 * file.  If not, there's nothing we can do.
	 */
	if (mfp->ftype != 0 && mfp->ftype != DB_FTYPE_SET) {
		MUTEX_LOCK(dbenv, dbmp->mutex);
		LIST_FOREACH(mpreg, &dbmp->dbregq, q)
			if (mpreg->ftype == mfp->ftype)
				break;
		MUTEX_UNLOCK(dbenv, dbmp->mutex);
		if (mpreg == NULL)
			return (EPERM);
	}

	/*
	 * Try and open the file, specifying the known underlying shared area.
	 *
	 * !!!
	 * There's no negative cache, so we may repeatedly try and open files
	 * that we have previously tried (and failed) to open.
	 */
	if ((ret = __memp_fcreate(dbenv, &dbmfp)) != 0)
		return (ret);
	if ((ret = __memp_fopen(dbmfp,
	    mfp, NULL, DB_DURABLE_UNKNOWN, 0, mfp->stat.st_pagesize)) != 0) {
		(void)__memp_fclose(dbmfp, 0);

		/*
		 * Ignore any error if the file is marked dead, assume the file
		 * was removed from under us.
		 */
		if (!mfp->deadfile)
			return (ret);

		dbmfp = NULL;
	}

pgwrite:
	MVCC_MPROTECT(bhp->buf, mfp->stat.st_pagesize,
	    PROT_READ | PROT_WRITE | PROT_EXEC);
	ret = __memp_pgwrite(dbenv, dbmfp, hp, bhp);
	if (dbmfp == NULL)
		return (ret);

	/*
	 * Discard our reference, and, if we're the last reference, make sure
	 * the file eventually gets closed.
	 */
	MUTEX_LOCK(dbenv, dbmp->mutex);
	if (dbmfp->ref == 1)
		F_SET(dbmfp, MP_FLUSH);
	else
		--dbmfp->ref;
	MUTEX_UNLOCK(dbenv, dbmp->mutex);

	return (ret);
}

/*
 * __memp_pgread --
 *	Read a page from a file.
 *
 * PUBLIC: int __memp_pgread __P((DB_MPOOLFILE *, DB_MPOOL_HASH *, BH *, int));
 */
int
__memp_pgread(dbmfp, hp, bhp, can_create)
	DB_MPOOLFILE *dbmfp;
	DB_MPOOL_HASH *hp;
	BH *bhp;
	int can_create;
{
	DB_ENV *dbenv;
	MPOOLFILE *mfp;
	size_t len, nr;
	u_int32_t pagesize;
	int ret;

	dbenv = dbmfp->dbenv;
	mfp = dbmfp->mfp;
	pagesize = mfp->stat.st_pagesize;

	/* We should never be called with a dirty or a locked buffer. */
	DB_ASSERT(dbenv, !F_ISSET(bhp, BH_DIRTY_CREATE | BH_LOCKED));
	DB_ASSERT(dbenv, can_create || !F_ISSET(bhp, BH_DIRTY));

	/* Lock the buffer and unlock the hash bucket. */
	F_SET(bhp, BH_LOCKED | BH_TRASH);
	MUTEX_UNLOCK(dbenv, hp->mtx_hash);

	/*
	 * Temporary files may not yet have been created.  We don't create
	 * them now, we create them when the pages have to be flushed.
	 */
	nr = 0;
	if (dbmfp->fhp != NULL)
		if ((ret = __os_io(dbenv, DB_IO_READ, dbmfp->fhp,
		    bhp->pgno, pagesize, 0, pagesize, bhp->buf, &nr)) != 0)
			goto err;

	/*
	 * The page may not exist; if it doesn't, nr may well be 0, but we
	 * expect the underlying OS calls not to return an error code in
	 * this case.
	 */
	if (nr < pagesize) {
		/*
		 * Don't output error messages for short reads.  In particular,
		 * DB recovery processing may request pages never written to
		 * disk or for which only some part have been written to disk,
		 * in which case we won't find the page.  The caller must know
		 * how to handle the error.
		 */
		if (!can_create) {
			ret = DB_PAGE_NOTFOUND;
			goto err;
		}

		/* Clear any bytes that need to be cleared. */
		len = mfp->clear_len == DB_CLEARLEN_NOTSET ?
		    pagesize : mfp->clear_len;
		memset(bhp->buf, 0, len);

#if defined(DIAGNOSTIC) || defined(UMRW)
		/*
		 * If we're running in diagnostic mode, corrupt any bytes on
		 * the page that are unknown quantities for the caller.
		 */
		if (len < pagesize)
			memset(bhp->buf + len, CLEAR_BYTE, pagesize - len);
#endif
		++mfp->stat.st_page_create;
	} else
		++mfp->stat.st_page_in;

	/* Call any pgin function. */
	ret = mfp->ftype == 0 ? 0 : __memp_pg(dbmfp, bhp, 1);

	/* Re-acquire the hash bucket lock. */
err:	MUTEX_LOCK(dbenv, hp->mtx_hash);

	/*
	 * If no errors occurred, the data is now valid, clear the BH_TRASH
	 * flag; regardless, clear the lock bit and let other threads proceed.
	 */
	F_CLR(bhp, BH_LOCKED);
	if (ret == 0)
		F_CLR(bhp, BH_TRASH);

	/*
	 * If a thread of control is waiting on this buffer, wake it up.
	 */
	if (F_ISSET(hp, IO_WAITER)) {
		F_CLR(hp, IO_WAITER);
		MUTEX_UNLOCK(dbenv, hp->mtx_io);
	}

	return (ret);
}

/*
 * __memp_pgwrite --
 *	Write a page to a file.
 */
static int
__memp_pgwrite(dbenv, dbmfp, hp, bhp)
	DB_ENV *dbenv;
	DB_MPOOLFILE *dbmfp;
	DB_MPOOL_HASH *hp;
	BH *bhp;
{
	DB_LSN lsn;
	MPOOLFILE *mfp;
	size_t nw;
	int callpgin, ret;

	mfp = dbmfp == NULL ? NULL : dbmfp->mfp;
	callpgin = ret = 0;

	/*
	 * We should never be called with a clean or trash buffer.
	 * The sync code does call us with already locked buffers.
	 */
	DB_ASSERT(dbenv, F_ISSET(bhp, BH_DIRTY));
	DB_ASSERT(dbenv, !F_ISSET(bhp, BH_TRASH));

	/* If not already done, lock the buffer and unlock the hash bucket. */
	if (!F_ISSET(bhp, BH_LOCKED)) {
		F_SET(bhp, BH_LOCKED);
		MUTEX_UNLOCK(dbenv, hp->mtx_hash);
	}

	/*
	 * It's possible that the underlying file doesn't exist, either
	 * because of an outright removal or because it was a temporary
	 * file that's been closed.
	 *
	 * !!!
	 * Once we pass this point, we know that dbmfp and mfp aren't NULL,
	 * and that we have a valid file reference.
	 */
	if (mfp == NULL || mfp->deadfile)
		goto file_dead;

	/*
	 * If the page is in a file for which we have LSN information, we have
	 * to ensure the appropriate log records are on disk.
	 */
	if (LOGGING_ON(dbenv) && mfp->lsn_off != -1 &&
	    !IS_CLIENT_PGRECOVER(dbenv)) {
		memcpy(&lsn, bhp->buf + mfp->lsn_off, sizeof(DB_LSN));
		if (!IS_NOT_LOGGED_LSN(lsn) &&
		    (ret = __log_flush(dbenv, &lsn)) != 0)
			goto err;
	}

#ifdef DIAGNOSTIC
	/*
	 * Verify write-ahead logging semantics.
	 *
	 * !!!
	 * Two special cases.  There is a single field on the meta-data page,
	 * the last-page-number-in-the-file field, for which we do not log
	 * changes.  If the page was originally created in a database that
	 * didn't have logging turned on, we can see a page marked dirty but
	 * for which no corresponding log record has been written.  However,
	 * the only way that a page can be created for which there isn't a
	 * previous log record and valid LSN is when the page was created
	 * without logging turned on, and so we check for that special-case
	 * LSN value.
	 *
	 * Second, when a client is reading database pages from a master
	 * during an internal backup, we may get pages modified after
	 * the current end-of-log.
	 */
	if (LOGGING_ON(dbenv) && !IS_NOT_LOGGED_LSN(LSN(bhp->buf)) &&
	    !IS_CLIENT_PGRECOVER(dbenv)) {
		/*
		 * There is a potential race here.  If we are in the midst of
		 * switching log files, it's possible we could test against the
		 * old file and the new offset in the log region's LSN.  If we
		 * fail the first test, acquire the log mutex and check again.
		 */
		DB_LOG *dblp;
		LOG *lp;

		dblp = dbenv->lg_handle;
		lp = dblp->reginfo.primary;
		if (!lp->db_log_inmemory &&
		    LOG_COMPARE(&lp->s_lsn, &LSN(bhp->buf)) <= 0) {
			MUTEX_LOCK(dbenv, lp->mtx_flush);
			DB_ASSERT(dbenv,
			    LOG_COMPARE(&lp->s_lsn, &LSN(bhp->buf)) > 0);
			MUTEX_UNLOCK(dbenv, lp->mtx_flush);
		}
	}
#endif

	/*
	 * Call any pgout function.  We set the callpgin flag so that we flag
	 * that the contents of the buffer will need to be passed through pgin
	 * before they are reused.
	 */
	if (mfp->ftype != 0 && !F_ISSET(bhp, BH_CALLPGIN)) {
		callpgin = 1;
		if ((ret = __memp_pg(dbmfp, bhp, 0)) != 0)
			goto err;
	}

	/* Write the page. */
	if ((ret = __os_io(
	    dbenv, DB_IO_WRITE, dbmfp->fhp, bhp->pgno, mfp->stat.st_pagesize,
	    0, mfp->stat.st_pagesize, bhp->buf, &nw)) != 0) {
		__db_errx(dbenv, "%s: write failed for page %lu",
		    __memp_fn(dbmfp), (u_long)bhp->pgno);
		goto err;
	}
	++mfp->stat.st_page_out;
	if (bhp->pgno > mfp->last_flushed_pgno) {
		MUTEX_LOCK(dbenv, mfp->mutex);
		if (bhp->pgno > mfp->last_flushed_pgno)
			mfp->last_flushed_pgno = bhp->pgno;
		MUTEX_UNLOCK(dbenv, mfp->mutex);
	}

err:
file_dead:
	/*
	 * !!!
	 * Once we pass this point, dbmfp and mfp may be NULL, we may not have
	 * a valid file reference.
	 *
	 * Re-acquire the hash lock.
	 */
	MUTEX_LOCK(dbenv, hp->mtx_hash);

	/*
	 * If we rewrote the page, it will need processing by the pgin
	 * routine before reuse.
	 */
	if (callpgin)
		F_SET(bhp, BH_CALLPGIN);

	/*
	 * Update the hash bucket statistics, reset the flags.  If we were
	 * successful, the page is no longer dirty.
	 */
	if (ret == 0) {
		DB_ASSERT(dbenv, hp->hash_page_dirty != 0);
		--hp->hash_page_dirty;
		F_CLR(bhp, BH_DIRTY | BH_DIRTY_CREATE);
	}

	/* Regardless, clear any sync wait-for count and remove our lock. */
	bhp->ref_sync = 0;
	F_CLR(bhp, BH_LOCKED);

	/*
	 * If a thread of control is waiting on this buffer, wake it up.
	 */
	if (F_ISSET(hp, IO_WAITER)) {
		F_CLR(hp, IO_WAITER);
		MUTEX_UNLOCK(dbenv, hp->mtx_io);
	}

	return (ret);
}

/*
 * __memp_pg --
 *	Call the pgin/pgout routine.
 *
 * PUBLIC: int __memp_pg __P((DB_MPOOLFILE *, BH *, int));
 */
int
__memp_pg(dbmfp, bhp, is_pgin)
	DB_MPOOLFILE *dbmfp;
	BH *bhp;
	int is_pgin;
{
	DBT dbt, *dbtp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPREG *mpreg;
	MPOOLFILE *mfp;
	int ftype, ret;

	dbenv = dbmfp->dbenv;
	dbmp = dbenv->mp_handle;
	mfp = dbmfp->mfp;

	if ((ftype = mfp->ftype) == DB_FTYPE_SET)
		mpreg = dbmp->pg_inout;
	else {
		MUTEX_LOCK(dbenv, dbmp->mutex);
		LIST_FOREACH(mpreg, &dbmp->dbregq, q)
			if (ftype == mpreg->ftype)
				break;
		MUTEX_UNLOCK(dbenv, dbmp->mutex);
	}
	if (mpreg == NULL)
		return (0);

	if (mfp->pgcookie_len == 0)
		dbtp = NULL;
	else {
		DB_SET_DBT(dbt, R_ADDR(
		    dbmp->reginfo, mfp->pgcookie_off), mfp->pgcookie_len);
		dbtp = &dbt;
	}

	if (is_pgin) {
		if (mpreg->pgin != NULL &&
		    (ret = mpreg->pgin(dbenv, bhp->pgno, bhp->buf, dbtp)) != 0)
			goto err;
	} else
		if (mpreg->pgout != NULL &&
		    (ret = mpreg->pgout(dbenv, bhp->pgno, bhp->buf, dbtp)) != 0)
			goto err;

	return (0);

err:	__db_errx(dbenv, "%s: %s failed for page %lu",
	    __memp_fn(dbmfp), is_pgin ? "pgin" : "pgout", (u_long)bhp->pgno);
	return (ret);
}

/*
 * __memp_bhfree --
 *	Free a bucket header and its referenced data.
 *
 * PUBLIC: int __memp_bhfree
 * PUBLIC:     __P((DB_MPOOL *, DB_MPOOL_HASH *, BH *, u_int32_t));
 */
int
__memp_bhfree(dbmp, hp, bhp, flags)
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	BH *bhp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	MPOOL *c_mp, *mp;
	MPOOLFILE *mfp;
	BH *next_bhp, *prev_bhp;
	u_int32_t n_cache;
	int reorder, ret, t_ret;
#ifdef DIAG_MVCC
	size_t pagesize;
#endif

	ret = 0;

	/*
	 * Assumes the hash bucket is locked and the MPOOL is not.
	 */
	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	n_cache = NCACHE(mp, bhp->mf_offset, bhp->pgno);
	mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);
#ifdef DIAG_MVCC
	pagesize = mfp->stat.st_pagesize;
#endif

	DB_ASSERT(dbenv, bhp->ref == 0);
	DB_ASSERT(dbenv, LF_ISSET(BH_FREE_UNLOCKED) ||
	    SH_CHAIN_SINGLETON(bhp, vc) ||
	    (SH_CHAIN_HASNEXT(bhp, vc) &&
	    F_ISSET(SH_CHAIN_NEXT(bhp, vc, __bh), BH_FROZEN) &&
	    bhp->td_off == INVALID_ROFF) ||
	    (SH_CHAIN_HASPREV(bhp, vc) ?
	    IS_MAX_LSN(*VISIBLE_LSN(dbenv, bhp)) :
	    BH_OBSOLETE(bhp, hp->old_reader)));

	/*
	 * Delete the buffer header from the hash bucket queue or the
	 * version chain and reset the hash bucket's priority, if necessary.
	 */
	reorder = (__memp_bh_priority(bhp) == bhp->priority);
	prev_bhp = SH_CHAIN_PREV(bhp, vc, __bh);
	if ((next_bhp = SH_CHAIN_NEXT(bhp, vc, __bh)) == NULL) {
		if (prev_bhp != NULL)
			SH_TAILQ_INSERT_AFTER(&hp->hash_bucket,
			    bhp, prev_bhp, hq, __bh);
		SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);
		next_bhp = prev_bhp;
	}
	SH_CHAIN_REMOVE(bhp, vc, __bh);
	if (reorder) {
		if (next_bhp != NULL)
			__memp_bucket_reorder(dbenv, hp, next_bhp);
		else
			hp->hash_priority = SH_TAILQ_EMPTY(&hp->hash_bucket) ?
			    0 : BH_PRIORITY(
			    SH_TAILQ_FIRSTP(&hp->hash_bucket, __bh));
	}

#ifdef DIAGNOSTIC
	__memp_check_order(dbenv, hp);
#endif

	/*
	 * Remove the reference to this buffer from the transaction that
	 * created it, if any.  When the BH_FREE_UNLOCKED flag is set, we're
	 * discarding the environment, so the transaction region is already
	 * gone.
	 */
	if (bhp->td_off != INVALID_ROFF && !LF_ISSET(BH_FREE_UNLOCKED)) {
		ret = __txn_remove_buffer(
		    dbenv, BH_OWNER(dbenv, bhp), hp->mtx_hash);
		bhp->td_off = INVALID_ROFF;
	}

	/*
	 * We're going to use the memory for something else -- it had better be
	 * accessible.
	 */
	MVCC_MPROTECT(bhp->buf, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC);

	/*
	 * If we're only removing this header from the chain for reuse, we're
	 * done.
	 */
	if (LF_ISSET(BH_FREE_REUSE))
		return (0);

	/*
	 * Discard the hash bucket's mutex, it's no longer needed, and
	 * we don't want to be holding it when acquiring other locks.
	 */
	if (!LF_ISSET(BH_FREE_UNLOCKED))
		MUTEX_UNLOCK(dbenv, hp->mtx_hash);

	/*
	 * If we're not reusing the buffer immediately, free the buffer for
	 * real.
	 */
	if (LF_ISSET(BH_FREE_FREEMEM)) {
		MPOOL_REGION_LOCK(dbenv, &dbmp->reginfo[n_cache]);

		__memp_free(&dbmp->reginfo[n_cache], mfp, bhp);
		c_mp = dbmp->reginfo[n_cache].primary;
		c_mp->stat.st_pages--;

		MPOOL_REGION_UNLOCK(dbenv, &dbmp->reginfo[n_cache]);
	}

	/*
	 * Decrement the reference count of the underlying MPOOLFILE.
	 * If this is its last reference, remove it.
	 */
	MUTEX_LOCK(dbenv, mfp->mutex);
	if (--mfp->block_cnt == 0 && mfp->mpf_cnt == 0) {
		if ((t_ret = __memp_mf_discard(dbmp, mfp)) != 0 && ret == 0)
			ret = t_ret;
	} else
		MUTEX_UNLOCK(dbenv, mfp->mutex);

	return (ret);
}
