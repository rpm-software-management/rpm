/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: rep_log.c,v 12.47 2006/09/11 19:41:20 sue Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/log.h"

static int __rep_chk_newfile __P((DB_ENV *, DB_LOGC *, REP *,
    REP_CONTROL *, int));

/*
 * __rep_allreq --
 *      Handle a REP_ALL_REQ message.
 *
 * PUBLIC: int __rep_allreq __P((DB_ENV *, REP_CONTROL *, int));
 */
int
__rep_allreq(dbenv, rp, eid)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	int eid;
{
	DB_LOGC *logc;
	DB_LSN oldfilelsn;
	DB_REP *db_rep;
	DBT data_dbt, newfiledbt;
	REP *rep;
	REP_BULK bulk;
	REP_THROTTLE repth;
	uintptr_t bulkoff;
	u_int32_t bulkflags, flags, use_bulk, version;
	int ret, t_ret;

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);
	memset(&data_dbt, 0, sizeof(data_dbt));
	/*
	 * If we're doing bulk transfer, allocate a bulk buffer to put our
	 * log records in.  We still need to initialize the throttle info
	 * because if we encounter a log record larger than our entire bulk
	 * buffer, we need to send it as a singleton and also we want to
	 * support throttling with bulk.
	 *
	 * Use a local var so we don't need to worry if someone else turns
	 * on/off bulk in the middle of our call.
	 */
	use_bulk = FLD_ISSET(rep->config, REP_C_BULK);
	if (use_bulk && (ret = __rep_bulk_alloc(dbenv, &bulk, eid,
	    &bulkoff, &bulkflags, REP_BULK_LOG)) != 0)
		goto err;
	memset(&repth, 0, sizeof(repth));
	REP_SYSTEM_LOCK(dbenv);
	repth.gbytes = rep->gbytes;
	repth.bytes = rep->bytes;
	oldfilelsn = repth.lsn = rp->lsn;
	repth.type = REP_LOG;
	repth.data_dbt = &data_dbt;
	REP_SYSTEM_UNLOCK(dbenv);
	flags = IS_ZERO_LSN(rp->lsn) ||
	    IS_INIT_LSN(rp->lsn) ?  DB_FIRST : DB_SET;
	/*
	 * We get the first item so that a client servicing requests
	 * can distinguish between not having the records and reaching
	 * the end of its log.  Return the DB_NOTFOUND if the client
	 * cannot get the record.  Return 0 if we finish the loop and
	 * sent all that we have.
	 */
	ret = __log_c_get(logc, &repth.lsn, &data_dbt, flags);
	/*
	 * If the client is asking for all records
	 * because it doesn't have any, and our first
	 * record is not in the first log file, then
	 * the client is outdated and needs to get a
	 * VERIFY_FAIL.
	 */
	if (ret == 0 && repth.lsn.file != 1 && flags == DB_FIRST) {
		(void)__rep_send_message(dbenv, eid,
		    REP_VERIFY_FAIL, &repth.lsn, NULL, 0, 0);
		goto err;
	}
	/*
	 * If we got DB_NOTFOUND it could be because the LSN we were
	 * given is at the end of the log file and we need to switch
	 * log files.  Reinitialize and get the current record when we return.
	 */
	if (ret == DB_NOTFOUND) {
		ret = __rep_chk_newfile(dbenv, logc, rep, rp, eid);
		/*
		 * If we still get DB_NOTFOUND the client gave us a
		 * bad or unknown LSN.  Ignore it if we're the master.
		 * Any other error is returned.
		 */
		if (ret == 0)
			ret = __log_c_get(logc, &repth.lsn,
			    &data_dbt, DB_CURRENT);
		else if (ret == DB_NOTFOUND && F_ISSET(rep, REP_F_MASTER)) {
			ret = 0;
			goto err;
		}
		if (ret != 0)
			goto err;
	}

	/*
	 * For singleton log records, we break when we get a REP_LOG_MORE.
	 * Or if we're not using throttling, or we are using bulk, we stop
	 * when we reach the end (i.e. ret != 0).
	 */
	for (;
	    ret == 0 && repth.type != REP_LOG_MORE;
	    ret = __log_c_get(logc, &repth.lsn, &data_dbt, DB_NEXT)) {
		if (repth.lsn.file != oldfilelsn.file) {
			if ((ret = __log_c_version(logc, &version)) != 0)
				break;
			memset(&newfiledbt, 0, sizeof(newfiledbt));
			newfiledbt.data = &version;
			newfiledbt.size = sizeof(version);
			(void)__rep_send_message(dbenv,
			    eid, REP_NEWFILE, &oldfilelsn, &newfiledbt, 0, 0);
		}
		/*
		 * If we are configured for bulk, try to send this as a bulk
		 * request.  If not configured, or it is too big for bulk
		 * then just send normally.
		 */
		if (use_bulk)
			ret = __rep_bulk_message(dbenv, &bulk, &repth,
			    &repth.lsn, &data_dbt, REPCTL_RESEND);
		if (!use_bulk || ret == DB_REP_BULKOVF)
			ret = __rep_send_throttle(dbenv, eid, &repth, 0);
		if (ret != 0)
			break;
		/*
		 * If we are about to change files, then we'll need the
		 * last LSN in the previous file.  Save it here.
		 */
		oldfilelsn = repth.lsn;
		oldfilelsn.offset += logc->c_len;
	}

	if (ret == DB_NOTFOUND)
		ret = 0;
	/*
	 * We're done, force out whatever remains in the bulk buffer and
	 * free it.
	 */
	if (use_bulk && (t_ret = __rep_bulk_free(dbenv, &bulk,
	    REPCTL_RESEND)) != 0 && ret == 0)
		ret = t_ret;
err:
	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __rep_log --
 *      Handle a REP_LOG/REP_LOG_MORE message.
 *
 * PUBLIC: int __rep_log __P((DB_ENV *, REP_CONTROL *, DBT *,
 * PUBLIC:     time_t, DB_LSN *));
 */
int
__rep_log(dbenv, rp, rec, savetime, ret_lsnp)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	time_t savetime;
	DB_LSN *ret_lsnp;
{
	DB_LOG *dblp;
	DB_LSN last_lsn, lsn;
	DB_REP *db_rep;
	LOG *lp;
	REP *rep;
	int is_dup, master, ret;

	is_dup = ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	ret = __rep_apply(dbenv, rp, rec, ret_lsnp, &is_dup, &last_lsn);
	switch (ret) {
	/*
	 * We're in an internal backup and we've gotten
	 * all the log we need to run recovery.  Do so now.
	 */
	case DB_REP_LOGREADY:
		if ((ret =
		    __rep_logready(dbenv, rep, savetime, &last_lsn)) != 0)
			goto out;
		break;
	/*
	 * If we get any of the "normal" returns, we only process
	 * LOG_MORE if this is not a duplicate record.  If the
	 * record is a duplicate we don't want to handle LOG_MORE
	 * and request a multiple data stream (or trigger internal
	 * initialization) since this could be a very old record
	 * that no longer exists on the master.
	 */
	case DB_REP_ISPERM:
	case DB_REP_NOTPERM:
	case 0:
		if (is_dup)
			goto out;
		else
			break;
	/*
	 * Any other return (errors), we're done.
	 */
	default:
		goto out;
	}
	if (rp->rectype == REP_LOG_MORE) {
		REP_SYSTEM_LOCK(dbenv);
		master = rep->master_id;
		REP_SYSTEM_UNLOCK(dbenv);
		LOG_SYSTEM_LOCK(dbenv);
		lsn = lp->lsn;
		LOG_SYSTEM_UNLOCK(dbenv);
		/*
		 * If the master_id is invalid, this means that since
		 * the last record was sent, somebody declared an
		 * election and we may not have a master to request
		 * things of.
		 *
		 * This is not an error;  when we find a new master,
		 * we'll re-negotiate where the end of the log is and
		 * try to bring ourselves up to date again anyway.
		 *
		 * If we've asked for a bunch of records, it could
		 * either be from a LOG_REQ or ALL_REQ.  If we're
		 * waiting for a gap to be filled, call loggap_req,
		 * otherwise use ALL_REQ again.
		 */
		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
		if (master == DB_EID_INVALID) {
			ret = 0;
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		} else if (IS_ZERO_LSN(lp->waiting_lsn)) {
			/*
			 * We're making an ALL_REQ.  However, since we're
			 * in a LOG_MORE, this is in reply to a request and
			 * it is likely we may receive new records, even if
			 * we don't have any at this moment.  So, to avoid
			 * multiple data streams, set the wait_recs high
			 * now to give the master a chance to start sending
			 * us these records before the gap code re-requests
			 * the same gap.  Wait_recs will get reset once we
			 * start receiving these records.
			 */
			lp->wait_recs = rep->max_gap;
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			if (__rep_send_message(dbenv, master, REP_ALL_REQ,
			    &lsn, NULL, 0, DB_REP_ANYWHERE) != 0)
				goto out;
		} else {
			ret = __rep_loggap_req(dbenv, rep, &lsn, REP_GAP_FORCE);
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		}
	}
out:
	return (ret);
}

/*
 * __rep_bulk_log --
 *      Handle a REP_BULK_LOG message.
 *
 * PUBLIC: int __rep_bulk_log __P((DB_ENV *, REP_CONTROL *, DBT *,
 * PUBLIC:     time_t, DB_LSN *));
 */
int
__rep_bulk_log(dbenv, rp, rec, savetime, ret_lsnp)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	time_t savetime;
	DB_LSN *ret_lsnp;
{
	DB_REP *db_rep;
	REP *rep;
	DB_LSN last_lsn;
	int ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	ret = __log_rep_split(dbenv, rp, rec, ret_lsnp, &last_lsn);
	switch (ret) {
	/*
	 * We're in an internal backup and we've gotten
	 * all the log we need to run recovery.  Do so now.
	 */
	case DB_REP_LOGREADY:
		ret = __rep_logready(dbenv, rep, savetime, &last_lsn);
		break;
	/*
	 * Any other return (errors), we're done.
	 */
	default:
		break;
	}
	return (ret);
}

/*
 * __rep_log_req --
 *      Handle a REP_LOG_REQ message.
 *
 * PUBLIC: int __rep_logreq __P((DB_ENV *, REP_CONTROL *, DBT *, int));
 */
int
__rep_logreq(dbenv, rp, rec, eid)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	int eid;
{
	DB_LOGC *logc;
	DB_LSN lsn, oldfilelsn;
	DB_REP *db_rep;
	DBT data_dbt, newfiledbt;
	REP *rep;
	REP_BULK bulk;
	REP_THROTTLE repth;
	uintptr_t bulkoff;
	u_int32_t bulkflags, use_bulk, version;
	int ret, t_ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	if (rec != NULL && rec->size != 0) {
		RPRINT(dbenv, (dbenv, &mb,
		    "[%lu][%lu]: LOG_REQ max lsn: [%lu][%lu]",
		    (u_long) rp->lsn.file, (u_long)rp->lsn.offset,
		    (u_long)((DB_LSN *)rec->data)->file,
		    (u_long)((DB_LSN *)rec->data)->offset));
	}
	/*
	 * There are three different cases here.
	 * 1. We asked log_c_get for a particular LSN and got it.
	 * 2. We asked log_c_get for an LSN and it's not found because it is
	 *	beyond the end of a log file and we need a NEWFILE msg.
	 *	and then the record that was requested.
	 * 3. We asked log_c_get for an LSN and it simply doesn't exist, but
	 *    doesn't meet any of those other criteria, in which case
	 *    it's an error (that should never happen on a master).
	 *
	 * If we have a valid LSN and the request has a data_dbt with
	 * it, the sender is asking for a chunk of log records.
	 * Then we need to send all records up to the LSN in the data dbt.
	 */
	memset(&data_dbt, 0, sizeof(data_dbt));
	oldfilelsn = lsn = rp->lsn;
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);
	ret = __log_c_get(logc, &lsn, &data_dbt, DB_SET);

	if (ret == 0) /* Case 1 */
		(void)__rep_send_message(dbenv,
		   eid, REP_LOG, &lsn, &data_dbt, REPCTL_RESEND, 0);
	else if (ret == DB_NOTFOUND) {
		ret = __rep_chk_newfile(dbenv, logc, rep, rp, eid);
		if (ret == DB_NOTFOUND) {
			/* Case 3 */
			/*
			 * If we're a master, this is a problem.
			 * If we're a client servicing a request
			 * just return the DB_NOTFOUND.
			 */
			if (F_ISSET(rep, REP_F_MASTER)) {
				__db_errx(dbenv,
				    "Request for LSN [%lu][%lu] fails",
				    (u_long)rp->lsn.file,
				    (u_long)rp->lsn.offset);
				ret = EINVAL;
			} else
				ret = DB_NOTFOUND;
		}
	}

	if (ret != 0)
		goto err;

	/*
	 * If the user requested a gap, send the whole thing, while observing
	 * the limits from rep_set_limit.
	 *
	 * If we're doing bulk transfer, allocate a bulk buffer to put our
	 * log records in.  We still need to initialize the throttle info
	 * because if we encounter a log record larger than our entire bulk
	 * buffer, we need to send it as a singleton.
	 *
	 * Use a local var so we don't need to worry if someone else turns
	 * on/off bulk in the middle of our call.
	 */
	use_bulk = FLD_ISSET(rep->config, REP_C_BULK);
	if (use_bulk && (ret = __rep_bulk_alloc(dbenv, &bulk, eid,
	    &bulkoff, &bulkflags, REP_BULK_LOG)) != 0)
		goto err;
	memset(&repth, 0, sizeof(repth));
	REP_SYSTEM_LOCK(dbenv);
	repth.gbytes = rep->gbytes;
	repth.bytes = rep->bytes;
	repth.type = REP_LOG;
	repth.data_dbt = &data_dbt;
	REP_SYSTEM_UNLOCK(dbenv);
	while (ret == 0 && rec != NULL && rec->size != 0 &&
	    repth.type == REP_LOG) {
		if ((ret =
		    __log_c_get(logc, &repth.lsn, &data_dbt, DB_NEXT)) != 0) {
			/*
			 * If we're a client and we only have part of the gap,
			 * return DB_NOTFOUND so that we send a REREQUEST
			 * back to the requester and it can ask for more.
			 */
			if (ret == DB_NOTFOUND && F_ISSET(rep, REP_F_MASTER))
				ret = 0;
			break;
		}
		if (LOG_COMPARE(&repth.lsn, (DB_LSN *)rec->data) >= 0)
			break;
		if (repth.lsn.file != oldfilelsn.file) {
			if ((ret = __log_c_version(logc, &version)) != 0)
				break;
			memset(&newfiledbt, 0, sizeof(newfiledbt));
			newfiledbt.data = &version;
			newfiledbt.size = sizeof(version);
			(void)__rep_send_message(dbenv,
			    eid, REP_NEWFILE, &oldfilelsn, &newfiledbt, 0, 0);
		}
		/*
		 * If we are configured for bulk, try to send this as a bulk
		 * request.  If not configured, or it is too big for bulk
		 * then just send normally.
		 */
		if (use_bulk)
			ret = __rep_bulk_message(dbenv, &bulk, &repth,
			    &repth.lsn, &data_dbt, REPCTL_RESEND);
		if (!use_bulk || ret == DB_REP_BULKOVF)
			ret = __rep_send_throttle(dbenv, eid, &repth, 0);
		if (ret != 0)
			break;
		/*
		 * If we are about to change files, then we'll need the
		 * last LSN in the previous file.  Save it here.
		 */
		oldfilelsn = repth.lsn;
		oldfilelsn.offset += logc->c_len;
	}

	/*
	 * We're done, force out whatever remains in the bulk buffer and
	 * free it.
	 */
	if (use_bulk && (t_ret = __rep_bulk_free(dbenv, &bulk,
	    REPCTL_RESEND)) != 0 && ret == 0)
		ret = t_ret;
err:
	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __rep_loggap_req -
 *	Request a log gap.  Assumes the caller holds the REP->mtx_clientdb.
 *
 * lsnp is the current LSN we're handling.  It is used to help decide
 *	if we ask for a gap or singleton.
 * gapflags are flags that may override the algorithm or control the
 *	processing in some way.
 *
 * PUBLIC: int __rep_loggap_req __P((DB_ENV *, REP *, DB_LSN *, u_int32_t));
 */
int
__rep_loggap_req(dbenv, rep, lsnp, gapflags)
	DB_ENV *dbenv;
	REP *rep;
	DB_LSN *lsnp;
	u_int32_t gapflags;
{
	DB_LOG *dblp;
	DBT max_lsn_dbt, *max_lsn_dbtp;
	DB_LSN next_lsn;
	LOG *lp;
	u_int32_t ctlflags, flags, type;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	LOG_SYSTEM_LOCK(dbenv);
	next_lsn = lp->lsn;
	LOG_SYSTEM_UNLOCK(dbenv);
	ctlflags = flags = 0;
	type = REP_LOG_REQ;

	/*
	 * Check if we need to ask for the gap.
	 * We ask for the gap if:
	 *	We are forced to with gapflags.
	 *	If max_wait_lsn is ZERO_LSN - we've never asked for
	 *	  records before.
	 *	If we asked for a single record and received it.
	 *
	 * If we want a gap, but don't have an ending LSN (waiting_lsn)
	 * send an ALL_REQ.  This is primarily used by REP_REREQUEST when
	 * an ALL_REQ was not able to be fulfilled by another client.
	 */
	if (FLD_ISSET(gapflags, (REP_GAP_FORCE | REP_GAP_REREQUEST)) ||
	    IS_ZERO_LSN(lp->max_wait_lsn) ||
	    (lsnp != NULL && LOG_COMPARE(lsnp, &lp->max_wait_lsn) == 0)) {
		lp->max_wait_lsn = lp->waiting_lsn;
		if (IS_ZERO_LSN(lp->max_wait_lsn))
			type = REP_ALL_REQ;
		memset(&max_lsn_dbt, 0, sizeof(max_lsn_dbt));
		max_lsn_dbt.data = &lp->waiting_lsn;
		max_lsn_dbt.size = sizeof(lp->waiting_lsn);
		max_lsn_dbtp = &max_lsn_dbt;
		/*
		 * Gap requests are "new" and can go anywhere, unless
		 * this is already a rerequest.
		 */
		if (FLD_ISSET(gapflags, REP_GAP_REREQUEST))
			flags = DB_REP_REREQUEST;
		else
			flags = DB_REP_ANYWHERE;
	} else {
		max_lsn_dbtp = NULL;
		lp->max_wait_lsn = next_lsn;
		/*
		 * If we're dropping to singletons, this is a rerequest.
		 */
		flags = DB_REP_REREQUEST;
	}
	if (rep->master_id != DB_EID_INVALID) {
		rep->stat.st_log_requested++;
		if (F_ISSET(rep, REP_F_RECOVER_LOG))
			ctlflags = REPCTL_INIT;
		(void)__rep_send_message(dbenv, rep->master_id,
		    type, &next_lsn, max_lsn_dbtp, ctlflags, flags);
	} else
		(void)__rep_send_message(dbenv, DB_EID_BROADCAST,
		    REP_MASTER_REQ, NULL, NULL, 0, 0);

	return (0);
}

/*
 * __rep_logready -
 *	Handle getting back REP_LOGREADY.  Any call to __rep_apply
 * can return it.
 *
 * PUBLIC: int __rep_logready __P((DB_ENV *, REP *, time_t, DB_LSN *));
 */
int
__rep_logready(dbenv, rep, savetime, last_lsnp)
	DB_ENV *dbenv;
	REP *rep;
	time_t savetime;
	DB_LSN *last_lsnp;
{
	int ret;

	if ((ret = __log_flush(dbenv, NULL)) != 0)
		goto out;
	if ((ret = __rep_verify_match(dbenv, last_lsnp,
	    savetime)) == 0) {
		REP_SYSTEM_LOCK(dbenv);
		ZERO_LSN(rep->first_lsn);
		F_CLR(rep, REP_F_RECOVER_LOG);
		REP_SYSTEM_UNLOCK(dbenv);
	} else {
out:		__db_errx(dbenv,
	"Client initialization failed.  Need to manually restore client");
		return (__db_panic(dbenv, ret));
	}
	return (ret);

}

/*
 * __rep_chk_newfile --
 *     Determine if getting DB_NOTFOUND is because we're at the
 * end of a log file and need to send a NEWFILE message.
 *
 * This function handles these cases:
 * [Case 1 was that we found the record we were looking for - it
 * is already handled by the caller.]
 * 2. We asked log_c_get for an LSN and it's not found because it is
 *	beyond the end of a log file and we need a NEWFILE msg.
 * 3. We asked log_c_get for an LSN and it simply doesn't exist, but
 *    doesn't meet any of those other criteria, in which case
 *    we return DB_NOTFOUND and the caller decides if it's an error.
 *
 * This function returns 0 if we had to send a message and the bad
 * LSN is dealt with and DB_NOTFOUND if this really is an unknown LSN
 * (on a client) and errors if it isn't found on the master.
 */
static int
__rep_chk_newfile(dbenv, logc, rep, rp, eid)
	DB_ENV *dbenv;
	DB_LOGC *logc;
	REP *rep;
	REP_CONTROL *rp;
	int eid;
{
	DB_LOG *dblp;
	DB_LSN endlsn;
	DBT data_dbt, newfiledbt;
	LOG *lp;
	u_int32_t version;
	int ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	ret = 0;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	memset(&data_dbt, 0, sizeof(data_dbt));
	LOG_SYSTEM_LOCK(dbenv);
	endlsn = lp->lsn;
	LOG_SYSTEM_UNLOCK(dbenv);
	if (endlsn.file > rp->lsn.file) {
		/*
		 * Case 2:
		 * Need to find the LSN of the last record in
		 * file lsn.file so that we can send it with
		 * the NEWFILE call.  In order to do that, we
		 * need to try to get {lsn.file + 1, 0} and
		 * then backup.
		 */
		endlsn.file = rp->lsn.file + 1;
		endlsn.offset = 0;
		if ((ret = __log_c_get(logc,
		    &endlsn, &data_dbt, DB_SET)) != 0 ||
		    (ret = __log_c_get(logc,
			&endlsn, &data_dbt, DB_PREV)) != 0) {
			RPRINT(dbenv, (dbenv, &mb,
			    "Unable to get prev of [%lu][%lu]",
			    (u_long)rp->lsn.file,
			    (u_long)rp->lsn.offset));
			/*
			 * We want to push the error back
			 * to the client so that the client
			 * does an internal backup.  The
			 * client asked for a log record
			 * we no longer have and it is
			 * outdated.
			 * XXX - This could be optimized by
			 * having the master perform and
			 * send a REP_UPDATE message.  We
			 * currently want the client to set
			 * up its 'update' state prior to
			 * requesting REP_UPDATE_REQ.
			 *
			 * If we're a client servicing a request
			 * just return DB_NOTFOUND.
			 */
			if (F_ISSET(rep, REP_F_MASTER)) {
				ret = 0;
				(void)__rep_send_message(dbenv, eid,
				    REP_VERIFY_FAIL, &rp->lsn,
				    NULL, 0, 0);
			} else
				ret = DB_NOTFOUND;
		} else {
			endlsn.offset += logc->c_len;
			if ((ret = __log_c_version(logc,
			    &version)) == 0) {
				memset(&newfiledbt, 0,
				    sizeof(newfiledbt));
				newfiledbt.data = &version;
				newfiledbt.size = sizeof(version);
				(void)__rep_send_message(dbenv, eid,
				    REP_NEWFILE, &endlsn,
				    &newfiledbt, 0, 0);
			}
		}
	} else
		ret = DB_NOTFOUND;

	return (ret);
}
