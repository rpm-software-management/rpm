/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: txn_stat.c,v 12.23 2007/06/22 18:27:51 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

#ifdef HAVE_STATISTICS
static int  __txn_compare __P((const void *, const void *));
static int  __txn_print_all __P((DB_ENV *, u_int32_t));
static int  __txn_print_stats __P((DB_ENV *, u_int32_t));
static int  __txn_stat __P((DB_ENV *, DB_TXN_STAT **, u_int32_t));
static char *__txn_status __P((DB_TXN_ACTIVE *));
static void __txn_gid __P((DB_ENV *, DB_MSGBUF *, DB_TXN_ACTIVE *));

#define	XID_FIELD_IS_SET(p)						\
	((p)->xa_status != 0 || (p)->status == TXN_PREPARED)

/*
 * __txn_stat_pp --
 *	DB_ENV->txn_stat pre/post processing.
 *
 * PUBLIC: int __txn_stat_pp __P((DB_ENV *, DB_TXN_STAT **, u_int32_t));
 */
int
__txn_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_TXN_STAT **statp;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->tx_handle, "DB_ENV->txn_stat", DB_INIT_TXN);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->txn_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__txn_stat(dbenv, statp, flags)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __txn_stat --
 *	DB_ENV->txn_stat.
 */
static int
__txn_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_TXN_STAT **statp;
	u_int32_t flags;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	DB_TXN_STAT *stats;
	TXN_DETAIL *td;
	size_t nbytes;
	u_int32_t maxtxn, ndx;
	int ret;

	*statp = NULL;
	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	/*
	 * Allocate for the maximum active transactions -- the DB_TXN_ACTIVE
	 * struct is small and the maximum number of active transactions is
	 * not going to be that large.  Don't have to lock anything to look
	 * at the region's maximum active transactions value, it's read-only
	 * and never changes after the region is created.
	 *
	 * The maximum active transactions isn't a hard limit, so allocate
	 * some extra room, and don't walk off the end.
	 */
	maxtxn = region->maxtxns + (region->maxtxns / 10) + 10;
	nbytes = sizeof(DB_TXN_STAT) + sizeof(DB_TXN_ACTIVE) * maxtxn;
	if ((ret = __os_umalloc(dbenv, nbytes, &stats)) != 0)
		return (ret);

	TXN_SYSTEM_LOCK(dbenv);
	memcpy(stats, &region->stat, sizeof(*stats));
	stats->st_last_txnid = region->last_txnid;
	stats->st_last_ckp = region->last_ckp;
	stats->st_time_ckp = region->time_ckp;
	stats->st_txnarray = (DB_TXN_ACTIVE *)&stats[1];

	for (ndx = 0,
	    td = SH_TAILQ_FIRST(&region->active_txn, __txn_detail);
	    td != NULL && ndx < maxtxn;
	    td = SH_TAILQ_NEXT(td, links, __txn_detail), ++ndx) {
		stats->st_txnarray[ndx].txnid = td->txnid;
		if (td->parent == INVALID_ROFF)
			stats->st_txnarray[ndx].parentid = TXN_INVALID;
		else
			stats->st_txnarray[ndx].parentid =
			    ((TXN_DETAIL *)R_ADDR(&mgr->reginfo,
			    td->parent))->txnid;
		stats->st_txnarray[ndx].pid = td->pid;
		stats->st_txnarray[ndx].tid = td->tid;
		stats->st_txnarray[ndx].lsn = td->begin_lsn;
		stats->st_txnarray[ndx].read_lsn = td->read_lsn;
		stats->st_txnarray[ndx].mvcc_ref = td->mvcc_ref;
		stats->st_txnarray[ndx].status = td->status;
		stats->st_txnarray[ndx].xa_status = td->xa_status;
		if (XID_FIELD_IS_SET(td))
			memcpy(stats->st_txnarray[ndx].xid,
			    td->xid, sizeof(td->xid));
		if (td->name != INVALID_ROFF) {
			(void)strncpy(stats->st_txnarray[ndx].name,
			    R_ADDR(&mgr->reginfo, td->name),
			    sizeof(stats->st_txnarray[ndx].name) - 1);
			stats->st_txnarray[ndx].name[
			    sizeof(stats->st_txnarray[ndx].name) - 1] = '\0';
		} else
			stats->st_txnarray[ndx].name[0] = '\0';
	}

	__mutex_set_wait_info(dbenv, region->mtx_region,
	    &stats->st_region_wait, &stats->st_region_nowait);
	stats->st_regsize = mgr->reginfo.rp->size;
	if (LF_ISSET(DB_STAT_CLEAR)) {
		if (!LF_ISSET(DB_STAT_SUBSYSTEM))
			__mutex_clear(dbenv, region->mtx_region);
		memset(&region->stat, 0, sizeof(region->stat));
		region->stat.st_maxtxns = region->maxtxns;
		region->stat.st_maxnactive =
		    region->stat.st_nactive = stats->st_nactive;
		region->stat.st_maxnsnapshot =
		    region->stat.st_nsnapshot = stats->st_nsnapshot;
	}

	TXN_SYSTEM_UNLOCK(dbenv);

	*statp = stats;
	return (0);
}

/*
 * __txn_stat_print_pp --
 *	DB_ENV->txn_stat_print pre/post processing.
 *
 * PUBLIC: int __txn_stat_print_pp __P((DB_ENV *, u_int32_t));
 */
int
__txn_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->tx_handle, "DB_ENV->txn_stat_print", DB_INIT_TXN);

	if ((ret = __db_fchk(dbenv, "DB_ENV->txn_stat_print",
	    flags, DB_STAT_ALL | DB_STAT_CLEAR)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__txn_stat_print(dbenv, flags)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __txn_stat_print
 *	DB_ENV->txn_stat_print method.
 *
 * PUBLIC: int  __txn_stat_print __P((DB_ENV *, u_int32_t));
 */
int
__txn_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	u_int32_t orig_flags;
	int ret;

	orig_flags = flags;
	LF_CLR(DB_STAT_CLEAR | DB_STAT_SUBSYSTEM);
	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		ret = __txn_print_stats(dbenv, orig_flags);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL) &&
	    (ret = __txn_print_all(dbenv, orig_flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __txn_print_stats --
 *	Display default transaction region statistics.
 */
static int
__txn_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_MSGBUF mb;
	DB_TXN_STAT *sp;
	DB_TXN_ACTIVE *txn;
	u_int32_t i;
	int ret;
	char buf[DB_THREADID_STRLEN], time_buf[CTIME_BUFLEN];

	if ((ret = __txn_stat(dbenv, &sp, flags)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL))
		__db_msg(dbenv, "Default transaction region information:");
	__db_msg(dbenv, "%lu/%lu\t%s",
	    (u_long)sp->st_last_ckp.file, (u_long)sp->st_last_ckp.offset,
	    sp->st_last_ckp.file == 0 ?
	    "No checkpoint LSN" : "File/offset for last checkpoint LSN");
	if (sp->st_time_ckp == 0)
		__db_msg(dbenv, "0\tNo checkpoint timestamp");
	else
		__db_msg(dbenv, "%.24s\tCheckpoint timestamp",
		    __db_ctime(&sp->st_time_ckp, time_buf));
	__db_msg(dbenv, "%#lx\tLast transaction ID allocated",
	    (u_long)sp->st_last_txnid);
	__db_dl(dbenv, "Maximum number of active transactions configured",
	    (u_long)sp->st_maxtxns);
	__db_dl(dbenv, "Active transactions", (u_long)sp->st_nactive);
	__db_dl(dbenv,
	    "Maximum active transactions", (u_long)sp->st_maxnactive);
	__db_dl(dbenv,
	    "Number of transactions begun", (u_long)sp->st_nbegins);
	__db_dl(dbenv,
	    "Number of transactions aborted", (u_long)sp->st_naborts);
	__db_dl(dbenv,
	    "Number of transactions committed", (u_long)sp->st_ncommits);
	__db_dl(dbenv, "Snapshot transactions", (u_long)sp->st_nsnapshot);
	__db_dl(dbenv, "Maximum snapshot transactions",
	    (u_long)sp->st_maxnsnapshot);
	__db_dl(dbenv,
	    "Number of transactions restored", (u_long)sp->st_nrestores);

	__db_dlbytes(dbenv, "Transaction region size",
	    (u_long)0, (u_long)0, (u_long)sp->st_regsize);
	__db_dl_pct(dbenv,
	    "The number of region locks that required waiting",
	    (u_long)sp->st_region_wait, DB_PCT(sp->st_region_wait,
	    sp->st_region_wait + sp->st_region_nowait), NULL);

	qsort(sp->st_txnarray,
	    sp->st_nactive, sizeof(sp->st_txnarray[0]), __txn_compare);
	__db_msg(dbenv, "Active transactions:");
	DB_MSGBUF_INIT(&mb);
	for (i = 0; i < sp->st_nactive; ++i) {
		txn = &sp->st_txnarray[i];
		__db_msgadd(dbenv, &mb,
	    "\t%lx: %s; pid/thread %s; begin LSN: file/offset %lu/%lu",
		    (u_long)txn->txnid, __txn_status(txn),
		    dbenv->thread_id_string(dbenv, txn->pid, txn->tid, buf),
		    (u_long)txn->lsn.file, (u_long)txn->lsn.offset);
		if (txn->parentid != 0)
			__db_msgadd(dbenv, &mb,
			    "; parent: %lx", (u_long)txn->parentid);
		if (!IS_MAX_LSN(txn->read_lsn))
			__db_msgadd(dbenv, &mb, "; read LSN: %lu/%lu",
			    (u_long)txn->read_lsn.file,
			    (u_long)txn->read_lsn.offset);
		if (txn->mvcc_ref != 0)
			__db_msgadd(dbenv, &mb,
			    "; mvcc refcount: %lu", (u_long)txn->mvcc_ref);
		if (txn->name[0] != '\0')
			__db_msgadd(dbenv, &mb, "; \"%s\"", txn->name);
		if (XID_FIELD_IS_SET(txn))
			__txn_gid(dbenv, &mb, txn);
		DB_MSGBUF_FLUSH(dbenv, &mb);
	}

	__os_ufree(dbenv, sp);

	return (0);
}

/*
 * __txn_print_all --
 *	Display debugging transaction region statistics.
 */
static int
__txn_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ TXN_IN_RECOVERY,	"TXN_IN_RECOVERY" },
		{ 0,			NULL }
	};
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	char time_buf[CTIME_BUFLEN];

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	TXN_SYSTEM_LOCK(dbenv);

	__db_print_reginfo(dbenv, &mgr->reginfo, "Transaction", flags);

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_TXNMGR handle information:");
	__mutex_print_debug_single(dbenv, "DB_TXNMGR mutex", mgr->mutex, flags);
	__db_dl(dbenv,
	    "Number of transactions discarded", (u_long)mgr->n_discards);

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_TXNREGION handle information:");
	__mutex_print_debug_single(
	    dbenv, "DB_TXNREGION region mutex", region->mtx_region, flags);
	STAT_ULONG("Maximum number of active txns", region->maxtxns);
	STAT_HEX("Last transaction ID allocated", region->last_txnid);
	STAT_HEX("Current maximum unused ID", region->cur_maxid);

	__mutex_print_debug_single(
	    dbenv, "checkpoint mutex", region->mtx_ckp, flags);
	STAT_LSN("Last checkpoint LSN", &region->last_ckp);
	__db_msg(dbenv,
	    "%.24s\tLast checkpoint timestamp",
	    region->time_ckp == 0 ? "0" :
	    __db_ctime(&region->time_ckp, time_buf));

	__db_prflags(dbenv, NULL, region->flags, fn, NULL, "\tFlags");

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "XA information:");
	STAT_LONG("XA RMID", dbenv->xa_rmid);
	/*
	 * XXX
	 * Display list of XA transactions in the DB_ENV handle.
	 */

	TXN_SYSTEM_UNLOCK(dbenv);

	return (0);
}

static char *
__txn_status(txn)
	DB_TXN_ACTIVE *txn;
{
	switch (txn->xa_status) {
	case 0:
		switch (txn->status) {
		case TXN_ABORTED:
			return ("aborted");
		case TXN_COMMITTED:
			return ("committed");
		case TXN_PREPARED:
			return ("prepared");
		case TXN_RUNNING:
			return ("running");
		default:
			break;
		}
		break;
	case TXN_XA_ABORTED:
		return ("XA aborted");
	case TXN_XA_DEADLOCKED:
		return ("XA deadlocked");
	case TXN_XA_ENDED:
		return ("XA ended");
	case TXN_XA_PREPARED:
		return ("XA prepared");
	case TXN_XA_STARTED:
		return ("XA started");
	case TXN_XA_SUSPENDED:
		return ("XA suspended");
	default:
		break;
	}
	return ("unknown state");
}

static void
__txn_gid(dbenv, mbp, txn)
	DB_ENV *dbenv;
	DB_MSGBUF *mbp;
	DB_TXN_ACTIVE *txn;
{
	u_int32_t v, *xp;
	u_int i;
	int cnt;

	__db_msgadd(dbenv, mbp, "\n\tGID/XID:");
	for (cnt = 0, xp = (u_int32_t *)txn->xid, i = 0;;) {
		memcpy(&v, xp++, sizeof(u_int32_t));
		__db_msgadd(dbenv, mbp, "%#lx ", (u_long)v);
		if ((i += sizeof(u_int32_t)) >= DB_XIDDATASIZE)
			break;
		if (++cnt == 4) {
			DB_MSGBUF_FLUSH(dbenv, mbp);
			__db_msgadd(dbenv, mbp, "\t\t");
			cnt = 0;
		}
	}
}

static int
__txn_compare(a1, b1)
	const void *a1, *b1;
{
	const DB_TXN_ACTIVE *a, *b;

	a = a1;
	b = b1;

	if (a->txnid > b->txnid)
		return (1);
	if (a->txnid < b->txnid)
		return (-1);
	return (0);
}

#else /* !HAVE_STATISTICS */

int
__txn_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_TXN_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}

int
__txn_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif
