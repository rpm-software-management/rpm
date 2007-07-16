/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: mp_alloc.c,v 12.20 2006/09/07 15:11:26 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static void __memp_bad_buffer __P((DB_ENV *, DB_MPOOL_HASH *));

/*
 * __memp_alloc --
 *	Allocate some space from a cache region.
 *
 * PUBLIC: int __memp_alloc __P((DB_MPOOL *,
 * PUBLIC:     REGINFO *, MPOOLFILE *, size_t, roff_t *, void *));
 */
int
__memp_alloc(dbmp, infop, mfp, len, offsetp, retp)
	DB_MPOOL *dbmp;
	REGINFO *infop;
	MPOOLFILE *mfp;
	size_t len;
	roff_t *offsetp;
	void *retp;
{
	BH *bhp, *oldest_bhp, *tbhp;
	BH_FROZEN_PAGE *frozen_bhp;
	DB_ENV *dbenv;
	DB_MPOOL_HASH *dbht, *hp, *hp_end, *hp_tmp;
	MPOOL *c_mp;
	MPOOLFILE *bh_mfp;
	size_t freed_space;
	db_mutex_t mutex;
	u_int32_t buckets, buffers, high_priority, priority;
	u_int32_t put_counter, total_buckets;
	int aggressive, alloc_freeze, giveup, got_oldest, ret;
	u_int8_t *endp;
	void *p;

	dbenv = dbmp->dbenv;
	c_mp = infop->primary;
	dbht = R_ADDR(infop, c_mp->htab);
	hp_end = &dbht[c_mp->htab_buckets];

	buckets = buffers = put_counter = total_buckets = 0;
	aggressive = alloc_freeze = giveup = got_oldest = 0;
	hp_tmp = NULL;

	c_mp->stat.st_alloc++;

	/*
	 * If we're allocating a buffer, and the one we're discarding is the
	 * same size, we don't want to waste the time to re-integrate it into
	 * the shared memory free list.  If the DB_MPOOLFILE argument isn't
	 * NULL, we'll compare the underlying page sizes of the two buffers
	 * before free-ing and re-allocating buffers.
	 */
	if (mfp != NULL) {
		len = SSZA(BH, buf) + mfp->stat.st_pagesize;
		/* Add space for alignment padding for MVCC diagnostics. */
		MVCC_BHSIZE(mfp, len);
	}

	MPOOL_REGION_LOCK(dbenv, infop);

	/*
	 * Anything newer than 1/10th of the buffer pool is ignored during
	 * allocation (unless allocation starts failing).
	 */
	high_priority = c_mp->lru_count - c_mp->stat.st_pages / 10;

	/*
	 * First we try to allocate from free memory.  If that fails, scan the
	 * buffer pool to find buffers with low priorities.  We consider small
	 * sets of hash buckets each time to limit the amount of work needing
	 * to be done.  This approximates LRU, but not very well.  We either
	 * find a buffer of the same size to use, or we will free 3 times what
	 * we need in the hopes it will coalesce into a contiguous chunk of the
	 * right size.  In the latter case we branch back here and try again.
	 */
alloc:	if ((ret = __db_shalloc(infop, len, 0, &p)) == 0) {
		if (mfp != NULL)
			c_mp->stat.st_pages++;
		MPOOL_REGION_UNLOCK(dbenv, infop);
		/*
		 * For MVCC diagnostics, align the pointer so that the buffer
		 * starts on a page boundary.
		 */
		MVCC_BHALIGN(mfp, p);

found:		if (offsetp != NULL)
			*offsetp = R_OFFSET(infop, p);
		*(void **)retp = p;

		/*
		 * Update the search statistics.
		 *
		 * We're not holding the region locked here, these statistics
		 * can't be trusted.
		 */
		total_buckets += buckets;
		if (total_buckets != 0) {
			if (total_buckets > c_mp->stat.st_alloc_max_buckets)
				c_mp->stat.st_alloc_max_buckets = total_buckets;
			c_mp->stat.st_alloc_buckets += total_buckets;
		}
		if (buffers != 0) {
			if (buffers > c_mp->stat.st_alloc_max_pages)
				c_mp->stat.st_alloc_max_pages = buffers;
			c_mp->stat.st_alloc_pages += buffers;
		}
		return (0);
	} else if (giveup || c_mp->stat.st_pages == 0) {
		MPOOL_REGION_UNLOCK(dbenv, infop);

		__db_errx(dbenv,
		    "unable to allocate space from the buffer cache");
		return (ret);
	}
	ret = 0;

	/*
	 * We re-attempt the allocation every time we've freed 3 times what
	 * we need.  Reset our free-space counter.
	 */
	freed_space = 0;
	total_buckets += buckets;
	buckets = 0;

	/*
	 * Walk the hash buckets and find the next two with potentially useful
	 * buffers.  Free the buffer with the lowest priority from the buckets'
	 * chains.
	 */
	for (;;) {
		/* All pages have been freed, make one last try */
		if (c_mp->stat.st_pages == 0)
			goto alloc;

		/* Check for wrap around. */
		hp = &dbht[c_mp->last_checked++];
		if (hp >= hp_end) {
			c_mp->last_checked = 0;
			hp = &dbht[c_mp->last_checked++];
		}

		/*
		 * Skip empty buckets.
		 *
		 * We can check for empty buckets before locking as we
		 * only care if the pointer is zero or non-zero.
		 */
		if (SH_TAILQ_FIRST(&hp->hash_bucket, __bh) == NULL)
			continue;

		/*
		 * The failure mode is when there are too many buffers we can't
		 * write or there's not enough memory in the system.  We don't
		 * have a way to know that allocation has no way to succeed.
		 * We fail if there were no pages returned to the cache after
		 * we've been trying for a relatively long time.
		 *
		 * Get aggressive if we've tried to flush the number of hash
		 * buckets as are in the system and have not found any more
		 * space.  Aggressive means:
		 *
		 * a: set a flag to attempt to flush high priority buffers as
		 *    well as other buffers.
		 * b: sync the mpool to force out queue extent pages.  While we
		 *    might not have enough space for what we want and flushing
		 *    is expensive, why not?
		 * c: look at a buffer in every hash bucket rather than choose
		 *    the more preferable of two.
		 * d: start to think about giving up.
		 *
		 * If we get here twice, sleep for a second, hopefully someone
		 * else will run and free up some memory.
		 *
		 * Always try to allocate memory too, in case some other thread
		 * returns its memory to the region.
		 *
		 * !!!
		 * This test ignores pathological cases like no buffers in the
		 * system -- that shouldn't be possible.
		 */
		if ((++buckets % c_mp->htab_buckets) == 0) {
			if (freed_space > 0)
				goto alloc;
			MPOOL_REGION_UNLOCK(dbenv, infop);

			switch (++aggressive) {
			case 1:
				break;
			case 2:
				put_counter = c_mp->put_counter;
				/* FALLTHROUGH */
			case 3:
			case 4:
			case 5:
			case 6:
				(void)__memp_sync_int(
				    dbenv, NULL, 0, DB_SYNC_ALLOC, NULL);

				__os_sleep(dbenv, 1, 0);
				break;
			default:
				aggressive = 1;
				if (put_counter == c_mp->put_counter)
					giveup = 1;
				break;
			}

			MPOOL_REGION_LOCK(dbenv, infop);
			goto alloc;
		}

		if (!aggressive) {
			/* Skip high priority buckets. */
			if (hp->hash_priority > high_priority)
				continue;

			/*
			 * Find two buckets and select the one with the lowest
			 * priority.  Performance testing shows that looking
			 * at two improves the LRUness and looking at more only
			 * does a little better.
			 */
			if (hp_tmp == NULL) {
				hp_tmp = hp;
				continue;
			}
			if (hp->hash_priority > hp_tmp->hash_priority)
				hp = hp_tmp;
			hp_tmp = NULL;
		}

		/* Remember the priority of the buffer we're looking for. */
		priority = hp->hash_priority;

		/* Unlock the region and lock the hash bucket. */
		MPOOL_REGION_UNLOCK(dbenv, infop);
		mutex = hp->mtx_hash;
		MUTEX_LOCK(dbenv, mutex);

#ifdef DIAGNOSTIC
		__memp_check_order(dbenv, hp);
#endif
		/*
		 * The lowest priority page is first in the bucket, as they are
		 * maintained in sorted order.
		 *
		 * The buffer may have been freed or its priority changed while
		 * we switched from the region lock to the hash lock.  If so,
		 * we have to restart.  We will still take the first buffer on
		 * the bucket's list, though, if it has a low enough priority.
		 */
this_hb:	if ((bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh)) == NULL)
			goto next_hb;

		buffers++;

		/* Find the associated MPOOLFILE. */
		bh_mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);

		/* Select the lowest priority buffer in the chain. */
		for (oldest_bhp = bhp, tbhp = SH_CHAIN_PREV(bhp, vc, __bh);
		    tbhp != NULL;
		    oldest_bhp = tbhp, tbhp = SH_CHAIN_PREV(tbhp, vc, __bh))
			if (tbhp->ref <= bhp->ref &&
			    tbhp->priority <= bhp->priority)
				bhp = tbhp;

		/*
		 * Prefer the last buffer in the chain.
		 *
		 * If the oldest buffer isn't obsolete with respect to the
		 * cached old reader LSN, recalculate the oldest reader LSN
		 * and retry.  There is a tradeoff here, because if we had the
		 * LSN earlier, we might have found pages to evict, but to get
		 * it, we need to lock the transaction region.
		 */
		if (oldest_bhp != bhp && oldest_bhp->ref == 0) {
			if (F_ISSET(bhp, BH_FROZEN) &&
			    !F_ISSET(oldest_bhp, BH_FROZEN))
				bhp = oldest_bhp;
			else if (BH_OBSOLETE(oldest_bhp, hp->old_reader))
				bhp = oldest_bhp;
			else if (!got_oldest &&
			    __txn_oldest_reader(dbenv, &hp->old_reader) == 0) {
				got_oldest = 1;
				if (BH_OBSOLETE(oldest_bhp, hp->old_reader))
					bhp = oldest_bhp;
			}
		}

		if (bhp->ref != 0 || (bhp != oldest_bhp &&
		    !aggressive && bhp->priority > priority))
			goto next_hb;

		/* If the page is dirty, pin it and write it. */
		ret = 0;
		if (F_ISSET(bhp, BH_DIRTY)) {
			++bhp->ref;
			ret = __memp_bhwrite(dbmp, hp, bh_mfp, bhp, 0);
			--bhp->ref;
			if (ret == 0)
				++c_mp->stat.st_rw_evict;
		} else
			++c_mp->stat.st_ro_evict;

		/*
		 * Freeze this buffer, if necessary.  That is, if the buffer
		 * itself or the next version created could be read by the
		 * oldest reader in the system.
		 */
		if (ret == 0 && bh_mfp->multiversion) {
			if (!got_oldest && !SH_CHAIN_HASPREV(bhp, vc) &&
			    !BH_OBSOLETE(bhp, hp->old_reader)) {
				(void)__txn_oldest_reader(dbenv,
				    &hp->old_reader);
				got_oldest = 1;
			}
			if (SH_CHAIN_HASPREV(bhp, vc) ||
			    !BH_OBSOLETE(bhp, hp->old_reader)) {
				/*
				 * Before freezing, double-check that we have
				 * an up-to-date old_reader LSN.
				 */
				if (!aggressive ||
				    F_ISSET(bhp, BH_FROZEN) || bhp->ref != 0)
					goto next_hb;
				ret = __memp_bh_freeze(dbmp,
				    infop, hp, bhp, &alloc_freeze);
			}
		}

		/*
		 * If a write fails for any reason, we can't proceed.
		 *
		 * We released the hash bucket lock while doing I/O, so another
		 * thread may have acquired this buffer and incremented the ref
		 * count after we wrote it, in which case we can't have it.
		 *
		 * If there's a write error and we're having problems finding
		 * something to allocate, avoid selecting this buffer again
		 * by making it the bucket's least-desirable buffer.
		 */
		if (ret != 0 || bhp->ref != 0) {
			if (ret != 0 && aggressive)
				__memp_bad_buffer(dbenv, hp);
			goto next_hb;
		}

		/*
		 * If we need some empty buffer headers for freezing, turn the
		 * buffer we've found into frozen headers and put them on the
		 * free list.  Only reset alloc_freeze if we've actually
		 * allocated some frozen buffer headers.
		 *
		 * Check to see if the buffer is the size we're looking for.
		 * If so, we can simply reuse it.  Otherwise, free the buffer
		 * and its space and keep looking.
		 */
		if (F_ISSET(bhp, BH_FROZEN)) {
			++bhp->ref;
			if ((ret = __memp_bh_thaw(dbmp, infop, hp,
			    bhp, NULL)) != 0) {
				MUTEX_UNLOCK(dbenv, mutex);
				return (ret);
			}
			alloc_freeze = 0;
			goto this_hb;
		} else if (alloc_freeze) {
			if ((ret = __memp_bhfree(dbmp, hp, bhp, 0)) != 0)
				return (ret);
			MVCC_MPROTECT(bhp->buf, bh_mfp->stat.st_pagesize,
			    PROT_READ | PROT_WRITE | PROT_EXEC);

			MPOOL_REGION_LOCK(dbenv, infop);
			SH_TAILQ_INSERT_TAIL(&c_mp->alloc_frozen,
			    (BH_FROZEN_ALLOC *)bhp, links);
			frozen_bhp = (BH_FROZEN_PAGE *)
			    ((BH_FROZEN_ALLOC *)bhp + 1);
			endp = (u_int8_t *)bhp->buf + bh_mfp->stat.st_pagesize;
			while ((u_int8_t *)(frozen_bhp + 1) < endp) {
				SH_TAILQ_INSERT_TAIL(&c_mp->free_frozen,
				    (BH *)frozen_bhp, hq);
				frozen_bhp++;
			}
			alloc_freeze = 0;
			continue;
		} else if (mfp != NULL &&
		    mfp->stat.st_pagesize == bh_mfp->stat.st_pagesize) {
			if ((ret = __memp_bhfree(dbmp, hp, bhp, 0)) != 0)
				return (ret);
			p = bhp;
			goto found;
		} else {
			freed_space += __db_shalloc_sizeof(bhp);
			if ((ret = __memp_bhfree(dbmp,
			    hp, bhp, BH_FREE_FREEMEM)) != 0)
				return (ret);
		}

		if (aggressive > 1)
			aggressive = 1;

		/*
		 * Unlock this hash bucket and re-acquire the region lock. If
		 * we're reaching here as a result of calling memp_bhfree, the
		 * hash bucket lock has already been discarded.
		 */
		if (0) {
next_hb:		MUTEX_UNLOCK(dbenv, mutex);
		}
		MPOOL_REGION_LOCK(dbenv, infop);

		/*
		 * Retry the allocation as soon as we've freed up sufficient
		 * space.  We're likely to have to coalesce of memory to
		 * satisfy the request, don't try until it's likely (possible?)
		 * we'll succeed.
		 */
		if (freed_space >= 3 * len)
			goto alloc;
	}
	/* NOTREACHED */
}

/*
 * __memp_free --
 *	Free some space from a cache region.
 *
 * PUBLIC: void __memp_free __P((REGINFO *, MPOOLFILE *, void *));
 */
void
__memp_free(infop, mfp, buf)
	REGINFO *infop;
	MPOOLFILE *mfp;
	void *buf;
{
	MVCC_BHUNALIGN(mfp, buf);
	COMPQUIET(mfp, NULL);
	__db_shalloc_free(infop, buf);
}

/*
 * __memp_bad_buffer --
 *	Make the first buffer in a hash bucket the least desirable buffer.
 */
static void
__memp_bad_buffer(dbenv, hp)
	DB_ENV *dbenv;
	DB_MPOOL_HASH *hp;
{
	BH *bhp, *last_bhp;
	u_int32_t priority;

	/*
	 * Get the first buffer from the bucket.  If it is also the last buffer
	 * (in other words, it is the only buffer in the bucket), we're done.
	 */
	bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh);
	last_bhp = SH_TAILQ_LASTP(&hp->hash_bucket, hq, __bh);
	if (bhp == last_bhp)
		return;

	/* There are multiple buffers in the bucket, remove the first one. */
	SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);

	/*
	 * Find the highest priority buffer in the bucket.  Buffers are
	 * sorted by priority, so it's the last one in the bucket.
	 */
	priority = BH_PRIORITY(last_bhp);

	/*
	 * Append our buffer to the bucket and set its priority to be just as
	 * bad.
	 */
	SH_TAILQ_INSERT_TAIL(&hp->hash_bucket, bhp, hq);
	for (; bhp != NULL ; bhp = SH_CHAIN_PREV(bhp, vc, __bh))
		bhp->priority = priority;

	/* Reset the hash bucket's priority. */
	hp->hash_priority = BH_PRIORITY(
	    SH_TAILQ_FIRSTP(&hp->hash_bucket, __bh));

#ifdef DIAGNOSTIC
	__memp_check_order(dbenv, hp);
#else
	COMPQUIET(dbenv, NULL);
#endif
}

#ifdef DIAGNOSTIC
/*
 * __memp_check_order --
 *	Verify the priority ordering of a hash bucket chain.
 *
 * PUBLIC: #ifdef DIAGNOSTIC
 * PUBLIC: void __memp_check_order __P((DB_ENV *, DB_MPOOL_HASH *));
 * PUBLIC: #endif
 */
void
__memp_check_order(dbenv, hp)
	DB_ENV *dbenv;
	DB_MPOOL_HASH *hp;
{
	BH *bhp, *first_bhp, *tbhp;
	u_int32_t priority, last_priority;

	/*
	 * Assumes the hash bucket is locked.
	 */
	last_priority = hp->hash_priority;
	for (bhp = first_bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh);
	    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, hq, __bh)) {
		DB_ASSERT(dbenv, !SH_CHAIN_HASNEXT(bhp, vc));

		priority = BH_PRIORITY(bhp);
		DB_ASSERT(dbenv, (bhp == first_bhp) ?
		    priority == last_priority : priority >= last_priority);
		last_priority = priority;

		/* Chains have referential integrity. */
		for (tbhp = SH_CHAIN_PREV(bhp, vc, __bh); tbhp != NULL;
		    tbhp = SH_CHAIN_PREV(tbhp, vc, __bh))
			DB_ASSERT(dbenv, tbhp == SH_CHAIN_PREV(
			    SH_CHAIN_NEXT(tbhp, vc, __bh), vc, __bh));

		/*
		 * No repeats.
		 * XXX This is O(N**2) where N is the number of buffers in the
		 * bucket, but we generally assume small buckets.
		 */
		for (tbhp = SH_TAILQ_NEXT(bhp, hq, __bh); tbhp != NULL;
		    tbhp = SH_TAILQ_NEXT(tbhp, hq, __bh))
			DB_ASSERT(dbenv, bhp->pgno != tbhp->pgno ||
			    bhp->mf_offset != tbhp->mf_offset);
	}
}
#endif
