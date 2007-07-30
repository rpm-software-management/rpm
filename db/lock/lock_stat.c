/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: lock_stat.c,v 12.29 2007/06/22 17:38:24 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/db_am.h"

#ifdef HAVE_STATISTICS
static void __lock_dump_locker
		__P((DB_ENV *, DB_MSGBUF *, DB_LOCKTAB *, DB_LOCKER *));
static void __lock_dump_object __P((DB_LOCKTAB *, DB_MSGBUF *, DB_LOCKOBJ *));
static int  __lock_print_all __P((DB_ENV *, u_int32_t));
static int  __lock_print_stats __P((DB_ENV *, u_int32_t));
static void __lock_print_header __P((DB_ENV *));
static int  __lock_stat __P((DB_ENV *, DB_LOCK_STAT **, u_int32_t));

/*
 * __lock_stat_pp --
 *	DB_ENV->lock_stat pre/post processing.
 *
 * PUBLIC: int __lock_stat_pp __P((DB_ENV *, DB_LOCK_STAT **, u_int32_t));
 */
int
__lock_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOCK_STAT **statp;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_ENV->lock_stat", DB_INIT_LOCK);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->lock_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__lock_stat(dbenv, statp, flags)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __lock_stat --
 *	DB_ENV->lock_stat.
 */
static int
__lock_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOCK_STAT **statp;
	u_int32_t flags;
{
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	DB_LOCK_STAT *stats, tmp;
	int ret;
	u_int32_t i;

	*statp = NULL;
	lt = dbenv->lk_handle;

	if ((ret = __os_umalloc(dbenv, sizeof(*stats), &stats)) != 0)
		return (ret);

	/* Copy out the global statistics. */
	LOCK_REGION_LOCK(dbenv);

	region = lt->reginfo.primary;
	memcpy(stats, &region->stat, sizeof(*stats));
	stats->st_locktimeout = region->lk_timeout;
	stats->st_txntimeout = region->tx_timeout;

	for (i = 0; i < region->object_t_size; i++) {
		stats->st_nrequests += lt->obj_stat[i].st_nrequests;
		stats->st_nreleases += lt->obj_stat[i].st_nreleases;
		stats->st_nupgrade += lt->obj_stat[i].st_nupgrade;
		stats->st_ndowngrade += lt->obj_stat[i].st_ndowngrade;
		stats->st_lock_wait += lt->obj_stat[i].st_lock_wait;
		stats->st_lock_nowait += lt->obj_stat[i].st_lock_nowait;
		stats->st_nlocktimeouts += lt->obj_stat[i].st_nlocktimeouts;
		stats->st_ntxntimeouts += lt->obj_stat[i].st_ntxntimeouts;
		if (stats->st_hash_len < lt->obj_stat[i].st_hash_len)
			stats->st_hash_len = lt->obj_stat[i].st_hash_len;
		if (LF_ISSET(DB_STAT_CLEAR)) {
			memset(&lt->obj_stat[i], 0, sizeof(lt->obj_stat[i]));
#ifdef HAVE_FINE_GRAINED_LOCK_MANAGER
			if (!LF_ISSET(DB_STAT_SUBSYSTEM))
				__mutex_clear(dbenv, lt->obj_mtx[i]);
#endif
		}
	}

	__mutex_set_wait_info(dbenv, region->mtx_region,
	    &stats->st_region_wait, &stats->st_region_nowait);
#ifdef HAVE_FINE_GRAINED_LOCK_MANAGER
	__mutex_set_wait_info(dbenv, region->mtx_objs,
	    &stats->st_objs_wait, &stats->st_objs_nowait);
	__mutex_set_wait_info(dbenv, region->mtx_lockers,
	    &stats->st_lockers_wait, &stats->st_lockers_nowait);
	__mutex_set_wait_info(dbenv, region->mtx_locks,
	    &stats->st_locks_wait, &stats->st_locks_nowait);
#endif
	stats->st_regsize = lt->reginfo.rp->size;
	if (LF_ISSET(DB_STAT_CLEAR)) {
		tmp = region->stat;
		memset(&region->stat, 0, sizeof(region->stat));
		if (!LF_ISSET(DB_STAT_SUBSYSTEM)) {
			__mutex_clear(dbenv, region->mtx_region);
#ifdef HAVE_FINE_GRAINED_LOCK_MANAGER
			__mutex_clear(dbenv, region->mtx_objs);
			__mutex_clear(dbenv, region->mtx_locks);
			__mutex_clear(dbenv, region->mtx_lockers);
#endif
			for (i = 0; i < region->object_t_size; i++)
				memset(&lt->obj_stat[i],
				    0, sizeof(lt->obj_stat[i]));
		}

		region->stat.st_id = tmp.st_id;
		region->stat.st_cur_maxid = tmp.st_cur_maxid;
		region->stat.st_maxlocks = tmp.st_maxlocks;
		region->stat.st_maxlockers = tmp.st_maxlockers;
		region->stat.st_maxobjects = tmp.st_maxobjects;
		region->stat.st_nlocks =
		    region->stat.st_maxnlocks = tmp.st_nlocks;
		region->stat.st_nlockers =
		    region->stat.st_maxnlockers = tmp.st_nlockers;
		region->stat.st_nobjects =
		    region->stat.st_maxnobjects = tmp.st_nobjects;
		region->stat.st_nmodes = tmp.st_nmodes;
	}

	LOCK_REGION_UNLOCK(dbenv);

	*statp = stats;
	return (0);
}

/*
 * __lock_stat_print_pp --
 *	DB_ENV->lock_stat_print pre/post processing.
 *
 * PUBLIC: int __lock_stat_print_pp __P((DB_ENV *, u_int32_t));
 */
int
__lock_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_ENV->lock_stat_print", DB_INIT_LOCK);

#define	DB_STAT_LOCK_FLAGS						\
	(DB_STAT_ALL | DB_STAT_CLEAR | DB_STAT_LOCK_CONF |		\
	 DB_STAT_LOCK_LOCKERS |	DB_STAT_LOCK_OBJECTS | DB_STAT_LOCK_PARAMS)
	if ((ret = __db_fchk(dbenv, "DB_ENV->lock_stat_print",
	    flags, DB_STAT_CLEAR | DB_STAT_LOCK_FLAGS)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__lock_stat_print(dbenv, flags)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __lock_stat_print --
 *	DB_ENV->lock_stat_print method.
 *
 * PUBLIC: int  __lock_stat_print __P((DB_ENV *, u_int32_t));
 */
int
__lock_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	u_int32_t orig_flags;
	int ret;

	orig_flags = flags;
	LF_CLR(DB_STAT_CLEAR | DB_STAT_SUBSYSTEM);
	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		ret = __lock_print_stats(dbenv, orig_flags);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL | DB_STAT_LOCK_CONF | DB_STAT_LOCK_LOCKERS |
	    DB_STAT_LOCK_OBJECTS | DB_STAT_LOCK_PARAMS) &&
	    (ret = __lock_print_all(dbenv, orig_flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __lock_print_stats --
 *	Display default lock region statistics.
 */
static int
__lock_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_LOCK_STAT *sp;
	int ret;

#ifdef LOCK_DIAGNOSTIC
	DB_LOCKTAB *lt;
	DB_LOCKREGION *region;
	u_int32_t i;
#ifdef HAVE_FINE_GRAINED_LOCK_MANAGER
	u_int32_t wait, nowait;
#endif

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	for (i = 0; i < region->object_t_size; i++) {
		if (lt->obj_stat[i].st_hash_len == 0)
			continue;
		__db_dl(dbenv,
		    "Hash bucket", (u_long)i);
#ifdef HAVE_FINE_GRAINED_LOCK_MANAGER
		__mutex_set_wait_info(dbenv, lt->obj_mtx[i], &wait, &nowait);
		__db_dl_pct(dbenv,
		    "The number of hash mutex requests that required waiting",
		    (u_long)wait, DB_PCT(wait, wait + nowait), NULL);
#endif
		__db_dl(dbenv,
		    "Maximum hash bucket length",
		    (u_long)lt->obj_stat[i].st_hash_len);
		__db_dl(dbenv,
		    "Total number of locks requested",
		    (u_long)lt->obj_stat[i].st_nrequests);
		__db_dl(dbenv,
		    "Total number of locks released",
		    (u_long)lt->obj_stat[i].st_nreleases);
		__db_dl(dbenv,
		    "Total number of locks upgraded",
		    (u_long)lt->obj_stat[i].st_nupgrade);
		__db_dl(dbenv,
		    "Total number of locks downgraded",
		    (u_long)lt->obj_stat[i].st_ndowngrade);
		__db_dl(dbenv,
	  "Lock requests not available due to conflicts, for which we waited",
		    (u_long)lt->obj_stat[i].st_lock_wait);
		__db_dl(dbenv,
  "Lock requests not available due to conflicts, for which we did not wait",
		    (u_long)lt->obj_stat[i].st_lock_nowait);
		__db_dl(dbenv, "Number of locks that have timed out",
		    (u_long)lt->obj_stat[i].st_nlocktimeouts);
		__db_dl(dbenv, "Number of transactions that have timed out",
		    (u_long)lt->obj_stat[i].st_ntxntimeouts);
	}
#endif
	if ((ret = __lock_stat(dbenv, &sp, flags)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL))
		__db_msg(dbenv, "Default locking region information:");
	__db_dl(dbenv, "Last allocated locker ID", (u_long)sp->st_id);
	__db_msg(dbenv, "%#lx\tCurrent maximum unused locker ID",
	    (u_long)sp->st_cur_maxid);
	__db_dl(dbenv, "Number of lock modes", (u_long)sp->st_nmodes);
	__db_dl(dbenv,
	    "Maximum number of locks possible", (u_long)sp->st_maxlocks);
	__db_dl(dbenv,
	    "Maximum number of lockers possible", (u_long)sp->st_maxlockers);
	__db_dl(dbenv, "Maximum number of lock objects possible",
	    (u_long)sp->st_maxobjects);
	__db_dl(dbenv, "Number of current locks", (u_long)sp->st_nlocks);
	__db_dl(dbenv, "Maximum number of locks at any one time",
	    (u_long)sp->st_maxnlocks);
	__db_dl(dbenv, "Number of current lockers", (u_long)sp->st_nlockers);
	__db_dl(dbenv, "Maximum number of lockers at any one time",
	    (u_long)sp->st_maxnlockers);
	__db_dl(dbenv,
	    "Number of current lock objects", (u_long)sp->st_nobjects);
	__db_dl(dbenv, "Maximum number of lock objects at any one time",
	    (u_long)sp->st_maxnobjects);
	__db_dl(dbenv,
	    "Total number of locks requested", (u_long)sp->st_nrequests);
	__db_dl(dbenv,
	    "Total number of locks released", (u_long)sp->st_nreleases);
	__db_dl(dbenv,
	    "Total number of locks upgraded", (u_long)sp->st_nupgrade);
	__db_dl(dbenv,
	    "Total number of locks downgraded", (u_long)sp->st_ndowngrade);
	__db_dl(dbenv,
	  "Lock requests not available due to conflicts, for which we waited",
	    (u_long)sp->st_lock_wait);
	__db_dl(dbenv,
  "Lock requests not available due to conflicts, for which we did not wait",
	    (u_long)sp->st_lock_nowait);
	__db_dl(dbenv, "Number of deadlocks", (u_long)sp->st_ndeadlocks);
	__db_dl(dbenv, "Lock timeout value", (u_long)sp->st_locktimeout);
	__db_dl(dbenv, "Number of locks that have timed out",
	    (u_long)sp->st_nlocktimeouts);
	__db_dl(dbenv,
	    "Transaction timeout value", (u_long)sp->st_txntimeout);
	__db_dl(dbenv, "Number of transactions that have timed out",
	    (u_long)sp->st_ntxntimeouts);

	__db_dlbytes(dbenv, "The size of the lock region",
	    (u_long)0, (u_long)0, (u_long)sp->st_regsize);
#ifdef HAVE_FINE_GRAINED_LOCK_MANAGER
	__db_dl_pct(dbenv,
	    "The number of object allocations that required waiting",
	    (u_long)sp->st_objs_wait, DB_PCT(sp->st_objs_wait,
	    sp->st_objs_wait + sp->st_objs_nowait), NULL);
	__db_dl_pct(dbenv,
	    "The number of locker allocations that required waiting",
	    (u_long)sp->st_lockers_wait, DB_PCT(sp->st_lockers_wait,
	    sp->st_lockers_wait + sp->st_lockers_nowait), NULL);
	__db_dl_pct(dbenv,
	    "The number of lock allocations that required waiting",
	    (u_long)sp->st_locks_wait, DB_PCT(sp->st_locks_wait,
	    sp->st_locks_wait + sp->st_locks_nowait), NULL);
#endif
	__db_dl_pct(dbenv,
	    "The number of region locks that required waiting",
	    (u_long)sp->st_region_wait, DB_PCT(sp->st_region_wait,
	    sp->st_region_wait + sp->st_region_nowait), NULL);

	__os_ufree(dbenv, sp);

	return (0);
}

/*
 * __lock_print_all --
 *	Display debugging lock region statistics.
 */
static int
__lock_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_LOCKER *lip;
	DB_LOCKOBJ *op;
	DB_LOCKREGION *lrp;
	DB_LOCKTAB *lt;
	DB_MSGBUF mb;
	int i, j;
	u_int32_t k;

	lt = dbenv->lk_handle;
	lrp = lt->reginfo.primary;
	DB_MSGBUF_INIT(&mb);

	LOCK_REGION_LOCK(dbenv);
	__db_print_reginfo(dbenv, &lt->reginfo, "Lock", flags);

	if (LF_ISSET(DB_STAT_ALL | DB_STAT_LOCK_PARAMS)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Lock region parameters:");
		__mutex_print_debug_single(dbenv,
		    "Lock region region mutex", lrp->mtx_region, flags);
		STAT_ULONG("locker table size", lrp->locker_t_size);
		STAT_ULONG("object table size", lrp->object_t_size);
		STAT_ULONG("obj_off", lrp->obj_off);
		STAT_ULONG("locker_off", lrp->locker_off);
		STAT_ULONG("need_dd", lrp->need_dd);
		if (timespecisset(&lrp->next_timeout)) {
#ifdef HAVE_STRFTIME
			time_t t = (time_t)lrp->next_timeout.tv_sec;
			char tbuf[64];
			if (strftime(tbuf, sizeof(tbuf),
			    "%m-%d-%H:%M:%S", localtime(&t)) != 0)
				__db_msg(dbenv, "next_timeout: %s.%09lu",
				     tbuf, (u_long)lrp->next_timeout.tv_nsec);
			else
#endif
				__db_msg(dbenv, "next_timeout: %lu.%09lu",
				     (u_long)lrp->next_timeout.tv_sec,
				     (u_long)lrp->next_timeout.tv_nsec);
		}
	}

	if (LF_ISSET(DB_STAT_ALL | DB_STAT_LOCK_CONF)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Lock conflict matrix:");
		for (i = 0; i < lrp->stat.st_nmodes; i++) {
			for (j = 0; j < lrp->stat.st_nmodes; j++)
				__db_msgadd(dbenv, &mb, "%lu\t", (u_long)
				    lt->conflicts[i * lrp->stat.st_nmodes + j]);
			DB_MSGBUF_FLUSH(dbenv, &mb);
		}
	}
	LOCK_REGION_UNLOCK(dbenv);

	if (LF_ISSET(DB_STAT_ALL | DB_STAT_LOCK_LOCKERS)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Locks grouped by lockers:");
		__lock_print_header(dbenv);
		for (k = 0; k < lrp->locker_t_size; k++)
			SH_TAILQ_FOREACH(
			    lip, &lt->locker_tab[k], links, __db_locker)
				__lock_dump_locker(dbenv, &mb, lt, lip);
	}

	if (LF_ISSET(DB_STAT_ALL | DB_STAT_LOCK_OBJECTS)) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Locks grouped by object:");
		__lock_print_header(dbenv);
		for (k = 0; k < lrp->object_t_size; k++)
			SH_TAILQ_FOREACH(
			    op, &lt->obj_tab[k], links, __db_lockobj) {
				__lock_dump_object(lt, &mb, op);
				__db_msg(dbenv, "%s", "");
			}
	}

	return (0);
}

static void
__lock_dump_locker(dbenv, mbp, lt, lip)
	DB_ENV *dbenv;
	DB_MSGBUF *mbp;
	DB_LOCKTAB *lt;
	DB_LOCKER *lip;
{
	struct __db_lock *lp;
	char buf[DB_THREADID_STRLEN];

	__db_msgadd(dbenv,
	    mbp, "%8lx dd=%2ld locks held %-4d write locks %-4d pid/thread %s",
	    (u_long)lip->id, (long)lip->dd_id, lip->nlocks, lip->nwrites,
	    dbenv->thread_id_string(dbenv, lip->pid, lip->tid, buf));
	__db_msgadd(
	    dbenv, mbp, "%s", F_ISSET(lip, DB_LOCKER_DELETED) ? "(D)" : "   ");
	if (timespecisset(&lip->tx_expire)) {
#ifdef HAVE_STRFTIME
		time_t t = (time_t)lip->tx_expire.tv_sec;
		char tbuf[64];
		if (strftime(tbuf, sizeof(tbuf),
		    "%m-%d-%H:%M:%S", localtime(&t)) != 0)
			__db_msgadd(dbenv, mbp, "expires %s.%09lu",
			    tbuf, (u_long)lip->tx_expire.tv_nsec);
		else
#endif
			__db_msgadd(dbenv, mbp, "expires %lu.%09lu",
			    (u_long)lip->tx_expire.tv_sec,
			    (u_long)lip->tx_expire.tv_nsec);
	}
	if (F_ISSET(lip, DB_LOCKER_TIMEOUT))
		__db_msgadd(dbenv, mbp, " lk timeout %u", lip->lk_timeout);
	if (timespecisset(&lip->lk_expire)) {
#ifdef HAVE_STRFTIME
		time_t t = (time_t)lip->lk_expire.tv_sec;
		char tbuf[64];
		if (strftime(tbuf,
		    sizeof(tbuf), "%m-%d-%H:%M:%S", localtime(&t)) != 0)
			__db_msgadd(dbenv, mbp, " lk expires %s.%09lu",
			    tbuf, (u_long)lip->lk_expire.tv_nsec);
		else
#endif
			__db_msgadd(dbenv, mbp, " lk expires %lu.%09lu",
			    (u_long)lip->lk_expire.tv_sec,
			    (u_long)lip->lk_expire.tv_nsec);
	}
	DB_MSGBUF_FLUSH(dbenv, mbp);

	SH_LIST_FOREACH(lp, &lip->heldby, locker_links, __db_lock)
		__lock_printlock(lt, mbp, lp, 1);
}

static void
__lock_dump_object(lt, mbp, op)
	DB_LOCKTAB *lt;
	DB_MSGBUF *mbp;
	DB_LOCKOBJ *op;
{
	struct __db_lock *lp;

	SH_TAILQ_FOREACH(lp, &op->holders, links, __db_lock)
		__lock_printlock(lt, mbp, lp, 1);
	SH_TAILQ_FOREACH(lp, &op->waiters, links, __db_lock)
		__lock_printlock(lt, mbp, lp, 1);
}

/*
 * __lock_print_header --
 */
static void
__lock_print_header(dbenv)
	DB_ENV *dbenv;
{
	__db_msg(dbenv, "%-8s %-10s%-4s %-7s %s",
	    "Locker", "Mode",
	    "Count", "Status", "----------------- Object ---------------");
}

/*
 * __lock_printlock --
 *
 * PUBLIC: void __lock_printlock
 * PUBLIC:     __P((DB_LOCKTAB *, DB_MSGBUF *mbp, struct __db_lock *, int));
 */
void
__lock_printlock(lt, mbp, lp, ispgno)
	DB_LOCKTAB *lt;
	DB_MSGBUF *mbp;
	struct __db_lock *lp;
	int ispgno;
{
	DB_ENV *dbenv;
	DB_LOCKOBJ *lockobj;
	DB_MSGBUF mb;
	db_pgno_t pgno;
	u_int32_t *fidp, type;
	u_int8_t *ptr;
	char *namep;
	const char *mode, *status;

	dbenv = lt->dbenv;

	if (mbp == NULL) {
		DB_MSGBUF_INIT(&mb);
		mbp = &mb;
	}

	switch (lp->mode) {
	case DB_LOCK_IREAD:
		mode = "IREAD";
		break;
	case DB_LOCK_IWR:
		mode = "IWR";
		break;
	case DB_LOCK_IWRITE:
		mode = "IWRITE";
		break;
	case DB_LOCK_NG:
		mode = "NG";
		break;
	case DB_LOCK_READ:
		mode = "READ";
		break;
	case DB_LOCK_READ_UNCOMMITTED:
		mode = "READ_UNCOMMITTED";
		break;
	case DB_LOCK_WRITE:
		mode = "WRITE";
		break;
	case DB_LOCK_WWRITE:
		mode = "WAS_WRITE";
		break;
	case DB_LOCK_WAIT:
		mode = "WAIT";
		break;
	default:
		mode = "UNKNOWN";
		break;
	}
	switch (lp->status) {
	case DB_LSTAT_ABORTED:
		status = "ABORT";
		break;
	case DB_LSTAT_EXPIRED:
		status = "EXPIRED";
		break;
	case DB_LSTAT_FREE:
		status = "FREE";
		break;
	case DB_LSTAT_HELD:
		status = "HELD";
		break;
	case DB_LSTAT_PENDING:
		status = "PENDING";
		break;
	case DB_LSTAT_WAITING:
		status = "WAIT";
		break;
	default:
		status = "UNKNOWN";
		break;
	}
	__db_msgadd(dbenv, mbp, "%8lx %-10s %4lu %-7s ",
	    (u_long)((DB_LOCKER *)R_ADDR(&lt->reginfo, lp->holder))->id,
	    mode, (u_long)lp->refcount, status);

	lockobj = (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);
	ptr = SH_DBT_PTR(&lockobj->lockobj);
	if (ispgno && lockobj->lockobj.size == sizeof(struct __db_ilock)) {
		/* Assume this is a DBT lock. */
		memcpy(&pgno, ptr, sizeof(db_pgno_t));
		fidp = (u_int32_t *)(ptr + sizeof(db_pgno_t));
		type = *(u_int32_t *)(ptr + sizeof(db_pgno_t) + DB_FILE_ID_LEN);
		if (__dbreg_get_name(lt->dbenv, (u_int8_t *)fidp, &namep) != 0)
			namep = NULL;
		if (namep == NULL)
			__db_msgadd(dbenv, mbp, "(%lx %lx %lx %lx %lx) ",
			    (u_long)fidp[0], (u_long)fidp[1], (u_long)fidp[2],
			    (u_long)fidp[3], (u_long)fidp[4]);
		else
			__db_msgadd(dbenv, mbp, "%-25s ", namep);
		__db_msgadd(dbenv, mbp, "%-7s %7lu",
			type == DB_PAGE_LOCK ? "page" :
			type == DB_RECORD_LOCK ? "record" : "handle",
			(u_long)pgno);
	} else {
		__db_msgadd(dbenv, mbp, "0x%lx ",
		    (u_long)R_OFFSET(&lt->reginfo, lockobj));
		__db_pr(dbenv, mbp, ptr, lockobj->lockobj.size);
	}
	DB_MSGBUF_FLUSH(dbenv, mbp);
}

#else /* !HAVE_STATISTICS */

int
__lock_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOCK_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}

int
__lock_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif
