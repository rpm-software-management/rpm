/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: rep_util.c,v 12.115 2007/06/22 18:46:45 paula Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

#ifdef REP_DIAGNOSTIC
#include "dbinc/db_page.h"
#include "dbinc/fop.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/qam.h"
#endif

/*
 * rep_util.c:
 *	Miscellaneous replication-related utility functions, including
 *	those called by other subsystems.
 */
#define	TIMESTAMP_CHECK(dbenv, ts, renv) do {				\
	if (renv->op_timestamp != 0 &&					\
	    renv->op_timestamp + DB_REGENV_TIMEOUT < ts) {		\
		REP_SYSTEM_LOCK(dbenv);					\
		F_CLR(renv, DB_REGENV_REPLOCKED);			\
		renv->op_timestamp = 0;					\
		REP_SYSTEM_UNLOCK(dbenv);				\
	}								\
} while (0)

static int __rep_lockout_int __P((DB_ENV *, REP *, u_int32_t *, u_int32_t,
    const char *, u_int32_t));
static int __rep_newmaster_empty __P((DB_ENV *, int));
#ifdef REP_DIAGNOSTIC
static void __rep_print_logmsg __P((DB_ENV *, const DBT *, DB_LSN *));
#endif

/*
 * __rep_bulk_message --
 *	This is a wrapper for putting a record into a bulk buffer.  Since
 * we have different bulk buffers, the caller must hand us the information
 * we need to put the record into the correct buffer.  All bulk buffers
 * are protected by the REP->mtx_clientdb.
 *
 * PUBLIC: int __rep_bulk_message __P((DB_ENV *, REP_BULK *, REP_THROTTLE *,
 * PUBLIC:     DB_LSN *, const DBT *, u_int32_t));
 */
int
__rep_bulk_message(dbenv, bulk, repth, lsn, dbt, flags)
	DB_ENV *dbenv;
	REP_BULK *bulk;
	REP_THROTTLE *repth;
	DB_LSN *lsn;
	const DBT *dbt;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REP *rep;
	int ret;
	u_int32_t recsize, typemore;
	u_int8_t *p;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	ret = 0;

	/*
	 * Figure out the total number of bytes needed for this record.
	 */
	recsize = dbt->size + sizeof(DB_LSN) + sizeof(dbt->size);

	/*
	 * If *this* buffer is actively being transmitted, wait until
	 * we can use it.
	 */
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	while (FLD_ISSET(*(bulk->flagsp), BULK_XMIT)) {
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		__os_sleep(dbenv, 1, 0);
		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	}

	/*
	 * If the record is bigger than the buffer entirely, send the
	 * current buffer and then return DB_REP_BULKOVF so that this
	 * record is sent as a singleton.  Do we have enough info to
	 * do that here?  XXX
	 */
	if (recsize > bulk->len) {
		RPRINT(dbenv, (dbenv,
		    "bulk_msg: Record %d (0x%x) larger than entire buffer 0x%x",
		    recsize, recsize, bulk->len));
		STAT(rep->stat.st_bulk_overflows++);
		(void)__rep_send_bulk(dbenv, bulk, flags);
		/*
		 * XXX __rep_send_message...
		 */
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		return (DB_REP_BULKOVF);
	}
	/*
	 * If this record doesn't fit, send the current buffer.
	 * Sending the buffer will reset the offset, but we will
	 * drop the mutex while sending so we need to keep checking
	 * if we're racing.
	 */
	while (recsize + *(bulk->offp) > bulk->len) {
		RPRINT(dbenv, (dbenv,
	    "bulk_msg: Record %lu (%#lx) doesn't fit.  Send %lu (%#lx) now.",
		    (u_long)recsize, (u_long)recsize,
		    (u_long)bulk->len, (u_long)bulk->len));
		STAT(rep->stat.st_bulk_fills++);
		if ((ret = __rep_send_bulk(dbenv, bulk, flags)) != 0) {
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			return (ret);
		}
	}

	/*
	 * If we're using throttling, see if we are at the throttling
	 * limit before we do any more work here, by checking if the
	 * call to rep_send_throttle changed the repth->type to the
	 * *_MORE message type.  If the throttling code hits the limit
	 * then we're done here.
	 */
	if (bulk->type == REP_BULK_LOG)
		typemore = REP_LOG_MORE;
	else
		typemore = REP_PAGE_MORE;
	if (repth != NULL) {
		if ((ret = __rep_send_throttle(dbenv,
		    bulk->eid, repth, REP_THROTTLE_ONLY, flags)) != 0) {
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			return (ret);
		}
		if (repth->type == typemore) {
			RPRINT(dbenv, (dbenv,
			    "bulk_msg: Record %lu (0x%lx) hit throttle limit.",
			    (u_long)recsize, (u_long)recsize));
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			return (ret);
		}
	}

	/*
	 * Now we own the buffer, and we know our record fits into it.
	 * The buffer is structured with the len, LSN and then the record.
	 * Copy the record into the buffer.  Then if we need to,
	 * send the buffer.
	 */
	/*
	 * First thing is the length of the dbt record.
	 */
	p = bulk->addr + *(bulk->offp);
	memcpy(p, &dbt->size, sizeof(dbt->size));
	p += sizeof(dbt->size);
	/*
	 * The next thing is the LSN.  We need LSNs for both pages and
	 * log records.  For log records, this is obviously, the LSN of
	 * this record.  For pages, the LSN is used by the internal init code.
	 */
	memcpy(p, lsn, sizeof(DB_LSN));
	RPRINT(dbenv, (dbenv,
	    "bulk_msg: Copying LSN [%lu][%lu] of %lu bytes to %#lx",
	    (u_long)lsn->file, (u_long)lsn->offset, (u_long)dbt->size,
	    P_TO_ULONG(p)));
	p += sizeof(DB_LSN);
	/*
	 * If we're the first record, we need to save the first
	 * LSN in the bulk structure.
	 */
	if (*(bulk->offp) == 0)
		bulk->lsn = *lsn;
	/*
	 * Now copy the record and finally adjust the offset.
	 */
	memcpy(p, dbt->data, dbt->size);
	p += dbt->size;
	*(bulk->offp) = (uintptr_t)p - (uintptr_t)bulk->addr;
	STAT(rep->stat.st_bulk_records++);
	/*
	 * Send the buffer if it is a perm record or a force.
	 */
	if (LF_ISSET(REPCTL_PERM)) {
		RPRINT(dbenv, (dbenv,
		    "bulk_msg: Send buffer after copy due to PERM"));
		ret = __rep_send_bulk(dbenv, bulk, flags);
	}
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	return (ret);

}

/*
 * __rep_send_bulk --
 *	This function transmits the bulk buffer given.  It assumes the
 * caller holds the REP->mtx_clientdb.  We may release it and reacquire
 * it during this call.  We will return with it held.
 *
 * PUBLIC: int __rep_send_bulk __P((DB_ENV *, REP_BULK *, u_int32_t));
 */
int
__rep_send_bulk(dbenv, bulkp, ctlflags)
	DB_ENV *dbenv;
	REP_BULK *bulkp;
	u_int32_t ctlflags;
{
	DB_REP *db_rep;
	REP *rep;
	DBT dbt;
	int ret;

	/*
	 * If the offset is 0, we're done.  There is nothing to send.
	 */
	if (*(bulkp->offp) == 0)
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	/*
	 * Set that this buffer is being actively transmitted.
	 */
	FLD_SET(*(bulkp->flagsp), BULK_XMIT);
	DB_INIT_DBT(dbt, bulkp->addr, *(bulkp->offp));
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	RPRINT(dbenv, (dbenv,
	    "send_bulk: Send %d (0x%x) bulk buffer bytes", dbt.size, dbt.size));

	/*
	 * Unlocked the mutex and now send the message.
	 */
	STAT(rep->stat.st_bulk_transfers++);
	if ((ret = __rep_send_message(dbenv,
	    bulkp->eid, bulkp->type, &bulkp->lsn, &dbt, ctlflags, 0)) != 0)
		ret = DB_REP_UNAVAIL;

	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	/*
	 * Ready the buffer for further records.
	 */
	*(bulkp->offp) = 0;
	FLD_CLR(*(bulkp->flagsp), BULK_XMIT);
	return (ret);
}

/*
 * __rep_bulk_alloc --
 *	This function allocates and initializes an internal bulk buffer.
 * This is used by the master when fulfilling a request for a chunk of
 * log records or a bunch of pages.
 *
 * PUBLIC: int __rep_bulk_alloc __P((DB_ENV *, REP_BULK *, int, uintptr_t *,
 * PUBLIC:    u_int32_t *, u_int32_t));
 */
int
__rep_bulk_alloc(dbenv, bulkp, eid, offp, flagsp, type)
	DB_ENV *dbenv;
	REP_BULK *bulkp;
	int eid;
	uintptr_t *offp;
	u_int32_t *flagsp, type;
{
	int ret;

	memset(bulkp, 0, sizeof(REP_BULK));
	*offp = *flagsp = 0;
	bulkp->len = MEGABYTE;
	if ((ret = __os_malloc(dbenv, bulkp->len, &bulkp->addr)) != 0)
		return (ret);
	bulkp->offp = offp;
	bulkp->type = type;
	bulkp->eid = eid;
	bulkp->flagsp = flagsp;
	return (ret);
}

/*
 * __rep_bulk_free --
 *	This function sends the remainder of the bulk buffer and frees it.
 *
 * PUBLIC: int __rep_bulk_free __P((DB_ENV *, REP_BULK *, u_int32_t));
 */
int
__rep_bulk_free(dbenv, bulkp, flags)
	DB_ENV *dbenv;
	REP_BULK *bulkp;
	u_int32_t flags;
{
	DB_REP *db_rep;
	int ret;

	db_rep = dbenv->rep_handle;

	MUTEX_LOCK(dbenv, db_rep->region->mtx_clientdb);
	ret = __rep_send_bulk(dbenv, bulkp, flags);
	MUTEX_UNLOCK(dbenv, db_rep->region->mtx_clientdb);
	__os_free(dbenv, bulkp->addr);
	return (ret);
}

/*
 * __rep_send_message --
 *	This is a wrapper for sending a message.  It takes care of constructing
 * the REP_CONTROL structure and calling the user's specified send function.
 *
 * PUBLIC: int __rep_send_message __P((DB_ENV *, int,
 * PUBLIC:     u_int32_t, DB_LSN *, const DBT *, u_int32_t, u_int32_t));
 */
int
__rep_send_message(dbenv, eid, rtype, lsnp, dbt, ctlflags, repflags)
	DB_ENV *dbenv;
	int eid;
	u_int32_t rtype;
	DB_LSN *lsnp;
	const DBT *dbt;
	u_int32_t ctlflags, repflags;
{
	DB_LOG *dblp;
	DB_REP *db_rep;
	DBT cdbt, scrap_dbt;
	LOG *lp;
	REP *rep;
	REP_CONTROL cntrl;
	REP_OLD_CONTROL ocntrl;
	int ret;
	u_int32_t myflags, rectype;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	ret = 0;

#if defined(DEBUG_ROP) || defined(DEBUG_WOP)
	if (db_rep->send == NULL)
		return (0);
#endif

	/* Set up control structure. */
	memset(&cntrl, 0, sizeof(cntrl));
	memset(&ocntrl, 0, sizeof(ocntrl));
	if (lsnp == NULL)
		ZERO_LSN(cntrl.lsn);
	else
		cntrl.lsn = *lsnp;
	/*
	 * Set the rectype based on the version we need to speak.
	 */
	if (rep->version == DB_REPVERSION)
		cntrl.rectype = rtype;
	else if (rep->version < DB_REPVERSION) {
		cntrl.rectype = __rep_msg_to_old(rep->version, rtype);
		RPRINT(dbenv, (dbenv,
		    "rep_send_msg: rtype %lu to version %lu record %lu.",
		    (u_long)rtype, (u_long)rep->version,
		    (u_long)cntrl.rectype));
		if (cntrl.rectype == REP_INVALID)
			return (ret);
	} else {
		__db_errx(dbenv,
    "rep_send_message: Unknown rep version %lu, my version %lu",
		    (u_long)rep->version, (u_long)DB_REPVERSION);
		return (__db_panic(dbenv, EINVAL));
	}
	cntrl.flags = ctlflags;
	cntrl.rep_version = rep->version;
	cntrl.log_version = lp->persist.version;
	cntrl.gen = rep->gen;

	/* Don't assume the send function will be tolerant of NULL records. */
	if (dbt == NULL) {
		memset(&scrap_dbt, 0, sizeof(DBT));
		dbt = &scrap_dbt;
	}

	/*
	 * There are several types of records: commit and checkpoint records
	 * that affect database durability, regular log records that might
	 * be buffered on the master before being transmitted, and control
	 * messages which don't require the guarantees of permanency, but
	 * should not be buffered.
	 *
	 * There are request records that can be sent anywhere, and there
	 * are rerequest records that the app might want to send to the master.
	 */
	myflags = repflags;
	if (FLD_ISSET(ctlflags, REPCTL_PERM))
		myflags |= DB_REP_PERMANENT;
	else if (rtype != REP_LOG || FLD_ISSET(ctlflags, REPCTL_RESEND))
		myflags |= DB_REP_NOBUFFER;
	if (rtype == REP_LOG && !FLD_ISSET(ctlflags, REPCTL_PERM)) {
		/*
		 * Check if this is a log record we just read that
		 * may need a REPCTL_PERM.  This is of type REP_LOG,
		 * so we know that dbt is a log record.
		 */
		memcpy(&rectype, dbt->data, sizeof(rectype));
		if (rectype == DB___txn_regop || rectype == DB___txn_ckp)
			F_SET(&cntrl, REPCTL_PERM);
	}

	/*
	 * Let everyone know if we've been in an established group.
	 */
	if (F_ISSET(rep, REP_F_GROUP_ESTD))
		F_SET(&cntrl, REPCTL_GROUP_ESTD);

	/*
	 * We're sending messages to some other version.  We cannot
	 * assume DB_REP_ANYWHERE is available.  Turn it off.
	 */
	if (rep->version != DB_REPVERSION)
		FLD_CLR(myflags, DB_REP_ANYWHERE);

	/*
	 * If we are a master sending a perm record, then set the
	 * REPCTL_LEASE flag to have the client reply.  Also set
	 * the start time that the client will echo back to us.
	 *
	 * !!! If we are a master, using leases, we had better not be
	 * sending to an older version.
	 */
	if (IS_REP_MASTER(dbenv) && IS_USING_LEASES(dbenv) &&
	    FLD_ISSET(ctlflags, REPCTL_PERM)) {
		F_SET(&cntrl, REPCTL_LEASE);
		DB_ASSERT(dbenv, rep->version == DB_REPVERSION);
		__os_gettime(dbenv, &cntrl.msg_time);
	}

	REP_PRINT_MESSAGE(dbenv, eid, &cntrl, "rep_send_message", myflags);
#ifdef REP_DIAGNOSTIC
	if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION) && rtype == REP_LOG)
		__rep_print_logmsg(dbenv, dbt, lsnp);
#endif

	/*
	 * If DB_REP_PERMANENT is set, the LSN better be non-zero.
	 */
	DB_ASSERT(dbenv, !FLD_ISSET(myflags, DB_REP_PERMANENT) ||
	    !IS_ZERO_LSN(cntrl.lsn));

	/*
	 * If we're talking to an old version, send an old control structure.
	 */
	memset(&cdbt, 0, sizeof(cdbt));
	if (rep->version < DB_REPVERSION) {
		ocntrl.rep_version = cntrl.rep_version;
		ocntrl.log_version = cntrl.log_version;
		ocntrl.lsn = cntrl.lsn;
		ocntrl.rectype = cntrl.rectype;
		ocntrl.gen = cntrl.gen;
		ocntrl.flags = cntrl.flags;
		cdbt.data = &ocntrl;
		cdbt.size = sizeof(ocntrl);
	} else {
		cdbt.data = &cntrl;
		cdbt.size = sizeof(cntrl);
	}

	/*
	 * We set the LSN above to something valid.  Give the master the
	 * actual LSN so that they can coordinate with permanent records from
	 * the client if they want to.
	 */
	ret = db_rep->send(dbenv, &cdbt, dbt, &cntrl.lsn, eid, myflags);

	/*
	 * We don't hold the rep lock, so this could miscount if we race.
	 * I don't think it's worth grabbing the mutex for that bit of
	 * extra accuracy.
	 */
	if (ret != 0) {
		RPRINT(dbenv, (dbenv,
		    "rep_send_function returned: %d", ret));
#ifdef HAVE_STATISTICS
		rep->stat.st_msgs_send_failures++;
	} else
		rep->stat.st_msgs_sent++;
#else
	}
#endif
	return (ret);
}

#ifdef REP_DIAGNOSTIC
/*
 * __rep_print_logmsg --
 *	This is a debugging routine for printing out log records that
 * we are about to transmit to a client.
 */
static void
__rep_print_logmsg(dbenv, logdbt, lsnp)
	DB_ENV *dbenv;
	const DBT *logdbt;
	DB_LSN *lsnp;
{
	/* Static structures to hold the printing functions. */
	static size_t ptabsize;
	static int (**ptab)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));

	if (ptab == NULL) {
		/* Initialize the table. */
		(void)__bam_init_print(dbenv, &ptab, &ptabsize);
		(void)__crdel_init_print(dbenv, &ptab, &ptabsize);
		(void)__db_init_print(dbenv, &ptab, &ptabsize);
		(void)__dbreg_init_print(dbenv, &ptab, &ptabsize);
		(void)__fop_init_print(dbenv, &ptab, &ptabsize);
		(void)__ham_init_print(dbenv, &ptab, &ptabsize);
		(void)__qam_init_print(dbenv, &ptab, &ptabsize);
		(void)__txn_init_print(dbenv, &ptab, &ptabsize);
	}

	(void)__db_dispatch(dbenv,
	    ptab, ptabsize, (DBT *)logdbt, lsnp, DB_TXN_PRINT, NULL);
}
#endif

/*
 * __rep_new_master --
 *	Called after a master election to sync back up with a new master.
 * It's possible that we already know of this new master in which case
 * we don't need to do anything.
 *
 * This is written assuming that this message came from the master; we
 * need to enforce that in __rep_process_record, but right now, we have
 * no way to identify the master.
 *
 * PUBLIC: int __rep_new_master __P((DB_ENV *, REP_CONTROL *, int));
 */
int
__rep_new_master(dbenv, cntrl, eid)
	DB_ENV *dbenv;
	REP_CONTROL *cntrl;
	int eid;
{
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN first_lsn, lsn;
	DB_REP *db_rep;
	DBT dbt;
	LOG *lp;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	db_timeout_t lease_to;
	u_int32_t unused;
	int change, do_req, lockout, ret, t_ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	ret = 0;
	logc = NULL;
	lockout = 0;
	REP_SYSTEM_LOCK(dbenv);
	change = rep->gen != cntrl->gen || rep->master_id != eid;
	if (change) {
		/*
		 * If we are already locking out others, we're either
		 * in the middle of sync-up recovery or internal init
		 * when this newmaster comes in (we also lockout in
		 * rep_start, but we cannot be racing that because we
		 * don't allow rep_proc_msg when rep_start is going on).
		 *
		 * If we were in the middle of an internal initialization
		 * and we've discovered a new master instead, clean up
		 * our old internal init information.  We need to clean
		 * up any flags and unlock our lockout.
		 */
		if (F_ISSET(rep, REP_F_READY_MSG))
			goto lckout;

		if ((ret = __rep_lockout_msg(dbenv, rep, 1)) != 0)
			goto errlck;

		lockout = 1;
		/*
		 * We must wait any remaining lease time before accepting
		 * this new master.  This must be after the lockout above
		 * to that no new message can be processed and re-grant
		 * the lease out from under us.
		 */
		if (IS_USING_LEASES(dbenv) &&
		    ((lease_to = __rep_lease_waittime(dbenv)) != 0)) {
			REP_SYSTEM_UNLOCK(dbenv);
			__os_sleep(dbenv, 0, (u_long)lease_to);
			REP_SYSTEM_LOCK(dbenv);
		}

		if ((ret = __env_init_rec(dbenv, cntrl->log_version)) != 0)
			goto errlck;

		REP_SYSTEM_UNLOCK(dbenv);

		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
		lp->wait_recs = rep->request_gap;
		lp->rcvd_recs = 0;
		ZERO_LSN(lp->verify_lsn);
		ZERO_LSN(lp->waiting_lsn);
		ZERO_LSN(lp->max_wait_lsn);
		/*
		 * Open if we need to, in preparation for the truncate
		 * we'll do in a moment.
		 */
		if (db_rep->rep_db == NULL &&
		    (ret = __rep_client_dbinit(dbenv, 0, REP_DB)) != 0) {
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			goto err;
		}

		REP_SYSTEM_LOCK(dbenv);
		if (F_ISSET(rep, REP_F_READY_API | REP_F_READY_OP)) {
			ret = __rep_init_cleanup(dbenv, rep, DB_FORCE);
			/*
			 * Note that if an in-progress internal init was indeed
			 * "cleaned up", clearing these flags now will allow the
			 * application to see a completely empty database
			 * environment for a moment (until the master responds
			 * to our ALL_REQ).
			 */
			F_CLR(rep, REP_F_RECOVER_MASK);
		}
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		if (ret != 0) {
			/* TODO: consider add'l error recovery steps. */
			goto errlck;
		}
		if ((ret = __db_truncate(db_rep->rep_db, NULL, &unused)) != 0)
			goto errlck;

		/*
		 * This needs to be performed under message lockout
		 * if we're actually changing master.
		 */
		__rep_elect_done(dbenv, rep);
		RPRINT(dbenv, (dbenv,
		    "Updating gen from %lu to %lu from master %d",
		    (u_long)rep->gen, (u_long)cntrl->gen, eid));
		rep->gen = cntrl->gen;
		(void)__rep_write_gen(dbenv, rep->gen);
		if (rep->egen <= rep->gen)
			rep->egen = rep->gen + 1;
		rep->master_id = eid;
		STAT(rep->stat.st_master_changes++);
		rep->stat.st_startup_complete = 0;
		__log_set_version(dbenv, cntrl->log_version);
		rep->version = cntrl->rep_version;
		RPRINT(dbenv, (dbenv,
		    "Egen: %lu. RepVersion %lu",
		    (u_long)rep->egen, (u_long)rep->version));

		/*
		 * If we're delaying client sync-up, we know we have a
		 * new/changed master now, set flag indicating we are
		 * actively delaying.
		 */
		if (FLD_ISSET(rep->config, REP_C_DELAYCLIENT))
			F_SET(rep, REP_F_DELAY);
		F_SET(rep, REP_F_NOARCHIVE | REP_F_RECOVER_VERIFY);
		F_CLR(rep, REP_F_READY_MSG);
		lockout = 0;
	} else
		__rep_elect_done(dbenv, rep);
	REP_SYSTEM_UNLOCK(dbenv);

	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	lsn = lp->ready_lsn;

	if (!change) {
		ret = 0;
		do_req = __rep_check_doreq(dbenv, rep);
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		/*
		 * If there wasn't a change, we might still have some
		 * catching up or verification to do.
		 */
		if (do_req &&
		    (F_ISSET(rep, REP_F_RECOVER_MASK) ||
		    LOG_COMPARE(&lsn, &cntrl->lsn) < 0)) {
			ret = __rep_resend_req(dbenv, 0);
			if (ret != 0)
				RPRINT(dbenv, (dbenv,
				    "resend_req ret is %lu", (u_long)ret));
		}
		/*
		 * If we're not in one of the recovery modes, we need to
		 * clear the NOARCHIVE flag.  Elections set NOARCHIVE
		 * and if we called an election and found the same
		 * master, we need to clear NOARCHIVE here.
		 */
		if (!F_ISSET(rep, REP_F_RECOVER_MASK)) {
			REP_SYSTEM_LOCK(dbenv);
			F_CLR(rep, REP_F_NOARCHIVE);
			REP_SYSTEM_UNLOCK(dbenv);
		}
		return (ret);
	}
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);

	/*
	 * If the master changed, we need to start the process of
	 * figuring out what our last valid log record is.  However,
	 * if both the master and we agree that the max LSN is 0,0,
	 * then there is no recovery to be done.  If we are at 0 and
	 * the master is not, then we just need to request all the log
	 * records from the master.
	 */
	if (IS_INIT_LSN(lsn) || IS_ZERO_LSN(lsn)) {
		if ((ret = __rep_newmaster_empty(dbenv, eid)) != 0)
			goto err;
		(void)__memp_set_config(dbenv, DB_MEMP_SYNC_INTERRUPT, 0);
		return (DB_REP_NEWMASTER);
	}

	memset(&dbt, 0, sizeof(dbt));
	/*
	 * If this client is farther ahead on the log file than the master, see
	 * if there is any overlap in the logs.  If not, the client is too
	 * far ahead of the master and we cannot determine they're part of
	 * the same replication group.
	 */
	if (cntrl->lsn.file < lsn.file) {
		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto err;
		ret = __logc_get(logc, &first_lsn, &dbt, DB_FIRST);
		if ((t_ret = __logc_close(logc)) != 0 && ret == 0)
			ret = t_ret;
		if (ret == DB_NOTFOUND)
			goto notfound;
		else if (ret != 0)
			goto err;
		if (cntrl->lsn.file < first_lsn.file) {
			__db_errx(dbenv,
    "Client too far ahead of master; unable to join replication group");
			ret = DB_REP_JOIN_FAILURE;
			goto err;
		}
	}
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		goto err;
	ret = __rep_log_backup(dbenv, rep, logc, &lsn);
	if ((t_ret = __logc_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	if (ret == DB_NOTFOUND)
		goto notfound;
	else if (ret != 0)
		goto err;

	/*
	 * Finally, we have a record to ask for.
	 */
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	lp->verify_lsn = lsn;
	lp->rcvd_recs = 0;
	lp->wait_recs = rep->request_gap;
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	if (!F_ISSET(rep, REP_F_DELAY))
		(void)__rep_send_message(dbenv,
		    eid, REP_VERIFY_REQ, &lsn, NULL, 0, DB_REP_ANYWHERE);

	(void)__memp_set_config(dbenv, DB_MEMP_SYNC_INTERRUPT, 0);
	return (DB_REP_NEWMASTER);

err:	/*
	 * If we failed, we need to clear the flags we may have set above
	 * because we're not going to be setting the verify_lsn.
	 */
	REP_SYSTEM_LOCK(dbenv);
errlck:	if (lockout)
		F_CLR(rep, REP_F_READY_MSG);
	F_CLR(rep, REP_F_RECOVER_MASK | REP_F_DELAY);
lckout:	REP_SYSTEM_UNLOCK(dbenv);
	return (ret);

notfound:
	/*
	 * If we don't have an identification record, we still
	 * might have some log records but we're discarding them
	 * to sync up with the master from the start.
	 * Therefore, truncate our log and treat it as if it
	 * were empty.  In-memory logs can't be completely
	 * zeroed using __log_vtruncate, so just zero them out.
	 */
	if (lp->db_log_inmemory)
		ZERO_LSN(lsn);
	else
		INIT_LSN(lsn);
	RPRINT(dbenv, (dbenv, "No commit or ckp found.  Truncate log."));
	ret = lp->db_log_inmemory ?
	    __log_zero(dbenv, &lsn) :
	    __log_vtruncate(dbenv, &lsn, &lsn, NULL);
	if (ret != 0 && ret != DB_NOTFOUND)
		return (ret);
	infop = dbenv->reginfo;
	renv = infop->primary;
	REP_SYSTEM_LOCK(dbenv);
	(void)time(&renv->rep_timestamp);
	REP_SYSTEM_UNLOCK(dbenv);
	if ((ret = __rep_newmaster_empty(dbenv, eid)) != 0)
		goto err;
	return (DB_REP_NEWMASTER);
}

/*
 * __rep_newmaster_empty
 *      Handle the case of a NEWMASTER message received when we have an empty
 * log.  This requires internal init.  If we can't do that because of
 * NOAUTOINIT, return JOIN_FAILURE.  If F_DELAY is in effect, don't even
 * consider NOAUTOINIT yet, because they could change it before rep_sync call.
 */
static int
__rep_newmaster_empty(dbenv, eid)
	DB_ENV *dbenv;
	int eid;
{
	DB_REP *db_rep;
	REP *rep;
	LOG *lp;
	int msg, ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	lp = dbenv->lg_handle->reginfo.primary;
	msg = ret = 0;

	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	REP_SYSTEM_LOCK(dbenv);
	lp->wait_recs = rep->request_gap;

	/* Usual case is to skip to UPDATE state; we may revise this below. */
	F_CLR(rep, REP_F_RECOVER_VERIFY);
	F_SET(rep, REP_F_RECOVER_UPDATE);

	if (F_ISSET(rep, REP_F_DELAY)) {
		/*
		 * Having properly set up wait_recs for later, nothing more to
		 * do now.
		 */
	} else if (FLD_ISSET(rep->config, REP_C_NOAUTOINIT)) {
		F_CLR(rep, REP_F_NOARCHIVE | REP_F_RECOVER_MASK);
		ret = DB_REP_JOIN_FAILURE;
	} else {
		/* Normal case: neither DELAY nor NOAUTOINIT. */
		msg = 1;
	}
	REP_SYSTEM_UNLOCK(dbenv);
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);

	if (msg)
		(void)__rep_send_message(dbenv, eid, REP_UPDATE_REQ,
		    NULL, NULL, 0, 0);
	return (ret);
}

/*
 * __rep_noarchive
 *	Used by log_archive to determine if it is okay to remove
 * log files.
 *
 * PUBLIC: int __rep_noarchive __P((DB_ENV *));
 */
int
__rep_noarchive(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	time_t timestamp;

	infop = dbenv->reginfo;
	renv = infop->primary;

	/*
	 * This is tested before REP_ON below because we always need
	 * to obey if any replication process has disabled archiving.
	 * Everything is in the environment region that we need here.
	 */
	if (F_ISSET(renv, DB_REGENV_REPLOCKED)) {
		(void)time(&timestamp);
		TIMESTAMP_CHECK(dbenv, timestamp, renv);
		/*
		 * Check if we're still locked out after checking
		 * the timestamp.
		 */
		if (F_ISSET(renv, DB_REGENV_REPLOCKED))
			return (EINVAL);
	}

	if (!REP_ON(dbenv))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	return (F_ISSET(rep, REP_F_NOARCHIVE) ? 1 : 0);
}

/*
 * __rep_send_vote
 *	Send this site's vote for the election.
 *
 * PUBLIC: void __rep_send_vote __P((DB_ENV *, DB_LSN *, int, int, int,
 * PUBLIC:    u_int32_t, u_int32_t, int, u_int32_t, u_int32_t));
 */
void
__rep_send_vote(dbenv, lsnp, nsites, nvotes, pri, tie, egen, eid, vtype, flags)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	int eid, nsites, nvotes, pri;
	u_int32_t flags, egen, tie, vtype;
{
	DB_REP *db_rep;
	DBT vote_dbt;
	REP *rep;
	REP_OLD_VOTE_INFO ovi;
	REP_VOTE_INFO vi;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	memset(&ovi, 0, sizeof(ovi));
	memset(&vi, 0, sizeof(vi));
	memset(&vote_dbt, 0, sizeof(vote_dbt));

	if (rep->version == DB_REPVERSION_42) {
		ovi.egen = egen;
		ovi.priority = pri;
		ovi.nsites = nsites;
		ovi.tiebreaker = tie;
		vote_dbt.data = &ovi;
		vote_dbt.size = sizeof(ovi);
	} else {
		vi.egen = egen;
		vi.priority = pri;
		vi.nsites = nsites;
		vi.nvotes = nvotes;
		vi.tiebreaker = tie;
		vote_dbt.data = &vi;
		vote_dbt.size = sizeof(vi);
	}

	(void)__rep_send_message(dbenv, eid, vtype, lsnp, &vote_dbt, flags, 0);
}

/*
 * __rep_elect_done
 *	Clear all election information for this site.  Assumes the
 *	caller hold the region mutex.
 *
 * PUBLIC: void __rep_elect_done __P((DB_ENV *, REP *));
 */
void
__rep_elect_done(dbenv, rep)
	DB_ENV *dbenv;
	REP *rep;
{
	int inelect;
	db_timespec endtime;

	inelect = IN_ELECTION(rep);
	F_CLR(rep,
	    REP_F_EPHASE0 | REP_F_EPHASE1 | REP_F_EPHASE2 | REP_F_TALLY);
	rep->sites = 0;
	rep->votes = 0;
	if (inelect) {
		if (timespecisset(&rep->etime)) {
			__os_gettime(dbenv, &endtime);
			timespecsub(&endtime, &rep->etime);
#ifdef HAVE_STATISTICS
			rep->stat.st_election_sec = (u_int32_t)endtime.tv_sec;
			rep->stat.st_election_usec = (u_int32_t)
			    (endtime.tv_nsec / NS_PER_US);
#endif
			RPRINT(dbenv, (dbenv,
			    "Election finished in %lu.%09lu sec",
			    (u_long)endtime.tv_sec, (u_long)endtime.tv_nsec));
			timespecclear(&rep->etime);
		}
		rep->egen++;
	}
	RPRINT(dbenv, (dbenv, "Election done; egen %lu", (u_long)rep->egen));
}

/*
 * __rep_grow_sites --
 *	Called to allocate more space in the election tally information.
 * Called with the rep mutex held.  We need to call the region mutex, so
 * we need to make sure that we *never* acquire those mutexes in the
 * opposite order.
 *
 * PUBLIC: int __rep_grow_sites __P((DB_ENV *, int));
 */
int
__rep_grow_sites(dbenv, nsites)
	DB_ENV *dbenv;
	int nsites;
{
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	int nalloc, ret, *tally;

	rep = dbenv->rep_handle->region;

	/*
	 * Allocate either twice the current allocation or nsites,
	 * whichever is more.
	 */
	nalloc = 2 * rep->asites;
	if (nalloc < nsites)
		nalloc = nsites;

	infop = dbenv->reginfo;
	renv = infop->primary;
	MUTEX_LOCK(dbenv, renv->mtx_regenv);

	/*
	 * We allocate 2 tally regions, one for tallying VOTE1's and
	 * one for VOTE2's.  Always grow them in tandem, because if we
	 * get more VOTE1's we'll always expect more VOTE2's then too.
	 */
	if ((ret = __env_alloc(infop,
	    (size_t)nalloc * sizeof(REP_VTALLY), &tally)) == 0) {
		if (rep->tally_off != INVALID_ROFF)
			 __env_alloc_free(
			     infop, R_ADDR(infop, rep->tally_off));
		rep->tally_off = R_OFFSET(infop, tally);
		if ((ret = __env_alloc(infop,
		    (size_t)nalloc * sizeof(REP_VTALLY), &tally)) == 0) {
			/* Success */
			if (rep->v2tally_off != INVALID_ROFF)
				 __env_alloc_free(infop,
				    R_ADDR(infop, rep->v2tally_off));
			rep->v2tally_off = R_OFFSET(infop, tally);
			rep->asites = nalloc;
			rep->nsites = nsites;
		} else {
			/*
			 * We were unable to allocate both.  So, we must
			 * free the first one and reinitialize.  If
			 * v2tally_off is valid, it is from an old
			 * allocation and we are clearing it all out due
			 * to the error.
			 */
			if (rep->v2tally_off != INVALID_ROFF)
				 __env_alloc_free(infop,
				    R_ADDR(infop, rep->v2tally_off));
			__env_alloc_free(infop,
			    R_ADDR(infop, rep->tally_off));
			rep->v2tally_off = rep->tally_off = INVALID_ROFF;
			rep->asites = 0;
			rep->nsites = 0;
		}
	}
	MUTEX_UNLOCK(dbenv, renv->mtx_regenv);
	return (ret);
}

/*
 * __env_rep_enter --
 *
 * Check if we are in the middle of replication initialization and/or
 * recovery, and if so, disallow operations.  If operations are allowed,
 * increment handle-counts, so that we do not start recovery while we
 * are operating in the library.
 *
 * PUBLIC: int __env_rep_enter __P((DB_ENV *, int));
 */
int
__env_rep_enter(dbenv, checklock)
	DB_ENV *dbenv;
	int checklock;
{
	DB_REP *db_rep;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	int cnt;
	time_t	timestamp;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	infop = dbenv->reginfo;
	renv = infop->primary;
	if (checklock && F_ISSET(renv, DB_REGENV_REPLOCKED)) {
		(void)time(&timestamp);
		TIMESTAMP_CHECK(dbenv, timestamp, renv);
		/*
		 * Check if we're still locked out after checking
		 * the timestamp.
		 */
		if (F_ISSET(renv, DB_REGENV_REPLOCKED))
			return (EINVAL);
	}

	REP_SYSTEM_LOCK(dbenv);
	for (cnt = 0; F_ISSET(rep, REP_F_READY_API);) {
		REP_SYSTEM_UNLOCK(dbenv);
		if (FLD_ISSET(rep->config, REP_C_NOWAIT)) {
			__db_errx(dbenv,
    "Operation locked out.  Waiting for replication lockout to complete");
			return (DB_REP_LOCKOUT);
		}
		__os_sleep(dbenv, 1, 0);
		REP_SYSTEM_LOCK(dbenv);
		if (++cnt % 60 == 0)
			__db_errx(dbenv,
    "DB_ENV handle waiting %d minutes for replication lockout to complete",
			    cnt / 60);
	}
	rep->handle_cnt++;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __env_db_rep_exit --
 *
 *	Decrement handle count upon routine exit.
 *
 * PUBLIC: int __env_db_rep_exit __P((DB_ENV *));
 */
int
__env_db_rep_exit(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	rep->handle_cnt--;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __db_rep_enter --
 *	Called in replicated environments to keep track of in-use handles
 * and prevent any concurrent operation during recovery.  If checkgen is
 * non-zero, then we verify that the dbp has the same handle as the env.
 *
 * If return_now is non-zero, we'll return DB_DEADLOCK immediately, else we'll
 * sleep before returning DB_DEADLOCK.  Without the sleep, it is likely
 * the application will immediately try again and could reach a retry
 * limit before replication has a chance to finish.  The sleep increases
 * the probability that an application retry will succeed.
 *
 * PUBLIC: int __db_rep_enter __P((DB *, int, int, int));
 */
int
__db_rep_enter(dbp, checkgen, checklock, return_now)
	DB *dbp;
	int checkgen, checklock, return_now;
{
	DB_ENV *dbenv;
	DB_REP *db_rep;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	time_t	timestamp;

	dbenv = dbp->dbenv;
	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	infop = dbenv->reginfo;
	renv = infop->primary;

	if (checklock && F_ISSET(renv, DB_REGENV_REPLOCKED)) {
		(void)time(&timestamp);
		TIMESTAMP_CHECK(dbenv, timestamp, renv);
		/*
		 * Check if we're still locked out after checking
		 * the timestamp.
		 */
		if (F_ISSET(renv, DB_REGENV_REPLOCKED))
			return (EINVAL);
	}
	REP_SYSTEM_LOCK(dbenv);
	if (F_ISSET(rep, REP_F_READY_OP)) {
		REP_SYSTEM_UNLOCK(dbenv);
		if (!return_now)
			__os_sleep(dbenv, 5, 0);
		return (DB_LOCK_DEADLOCK);
	}

	if (checkgen && dbp->timestamp != renv->rep_timestamp) {
		REP_SYSTEM_UNLOCK(dbenv);
		__db_errx(dbenv, "%s %s",
		    "replication recovery unrolled committed transactions;",
		    "open DB and DBcursor handles must be closed");
		return (DB_REP_HANDLE_DEAD);
	}
	rep->handle_cnt++;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __op_rep_enter --
 *
 *	Check if we are in the middle of replication initialization and/or
 * recovery, and if so, disallow new multi-step operations, such as
 * transaction and memp gets.  If operations are allowed,
 * increment the op_cnt, so that we do not start recovery while we have
 * active operations.
 *
 * PUBLIC: int __op_rep_enter __P((DB_ENV *));
 */
int
__op_rep_enter(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;
	int cnt;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	for (cnt = 0; F_ISSET(rep, REP_F_READY_OP);) {
		REP_SYSTEM_UNLOCK(dbenv);
		if (FLD_ISSET(rep->config, REP_C_NOWAIT)) {
			__db_errx(dbenv,
    "Operation locked out.  Waiting for replication lockout to complete");
			return (DB_REP_LOCKOUT);
		}
		__os_sleep(dbenv, 5, 0);
		cnt += 5;
		REP_SYSTEM_LOCK(dbenv);
		if (cnt % 60 == 0)
			__db_errx(dbenv,
	"__op_rep_enter waiting %d minutes for lockout to complete",
			    cnt / 60);
	}
	rep->op_cnt++;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __op_rep_exit --
 *
 *	Decrement op count upon transaction commit/abort/discard or
 *	memp_fput.
 *
 * PUBLIC: int __op_rep_exit __P((DB_ENV *));
 */
int
__op_rep_exit(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	DB_ASSERT(dbenv, rep->op_cnt > 0);
	rep->op_cnt--;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __rep_lockout_api --
 *	Coordinate with other threads in the library and active txns so
 *	that we can run single-threaded, for recovery or internal backup.
 *	Assumes the caller holds the region mutex.
 *
 * PUBLIC: int __rep_lockout_api __P((DB_ENV *, REP *));
 */
int
__rep_lockout_api(dbenv, rep)
	DB_ENV *dbenv;
	REP *rep;
{
	int ret;

	if ((ret = __rep_lockout_int(dbenv, rep, &rep->op_cnt, 0,
	    "op_cnt", REP_F_READY_OP)) != 0)
		return (ret);
	return (__rep_lockout_int(dbenv, rep, &rep->handle_cnt, 0,
	    "handle_cnt", REP_F_READY_API));
}

/*
 * __rep_lockout_apply --
 *	Coordinate with other threads processing messages so that
 *	we can run single-threaded and know that no incoming
 *	message can apply new log records.
 *	This call should be short-term covering a specific critical
 *	operation where we need to make sure no new records change
 *	the log.  Currently used to coordinate with elections.
 *	Assumes the caller holds the region mutex.
 *
 * PUBLIC: int __rep_lockout_apply __P((DB_ENV *, REP *, u_int32_t));
 */
int
__rep_lockout_apply(dbenv, rep, apply_th)
	DB_ENV *dbenv;
	REP *rep;
	u_int32_t apply_th;
{
	return (__rep_lockout_int(dbenv, rep, &rep->apply_th, apply_th,
	    "apply_th", REP_F_READY_APPLY));
}

/*
 * __rep_lockout_msg --
 *	Coordinate with other threads processing messages so that
 *	we can run single-threaded and know that no incoming
 *	message can change the world (i.e., like a NEWMASTER message).
 *	This call should be short-term covering a specific critical
 *	operation where we need to make sure no new messages arrive
 *	in the middle and all message threads are out before we start it.
 *	Assumes the caller holds the region mutex.
 *
 * PUBLIC: int __rep_lockout_msg __P((DB_ENV *, REP *, u_int32_t));
 */
int
__rep_lockout_msg(dbenv, rep, msg_th)
	DB_ENV *dbenv;
	REP *rep;
	u_int32_t msg_th;
{
	return (__rep_lockout_int(dbenv, rep, &rep->msg_th, msg_th,
	    "msg_th", REP_F_READY_MSG));
}

/*
 * __rep_lockout_int --
 *	Internal common code for locking out and coordinating
 *	with other areas of the code.
 *	Assumes the caller holds the region mutex.
 *
 */
static int
__rep_lockout_int(dbenv, rep, fieldp, field_val, msg, lockout_flag)
	DB_ENV *dbenv;
	REP *rep;
	u_int32_t *fieldp;
	const char *msg;
	u_int32_t field_val, lockout_flag;
{
	int wait_cnt;

	F_SET(rep, lockout_flag);
	for (wait_cnt = 0; *fieldp > field_val;) {
		REP_SYSTEM_UNLOCK(dbenv);
		__os_sleep(dbenv, 1, 0);
#ifdef DIAGNOSTIC
		if (wait_cnt == 5)
			__db_errx(dbenv,
"Waiting for %s (%lu) to complete replication lockout",
			msg, (u_long)*fieldp);
		if (++wait_cnt % 60 == 0)
			__db_errx(dbenv,
"Waiting for %s (%lu) to complete replication lockout for %d minutes",
			msg, (u_long)*fieldp, wait_cnt / 60);
#endif
		REP_SYSTEM_LOCK(dbenv);
	}

	COMPQUIET(msg, NULL);
	return (0);
}

/*
 * __rep_send_throttle -
 *	Send a record, throttling if necessary.  Callers of this function
 * will throttle - breaking out of their loop, if the repth->type field
 * changes from the normal message type to the *_MORE message type.
 * This function will send the normal type unless throttling gets invoked.
 * Then it sets the type field and sends the _MORE message.
 *
 * Throttling is always only relevant in serving requests, so we always send
 * with REPCTL_RESEND.  Additional desired flags can be passed in the ctlflags
 * argument.
 *
 * PUBLIC: int __rep_send_throttle __P((DB_ENV *, int, REP_THROTTLE *,
 * PUBLIC:    u_int32_t, u_int32_t));
 */
int
__rep_send_throttle(dbenv, eid, repth, flags, ctlflags)
	DB_ENV *dbenv;
	int eid;
	REP_THROTTLE *repth;
	u_int32_t ctlflags, flags;
{
	DB_REP *db_rep;
	REP *rep;
	u_int32_t size, typemore;
	int check_limit;

	check_limit = repth->gbytes != 0 || repth->bytes != 0;
	/*
	 * If we only want to do throttle processing and we don't have it
	 * turned on, return immediately.
	 */
	if (!check_limit && LF_ISSET(REP_THROTTLE_ONLY))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	typemore = 0;
	if (repth->type == REP_LOG)
		typemore = REP_LOG_MORE;
	if (repth->type == REP_PAGE)
		typemore = REP_PAGE_MORE;
	DB_ASSERT(dbenv, typemore != 0);

	/*
	 * data_dbt.size is only the size of the log
	 * record;  it doesn't count the size of the
	 * control structure. Factor that in as well
	 * so we're not off by a lot if our log records
	 * are small.
	 */
	size = repth->data_dbt->size + sizeof(REP_CONTROL);
	if (check_limit) {
		while (repth->bytes <= size) {
			if (repth->gbytes > 0) {
				repth->bytes += GIGABYTE;
				--(repth->gbytes);
				continue;
			}
			/*
			 * We don't hold the rep mutex,
			 * and may miscount.
			 */
			STAT(rep->stat.st_nthrottles++);
			repth->type = typemore;
			goto send;
		}
		repth->bytes -= size;
	}
	/*
	 * Always send if it is typemore, otherwise send only if
	 * REP_THROTTLE_ONLY is not set.
	 */
send:	if ((repth->type == typemore || !LF_ISSET(REP_THROTTLE_ONLY)) &&
	    (__rep_send_message(dbenv, eid, repth->type,
	    &repth->lsn, repth->data_dbt, (REPCTL_RESEND | ctlflags), 0) != 0))
		return (DB_REP_UNAVAIL);
	return (0);
}

/*
 * __rep_msg_to_old --
 *	Convert current message numbers to old message numbers.
 *
 * PUBLIC: u_int32_t __rep_msg_to_old __P((u_int32_t, u_int32_t));
 */
u_int32_t
__rep_msg_to_old(version, rectype)
	u_int32_t version, rectype;
{
	/*
	 * We need to convert from current message numbers to old numbers and
	 * we need to convert from old numbers to current numbers.  Offset by
	 * one for more readable code.
	 */
	/*
	 * Everything for version 0 is invalid, there is no version 0.
	 */
	static const u_int32_t table[DB_REPVERSION][REP_MAX_MSG+1] = {
	/* There is no DB_REPVERSION 0. */
	{   REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID },
	/*
	 * From 4.6 message number To 4.2 message number
	 */
	{   REP_INVALID,	/* NO message 0 */
	    1,			/* REP_ALIVE */
	    2,			/* REP_ALIVE_REQ */
	    3,			/* REP_ALL_REQ */
	    REP_INVALID,	/* REP_BULK_LOG */
	    REP_INVALID,	/* REP_BULK_PAGE */
	    4,			/* REP_DUPMASTER */
	    5,			/* REP_FILE */
	    REP_INVALID,	/* REP_FILE_FAIL */
	    6,			/* REP_FILE_REQ */
	    REP_INVALID,	/* REP_LEASE_GRANT */
	    7,			/* REP_LOG */
	    8,			/* REP_LOG_MORE */
	    9,			/* REP_LOG_REQ */
	    10,			/* REP_MASTER_REQ */
	    11,			/* REP_NEWCLIENT */
	    12,			/* REP_NEWFILE */
	    13,			/* REP_NEWMASTER */
	    14,			/* REP_NEWSITE */
	    15,			/* REP_PAGE */
	    REP_INVALID,	/* REP_PAGE_FAIL */
	    REP_INVALID,	/* REP_PAGE_MORE */
	    16,			/* REP_PAGE_REQ */
	    REP_INVALID,	/* REP_REREQUEST */
	    REP_INVALID,	/* REP_START_SYNC */
	    REP_INVALID,	/* REP_UPDATE */
	    REP_INVALID,	/* REP_UPDATE_REQ */
	    19,			/* REP_VERIFY */
	    20,			/* REP_VERIFY_FAIL */
	    21,			/* REP_VERIFY_REQ */
	    22,			/* REP_VOTE1 */
	    23			/* REP_VOTE2 */
	},
	/*
	 * From 4.6 message number To 4.3 message number
	 */
	{   REP_INVALID,	/* NO message 0 */
	    1,			/* REP_ALIVE */
	    2,			/* REP_ALIVE_REQ */
	    3,			/* REP_ALL_REQ */
	    REP_INVALID,	/* REP_BULK_LOG */
	    REP_INVALID,	/* REP_BULK_PAGE */
	    4,			/* REP_DUPMASTER */
	    5,			/* REP_FILE */
	    6,			/* REP_FILE_FAIL */
	    7,			/* REP_FILE_REQ */
	    REP_INVALID,	/* REP_LEASE_GRANT */
	    8,			/* REP_LOG */
	    9,			/* REP_LOG_MORE */
	    10,			/* REP_LOG_REQ */
	    11,			/* REP_MASTER_REQ */
	    12,			/* REP_NEWCLIENT */
	    13,			/* REP_NEWFILE */
	    14,			/* REP_NEWMASTER */
	    15,			/* REP_NEWSITE */
	    16,			/* REP_PAGE */
	    17,			/* REP_PAGE_FAIL */
	    18,			/* REP_PAGE_MORE */
	    19,			/* REP_PAGE_REQ */
	    REP_INVALID,	/* REP_REREQUEST */
	    REP_INVALID,	/* REP_START_SYNC */
	    20,			/* REP_UPDATE */
	    21,			/* REP_UPDATE_REQ */
	    22,			/* REP_VERIFY */
	    23,			/* REP_VERIFY_FAIL */
	    24,			/* REP_VERIFY_REQ */
	    25,			/* REP_VOTE1 */
	    26			/* REP_VOTE2 */
	},
	/*
	 * From 4.6 message number To 4.4/4.5 message number
	 */
	{   REP_INVALID,	/* NO message 0 */
	    1,			/* REP_ALIVE */
	    2,			/* REP_ALIVE_REQ */
	    3,			/* REP_ALL_REQ */
	    4,			/* REP_BULK_LOG */
	    5,			/* REP_BULK_PAGE */
	    6,			/* REP_DUPMASTER */
	    7,			/* REP_FILE */
	    8,			/* REP_FILE_FAIL */
	    9,			/* REP_FILE_REQ */
	    REP_INVALID,	/* REP_LEASE_GRANT */
	    10,			/* REP_LOG */
	    11,			/* REP_LOG_MORE */
	    12,			/* REP_LOG_REQ */
	    13,			/* REP_MASTER_REQ */
	    14,			/* REP_NEWCLIENT */
	    15,			/* REP_NEWFILE */
	    16,			/* REP_NEWMASTER */
	    17,			/* REP_NEWSITE */
	    18,			/* REP_PAGE */
	    19,			/* REP_PAGE_FAIL */
	    20,			/* REP_PAGE_MORE */
	    21,			/* REP_PAGE_REQ */
	    22,			/* REP_REREQUEST */
	    REP_INVALID,	/* REP_START_SYNC */
	    23,			/* REP_UPDATE */
	    24,			/* REP_UPDATE_REQ */
	    25,			/* REP_VERIFY */
	    26,			/* REP_VERIFY_FAIL */
	    27,			/* REP_VERIFY_REQ */
	    28,			/* REP_VOTE1 */
	    29			/* REP_VOTE2 */
	}
	};
	return (table[version][rectype]);
}

/*
 * __rep_msg_from_old --
 *	Convert old message numbers to current message numbers.
 *
 * PUBLIC: u_int32_t __rep_msg_from_old __P((u_int32_t, u_int32_t));
 */
u_int32_t
__rep_msg_from_old(version, rectype)
	u_int32_t version, rectype;
{
	/*
	 * We need to convert from current message numbers to old numbers and
	 * we need to convert from old numbers to current numbers.  Offset by
	 * one for more readable code.
	 */
	/*
	 * Everything for version 0 is invalid, there is no version 0.
	 */
	const u_int32_t table[DB_REPVERSION][REP_MAX_MSG+1] = {
	/* There is no DB_REPVERSION 0. */
	{   REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID,
	    REP_INVALID, REP_INVALID, REP_INVALID, REP_INVALID },
	/*
	 * From 4.2 message number To 4.6 message number
	 */
	{   REP_INVALID,	/* NO message 0 */
	    1,			/* REP_ALIVE */
	    2,			/* REP_ALIVE_REQ */
	    3,			/* REP_ALL_REQ */
	    /* 4, REP_BULK_LOG doesn't exist */
	    /* 5, REP_BULK_PAGE doesn't exist */
	    6,			/* 4, REP_DUPMASTER */
	    7,			/* 5, REP_FILE */
	    /* 8, REP_FILE_FAIL doesn't exist */
	    9,			/* 6, REP_FILE_REQ */
	    /* 10, REP_LEASE_GRANT doesn't exist */
	    11,			/* 7, REP_LOG */
	    12,			/* 8, REP_LOG_MORE */
	    13,			/* 9, REP_LOG_REQ */
	    14,			/* 10, REP_MASTER_REQ */
	    15,			/* 11, REP_NEWCLIENT */
	    16,			/* 12, REP_NEWFILE */
	    17,			/* 13, REP_NEWMASTER */
	    18,			/* 14, REP_NEWSITE */
	    19,			/* 15, REP_PAGE */
	    /* 20, REP_PAGE_FAIL doesn't exist */
	    /* 21, REP_PAGE_MORE doesn't exist */
	    22,			/* 16, REP_PAGE_REQ */
	    REP_INVALID,	/* 17, REP_PLIST (UNUSED) */
	    REP_INVALID,	/* 18, REP_PLIST_REQ (UNUSED) */
	    /* 23, REP_REREQUEST doesn't exist */
	    /* 24, REP_START_SYNC doesn't exist */
	    /* 25, REP_UPDATE doesn't exist */
	    /* 26, REP_UPDATE_REQ doesn't exist */
	    27,			/* 19, REP_VERIFY */
	    28,			/* 20, REP_VERIFY_FAIL */
	    29,			/* 21, REP_VERIFY_REQ */
	    30,			/* 22, REP_VOTE1 */
	    31,			/* 23, REP_VOTE2 */
	    REP_INVALID,	/* 24, 4.2 no message */
	    REP_INVALID,	/* 25, 4.2 no message */
	    REP_INVALID,	/* 26, 4.2 no message */
	    REP_INVALID,	/* 27, 4.2 no message */
	    REP_INVALID,	/* 28, 4.2 no message */
	    REP_INVALID,	/* 29, 4.2 no message */
	    REP_INVALID,	/* 30, 4.2 no message */
	    REP_INVALID		/* 31, 4.2 no message */
	},
	/*
	 * From 4.3 message number To 4.6 message number
	 */
	{   REP_INVALID,	/* NO message 0 */
	    1,			/* 1, REP_ALIVE */
	    2,			/* 2, REP_ALIVE_REQ */
	    3,			/* 3, REP_ALL_REQ */
	    /* 4, REP_BULK_LOG doesn't exist */
	    /* 5, REP_BULK_PAGE doesn't exist */
	    6,			/* 4, REP_DUPMASTER */
	    7,			/* 5, REP_FILE */
	    8,			/* 6, REP_FILE_FAIL */
	    9,			/* 7, REP_FILE_REQ */
	    /* 10, REP_LEASE_GRANT doesn't exist */
	    11,			/* 8, REP_LOG */
	    12,			/* 9, REP_LOG_MORE */
	    13,			/* 10, REP_LOG_REQ */
	    14,			/* 11, REP_MASTER_REQ */
	    15,			/* 12, REP_NEWCLIENT */
	    16,			/* 13, REP_NEWFILE */
	    17,			/* 14, REP_NEWMASTER */
	    18,			/* 15, REP_NEWSITE */
	    19,			/* 16, REP_PAGE */
	    20,			/* 17, REP_PAGE_FAIL */
	    21,			/* 18, REP_PAGE_MORE */
	    22,			/* 19, REP_PAGE_REQ */
	    /* 23, REP_REREQUEST doesn't exist */
	    /* 24, REP_START_SYNC doesn't exist */
	    25,			/* 20, REP_UPDATE */
	    26,			/* 21, REP_UPDATE_REQ */
	    27,			/* 22, REP_VERIFY */
	    28,			/* 23, REP_VERIFY_FAIL */
	    29,			/* 24, REP_VERIFY_REQ */
	    30,			/* 25, REP_VOTE1 */
	    31,			/* 26, REP_VOTE2 */
	    REP_INVALID,	/* 27, 4.3 no message */
	    REP_INVALID,	/* 28, 4.3 no message */
	    REP_INVALID,	/* 29, 4.3 no message */
	    REP_INVALID,	/* 30, 4.3 no message */
	    REP_INVALID		/* 31, 4.3 no message */
	},
	/*
	 * From 4.4/4.5 message number To 4.6 message number
	 */
	{   REP_INVALID,	/* NO message 0 */
	    1,			/* 1, REP_ALIVE */
	    2,			/* 2, REP_ALIVE_REQ */
	    3,			/* 3, REP_ALL_REQ */
	    4,			/* 4, REP_ALL_REQ */
	    5,			/* 5, REP_ALL_REQ */
	    6,			/* 6, REP_DUPMASTER */
	    7,			/* 7, REP_FILE */
	    8,			/* 8, REP_FILE_FAIL */
	    9,			/* 9, REP_FILE_REQ */
	    /* 10, REP_LEASE_GRANT doesn't exist */
	    11,			/* 10, REP_LOG */
	    12,			/* 11, REP_LOG_MORE */
	    13,			/* 12, REP_LOG_REQ */
	    14,			/* 13, REP_MASTER_REQ */
	    15,			/* 14, REP_NEWCLIENT */
	    16,			/* 15, REP_NEWFILE */
	    17,			/* 16, REP_NEWMASTER */
	    18,			/* 17, REP_NEWSITE */
	    19,			/* 18, REP_PAGE */
	    20,			/* 19, REP_PAGE_FAIL */
	    21,			/* 20, REP_PAGE_MORE */
	    22,			/* 21, REP_PAGE_REQ */
	    23,			/* 22, REP_REREQUEST */
	    /* 24, REP_START_SYNC doesn't exist */
	    25,			/* 23, REP_UPDATE */
	    26,			/* 24, REP_UPDATE_REQ */
	    27,			/* 25, REP_VERIFY */
	    28,			/* 26, REP_VERIFY_FAIL */
	    29,			/* 27, REP_VERIFY_REQ */
	    30,			/* 28, REP_VOTE1 */
	    31,			/* 29, REP_VOTE2 */
	    REP_INVALID,	/* 30, 4.4/4.5 no message */
	    REP_INVALID		/* 31, 4.4/4.5 no message */
	}
	};
	return (table[version][rectype]);
}

/*
 * __rep_print --
 *	Optionally print a verbose message.
 *
 * PUBLIC: void __rep_print __P((DB_ENV *, const char *, ...))
 * PUBLIC:    __attribute__ ((__format__ (__printf__, 2, 3)));
 */
void
#ifdef STDC_HEADERS
__rep_print(DB_ENV *dbenv, const char *fmt, ...)
#else
__rep_print(dbenv, fmt, va_alist)
	DB_ENV *dbenv;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
	DB_MSGBUF mb;
	REP *rep;
	const char *s;

	DB_MSGBUF_INIT(&mb);

	s = NULL;
	if (dbenv->db_errpfx != NULL)
		s = dbenv->db_errpfx;
	else if (REP_ON(dbenv)) {
		rep = dbenv->rep_handle->region;
		if (F_ISSET(rep, REP_F_CLIENT))
			s = "CLIENT";
		else if (F_ISSET(rep, REP_F_MASTER))
			s = "MASTER";
	}
	if (s == NULL)
		s = "REP_UNDEF";
	__db_msgadd(dbenv, &mb, "%s: ", s);

#ifdef STDC_HEADERS
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	__db_msgadd_ap(dbenv, &mb, fmt, ap);
	va_end(ap);

	DB_MSGBUF_FLUSH(dbenv, &mb);
}

/*
 * PUBLIC: void __rep_print_message
 * PUBLIC:     __P((DB_ENV *, int, REP_CONTROL *, char *, u_int32_t));
 */
void
__rep_print_message(dbenv, eid, rp, str, flags)
	DB_ENV *dbenv;
	int eid;
	REP_CONTROL *rp;
	char *str;
	u_int32_t flags;
{
	u_int32_t ctlflags, rectype;
	char ftype[64], *type;

	rectype = rp->rectype;
	ctlflags = rp->flags;
	if (rp->rep_version != DB_REPVERSION)
		rectype = __rep_msg_from_old(rp->rep_version, rectype);
	switch (rectype) {
	case REP_ALIVE:
		type = "alive";
		break;
	case REP_ALIVE_REQ:
		type = "alive_req";
		break;
	case REP_ALL_REQ:
		type = "all_req";
		break;
	case REP_BULK_LOG:
		type = "bulk_log";
		break;
	case REP_BULK_PAGE:
		type = "bulk_page";
		break;
	case REP_DUPMASTER:
		type = "dupmaster";
		break;
	case REP_FILE:
		type = "file";
		break;
	case REP_FILE_FAIL:
		type = "file_fail";
		break;
	case REP_FILE_REQ:
		type = "file_req";
		break;
	case REP_LEASE_GRANT:
		type = "lease_grant";
		break;
	case REP_LOG:
		type = "log";
		break;
	case REP_LOG_MORE:
		type = "log_more";
		break;
	case REP_LOG_REQ:
		type = "log_req";
		break;
	case REP_MASTER_REQ:
		type = "master_req";
		break;
	case REP_NEWCLIENT:
		type = "newclient";
		break;
	case REP_NEWFILE:
		type = "newfile";
		break;
	case REP_NEWMASTER:
		type = "newmaster";
		break;
	case REP_NEWSITE:
		type = "newsite";
		break;
	case REP_PAGE:
		type = "page";
		break;
	case REP_PAGE_FAIL:
		type = "page_fail";
		break;
	case REP_PAGE_MORE:
		type = "page_more";
		break;
	case REP_PAGE_REQ:
		type = "page_req";
		break;
	case REP_REREQUEST:
		type = "rerequest";
		break;
	case REP_START_SYNC:
		type = "start_sync";
		break;
	case REP_UPDATE:
		type = "update";
		break;
	case REP_UPDATE_REQ:
		type = "update_req";
		break;
	case REP_VERIFY:
		type = "verify";
		break;
	case REP_VERIFY_FAIL:
		type = "verify_fail";
		break;
	case REP_VERIFY_REQ:
		type = "verify_req";
		break;
	case REP_VOTE1:
		type = "vote1";
		break;
	case REP_VOTE2:
		type = "vote2";
		break;
	default:
		type = "NOTYPE";
		break;
	}

	/*
	 * !!!
	 * If adding new flags to print out make sure the aggregate
	 * length cannot overflow the buffer.
	 */
	ftype[0] = '\0';
	if (LF_ISSET(DB_REP_ANYWHERE))
		(void)strcat(ftype, " any");		/* 4 */
	if (FLD_ISSET(ctlflags, REPCTL_FLUSH))
		(void)strcat(ftype, " flush");		/* 10 */
	/*
	 * We expect most of the time the messages will indicate
	 * group membership.  Only print if we're not already
	 * part of a group.
	 */
	if (!FLD_ISSET(ctlflags, REPCTL_GROUP_ESTD))
		(void)strcat(ftype, " nogroup");	/* 18 */
	if (FLD_ISSET(ctlflags, REPCTL_LEASE))
		(void)strcat(ftype, " lease");		/* 24 */
	if (LF_ISSET(DB_REP_NOBUFFER))
		(void)strcat(ftype, " nobuf");		/* 30 */
	if (LF_ISSET(DB_REP_PERMANENT))
		(void)strcat(ftype, " perm");		/* 35 */
	if (LF_ISSET(DB_REP_REREQUEST))
		(void)strcat(ftype, " rereq");		/* 41 */
	if (FLD_ISSET(ctlflags, REPCTL_RESEND))
		(void)strcat(ftype, " resend");		/* 48 */
	if (FLD_ISSET(ctlflags, REPCTL_LOG_END))
		(void)strcat(ftype, " logend");		/* 55 */
	RPRINT(dbenv,
	    (dbenv,
    "%s %s: msgv = %lu logv %lu gen = %lu eid %d, type %s, LSN [%lu][%lu] %s",
	    dbenv->db_home, str,
	    (u_long)rp->rep_version, (u_long)rp->log_version, (u_long)rp->gen,
	    eid, type, (u_long)rp->lsn.file, (u_long)rp->lsn.offset, ftype));
}

/*
 * PUBLIC: void __rep_fire_event __P((DB_ENV *, u_int32_t, void *));
 */
void
__rep_fire_event(dbenv, event, info)
	DB_ENV *dbenv;
	u_int32_t event;
	void *info;
{
	int ret;

	/*
	 * Give repmgr first crack at handling all replication-related events.
	 * If it can't (or chooses not to) handle the event fully, then pass it
	 * along to the application.
	 */
	ret = __repmgr_handle_event(dbenv, event, info);
	DB_ASSERT(dbenv, ret == 0 || ret == DB_EVENT_NOT_HANDLED);

	if (ret == DB_EVENT_NOT_HANDLED)
		DB_EVENT(dbenv, event, info);
}
