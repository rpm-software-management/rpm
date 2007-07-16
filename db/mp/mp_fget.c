/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: mp_fget.c,v 12.33 2006/09/13 14:53:42 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

/*
 * __memp_fget_pp --
 *	DB_MPOOLFILE->get pre/post processing.
 *
 * PUBLIC: int __memp_fget_pp
 * PUBLIC:     __P((DB_MPOOLFILE *, db_pgno_t *, DB_TXN *, u_int32_t, void *));
 */
int
__memp_fget_pp(dbmfp, pgnoaddr, txnp, flags, addrp)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t *pgnoaddr;
	DB_TXN *txnp;
	u_int32_t flags;
	void *addrp;
{
	DB_ENV *dbenv;
	DB_THREAD_INFO *ip;
	int rep_check, ret;

	dbenv = dbmfp->dbenv;

	PANIC_CHECK(dbenv);
	MPF_ILLEGAL_BEFORE_OPEN(dbmfp, "DB_MPOOLFILE->get");

	/*
	 * Validate arguments.
	 *
	 * !!!
	 * Don't test for DB_MPOOL_CREATE and DB_MPOOL_NEW flags for readonly
	 * files here, and create non-existent pages in readonly files if the
	 * flags are set, later.  The reason is that the hash access method
	 * wants to get empty pages that don't really exist in readonly files.
	 * The only alternative is for hash to write the last "bucket" all the
	 * time, which we don't want to do because one of our big goals in life
	 * is to keep database files small.  It's sleazy as hell, but we catch
	 * any attempt to actually write the file in memp_fput().
	 */
#define	OKFLAGS		(DB_MPOOL_CREATE | DB_MPOOL_DIRTY | \
	    DB_MPOOL_EDIT | DB_MPOOL_LAST | DB_MPOOL_NEW)
	if (flags != 0) {
		if ((ret = __db_fchk(dbenv, "memp_fget", flags, OKFLAGS)) != 0)
			return (ret);

		switch (flags) {
		case DB_MPOOL_DIRTY:
		case DB_MPOOL_CREATE:
		case DB_MPOOL_EDIT:
		case DB_MPOOL_LAST:
		case DB_MPOOL_NEW:
			break;
		default:
			return (__db_ferr(dbenv, "memp_fget", 1));
		}
	}

	ENV_ENTER(dbenv, ip);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check && (ret = __op_rep_enter(dbenv)) != 0)
		goto err;
	ret = __memp_fget(dbmfp, pgnoaddr, txnp, flags, addrp);
	/*
	 * We only decrement the count in op_rep_exit if the operation fails.
	 * Otherwise the count will be decremented when the page is no longer
	 * pinned in memp_fput.
	 */
	if (ret != 0 && rep_check)
		(void)__op_rep_exit(dbenv);

	/* Similarly if an app has a page pinned it is ACTIVE. */
err:	if (ret != 0)
		ENV_LEAVE(dbenv, ip);

	return (ret);
}

/*
 * __memp_fget --
 *	Get a page from the file.
 *
 * PUBLIC: int __memp_fget
 * PUBLIC:     __P((DB_MPOOLFILE *, db_pgno_t *, DB_TXN *, u_int32_t, void *));
 */
int
__memp_fget(dbmfp, pgnoaddr, txn, flags, addrp)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t *pgnoaddr;
	DB_TXN *txn;
	u_int32_t flags;
	void *addrp;
{
	enum { FIRST_FOUND, FIRST_MISS, SECOND_FOUND, SECOND_MISS } state;
	BH *alloc_bhp, *bhp, *current_bhp, *frozen_bhp, *oldest_bhp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	MPOOL *c_mp, *mp;
	MPOOLFILE *mfp;
	REGINFO *infop;
	TXN_DETAIL *td;
	DB_LSN *read_lsnp;
	roff_t mf_offset;
	u_int32_t n_cache, st_hsearch;
	int b_incr, b_locked, dirty, edit, extending, first;
	int makecopy, mvcc, need_free, reorder, ret;

	*(void **)addrp = NULL;
	COMPQUIET(oldest_bhp, NULL);

	dbenv = dbmfp->dbenv;
	dbmp = dbenv->mp_handle;

	c_mp = NULL;
	mp = dbmp->reginfo[0].primary;
	mfp = dbmfp->mfp;
	mvcc = mfp->multiversion;
	mf_offset = R_OFFSET(dbmp->reginfo, mfp);
	alloc_bhp = bhp = frozen_bhp = NULL;
	read_lsnp = NULL;
	hp = NULL;
	b_incr = b_locked = extending = makecopy = ret = 0;
	n_cache = 0;
	infop = NULL;
	td = NULL;

	if (LF_ISSET(DB_MPOOL_DIRTY)) {
		if (F_ISSET(dbmfp, MP_READONLY)) {
			__db_errx(dbenv,
			    "%s: dirty flag set for readonly file page",
			    __memp_fn(dbmfp));
			return (EINVAL);
		}
		if ((ret = __db_fcchk(dbenv, "DB_MPOOLFILE->get",
		    flags, DB_MPOOL_DIRTY, DB_MPOOL_EDIT)) != 0)
			return (ret);
	}

	dirty = LF_ISSET(DB_MPOOL_DIRTY);
	edit = LF_ISSET(DB_MPOOL_EDIT);
	LF_CLR(DB_MPOOL_DIRTY | DB_MPOOL_EDIT);

	/*
	 * If the transaction is being used to update a multiversion database
	 * for the first time, set the read LSN.  In addition, if this is an
	 * update, allocate a mutex.  If no transaction has been supplied, that
	 * will be caught later, when we know whether one is required.
	 */
	if (mvcc && txn != NULL && txn->td != NULL) {
		/* We're only interested in the ultimate parent transaction. */
		while (txn->parent != NULL)
			txn = txn->parent;
		td = (TXN_DETAIL *)txn->td;
		if (F_ISSET(txn, TXN_SNAPSHOT)) {
			read_lsnp = &td->read_lsn;
			if (IS_MAX_LSN(*read_lsnp) &&
			    (ret = __log_current_lsn(dbenv, read_lsnp,
			    NULL, NULL)) != 0)
				return (ret);
		}
		if ((dirty || LF_ISSET(DB_MPOOL_CREATE | DB_MPOOL_NEW)) &&
		    td->mvcc_mtx == MUTEX_INVALID && (ret =
		    __mutex_alloc(dbenv, MTX_TXN_MVCC, 0, &td->mvcc_mtx)) != 0)
			return (ret);
	}

	switch (flags) {
	case DB_MPOOL_LAST:
		/* Get the last page number in the file. */
		MUTEX_LOCK(dbenv, mfp->mutex);
		*pgnoaddr = mfp->last_pgno;
		MUTEX_UNLOCK(dbenv, mfp->mutex);
		break;
	case DB_MPOOL_NEW:
		/*
		 * If always creating a page, skip the first search
		 * of the hash bucket.
		 */
		state = FIRST_MISS;
		goto alloc;
	case DB_MPOOL_CREATE:
	default:
		break;
	}

	/*
	 * If mmap'ing the file and the page is not past the end of the file,
	 * just return a pointer.  We can't use R_ADDR here: this is an offset
	 * into an mmap'd file, not a shared region, and doesn't change for
	 * private environments.
	 *
	 * The page may be past the end of the file, so check the page number
	 * argument against the original length of the file.  If we previously
	 * returned pages past the original end of the file, last_pgno will
	 * have been updated to match the "new" end of the file, and checking
	 * against it would return pointers past the end of the mmap'd region.
	 *
	 * If another process has opened the file for writing since we mmap'd
	 * it, we will start playing the game by their rules, i.e. everything
	 * goes through the cache.  All pages previously returned will be safe,
	 * as long as the correct locking protocol was observed.
	 *
	 * We don't discard the map because we don't know when all of the
	 * pages will have been discarded from the process' address space.
	 * It would be possible to do so by reference counting the open
	 * pages from the mmap, but it's unclear to me that it's worth it.
	 */
	if (dbmfp->addr != NULL &&
	    F_ISSET(mfp, MP_CAN_MMAP) && *pgnoaddr <= mfp->orig_last_pgno) {
		*(void **)addrp = (u_int8_t *)dbmfp->addr +
		    (*pgnoaddr * mfp->stat.st_pagesize);
		++mfp->stat.st_map;
		return (0);
	}

hb_search:
	/*
	 * Determine the cache and hash bucket where this page lives and get
	 * local pointers to them.  Reset on each pass through this code, the
	 * page number can change.
	 */
	n_cache = NCACHE(mp, mf_offset, *pgnoaddr);
	infop = &dbmp->reginfo[n_cache];
	c_mp = infop->primary;
	hp = R_ADDR(infop, c_mp->htab);
	hp = &hp[NBUCKET(c_mp, mf_offset, *pgnoaddr)];

	/* Search the hash chain for the page. */
retry:	st_hsearch = 0;
	MUTEX_LOCK(dbenv, hp->mtx_hash);
	b_locked = 1;
	SH_TAILQ_FOREACH(bhp, &hp->hash_bucket, hq, __bh) {
		++st_hsearch;
		if (bhp->pgno != *pgnoaddr || bhp->mf_offset != mf_offset)
			continue;

		if (mvcc) {
			/*
			 * Snapshot reads -- get the version of the page that
			 * was visible *at* the read_lsn.
			 */
			current_bhp = bhp;
			if (read_lsnp != NULL &&
			    !BH_OWNED_BY(dbenv, bhp, txn) && !edit) {
				while (bhp != NULL &&
				    bhp->td_off != INVALID_ROFF &&
				    log_compare(VISIBLE_LSN(dbenv, bhp),
				    read_lsnp) > 0)
					bhp = SH_CHAIN_PREV(bhp, vc, __bh);

				DB_ASSERT(dbenv, bhp != NULL);
			}

			makecopy = dirty && !BH_OWNED_BY(dbenv, bhp, txn);
			if (makecopy && bhp != current_bhp) {
				ret = DB_LOCK_DEADLOCK;
				goto err;
			}

			if (F_ISSET(bhp, BH_FROZEN) &&
			    !F_ISSET(bhp, BH_FREED)) {
				DB_ASSERT(dbenv, frozen_bhp == NULL);
				frozen_bhp = bhp;
			}
		}

		/*
		 * Increment the reference count.  We may discard the hash
		 * bucket lock as we evaluate and/or read the buffer, so we
		 * need to ensure it doesn't move and its contents remain
		 * unchanged.
		 */
		if (bhp->ref == UINT16_MAX) {
			__db_errx(dbenv,
			    "%s: page %lu: reference count overflow",
			    __memp_fn(dbmfp), (u_long)bhp->pgno);
			ret = __db_panic(dbenv, EINVAL);
			goto err;
		}
		++bhp->ref;
		b_incr = 1;

		/*
		 * BH_LOCKED --
		 * I/O is in progress or sync is waiting on the buffer to write
		 * it.  Because we've incremented the buffer reference count,
		 * we know the buffer can't move.  Unlock the bucket lock, wait
		 * for the buffer to become available, re-acquire the bucket.
		 */
		for (first = 1; F_ISSET(bhp, BH_LOCKED) &&
		    !F_ISSET(dbenv, DB_ENV_NOLOCKING); first = 0) {
			/*
			 * If someone is trying to sync this buffer and the
			 * buffer is hot, they may never get in.  Give up and
			 * try again.
			 */
			if (!first && bhp->ref_sync != 0) {
				--bhp->ref;
				MUTEX_UNLOCK(dbenv, hp->mtx_hash);
				b_incr = b_locked = 0;
				__os_sleep(dbenv, 0, 1);
				goto retry;
			}

			/*
			 * If we're the first thread waiting on I/O, set the
			 * flag so the thread doing I/O knows to wake us up,
			 * and lock the mutex.
			 */
			if (!F_ISSET(hp, IO_WAITER)) {
				F_SET(hp, IO_WAITER);
				MUTEX_LOCK(dbenv, hp->mtx_io);
			}
			++hp->hash_io_wait;

			/* Release the hash bucket lock. */
			MUTEX_UNLOCK(dbenv, hp->mtx_hash);

			/* Wait for I/O to finish. */
			MUTEX_LOCK(dbenv, hp->mtx_io);
			MUTEX_UNLOCK(dbenv, hp->mtx_io);

			/* Re-acquire the hash bucket lock. */
			MUTEX_LOCK(dbenv, hp->mtx_hash);
		}

		/*
		 * If the buffer was frozen before we waited for any I/O to
		 * complete and is still frozen, we need to unfreeze it.
		 * Otherwise, it was unfrozen while we waited, and we need to
		 * search again.
		 */
		if (frozen_bhp != NULL && !F_ISSET(frozen_bhp, BH_FROZEN)) {
thawed:			need_free = (--frozen_bhp->ref == 0);
			b_incr = 0;
			MUTEX_UNLOCK(dbenv, hp->mtx_hash);
			MPOOL_REGION_LOCK(dbenv, infop);
			if (alloc_bhp != NULL) {
				__memp_free(infop, mfp, alloc_bhp);
				alloc_bhp = NULL;
			}
			if (need_free)
				SH_TAILQ_INSERT_TAIL(&c_mp->free_frozen,
				    frozen_bhp, hq);
			MPOOL_REGION_UNLOCK(dbenv, infop);
			frozen_bhp = NULL;
			goto retry;
		}

		++mfp->stat.st_cache_hit;
		break;
	}

	/*
	 * Update the hash bucket search statistics -- do now because our next
	 * search may be for a different bucket.
	 */
	++c_mp->stat.st_hash_searches;
	if (st_hsearch > c_mp->stat.st_hash_longest)
		c_mp->stat.st_hash_longest = st_hsearch;
	c_mp->stat.st_hash_examined += st_hsearch;

	/*
	 * There are 4 possible paths to this location:
	 *
	 * FIRST_MISS:
	 *	Didn't find the page in the hash bucket on our first pass:
	 *	bhp == NULL, alloc_bhp == NULL
	 *
	 * FIRST_FOUND:
	 *	Found the page in the hash bucket on our first pass:
	 *	bhp != NULL, alloc_bhp == NULL
	 *
	 * SECOND_FOUND:
	 *	Didn't find the page in the hash bucket on the first pass,
	 *	allocated space, and found the page in the hash bucket on
	 *	our second pass:
	 *	bhp != NULL, alloc_bhp != NULL
	 *
	 * SECOND_MISS:
	 *	Didn't find the page in the hash bucket on the first pass,
	 *	allocated space, and didn't find the page in the hash bucket
	 *	on our second pass:
	 *	bhp == NULL, alloc_bhp != NULL
	 */
	state = bhp == NULL ?
	    (alloc_bhp == NULL ? FIRST_MISS : SECOND_MISS) :
	    (alloc_bhp == NULL ? FIRST_FOUND : SECOND_FOUND);

	switch (state) {
	case FIRST_FOUND:
		/*
		 * If we are to free the buffer, then this had better be the
		 * only reference. If so, just free the buffer.  If not,
		 * complain and get out.
		 */
		if (flags == DB_MPOOL_FREE) {
			if (--bhp->ref == 0) {
				/*
				 * In a multiversion database, this page could
				 * be requested again so we have to leave it in
				 * cache for now.  It should *not* ever be
				 * requested again for modification without an
				 * intervening DB_MPOOL_CREATE or DB_MPOOL_NEW.
				 *
				 * Mark it with BH_FREED so we don't reuse the
				 * data when the page is resurrected.
				 */
				if (mvcc && (!SH_CHAIN_SINGLETON(bhp, vc) ||
				    bhp->td_off == INVALID_ROFF ||
				    !IS_MAX_LSN(*VISIBLE_LSN(dbenv, bhp)))) {
					if (F_ISSET(bhp, BH_DIRTY)) {
						--hp->hash_page_dirty;
						F_CLR(bhp,
						    BH_DIRTY | BH_DIRTY_CREATE);
					}
					F_SET(bhp, BH_FREED);
					MUTEX_UNLOCK(dbenv, hp->mtx_hash);
					return (0);
				}
				return (__memp_bhfree(
				    dbmp, hp, bhp, BH_FREE_FREEMEM));
			}
			__db_errx(dbenv,
			    "File %s: freeing pinned buffer for page %lu",
				__memp_fns(dbmp, mfp), (u_long)*pgnoaddr);
			ret = __db_panic(dbenv, EINVAL);
			goto err;
		}

		if (mvcc) {
			if (flags == DB_MPOOL_CREATE &&
			    F_ISSET(bhp, BH_FREED)) {
				extending = makecopy = 1;
				MUTEX_UNLOCK(dbenv, hp->mtx_hash);
				MUTEX_LOCK(dbenv, mfp->mutex);
				if (*pgnoaddr > mfp->last_pgno)
					mfp->last_pgno = *pgnoaddr;
				MUTEX_UNLOCK(dbenv, mfp->mutex);
				MUTEX_LOCK(dbenv, hp->mtx_hash);
			}

			/*
			 * With multiversion databases, we might need to
			 * allocate a new buffer into which we can copy the one
			 * that we found.  In that case, check the last buffer
			 * in the chain to see whether we can reuse an obsolete
			 * buffer.
			 *
			 * To provide snapshot isolation, we need to make sure
			 * that we've seen a buffer older than the oldest
			 * snapshot read LSN.
			 */
			if ((makecopy || frozen_bhp != NULL) && (oldest_bhp =
			    SH_CHAIN_PREV(bhp, vc, __bh)) != NULL) {
				while (SH_CHAIN_HASPREV(oldest_bhp, vc))
					oldest_bhp = SH_CHAIN_PREVP(oldest_bhp,
					    vc, __bh);

				if (oldest_bhp->ref == 0 &&
				    !F_ISSET(oldest_bhp, BH_FROZEN) &&
				    (BH_OBSOLETE(oldest_bhp, hp->old_reader) ||
				    ((ret = __txn_oldest_reader(dbenv,
				    &hp->old_reader)) == 0 &&
				    BH_OBSOLETE(oldest_bhp, hp->old_reader)))) {
					if ((ret = __memp_bhfree(dbmp, hp,
					    oldest_bhp, BH_FREE_REUSE)) != 0)
						goto err;
					alloc_bhp = oldest_bhp;
				} else if (ret != 0)
					goto err;

				DB_ASSERT(dbenv, alloc_bhp == NULL ||
				    !F_ISSET(alloc_bhp, BH_FROZEN));
			}
		}

		if ((!makecopy && frozen_bhp == NULL) || alloc_bhp != NULL)
			/* We found the buffer -- we're done. */
			break;

		/* FALLTHROUGH */
	case FIRST_MISS:
		/*
		 * We didn't find the buffer in our first check.  Figure out
		 * if the page exists, and allocate structures so we can add
		 * the page to the buffer pool.
		 */
		MUTEX_UNLOCK(dbenv, hp->mtx_hash);
		b_locked = 0;

		/*
		 * The buffer is not in the pool, so we don't need to free it.
		 */
		if (flags == DB_MPOOL_FREE)
			return (0);

alloc:		/*
		 * If DB_MPOOL_NEW is set, we have to allocate a page number.
		 * If neither DB_MPOOL_CREATE or DB_MPOOL_NEW is set, then
		 * it's an error to try and get a page past the end of file.
		 */
		MUTEX_LOCK(dbenv, mfp->mutex);
		switch (flags) {
		case DB_MPOOL_NEW:
			extending = 1;
			if (mfp->maxpgno != 0 &&
			    mfp->last_pgno >= mfp->maxpgno) {
				__db_errx(
				    dbenv, "%s: file limited to %lu pages",
				    __memp_fn(dbmfp), (u_long)mfp->maxpgno);
				ret = ENOSPC;
			} else
				*pgnoaddr = mfp->last_pgno + 1;
			break;
		case DB_MPOOL_CREATE:
			if (mfp->maxpgno != 0 && *pgnoaddr > mfp->maxpgno) {
				__db_errx(
				    dbenv, "%s: file limited to %lu pages",
				    __memp_fn(dbmfp), (u_long)mfp->maxpgno);
				ret = ENOSPC;
			} else if (!extending)
				extending = *pgnoaddr > mfp->last_pgno;
			break;
		default:
			ret = *pgnoaddr > mfp->last_pgno ? DB_PAGE_NOTFOUND : 0;
			break;
		}
		MUTEX_UNLOCK(dbenv, mfp->mutex);
		if (ret != 0)
			goto err;

		/*
		 * !!!
		 * In the DB_MPOOL_NEW code path, mf_offset and n_cache have
		 * not yet been initialized.
		 */
		mf_offset = R_OFFSET(dbmp->reginfo, mfp);
		n_cache = NCACHE(mp, mf_offset, *pgnoaddr);
		infop = &dbmp->reginfo[n_cache];
		c_mp = infop->primary;

		/* Allocate a new buffer header and data space. */
		if ((ret =
		    __memp_alloc(dbmp,infop, mfp, 0, NULL, &alloc_bhp)) != 0)
			goto err;
#ifdef DIAGNOSTIC
		if ((uintptr_t)alloc_bhp->buf & (sizeof(size_t) - 1)) {
			__db_errx(dbenv,
		    "DB_MPOOLFILE->get: buffer data is NOT size_t aligned");
			ret = __db_panic(dbenv, EINVAL);
			goto err;
		}
#endif
		/*
		 * If we are extending the file, we'll need the mfp lock
		 * again.
		 */
		if (extending)
			MUTEX_LOCK(dbenv, mfp->mutex);

		/*
		 * DB_MPOOL_NEW does not guarantee you a page unreferenced by
		 * any other thread of control.  (That guarantee is interesting
		 * for DB_MPOOL_NEW, unlike DB_MPOOL_CREATE, because the caller
		 * did not specify the page number, and so, may reasonably not
		 * have any way to lock the page outside of mpool.) Regardless,
		 * if we allocate the page, and some other thread of control
		 * requests the page by number, we will not detect that and the
		 * thread of control that allocated using DB_MPOOL_NEW may not
		 * have a chance to initialize the page.  (Note: we *could*
		 * detect this case if we set a flag in the buffer header which
		 * guaranteed that no gets of the page would succeed until the
		 * reference count went to 0, that is, until the creating page
		 * put the page.)  What we do guarantee is that if two threads
		 * of control are both doing DB_MPOOL_NEW calls, they won't
		 * collide, that is, they won't both get the same page.
		 *
		 * There's a possibility that another thread allocated the page
		 * we were planning to allocate while we were off doing buffer
		 * allocation.  We can do that by making sure the page number
		 * we were going to use is still available.  If it's not, then
		 * we check to see if the next available page number hashes to
		 * the same mpool region as the old one -- if it does, we can
		 * continue, otherwise, we have to start over.
		 */
		if (flags == DB_MPOOL_NEW && *pgnoaddr != mfp->last_pgno + 1) {
			*pgnoaddr = mfp->last_pgno + 1;
			if (n_cache != NCACHE(mp, mf_offset, *pgnoaddr)) {
				/*
				 * flags == DB_MPOOL_NEW, so extending is set
				 * and we're holding the mfp locked.
				 */
				MUTEX_UNLOCK(dbenv, mfp->mutex);

				MPOOL_REGION_LOCK(dbenv, infop);
				__memp_free(infop, mfp, alloc_bhp);
				c_mp->stat.st_pages--;
				MPOOL_REGION_UNLOCK(dbenv, infop);

				alloc_bhp = NULL;
				goto alloc;
			}
		}

		/*
		 * We released the mfp lock, so another thread might have
		 * extended the file.  Update the last_pgno and initialize
		 * the file, as necessary, if we extended the file.
		 */
		if (extending) {
			if (*pgnoaddr > mfp->last_pgno)
				mfp->last_pgno = *pgnoaddr;

			MUTEX_UNLOCK(dbenv, mfp->mutex);
			if (ret != 0)
				goto err;
		}

		/*
		 * If we're doing copy-on-write, we will already have the
		 * buffer header.  In that case, we don't need to search again.
		 */
		if (bhp != NULL) {
			MUTEX_LOCK(dbenv, hp->mtx_hash);
			b_locked = 1;
			break;
		}
		goto hb_search;
	case SECOND_FOUND:
		/*
		 * We allocated buffer space for the requested page, but then
		 * found the page in the buffer cache on our second check.
		 * That's OK -- we can use the page we found in the pool,
		 * unless DB_MPOOL_NEW is set.  If we're about to copy-on-write,
		 * this is exactly the situation we want.
		 *
		 * For multiversion files, we may have left some pages in cache
		 * beyond the end of a file after truncating.  In that case, we
		 * would get to here with extending set.  If so, we need to
		 * insert the new page in the version chain similar to when
		 * we copy on write.
		 */
		if (extending && F_ISSET(bhp, BH_FREED))
			makecopy = 1;
		if (makecopy || frozen_bhp != NULL)
			break;

		/* Free the allocated memory, we no longer need it.  Since we
		 * can't acquire the region lock while holding the hash bucket
		 * lock, we have to release the hash bucket and re-acquire it.
		 * That's OK, because we have the buffer pinned down.
		 */
		MUTEX_UNLOCK(dbenv, hp->mtx_hash);
		MPOOL_REGION_LOCK(dbenv, infop);
		__memp_free(infop, mfp, alloc_bhp);
		c_mp->stat.st_pages--;
		MPOOL_REGION_UNLOCK(dbenv, infop);
		alloc_bhp = NULL;

		/*
		 * We can't use the page we found in the pool if DB_MPOOL_NEW
		 * was set.  (For details, see the above comment beginning
		 * "DB_MPOOL_NEW does not guarantee you a page unreferenced by
		 * any other thread of control".)  If DB_MPOOL_NEW is set, we
		 * release our pin on this particular buffer, and try to get
		 * another one.
		 */
		if (flags == DB_MPOOL_NEW) {
			--bhp->ref;
			b_incr = b_locked = 0;
			bhp = NULL;
			goto alloc;
		}

		/* We can use the page -- get the bucket lock. */
		MUTEX_LOCK(dbenv, hp->mtx_hash);
		break;
	case SECOND_MISS:
		/*
		 * We allocated buffer space for the requested page, and found
		 * the page still missing on our second pass through the buffer
		 * cache.  Instantiate the page.
		 */
		bhp = alloc_bhp;
		alloc_bhp = NULL;

		/*
		 * Initialize all the BH and hash bucket fields so we can call
		 * __memp_bhfree if an error occurs.
		 *
		 * Append the buffer to the tail of the bucket list and update
		 * the hash bucket's priority.
		 */
		/*lint --e{668} (flexelint: bhp cannot be NULL). */
#ifdef DIAG_MVCC
		memset(bhp, 0, SSZ(BH, align_off));
#else
		memset(bhp, 0, sizeof(BH));
#endif
		bhp->ref = 1;
		b_incr = 1;
		bhp->priority = UINT32_MAX;
		bhp->pgno = *pgnoaddr;
		bhp->mf_offset = mf_offset;
		SH_TAILQ_INSERT_TAIL(&hp->hash_bucket, bhp, hq);
		SH_CHAIN_INIT(bhp, vc);

		hp->hash_priority =
		    BH_PRIORITY(SH_TAILQ_FIRSTP(&hp->hash_bucket, __bh));

		/* We created a new page, it starts dirty. */
		if (extending) {
			++hp->hash_page_dirty;
			F_SET(bhp, BH_DIRTY | BH_DIRTY_CREATE);
		}

		/*
		 * If we created the page, zero it out.  If we didn't create
		 * the page, read from the backing file.
		 *
		 * !!!
		 * DB_MPOOL_NEW doesn't call the pgin function.
		 *
		 * If DB_MPOOL_CREATE is used, then the application's pgin
		 * function has to be able to handle pages of 0's -- if it
		 * uses DB_MPOOL_NEW, it can detect all of its page creates,
		 * and not bother.
		 *
		 * If we're running in diagnostic mode, smash any bytes on the
		 * page that are unknown quantities for the caller.
		 *
		 * Otherwise, read the page into memory, optionally creating it
		 * if DB_MPOOL_CREATE is set.
		 */
		if (extending) {
			MVCC_MPROTECT(bhp->buf, mfp->stat.st_pagesize,
			    PROT_READ | PROT_WRITE);
			if (mfp->clear_len == DB_CLEARLEN_NOTSET)
				memset(bhp->buf, 0, mfp->stat.st_pagesize);
			else {
				memset(bhp->buf, 0, mfp->clear_len);
#if defined(DIAGNOSTIC) || defined(UMRW)
				memset(bhp->buf + mfp->clear_len, CLEAR_BYTE,
				    mfp->stat.st_pagesize - mfp->clear_len);
#endif
			}

			if (flags == DB_MPOOL_CREATE && mfp->ftype != 0)
				F_SET(bhp, BH_CALLPGIN);

			++mfp->stat.st_page_create;
		} else {
			F_SET(bhp, BH_TRASH);
			++mfp->stat.st_cache_miss;
		}

		/* Increment buffer count referenced by MPOOLFILE. */
		MUTEX_LOCK(dbenv, mfp->mutex);
		++mfp->block_cnt;
		MUTEX_UNLOCK(dbenv, mfp->mutex);
	}

	DB_ASSERT(dbenv, bhp != NULL);
	DB_ASSERT(dbenv, bhp->ref != 0);

	/* We've got a buffer header we're re-instantiating. */
	if (frozen_bhp != NULL) {
		DB_ASSERT(dbenv, alloc_bhp != NULL);

		/*
		 * If the empty buffer has been filled in the meantime, don't
		 * overwrite it.
		 */
		if (!F_ISSET(frozen_bhp, BH_FROZEN))
			goto thawed;
		else {
			if ((ret = __memp_bh_thaw(dbmp, infop, hp,
			    frozen_bhp, alloc_bhp)) != 0)
				goto err;
			bhp = alloc_bhp;
		}

		frozen_bhp = alloc_bhp = NULL;

		/*
		 * If we're updating a buffer that was frozen, we have to go
		 * through all of that again to allocate another buffer to hold
		 * the new copy.
		 */
		if (makecopy) {
			MUTEX_UNLOCK(dbenv, hp->mtx_hash);
			goto alloc;
		}
	}

	/*
	 * If we're the only reference, update buffer and bucket priorities.
	 * We may be about to release the hash bucket lock, and everything
	 * should be correct, first.  (We've already done this if we created
	 * the buffer, so there is no need to do it again.)
	 */
	if (state != SECOND_MISS && bhp->ref == 1) {
		if (SH_CHAIN_SINGLETON(bhp, vc)) {
			bhp->priority = UINT32_MAX;
			if (bhp != SH_TAILQ_LAST(&hp->hash_bucket, hq, __bh)) {
				SH_TAILQ_REMOVE(&hp->hash_bucket,
				    bhp, hq, __bh);
				SH_TAILQ_INSERT_TAIL(&hp->hash_bucket, bhp, hq);
			}
			hp->hash_priority = BH_PRIORITY(
			    SH_TAILQ_FIRSTP(&hp->hash_bucket, __bh));
		} else {
			reorder = (BH_PRIORITY(bhp) == bhp->priority);
			bhp->priority = UINT32_MAX;
			if (reorder)
				__memp_bucket_reorder(dbenv, hp, bhp);
		}
	}

	/*
	 * BH_TRASH --
	 * The buffer we found may need to be filled from the disk.
	 *
	 * It's possible for the read function to fail, which means we fail as
	 * well.  Note, the __memp_pgread() function discards and reacquires
	 * the hash lock, so the buffer must be pinned down so that it cannot
	 * move and its contents are unchanged.  Discard the buffer on failure
	 * unless another thread is waiting on our I/O to complete.  It's OK to
	 * leave the buffer around, as the waiting thread will see the BH_TRASH
	 * flag set, and will also attempt to discard it.  If there's a waiter,
	 * we need to decrement our reference count.
	 */
	if (F_ISSET(bhp, BH_TRASH) &&
	    (ret = __memp_pgread(dbmfp,
	    hp, bhp, LF_ISSET(DB_MPOOL_CREATE) ? 1 : 0)) != 0)
		goto err;

	/*
	 * BH_CALLPGIN --
	 * The buffer was processed for being written to disk, and now has
	 * to be re-converted for use.
	 */
	if (F_ISSET(bhp, BH_CALLPGIN)) {
		MVCC_MPROTECT(bhp->buf, mfp->stat.st_pagesize,
		    PROT_READ | PROT_WRITE);
		if ((ret = __memp_pg(dbmfp, bhp, 1)) != 0)
			goto err;
		F_CLR(bhp, BH_CALLPGIN);
	}

	/* Copy-on-write. */
	if (makecopy && state != SECOND_MISS) {
		DB_ASSERT(dbenv, !SH_CHAIN_HASNEXT(bhp, vc));
		DB_ASSERT(dbenv, bhp != NULL);
		DB_ASSERT(dbenv, alloc_bhp != NULL);
		DB_ASSERT(dbenv, alloc_bhp != bhp);

		if (bhp->ref == 1)
			MVCC_MPROTECT(bhp->buf, mfp->stat.st_pagesize,
			    PROT_READ);

		alloc_bhp->ref = 1;
		alloc_bhp->ref_sync = 0;
		alloc_bhp->flags = F_ISSET(bhp, BH_DIRTY | BH_DIRTY_CREATE);
		F_CLR(bhp, BH_DIRTY | BH_DIRTY_CREATE);
		alloc_bhp->priority = bhp->priority;
		alloc_bhp->pgno = bhp->pgno;
		alloc_bhp->mf_offset = bhp->mf_offset;
		alloc_bhp->td_off = INVALID_ROFF;
		if (extending) {
			memset(alloc_bhp->buf, 0, mfp->stat.st_pagesize);
			F_SET(alloc_bhp, BH_DIRTY_CREATE);
		} else
			memcpy(alloc_bhp->buf, bhp->buf, mfp->stat.st_pagesize);

		SH_CHAIN_INSERT_AFTER(bhp, alloc_bhp, vc, __bh);
		SH_TAILQ_INSERT_BEFORE(&hp->hash_bucket,
		    bhp, alloc_bhp, hq, __bh);
		SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);
		if (--bhp->ref == 0)
			MVCC_MPROTECT(bhp->buf, mfp->stat.st_pagesize, 0);
		bhp = alloc_bhp;

		if (alloc_bhp != oldest_bhp) {
			MUTEX_LOCK(dbenv, mfp->mutex);
			++mfp->block_cnt;
			MUTEX_UNLOCK(dbenv, mfp->mutex);
		}

		alloc_bhp = NULL;
	}

	if ((dirty || edit || extending) && !F_ISSET(bhp, BH_DIRTY)) {
		++hp->hash_page_dirty;
		F_SET(bhp, BH_DIRTY);
	}

	if (mvcc &&
	    ((makecopy && !extending) || (extending && txn != NULL)) &&
	    (ret = __memp_bh_settxn(dbmp, mfp, bhp, td)) != 0)
		goto err;

	MVCC_MPROTECT(bhp->buf, mfp->stat.st_pagesize, PROT_READ |
	    (dirty || edit || extending || F_ISSET(bhp, BH_DIRTY) ?
	    PROT_WRITE : 0));

#ifdef DIAGNOSTIC
	__memp_check_order(dbenv, hp);
#endif

	DB_ASSERT(dbenv, !(mfp->multiversion && F_ISSET(bhp, BH_DIRTY)) ||
	    !SH_CHAIN_HASNEXT(bhp, vc));

	MUTEX_UNLOCK(dbenv, hp->mtx_hash);

#ifdef DIAGNOSTIC
	/* Update the file's pinned reference count. */
	MPOOL_SYSTEM_LOCK(dbenv);
	++dbmfp->pinref;
	MPOOL_SYSTEM_UNLOCK(dbenv);

	/*
	 * We want to switch threads as often as possible, and at awkward
	 * times.  Yield every time we get a new page to ensure contention.
	 */
	if (F_ISSET(dbenv, DB_ENV_YIELDCPU))
		__os_yield(dbenv);
#endif

	DB_ASSERT(dbenv, alloc_bhp == NULL);

	*(void **)addrp = bhp->buf;
	return (0);

err:	/*
	 * Discard our reference.  If we're the only reference, discard the
	 * the buffer entirely.  If we held a reference to a buffer, we are
	 * also still holding the hash bucket mutex.
	 */
	if (b_incr || frozen_bhp != NULL) {
		if (!b_locked) {
			MUTEX_LOCK(dbenv, hp->mtx_hash);
			b_locked = 1;
		}
		if (frozen_bhp != NULL)
			--frozen_bhp;
		if (b_incr && --bhp->ref == 0) {
			(void)__memp_bhfree(dbmp, hp, bhp, BH_FREE_FREEMEM);
			b_locked = 0;
		}
	}
	if (b_locked)
		MUTEX_UNLOCK(dbenv, hp->mtx_hash);

	/* If alloc_bhp is set, free the memory. */
	if (alloc_bhp != NULL) {
		MPOOL_REGION_LOCK(dbenv, infop);
		__memp_free(infop, mfp, alloc_bhp);
		c_mp->stat.st_pages--;
		MPOOL_REGION_UNLOCK(dbenv, infop);
	}

	return (ret);
}
