/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: mp.h,v 12.23 2006/09/07 15:11:26 mjc Exp $
 */

#ifndef	_DB_MP_H_
#define	_DB_MP_H_

#if defined(__cplusplus)
extern "C" {
#endif

struct __bh;		typedef struct __bh BH;
struct __bh_frozen;	typedef struct __bh_frozen_p BH_FROZEN_PAGE;
struct __bh_frozen_a;	typedef struct __bh_frozen_a BH_FROZEN_ALLOC;
struct __db_mpool_hash; typedef struct __db_mpool_hash DB_MPOOL_HASH;
struct __db_mpreg;	typedef struct __db_mpreg DB_MPREG;
struct __mpool;		typedef struct __mpool MPOOL;

				/* We require at least 20KB of cache. */
#define	DB_CACHESIZE_MIN	(20 * 1024)

/*
 * DB_MPOOLFILE initialization methods cannot be called after open is called,
 * other methods cannot be called before open is called
 */
#define	MPF_ILLEGAL_AFTER_OPEN(dbmfp, name)				\
	if (F_ISSET(dbmfp, MP_OPEN_CALLED))				\
		return (__db_mi_open((dbmfp)->dbenv, name, 1));
#define	MPF_ILLEGAL_BEFORE_OPEN(dbmfp, name)				\
	if (!F_ISSET(dbmfp, MP_OPEN_CALLED))				\
		return (__db_mi_open((dbmfp)->dbenv, name, 0));

typedef enum {
	DB_SYNC_ALLOC,		/* Flush for allocation. */
	DB_SYNC_CACHE,		/* Checkpoint or flush entire cache. */
	DB_SYNC_FILE,		/* Flush file. */
	DB_SYNC_TRICKLE		/* Trickle sync. */
} db_sync_op;

/*
 * DB_MPOOL --
 *	Per-process memory pool structure.
 */
struct __db_mpool {
	/* These fields need to be protected for multi-threaded support. */
	db_mutex_t mutex;		/* Thread mutex. */

	/*
	 * DB_MPREG structure for the DB pgin/pgout routines.
	 *
	 * Linked list of application-specified pgin/pgout routines.
	 */
	DB_MPREG *pg_inout;
	LIST_HEAD(__db_mpregh, __db_mpreg) dbregq;

					/* List of DB_MPOOLFILE's. */
	TAILQ_HEAD(__db_mpoolfileh, __db_mpoolfile) dbmfq;

	/*
	 * The dbenv, nreg and reginfo fields are not thread protected,
	 * as they are initialized during mpool creation, and not modified
	 * again.
	 */
	DB_ENV	   *dbenv;		/* Enclosing environment. */

	u_int32_t   nreg;		/* N underlying cache regions. */
	REGINFO	   *reginfo;		/* Underlying cache regions. */
};

/*
 * DB_MPREG --
 *	DB_MPOOL registry of pgin/pgout functions.
 */
struct __db_mpreg {
	LIST_ENTRY(__db_mpreg) q;	/* Linked list. */

	int32_t ftype;			/* File type. */
					/* Pgin, pgout routines. */
	int (*pgin) __P((DB_ENV *, db_pgno_t, void *, DBT *));
	int (*pgout) __P((DB_ENV *, db_pgno_t, void *, DBT *));
};

/*
 * NCACHE --
 *	Select a cache based on the file and the page number.  Assumes accesses
 *	are uniform across pages, which is probably OK.  What we really want to
 *	avoid is anything that puts all pages from any single file in the same
 *	cache, as we expect that file access will be bursty, and to avoid
 *	putting all page number N pages in the same cache as we expect access
 *	to the metapages (page 0) and the root of a btree (page 1) to be much
 *	more frequent than a random data page.
 */
#define	NCACHE(mp, mf_offset, pgno)					\
	(((MPOOL *)mp)->nreg == 1 ? 0 :					\
	(((pgno) ^ ((u_int32_t)(mf_offset) >> 3)) % ((MPOOL *)mp)->nreg))

/*
 * NBUCKET --
 *	 We make the assumption that early pages of the file are more likely
 *	 to be retrieved than the later pages, which means the top bits will
 *	 be more interesting for hashing as they're less likely to collide.
 *	 That said, as 512 8K pages represents a 4MB file, so only reasonably
 *	 large files will have page numbers with any other than the bottom 9
 *	 bits set.  We XOR in the MPOOL offset of the MPOOLFILE that backs the
 *	 page, since that should also be unique for the page.  We don't want
 *	 to do anything very fancy -- speed is more important to us than using
 *	 good hashing.
 */
#define	NBUCKET(mc, mf_offset, pgno)					\
	(((pgno) ^ ((mf_offset) << 9)) % (mc)->htab_buckets)

/*
 * File hashing --
 *	We hash each file to hash bucket based on its fileid
 *	or, in the case of in memory files, its name.
 */

/* Number of file hash buckets, a small prime number */
#define	MPOOL_FILE_BUCKETS	17

#define	FHASH(id, len)	__ham_func5(NULL, id, (u_int32_t)(len))

#define	FNBUCKET(id, len)						\
	(FHASH(id, len) % MPOOL_FILE_BUCKETS)

/* Macros to lock/unlock the mpool region as a whole. */
#define	MPOOL_SYSTEM_LOCK(dbenv)					\
	MUTEX_LOCK(dbenv, ((MPOOL *)					\
	    (dbenv)->mp_handle->reginfo[0].primary)->mtx_region)
#define	MPOOL_SYSTEM_UNLOCK(dbenv)					\
	MUTEX_UNLOCK(dbenv, ((MPOOL *)					\
	    (dbenv)->mp_handle->reginfo[0].primary)->mtx_region)

/* Macros to lock/unlock a specific mpool region. */
#define	MPOOL_REGION_LOCK(dbenv, infop)					\
	MUTEX_LOCK(dbenv, ((MPOOL *)(infop)->primary)->mtx_region)
#define	MPOOL_REGION_UNLOCK(dbenv, infop)				\
	MUTEX_UNLOCK(dbenv, ((MPOOL *)(infop)->primary)->mtx_region)

/*
 * MPOOL --
 *	Shared memory pool region.
 */
struct __mpool {
	/*
	 * The memory pool can be broken up into individual pieces/files.
	 * Not what we would have liked, but on Solaris you can allocate
	 * only a little more than 2GB of memory in a contiguous chunk,
	 * and I expect to see more systems with similar issues.
	 *
	 * While this structure is duplicated in each piece of the cache,
	 * the first of these pieces/files describes the entire pool, the
	 * second only describe a piece of the cache.
	 */
	db_mutex_t	mtx_region;	/* Region mutex. */

	/*
	 * The lsn field and list of underlying MPOOLFILEs are thread protected
	 * by the region lock.
	 */
	DB_LSN	  lsn;			/* Maximum checkpoint LSN. */

	/* Configuration information: protected by the region lock. */
	size_t mp_mmapsize;		/* Maximum file size for mmap. */
	int    mp_maxopenfd;		/* Maximum open file descriptors. */
	int    mp_maxwrite;		/* Maximum buffers to write. */
	int    mp_maxwrite_sleep;	/* Sleep after writing max buffers. */

	/*
	 * The nreg, regids and maint_off fields are not thread protected,
	 * as they are initialized during mpool creation, and not modified
	 * again.
	 */
	u_int32_t nreg;			/* Number of underlying REGIONS. */
	roff_t	  regids;		/* Array of underlying REGION Ids. */

	/*
	 * The following structure fields only describe the per-cache portion
	 * of the region.
	 *
	 * The htab and htab_buckets fields are not thread protected as they
	 * are initialized during mpool creation, and not modified again.
	 *
	 * The last_checked and lru_count fields are thread protected by
	 * the region lock.
	 */
	u_int32_t htab_buckets;	/* Number of hash table entries. */
	roff_t	  htab;		/* Hash table offset. */
	u_int32_t last_checked;	/* Last bucket checked for free. */
	u_int32_t lru_count;	/* Counter for buffer LRU */

	roff_t	  ftab;		/* Hash table of files. */

	/*
	 * The stat fields are generally not thread protected, and cannot be
	 * trusted.  Note that st_pages is an exception, and is always updated
	 * inside a region lock (although it is sometimes read outside of the
	 * region lock).
	 */
	DB_MPOOL_STAT stat;		/* Per-cache mpool statistics. */

	/*
	 * We track page puts so that we can decide when allocation is never
	 * going to succeed.  We don't lock the field, all we care about is
	 * if it changes.
	 */
	u_int32_t  put_counter;		/* Count of page put calls. */

	/* Free frozen buffer headers, protected by the region lock. */
	SH_TAILQ_HEAD(__free_frozen) free_frozen;

	/* Allocated blocks of frozen buffer headers. */
	SH_TAILQ_HEAD(__alloc_frozen) alloc_frozen;
};

struct __db_mpool_hash {
	db_mutex_t	mtx_hash;	/* Per-bucket mutex. */
	db_mutex_t	mtx_io;		/* Buffer I/O mutex. */

	DB_HASHTAB	hash_bucket;	/* Head of bucket. */

	u_int32_t	hash_page_dirty;/* Count of dirty pages. */
	u_int32_t	hash_priority;	/* Minimum priority of bucket buffer. */

	u_int32_t	hash_io_wait;	/* Count of I/O waits. */
	u_int32_t	hash_frozen;	/* Count of frozen buffers. */
	u_int32_t	hash_thawed;	/* Count of thawed buffers. */
	u_int32_t	hash_frozen_freed;/* Count of freed frozen buffers. */

	DB_LSN		old_reader;	/* Oldest snapshot reader (cached). */

#define	IO_WAITER	0x001		/* Thread is waiting on page. */
	u_int32_t	flags;
};

/*
 * The base mpool priority is 1/4th of the name space, or just under 2^30.
 * When the LRU counter wraps, we shift everybody down to a base-relative
 * value.
 */
#define	MPOOL_BASE_DECREMENT	(UINT32_MAX - (UINT32_MAX / 4))

/*
 * Mpool priorities from low to high.  Defined in terms of fractions of the
 * buffers in the pool.
 */
#define	MPOOL_PRI_VERY_LOW	-1	/* Dead duck.  Check and set to 0. */
#define	MPOOL_PRI_LOW		-2	/* Low. */
#define	MPOOL_PRI_DEFAULT	0	/* No adjustment -- special case.*/
#define	MPOOL_PRI_HIGH		10	/* With the dirty buffers. */
#define	MPOOL_PRI_DIRTY		10	/* Dirty gets a 10% boost. */
#define	MPOOL_PRI_VERY_HIGH	1	/* Add number of buffers in pool. */

/*
 * MPOOLFILE --
 *	Shared DB_MPOOLFILE information.
 */
struct __mpoolfile {
	db_mutex_t mutex;		/* MPOOLFILE mutex. */

	/* Protected by MPOOLFILE mutex. */
	u_int32_t mpf_cnt;		/* Ref count: DB_MPOOLFILEs. */
	u_int32_t block_cnt;		/* Ref count: blocks in cache. */
	db_pgno_t last_pgno;		/* Last page in the file. */
	db_pgno_t last_flushed_pgno;	/* Last page flushed to disk. */
	db_pgno_t orig_last_pgno;	/* Original last page in the file. */
	db_pgno_t maxpgno;		/* Maximum page number. */

	roff_t	  path_off;		/* File name location. */

	/* Protected by hash bucket mutex. */
	SH_TAILQ_ENTRY q;		/* List of MPOOLFILEs */

	/*
	 * The following are used for file compaction processing.
	 * They are only used when a thread is in the process
	 * of trying to move free pages to the end of the file.
	 * Other threads may look here when freeing a page.
	 * Protected by a lock on the metapage.
	 */
	u_int32_t free_ref;		/* Refcount to freelist. */
	u_int32_t free_cnt;		/* Count of free pages. */
	size_t	  free_size;		/* Allocated size of free list. */
	roff_t	  free_list;		/* Offset to free list. */

	/*
	 * We normally don't lock the deadfile field when we read it since we
	 * only care if the field is zero or non-zero.  We do lock on read when
	 * searching for a matching MPOOLFILE -- see that code for more detail.
	 */
	int32_t	  deadfile;		/* Dirty pages can be discarded. */

	u_int32_t bucket;		/* hash bucket for this file. */

	/*
	 * None of the following fields are thread protected.
	 *
	 * There are potential races with the ftype field because it's read
	 * without holding a lock.  However, it has to be set before adding
	 * any buffers to the cache that depend on it being set, so there
	 * would need to be incorrect operation ordering to have a problem.
	 */
	int32_t	  ftype;		/* File type. */

	/*
	 * There are potential races with the priority field because it's read
	 * without holding a lock.  However, a collision is unlikely and if it
	 * happens is of little consequence.
	 */
	int32_t   priority;		/* Priority when unpinning buffer. */

	/*
	 * There are potential races with the file_written field (many threads
	 * may be writing blocks at the same time), and with no_backing_file
	 * and unlink_on_close fields, as they may be set while other threads
	 * are reading them.  However, we only care if the field value is zero
	 * or non-zero, so don't lock the memory.
	 *
	 * !!!
	 * Theoretically, a 64-bit architecture could put two of these fields
	 * in a single memory operation and we could race.  I have never seen
	 * an architecture where that's a problem, and I believe Java requires
	 * that to never be the case.
	 *
	 * File_written is set whenever a buffer is marked dirty in the cache.
	 * It can be cleared in some cases, after all dirty buffers have been
	 * written AND the file has been flushed to disk.
	 */
	int32_t	  file_written;		/* File was written. */
	int32_t	  no_backing_file;	/* Never open a backing file. */
	int32_t	  unlink_on_close;	/* Unlink file on last close. */
	int32_t	  multiversion;		/* Number of DB_MULTIVERSION handles. */

	/*
	 * We do not protect the statistics in "stat" because of the cost of
	 * the mutex in the get/put routines.  There is a chance that a count
	 * will get lost.
	 */
	DB_MPOOL_FSTAT stat;		/* Per-file mpool statistics. */

	/*
	 * The remaining fields are initialized at open and never subsequently
	 * modified.
	 */
	int32_t	  lsn_off;		/* Page's LSN offset. */
	u_int32_t clear_len;		/* Bytes to clear on page create. */

	roff_t	  fileid_off;		/* File ID string location. */

	roff_t	  pgcookie_len;		/* Pgin/pgout cookie length. */
	roff_t	  pgcookie_off;		/* Pgin/pgout cookie location. */

	/*
	 * The flags are initialized at open and never subsequently modified.
	 */
#define	MP_CAN_MMAP		0x001	/* If the file can be mmap'd. */
#define	MP_DIRECT		0x002	/* No OS buffering. */
#define	MP_DURABLE_UNKNOWN	0x004	/* We don't care about durability. */
#define	MP_EXTENT		0x008	/* Extent file. */
#define	MP_FAKE_DEADFILE	0x010	/* Deadfile field: fake flag. */
#define	MP_FAKE_FILEWRITTEN	0x020	/* File_written field: fake flag. */
#define	MP_FAKE_NB		0x040	/* No_backing_file field: fake flag. */
#define	MP_FAKE_UOC		0x080	/* Unlink_on_close field: fake flag. */
#define	MP_NOT_DURABLE		0x100	/* File is not durable. */
#define	MP_TEMP			0x200	/* Backing file is a temporary. */
	u_int32_t  flags;
};

/*
 * Flags to __memp_bh_free.
 */
#define	BH_FREE_FREEMEM		0x01
#define	BH_FREE_REUSE		0x02
#define	BH_FREE_UNLOCKED	0x04

/*
 * BH --
 *	Buffer header.
 */
struct __bh {
	u_int16_t	ref;		/* Reference count. */
	u_int16_t	ref_sync;	/* Sync wait-for reference count. */

#define	BH_CALLPGIN	0x001		/* Convert the page before use. */
#define	BH_DIRTY	0x002		/* Page is modified. */
#define	BH_DIRTY_CREATE	0x004		/* Page is modified. */
#define	BH_DISCARD	0x008		/* Page is useless. */
#define	BH_FREED	0x010		/* Page was freed. */
#define	BH_FROZEN	0x020		/* Frozen buffer: allocate & re-read. */
#define	BH_LOCKED	0x040		/* Page is locked (I/O in progress). */
#define	BH_TRASH	0x080		/* Page is garbage. */
	u_int16_t	flags;

	u_int32_t	priority;	/* LRU priority. */
	SH_TAILQ_ENTRY	hq;		/* MPOOL hash bucket queue. */

	db_pgno_t	pgno;		/* Underlying MPOOLFILE page number. */
	roff_t		mf_offset;	/* Associated MPOOLFILE offset. */

	roff_t		td_off;		/* MVCC: creating TXN_DETAIL offset. */
	SH_CHAIN_ENTRY	vc;		/* MVCC: version chain. */
#ifdef DIAG_MVCC
	u_int16_t	align_off;	/* Alignment offset for diagnostics.*/
#endif

	/*
	 * !!!
	 * This array must be at least size_t aligned -- the DB access methods
	 * put PAGE and other structures into it, and then access them directly.
	 * (We guarantee size_t alignment to applications in the documentation,
	 * too.)
	 */
	u_int8_t   buf[1];		/* Variable length data. */
};

/*
 * BH_FROZEN_PAGE --
 *	Data used to find a frozen buffer header.
 */
struct __bh_frozen_p {
	BH header;
	db_pgno_t	spgno;		/* Page number in freezer file. */
};

/*
 * BH_FROZEN_ALLOC --
 *	Frozen buffer headers are allocated a page at a time in general.  This
 *	structure is allocated at the beginning of the page so that the
 *	allocation chunks can be tracked and freed (for private environments).
 */
struct __bh_frozen_a {
	SH_TAILQ_ENTRY links;
};

#define	MULTIVERSION(dbp)	((dbp)->mpf->mfp->multiversion)
#define	IS_DIRTY(p)							\
    F_ISSET((BH *)((u_int8_t *)(p) - SSZA(BH, buf)), BH_DIRTY)

#define	BH_OWNER(dbenv, bhp)						\
    ((TXN_DETAIL *)R_ADDR(&dbenv->tx_handle->reginfo, bhp->td_off))

#define	BH_OWNED_BY(dbenv, bhp, txn)	((txn) != NULL &&		\
    (bhp)->td_off != INVALID_ROFF &&					\
    (txn)->td == BH_OWNER(dbenv, bhp))

#define	BH_PRIORITY(bhp)						\
    (SH_CHAIN_SINGLETON(bhp, vc) ? (bhp)->priority :			\
     __memp_bh_priority(bhp))

#define	VISIBLE_LSN(dbenv, bhp)						\
    (&BH_OWNER(dbenv, bhp)->visible_lsn)

#define	BH_OBSOLETE(bhp, old_lsn)	((SH_CHAIN_HASNEXT(bhp, vc) ?	\
	LOG_COMPARE(&(old_lsn), VISIBLE_LSN(dbenv,			\
	SH_CHAIN_NEXTP(bhp, vc, __bh))) :				\
	(bhp->td_off == INVALID_ROFF ? 1 :				\
	LOG_COMPARE(&(old_lsn), VISIBLE_LSN(dbenv, bhp)))) > 0)

#define	MVCC_SKIP_CURADJ(dbc, pgno)					\
    (dbc->txn != NULL && F_ISSET(dbc->txn, TXN_SNAPSHOT) &&		\
    dbc->txn->td != NULL && __memp_skip_curadj(dbc, pgno))

#if defined(DIAG_MVCC) && defined(HAVE_MPROTECT)
#define	VM_PAGESIZE 4096
#define	MVCC_BHSIZE(mfp, sz) do {					\
	sz += VM_PAGESIZE + sizeof(BH);					\
	if (mfp->stat.st_pagesize < VM_PAGESIZE)			\
		sz += VM_PAGESIZE - mfp->stat.st_pagesize;		\
} while (0)

#define	MVCC_BHALIGN(mfp, p) do {					\
	if (mfp != NULL) {						\
		BH *__bhp;						\
		void *__orig = (p);					\
		p = ALIGNP_INC(p, VM_PAGESIZE);				\
		if ((u_int8_t *)p < (u_int8_t *)__orig + sizeof(BH))	\
			p = (u_int8_t *)p + VM_PAGESIZE;		\
		__bhp = (BH *)((u_int8_t *)p - SSZA(BH, buf));		\
		DB_ASSERT(dbenv,					\
		    ((uintptr_t)__bhp->buf & (VM_PAGESIZE - 1)) == 0);	\
		DB_ASSERT(dbenv,					\
		    (u_int8_t *)__bhp >= (u_int8_t *)__orig);		\
		DB_ASSERT(dbenv, (u_int8_t *)p + mfp->stat.st_pagesize <\
		    (u_int8_t *)__orig + len);				\
		__bhp->align_off =					\
		    (u_int16_t)((u_int8_t *)__bhp - (u_int8_t *)__orig);\
		p = __bhp;						\
	}								\
} while (0)

#define	MVCC_BHUNALIGN(mfp, p) do {					\
	if ((mfp) != NULL) {						\
		BH *bhp = (BH *)(p);					\
		(p) = ((u_int8_t *)bhp - bhp->align_off);		\
	}								\
} while (0)

#ifdef linux
#define	MVCC_MPROTECT(buf, sz, mode) do {				\
	int __ret = mprotect((buf), (sz), (mode));			\
	DB_ASSERT(dbenv, __ret == 0);					\
} while (0)
#else
#define	MVCC_MPROTECT(buf, sz, mode) do {				\
	if (!F_ISSET(dbenv, DB_ENV_PRIVATE | DB_ENV_SYSTEM_MEM)) {	\
		int __ret = mprotect((buf), (sz), (mode));		\
		DB_ASSERT(dbenv, __ret == 0);				\
	}								\
} while (0)
#endif /* linux */

#else /* defined(DIAG_MVCC) && defined(HAVE_MPROTECT) */
#define	MVCC_BHSIZE(mfp, sz) do {} while (0)
#define	MVCC_BHALIGN(mfp, p) do {} while (0)
#define	MVCC_BHUNALIGN(mfp, p) do {} while (0)
#define	MVCC_MPROTECT(buf, size, mode) do {} while (0)
#endif

/*
 * Flags to __memp_ftruncate.
 */
#define	MP_TRUNC_RECOVER	0x01

#if defined(__cplusplus)
}
#endif

#include "dbinc_auto/mp_ext.h"
#endif /* !_DB_MP_H_ */
