/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2003
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: rep_record.c,v 1.193 2003/11/14 05:32:31 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static int __rep_apply __P((DB_ENV *, REP_CONTROL *, DBT *, DB_LSN *));
static int __rep_collect_txn __P((DB_ENV *, DB_LSN *, LSN_COLLECTION *));
static int __rep_dorecovery __P((DB_ENV *, DB_LSN *, DB_LSN *));
static int __rep_lsn_cmp __P((const void *, const void *));
static int __rep_newfile __P((DB_ENV *, REP_CONTROL *, DB_LSN *));
static int __rep_verify_match __P((DB_ENV *, REP_CONTROL *, time_t));

#define	IS_SIMPLE(R)	((R) != DB___txn_regop && (R) != DB___txn_xa_regop && \
    (R) != DB___txn_ckp && (R) != DB___dbreg_register)

/* Used to consistently designate which messages ought to be received where. */

#ifdef DIAGNOSTIC
#define	MASTER_ONLY(rep, rp) do {					\
	if (!F_ISSET(rep, REP_F_MASTER)) {				\
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {	\
			__db_err(dbenv, "Master record received on client"); \
			__rep_print_message(dbenv,			\
			    *eidp, rp, "rep_process_message");		\
		}							\
		ret = EINVAL;						\
		goto errlock;						\
	}								\
} while (0)

#define	CLIENT_ONLY(rep, rp) do {					\
	if (!F_ISSET(rep, REP_ISCLIENT)) {				\
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {	\
			__db_err(dbenv, "Client record received on master"); \
			__rep_print_message(dbenv,			\
			    *eidp, rp, "rep_process_message");		\
		}							\
		(void)__rep_send_message(dbenv,				\
		    DB_EID_BROADCAST, REP_DUPMASTER, NULL, NULL, 0);	\
		ret = DB_REP_DUPMASTER;					\
		goto errlock;						\
	}								\
} while (0)

#define	MASTER_CHECK(dbenv, eid, rep)					\
do {									\
	if (rep->master_id == DB_EID_INVALID) {				\
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))	\
			__db_err(dbenv,					\
			    "Received record from %d, master is INVALID",\
			    eid);					\
		ret = 0;						\
		(void)__rep_send_message(dbenv,				\
		    DB_EID_BROADCAST, REP_MASTER_REQ, NULL, NULL, 0);	\
		goto errlock;						\
	}								\
	if (eid != rep->master_id) {					\
		__db_err(dbenv,						\
		   "Received master record from %d, master is %d",	\
		   eid, rep->master_id);				\
		ret = EINVAL;						\
		goto errlock;						\
	}								\
} while (0)
#else
#define	MASTER_ONLY(rep, rp) do {					\
	if (!F_ISSET(rep, REP_F_MASTER)) {				\
		ret = EINVAL;						\
		goto errlock;						\
	}								\
} while (0)

#define	CLIENT_ONLY(rep, rp) do {					\
	if (!F_ISSET(rep, REP_ISCLIENT)) {				\
		(void)__rep_send_message(dbenv,				\
		    DB_EID_BROADCAST, REP_DUPMASTER, NULL, NULL, 0);	\
		ret = DB_REP_DUPMASTER;					\
		goto errlock;						\
	}								\
} while (0)

#define	MASTER_CHECK(dbenv, eid, rep)					\
do {									\
	if (rep->master_id == DB_EID_INVALID) {				\
		ret = 0;						\
		(void)__rep_send_message(dbenv,				\
		    DB_EID_BROADCAST, REP_MASTER_REQ, NULL, NULL, 0);	\
		goto errlock;						\
	}								\
	if (eid != rep->master_id) {					\
		__db_err(dbenv,						\
		   "Received master record from %d, master is %d",	\
		   eid, rep->master_id);				\
		ret = EINVAL;						\
		goto errlock;						\
	}								\
} while (0)
#endif

#define	ANYSITE(rep)

/*
 * __rep_process_message --
 *
 * This routine takes an incoming message and processes it.
 *
 * control: contains the control fields from the record
 * rec: contains the actual record
 * eidp: contains the machine id of the sender of the message;
 *	in the case of a DB_NEWMASTER message, returns the eid
 *	of the new master.
 * ret_lsnp: On DB_REP_ISPERM and DB_REP_NOTPERM returns, contains the
 *	lsn of the maximum permanent or current not permanent log record
 *	(respectively).
 *
 * PUBLIC: int __rep_process_message __P((DB_ENV *, DBT *, DBT *, int *,
 * PUBLIC:     DB_LSN *));
 */
int
__rep_process_message(dbenv, control, rec, eidp, ret_lsnp)
	DB_ENV *dbenv;
	DBT *control, *rec;
	int *eidp;
	DB_LSN *ret_lsnp;
{
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN endlsn, lsn, oldfilelsn;
	DB_REP *db_rep;
	DBT *d, data_dbt, mylog;
	LOG *lp;
	REP *rep;
	REP_CONTROL *rp;
	REP_VOTE_INFO *vi;
	u_int32_t bytes, egen, flags, gen, gbytes, type;
	int check_limit, cmp, done, do_req;
	int master, old, recovering, ret, t_ret;
	time_t savetime;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle, "rep_process_message",
	    DB_INIT_REP);

	/* Control argument must be non-Null. */
	if (control == NULL || control->size == 0) {
		__db_err(dbenv,
	"DB_ENV->rep_process_message: control argument must be specified");
		return (EINVAL);
	}

	if (!IS_REP_MASTER(dbenv) && !IS_REP_CLIENT(dbenv)) {
		__db_err(dbenv,
	"Environment not configured as replication master or client");
		return (EINVAL);
	}

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	rp = (REP_CONTROL *)control->data;

	/*
	 * Acquire the replication lock.
	 */
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (rep->start_th != 0) {
		/*
		 * If we're racing with a thread in rep_start, then
		 * just ignore the message and return.
		 */
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		if (F_ISSET(rp, DB_LOG_PERM)) {
			if (ret_lsnp != NULL)
				*ret_lsnp = rp->lsn;
			return (DB_REP_NOTPERM);
		} else
			return (0);
	}
	if (rep->in_recovery != 0) {
		/*
		 * If we're racing with a thread in __db_apprec,
		 * just ignore the message and return.
		 */
		rep->stat.st_msgs_recover++;
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		return (0);
	}
	rep->msg_th++;
	gen = rep->gen;
	recovering = rep->in_recovery ||
	    F_ISSET(rep, REP_F_READY | REP_F_RECOVER);
	savetime = rep->timestamp;

	rep->stat.st_msgs_processed++;
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

#ifdef DIAGNOSTIC
	if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
		__rep_print_message(dbenv, *eidp, rp, "rep_process_message");
#endif

	/* Complain if we see an improper version number. */
	if (rp->rep_version != DB_REPVERSION) {
		__db_err(dbenv,
		    "unexpected replication message version %lu, expected %d",
		    (u_long)rp->rep_version, DB_REPVERSION);
		ret = EINVAL;
		goto errlock;
	}
	if (rp->log_version != DB_LOGVERSION) {
		__db_err(dbenv,
		    "unexpected log record version %lu, expected %d",
		    (u_long)rp->log_version, DB_LOGVERSION);
		ret = EINVAL;
		goto errlock;
	}

	/*
	 * Check for generation number matching.  Ignore any old messages
	 * except requests that are indicative of a new client that needs
	 * to get in sync.
	 */
	if (rp->gen < gen && rp->rectype != REP_ALIVE_REQ &&
	    rp->rectype != REP_NEWCLIENT && rp->rectype != REP_MASTER_REQ &&
	    rp->rectype != REP_DUPMASTER) {
		/*
		 * We don't hold the rep mutex, and could miscount if we race.
		 */
		rep->stat.st_msgs_badgen++;
		goto errlock;
	}

	if (rp->gen > gen) {
		/*
		 * If I am a master and am out of date with a lower generation
		 * number, I am in bad shape and should downgrade.
		 */
		if (F_ISSET(rep, REP_F_MASTER)) {
			rep->stat.st_dupmasters++;
			ret = DB_REP_DUPMASTER;
			if (rp->rectype != REP_DUPMASTER)
				(void)__rep_send_message(dbenv,
				    DB_EID_BROADCAST, REP_DUPMASTER,
				    NULL, NULL, 0);
			goto errlock;
		}

		/*
		 * I am a client and am out of date.  If this is an election,
		 * or a response from the first site I contacted, then I can
		 * accept the generation number and participate in future
		 * elections and communication. Otherwise, I need to hear about
		 * a new master and sync up.
		 */
		if (rp->rectype == REP_ALIVE ||
		    rp->rectype == REP_VOTE1 || rp->rectype == REP_VOTE2) {
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Updating gen from %lu to %lu",
				    (u_long)gen, (u_long)rp->gen);
#endif
			gen = rep->gen = rp->gen;
			if (rep->egen <= gen)
				rep->egen = rep->gen + 1;
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Updating egen to %lu",
				    (u_long)rep->egen);
#endif
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		} else if (rp->rectype != REP_NEWMASTER) {
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_MASTER_REQ, NULL, NULL, 0);
			goto errlock;
		}

		/*
		 * If you get here, then you're a client and either you're
		 * in an election or you have a NEWMASTER or an ALIVE message
		 * whose processing will do the right thing below.
		 */

	}

	/*
	 * We need to check if we're in recovery and if we are
	 * then we need to ignore any messages except VERIFY*, VOTE*,
	 * NEW* and ALIVE_REQ.
	 */
	if (recovering) {
		switch (rp->rectype) {
			case REP_VERIFY:
				MUTEX_LOCK(dbenv, db_rep->db_mutexp);
				cmp = log_compare(&lp->verify_lsn, &rp->lsn);
				MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
				if (cmp != 0)
					goto skip;
				break;
			case REP_ALIVE:
			case REP_ALIVE_REQ:
			case REP_DUPMASTER:
			case REP_NEWCLIENT:
			case REP_NEWMASTER:
			case REP_NEWSITE:
			case REP_VERIFY_FAIL:
			case REP_VOTE1:
			case REP_VOTE2:
				break;
			default:
skip:				/*
				 * We don't hold the rep mutex, and could
				 * miscount if we race.
				 */
				rep->stat.st_msgs_recover++;

				/* Check for need to retransmit. */
				MUTEX_LOCK(dbenv, db_rep->db_mutexp);
				MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
				do_req = ++lp->rcvd_recs >= lp->wait_recs;
				MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
				if (do_req) {
					lp->wait_recs *= 2;
					if (lp->wait_recs > rep->max_gap)
						lp->wait_recs = rep->max_gap;
					lp->rcvd_recs = 0;
					lsn = lp->verify_lsn;
				}
				MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
				if (do_req) {
					/*
					 * Don't respond to a MASTER_REQ with
					 * a MASTER_REQ.
					 */
					if (rep->master_id == DB_EID_INVALID &&
					    rp->rectype != REP_MASTER_REQ)
						(void)__rep_send_message(dbenv,
						    DB_EID_BROADCAST,
						    REP_MASTER_REQ,
						    NULL, NULL, 0);
					else if (*eidp == rep->master_id)
						(void)__rep_send_message(
						    dbenv, *eidp,
						    REP_VERIFY_REQ,
						    &lsn, NULL, 0);
				}
				goto errlock;
		}
	}

	switch (rp->rectype) {
	case REP_ALIVE:
		ANYSITE(rep);
		egen = *(u_int32_t *)rec->data;
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
			__db_err(dbenv, "Received ALIVE egen of %lu, mine %lu",
			    (u_long)egen, (u_long)rep->egen);
#endif
		if (egen > rep->egen)
			rep->egen = egen;
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		break;
	case REP_ALIVE_REQ:
		ANYSITE(rep);
		dblp = dbenv->lg_handle;
		R_LOCK(dbenv, &dblp->reginfo);
		lsn = ((LOG *)dblp->reginfo.primary)->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		egen = rep->egen;
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		data_dbt.data = &egen;
		data_dbt.size = sizeof(egen);
		(void)__rep_send_message(dbenv,
		    *eidp, REP_ALIVE, &lsn, &data_dbt, 0);
		goto errlock;
	case REP_DUPMASTER:
		if (F_ISSET(rep, REP_F_MASTER))
			ret = DB_REP_DUPMASTER;
		goto errlock;
	case REP_ALL_REQ:
		MASTER_ONLY(rep, rp);
		gbytes  = bytes = 0;
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		gbytes = rep->gbytes;
		bytes = rep->bytes;
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		check_limit = gbytes != 0 || bytes != 0;
		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto errlock;
		memset(&data_dbt, 0, sizeof(data_dbt));
		oldfilelsn = lsn = rp->lsn;
		type = REP_LOG;
		flags = IS_ZERO_LSN(rp->lsn) ||
		    IS_INIT_LSN(rp->lsn) ?  DB_FIRST : DB_SET;
		for (ret = __log_c_get(logc, &lsn, &data_dbt, flags);
		    ret == 0 && type == REP_LOG;
		    ret = __log_c_get(logc, &lsn, &data_dbt, DB_NEXT)) {
			/*
			 * When a log file changes, we'll have a real log
			 * record with some lsn [n][m], and we'll also want
			 * to send a NEWFILE message with lsn [n-1][MAX].
			 */
			if (lsn.file != oldfilelsn.file)
				(void)__rep_send_message(dbenv,
				    *eidp, REP_NEWFILE, &oldfilelsn, NULL, 0);
			if (check_limit) {
				/*
				 * data_dbt.size is only the size of the log
				 * record;  it doesn't count the size of the
				 * control structure. Factor that in as well
				 * so we're not off by a lot if our log records
				 * are small.
				 */
				while (bytes <
				    data_dbt.size + sizeof(REP_CONTROL)) {
					if (gbytes > 0) {
						bytes += GIGABYTE;
						--gbytes;
						continue;
					}
					/*
					 * We don't hold the rep mutex,
					 * and may miscount.
					 */
					rep->stat.st_nthrottles++;
					type = REP_LOG_MORE;
					goto send;
				}
				bytes -= (data_dbt.size + sizeof(REP_CONTROL));
			}

send:			if (__rep_send_message(dbenv,
			    *eidp, type, &lsn, &data_dbt, 0) != 0)
				break;

			/*
			 * If we are about to change files, then we'll need the
			 * last LSN in the previous file.  Save it here.
			 */
			oldfilelsn = lsn;
			oldfilelsn.offset += logc->c_len;
		}

		if (ret == DB_NOTFOUND)
			ret = 0;
		if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
			ret = t_ret;
		goto errlock;
#ifdef NOTYET
	case REP_FILE: /* TODO */
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		break;
	case REP_FILE_REQ:
		MASTER_ONLY(rep, rp);
		ret = __rep_send_file(dbenv, rec, *eidp);
		goto errlock;
#endif
	case REP_LOG:
	case REP_LOG_MORE:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		if ((ret = __rep_apply(dbenv, rp, rec, ret_lsnp)) != 0)
			goto errlock;
		if (rp->rectype == REP_LOG_MORE) {
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			master = rep->master_id;
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			/*
			 * If the master_id is invalid, this means that since
			 * the last record was sent, somebody declared an
			 * election and we may not have a master to request
			 * things of.
			 *
			 * This is not an error;  when we find a new master,
			 * we'll re-negotiate where the end of the log is and
			 * try to bring ourselves up to date again anyway.
			 */
			if (master == DB_EID_INVALID)
				ret = 0;
			else
				if (__rep_send_message(dbenv,
				    master, REP_ALL_REQ, &lsn, NULL, 0) != 0)
					break;
		}
		goto errlock;
	case REP_LOG_REQ:
		MASTER_ONLY(rep, rp);
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION) &&
		    rec != NULL && rec->size != 0) {
			__db_err(dbenv,
			    "[%lu][%lu]: LOG_REQ max lsn: [%lu][%lu]",
			    (u_long) rp->lsn.file, (u_long)rp->lsn.offset,
			    (u_long)((DB_LSN *)rec->data)->file,
			    (u_long)((DB_LSN *)rec->data)->offset);
		}
#endif
		/*
		 * There are three different cases here.
		 * 1. We asked for a particular LSN and got it.
		 * 2. We asked for an LSN and it's not found because it is
		 *	beyond the end of a log file and we need a NEWFILE msg.
		 *	and then the record that was requested.
		 * 3. We asked for an LSN and it simply doesn't exist, but
		 *    doesn't meet any of those other criteria, in which case
		 *    it's an error (that should never happen).
		 * If we have a valid LSN and the request has a data_dbt with
		 * it, then we need to send all records up to the LSN in the
		 * data dbt.
		 */
		lsn = rp->lsn;
		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto errlock;
		memset(&data_dbt, 0, sizeof(data_dbt));
		ret = __log_c_get(logc, &rp->lsn, &data_dbt, DB_SET);

		if (ret == 0) /* Case 1 */
			(void)__rep_send_message(dbenv,
				    *eidp, REP_LOG, &rp->lsn, &data_dbt, 0);
		else if (ret == DB_NOTFOUND) {
			R_LOCK(dbenv, &dblp->reginfo);
			endlsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			if (endlsn.file > lsn.file) {
				/*
				 * Case 2:
				 * Need to find the LSN of the last record in
				 * file lsn.file so that we can send it with
				 * the NEWFILE call.  In order to do that, we
				 * need to try to get {lsn.file + 1, 0} and
				 * then backup.
				 */
				endlsn.file = lsn.file + 1;
				endlsn.offset = 0;
				if ((ret = __log_c_get(logc,
				    &endlsn, &data_dbt, DB_SET)) != 0 ||
				    (ret = __log_c_get(logc,
					&endlsn, &data_dbt, DB_PREV)) != 0) {
					if (FLD_ISSET(dbenv->verbose,
					    DB_VERB_REPLICATION))
						__db_err(dbenv,
					"Unable to get prev of [%lu][%lu]",
						    (u_long)lsn.file,
						    (u_long)lsn.offset);
					ret = DB_REP_OUTDATED;
				} else {
					endlsn.offset += logc->c_len;
					(void)__rep_send_message(dbenv, *eidp,
					    REP_NEWFILE, &endlsn, NULL, 0);
				}
			} else {
				/* Case 3 */
				DB_ASSERT(0);
				__db_err(dbenv,
				    "Request for LSN [%lu][%lu] fails",
				    (u_long)lsn.file, (u_long)lsn.offset);
				ret = EINVAL;
			}
		}

		/*
		 * XXX
		 * Note that we are not observing the limits here that
		 * we observe on ALL_REQs.  If we think that we need to,
		 * then we need to figure out how to convey back to the
		 * client the max_lsn with the LOG_MORE message and I
		 * can't quite figure out how to do that.
		 */
		while (ret == 0 && rec != NULL && rec->size != 0) {
			if ((ret =
			    __log_c_get(logc, &lsn, &data_dbt, DB_NEXT)) != 0) {
				if (ret == DB_NOTFOUND)
					ret = 0;
				break;;
			}
			if (log_compare(&lsn, (DB_LSN *)rec->data) >= 0)
				break;
			if (__rep_send_message(dbenv,
			    *eidp, REP_LOG, &lsn, &data_dbt, 0) != 0)
				break;
		}

		if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
			ret = t_ret;
		goto errlock;
	case REP_NEWSITE:
		/* We don't hold the rep mutex, and may miscount. */
		rep->stat.st_newsites++;

		/* This is a rebroadcast; simply tell the application. */
		if (F_ISSET(rep, REP_F_MASTER)) {
			dblp = dbenv->lg_handle;
			lp = dblp->reginfo.primary;
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0);
		}
		ret = DB_REP_NEWSITE;
		goto errlock;
	case REP_NEWCLIENT:
		/*
		 * This message was received and should have resulted in the
		 * application entering the machine ID in its machine table.
		 * We respond to this with an ALIVE to send relevant information
		 * to the new client (if we are a master, we'll send a
		 * NEWMASTER, so we only need to send the ALIVE if we're a
		 * client).  But first, broadcast the new client's record to
		 * all the clients.
		 */
		(void)__rep_send_message(dbenv,
		    DB_EID_BROADCAST, REP_NEWSITE, &rp->lsn, rec, 0);

		ret = DB_REP_NEWSITE;

		if (F_ISSET(rep, REP_F_UPGRADE)) {
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			egen = rep->egen;
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			data_dbt.data = &egen;
			data_dbt.size = sizeof(egen);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_ALIVE, &rp->lsn, &data_dbt, 0);
			goto errlock;
		}
		/* FALLTHROUGH */
	case REP_MASTER_REQ:
		if (F_ISSET(rep, REP_F_MASTER)) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_NEWMASTER, &lsn, NULL, 0);
		}
		/*
		 * Otherwise, clients just ignore it.
		 */
		goto errlock;
	case REP_NEWFILE:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		ret = __rep_apply(dbenv, rp, rec, ret_lsnp);
		goto errlock;
	case REP_NEWMASTER:
		ANYSITE(rep);
		if (F_ISSET(rep, REP_F_MASTER) &&
		    *eidp != dbenv->rep_eid) {
			/* We don't hold the rep mutex, and may miscount. */
			rep->stat.st_dupmasters++;
			ret = DB_REP_DUPMASTER;
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_DUPMASTER, NULL, NULL, 0);
			goto errlock;
		}
		ret = __rep_new_master(dbenv, rp, *eidp);
		goto errlock;
	case REP_PAGE: /* TODO */
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		break;
	case REP_PAGE_REQ: /* TODO */
		MASTER_ONLY(rep, rp);
		break;
	case REP_PLIST: /* TODO */
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		break;
	case REP_PLIST_REQ: /* TODO */
		MASTER_ONLY(rep, rp);
		break;
	case REP_VERIFY:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		DB_ASSERT((F_ISSET(rep, REP_F_RECOVER) &&
		    !IS_ZERO_LSN(lp->verify_lsn)) ||
		    (!F_ISSET(rep, REP_F_RECOVER) &&
		    IS_ZERO_LSN(lp->verify_lsn)));
		if (IS_ZERO_LSN(lp->verify_lsn))
			goto errlock;

		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto errlock;
		memset(&mylog, 0, sizeof(mylog));
		if ((ret = __log_c_get(logc, &rp->lsn, &mylog, DB_SET)) != 0)
			goto rep_verify_err;
		if (mylog.size == rec->size &&
		    memcmp(mylog.data, rec->data, rec->size) == 0) {
			ret = __rep_verify_match(dbenv, rp, savetime);
		} else if ((ret =
		    __log_c_get(logc, &lsn, &mylog, DB_PREV)) == 0) {
			MUTEX_LOCK(dbenv, db_rep->db_mutexp);
			lp->verify_lsn = lsn;
			lp->rcvd_recs = 0;
			lp->wait_recs = rep->request_gap;
			MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_VERIFY_REQ, &lsn, NULL, 0);
		} else if (ret == DB_NOTFOUND) {
			/* We've either run out of records because
			 * logs have been removed or we've rolled back
			 * all the way to the beginning.  In both cases
			 * we to return DB_REP_OUTDATED; in the latter
			 * we don't think these sites were every part of
			 * the same environment and we'll say so.
			 */
			ret = DB_REP_OUTDATED;
			if (rp->lsn.file != 1)
				__db_err(dbenv,
				    "Too few log files to sync with master");
			else
				__db_err(dbenv,
			"Client was never part of master's environment");
		}

rep_verify_err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
			ret = t_ret;
		goto errlock;
	case REP_VERIFY_FAIL:
		rep->stat.st_outdated++;
		ret = DB_REP_OUTDATED;
		goto errlock;
	case REP_VERIFY_REQ:
		MASTER_ONLY(rep, rp);
		type = REP_VERIFY;
		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto errlock;
		d = &data_dbt;
		memset(d, 0, sizeof(data_dbt));
		F_SET(logc, DB_LOG_SILENT_ERR);
		ret = __log_c_get(logc, &rp->lsn, d, DB_SET);
		/*
		 * If the LSN was invalid, then we might get a not
		 * found, we might get an EIO, we could get anything.
		 * If we get a DB_NOTFOUND, then there is a chance that
		 * the LSN comes before the first file present in which
		 * case we need to return a fail so that the client can return
		 * a DB_OUTDATED.
		 */
		if (ret == DB_NOTFOUND &&
		    __log_is_outdated(dbenv, rp->lsn.file, &old) == 0 &&
		    old != 0)
			type = REP_VERIFY_FAIL;

		if (ret != 0)
			d = NULL;

		(void)__rep_send_message(dbenv, *eidp, type, &rp->lsn, d, 0);
		ret = __log_c_close(logc);
		goto errlock;
	case REP_VOTE1:
		if (F_ISSET(rep, REP_F_MASTER)) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Master received vote");
#endif
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0);
			goto errlock;
		}

		vi = (REP_VOTE_INFO *)rec->data;
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);

		/*
		 * If we get a vote from a later election gen, we
		 * clear everything from the current one, and we'll
		 * start over by tallying it.
		 */
		if (vi->egen < rep->egen) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
			    "Received old vote %lu, egen %lu, ignoring vote1",
				    (u_long)vi->egen, (u_long)rep->egen);
#endif
			goto errunlock;
		}
		if (vi->egen > rep->egen) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
			    "Received VOTE1 from egen %lu, my egen %lu; reset",
				    (u_long)vi->egen, (u_long)rep->egen);
#endif
			__rep_elect_done(dbenv, rep);
			rep->egen = vi->egen;
		}
		if (!IN_ELECTION(rep))
			F_SET(rep, REP_F_TALLY);

		/* Check if this site knows about more sites than we do. */
		if (vi->nsites > rep->nsites)
			rep->nsites = vi->nsites;

		/*
		 * We are keeping the vote, let's see if that changes our
		 * count of the number of sites.
		 */
		if (rep->sites + 1 > rep->nsites)
			rep->nsites = rep->sites + 1;
		if (rep->nsites > rep->asites &&
		    (ret = __rep_grow_sites(dbenv, rep->nsites)) != 0) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
				    "Grow sites returned error %d", ret);
#endif
			goto errunlock;
		}

		/*
		 * Ignore vote1's if we're in phase 2.
		 */
		if (F_ISSET(rep, REP_F_EPHASE2)) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "In phase 2, ignoring vote1");
#endif
			goto errunlock;
		}

		/*
		 * Record this vote.  If we get back non-zero, we
		 * ignore the vote.
		 */
		if ((ret = __rep_tally(dbenv, rep, *eidp, &rep->sites,
		    vi->egen, rep->tally_off)) != 0) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Tally returned %d, sites %d",
				    ret, rep->sites);
#endif
			ret = 0;
			goto errunlock;
		}
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {
			__db_err(dbenv,
	    "Incoming vote: (eid)%d (pri)%d (gen)%lu (egen)%lu [%lu,%lu]",
			    *eidp, vi->priority,
			    (u_long)rp->gen, (u_long)vi->egen,
			    (u_long)rp->lsn.file, (u_long)rp->lsn.offset);
			if (rep->sites > 1)
				__db_err(dbenv,
	    "Existing vote: (eid)%d (pri)%d (gen)%lu (sites)%d [%lu,%lu]",
				    rep->winner, rep->w_priority,
				    (u_long)rep->w_gen, rep->sites,
				    (u_long)rep->w_lsn.file,
				    (u_long)rep->w_lsn.offset);
		}
#endif
		__rep_cmp_vote(dbenv, rep, eidp, &rp->lsn, vi->priority,
		    rp->gen, vi->tiebreaker);
		/*
		 * If you get a vote and you're not in an election, we've
		 * already recorded this vote.  But that is all we need
		 * to do.
		 */
		if (!IN_ELECTION(rep)) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
				    "Not in election, but received vote1 0x%x",
				    rep->flags);
#endif
			ret = DB_REP_HOLDELECTION;
			goto errunlock;
		}

		master = rep->winner;
		lsn = rep->w_lsn;
		/*
		 * We need to check sites == nsites, not more than half
		 * like we do in __rep_elect and the VOTE2 code below.  The
		 * reason is that we want to process all the incoming votes
		 * and not short-circuit once we reach more than half.  The
		 * real winner's vote may be in the last half.
		 */
		done = rep->sites >= rep->nsites && rep->w_priority != 0;
		if (done) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {
				__db_err(dbenv, "Phase1 election done");
				__db_err(dbenv, "Voting for %d%s",
				    master, master == rep->eid ? "(self)" : "");
			}
#endif
			egen = rep->egen;
			F_SET(rep, REP_F_EPHASE2);
			F_CLR(rep, REP_F_EPHASE1);
			if (master == rep->eid) {
				(void)__rep_tally(dbenv, rep, rep->eid,
				    &rep->votes, egen, rep->v2tally_off);
				goto errunlock;
			}
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

			/* Vote for someone else. */
			__rep_send_vote(dbenv, NULL, 0, 0, 0, egen,
			    master, REP_VOTE2);
		} else
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

		/* Election is still going on. */
		break;
	case REP_VOTE2:
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
			__db_err(dbenv, "We received a vote%s",
			    F_ISSET(rep, REP_F_MASTER) ?
			    " (master)" : "");
#endif
		if (F_ISSET(rep, REP_F_MASTER)) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			rep->stat.st_elections_won++;
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0);
			goto errlock;
		}

		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);

		/* If we have priority 0, we should never get a vote. */
		DB_ASSERT(rep->priority != 0);

		/*
		 * We might be the last to the party and we haven't had
		 * time to tally all the vote1's, but others have and
		 * decided we're the winner.  So, if we're in the process
		 * of tallying sites, keep the vote so that when our
		 * election thread catches up we'll have the votes we
		 * already received.
		 */
		vi = (REP_VOTE_INFO *)rec->data;
		if (!IN_ELECTION_TALLY(rep) && vi->egen >= rep->egen) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
				    "Not in election gen %lu, at %lu, got vote",
				    (u_long)vi->egen, (u_long)rep->egen);
#endif
			ret = DB_REP_HOLDELECTION;
			goto errunlock;
		}

		/*
		 * Record this vote.  In a VOTE2, the only valid entry
		 * in the REP_VOTE_INFO is the election generation.
		 *
		 * There are several things which can go wrong that we
		 * need to account for:
		 * 1. If we receive a latent VOTE2 from an earlier election,
		 * we want to ignore it.
		 * 2. If we receive a VOTE2 from a site from which we never
		 * received a VOTE1, we want to ignore it.
		 * 3. If we have received a duplicate VOTE2 from this election
		 * from the same site we want to ignore it.
		 * 4. If this is from the current election and someone is
		 * really voting for us, then we finally get to record it.
		 */
		/*
		 * __rep_cmp_vote2 checks for cases 1 and 2.
		 */
		if ((ret = __rep_cmp_vote2(dbenv, rep, *eidp, vi->egen)) != 0) {
			ret = 0;
			goto errunlock;
		}
		/*
		 * __rep_tally takes care of cases 3 and 4.
		 */
		if ((ret = __rep_tally(dbenv, rep, *eidp, &rep->votes,
		    vi->egen, rep->v2tally_off)) != 0) {
			ret = 0;
			goto errunlock;
		}
		done = rep->votes > rep->nsites / 2;
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
			__db_err(dbenv, "Counted vote %d", rep->votes);
#endif
		if (done) {
			__rep_elect_master(dbenv, rep, eidp);
			ret = DB_REP_NEWMASTER;
			goto errunlock;
		} else
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		break;
	default:
		__db_err(dbenv,
	"DB_ENV->rep_process_message: unknown replication message: type %lu",
		   (u_long)rp->rectype);
		ret = EINVAL;
		goto errlock;
	}

	/*
	 * If we already hold rep_mutexp then we goto 'errunlock'
	 * Otherwise we goto 'errlock' to acquire it before we
	 * decrement our message thread count.
	 */
errlock:
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
errunlock:
	rep->msg_th--;
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	return (ret);
}

/*
 * __rep_apply --
 *
 * Handle incoming log records on a client, applying when possible and
 * entering into the bookkeeping table otherwise.  This is the guts of
 * the routine that handles the state machine that describes how we
 * process and manage incoming log records.
 */
static int
__rep_apply(dbenv, rp, rec, ret_lsnp)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	DB_LSN *ret_lsnp;
{
	__dbreg_register_args dbreg_args;
	__txn_ckp_args ckp_args;
	DB_REP *db_rep;
	DBT control_dbt, key_dbt, lsn_dbt;
	DBT max_lsn_dbt, *max_lsn_dbtp, nextrec_dbt, rec_dbt;
	DB *dbp;
	DBC *dbc;
	DB_LOG *dblp;
	DB_LSN ckp_lsn, max_lsn, next_lsn;
	LOG *lp;
	REP *rep;
	REP_CONTROL *grp;
	u_int32_t rectype, txnid;
	int cmp, do_req, eid, gap, ret, t_ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dbp = db_rep->rep_db;
	dbc = NULL;
	ret = gap = 0;
	memset(&control_dbt, 0, sizeof(control_dbt));
	memset(&rec_dbt, 0, sizeof(rec_dbt));
	max_lsn_dbtp = NULL;

	/*
	 * If this is a log record and it's the next one in line, simply
	 * write it to the log.  If it's a "normal" log record, i.e., not
	 * a COMMIT or CHECKPOINT or something that needs immediate processing,
	 * just return.  If it's a COMMIT, CHECKPOINT, LOG_REGISTER, PREPARE
	 * (i.e., not SIMPLE), handle it now.  If it's a NEWFILE record,
	 * then we have to be prepared to deal with a logfile change.
	 */
	dblp = dbenv->lg_handle;
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	lp = dblp->reginfo.primary;
	cmp = log_compare(&rp->lsn, &lp->ready_lsn);

	/*
	 * This is written to assume that you don't end up with a lot of
	 * records after a hole.  That is, it optimizes for the case where
	 * there is only a record or two after a hole.  If you have a lot
	 * of records after a hole, what you'd really want to do is write
	 * all of them and then process all the commits, checkpoints, etc.
	 * together.  That is more complicated processing that we can add
	 * later if necessary.
	 *
	 * That said, I really don't want to do db operations holding the
	 * log mutex, so the synchronization here is tricky.
	 */
	if (cmp == 0) {
		/* We got the log record that we are expecting. */
		if (rp->rectype == REP_NEWFILE) {
			ret = __rep_newfile(dbenv, rp, &lp->ready_lsn);

			/* Make this evaluate to a simple rectype. */
			rectype = 0;
		} else {
			if (F_ISSET(rp, DB_LOG_PERM)) {
				gap = 1;
				max_lsn = rp->lsn;
			}
			ret = __log_rep_put(dbenv, &rp->lsn, rec);
			memcpy(&rectype, rec->data, sizeof(rectype));
			if (ret == 0)
				/*
				 * We may miscount if we race, since we
				 * don't currently hold the rep mutex.
				 */
				rep->stat.st_log_records++;
		}
		/*
		 * If we get the record we are expecting, reset
		 * the count of records we've received and are applying
		 * towards the request interval.
		 */
		lp->rcvd_recs = 0;

		while (ret == 0 && IS_SIMPLE(rectype) &&
		    log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0) {
			/*
			 * We just filled in a gap in the log record stream.
			 * Write subsequent records to the log.
			 */
gap_check:		max_lsn_dbtp = NULL;
			lp->wait_recs = 0;
			lp->rcvd_recs = 0;
			ZERO_LSN(lp->max_wait_lsn);
			if (dbc == NULL &&
			    (ret = __db_cursor(dbp, NULL, &dbc, 0)) != 0)
				goto err;

			/* The DBTs need to persist through another call. */
			F_SET(&control_dbt, DB_DBT_REALLOC);
			F_SET(&rec_dbt, DB_DBT_REALLOC);
			if ((ret = __db_c_get(dbc,
			    &control_dbt, &rec_dbt, DB_RMW | DB_FIRST)) != 0)
				goto err;

			rp = (REP_CONTROL *)control_dbt.data;
			rec = &rec_dbt;
			memcpy(&rectype, rec->data, sizeof(rectype));
			if (rp->rectype != REP_NEWFILE) {
				ret = __log_rep_put(dbenv, &rp->lsn, rec);
				/*
				 * We may miscount if we race, since we
				 * don't currently hold the rep mutex.
				 */
				if (ret == 0)
					rep->stat.st_log_records++;
			} else {
				ret = __rep_newfile(dbenv, rp, &lp->ready_lsn);
				rectype = 0;
			}
			if ((ret = __db_c_del(dbc, 0)) != 0)
				goto err;

			/*
			 * If we just processed a permanent log record, make
			 * sure that we note that we've done so and that we
			 * save its LSN.
			 */
			if (F_ISSET(rp, DB_LOG_PERM)) {
				gap = 1;
				max_lsn = rp->lsn;
			}
			/*
			 * We may miscount, as we don't hold the rep
			 * mutex.
			 */
			--rep->stat.st_log_queued;

			/*
			 * Update waiting_lsn.  We need to move it
			 * forward to the LSN of the next record
			 * in the queue.
			 *
			 * If the next item in the database is a log
			 * record--the common case--we're not
			 * interested in its contents, just in its LSN.
			 * Optimize by doing a partial get of the data item.
			 */
			memset(&nextrec_dbt, 0, sizeof(nextrec_dbt));
			F_SET(&nextrec_dbt, DB_DBT_PARTIAL);
			nextrec_dbt.ulen = nextrec_dbt.dlen = 0;

			memset(&lsn_dbt, 0, sizeof(lsn_dbt));
			ret = __db_c_get(dbc, &lsn_dbt, &nextrec_dbt, DB_NEXT);
			if (ret != DB_NOTFOUND && ret != 0)
				goto err;

			if (ret == DB_NOTFOUND) {
				ZERO_LSN(lp->waiting_lsn);
				/*
				 * Whether or not the current record is
				 * simple, there's no next one, and
				 * therefore we haven't got anything
				 * else to do right now.  Break out.
				 */
				break;
			}
			grp = (REP_CONTROL *)lsn_dbt.data;
			lp->waiting_lsn = grp->lsn;

			/*
			 * If the current rectype is simple, we're done with it,
			 * and we should check and see whether the next record
			 * queued is the next one we're ready for.  This is
			 * just the loop condition, so we continue.
			 *
			 * If this record isn't simple, then we need to
			 * process it before continuing.
			 */
			if (!IS_SIMPLE(rectype))
				break;
		}

		/*
		 * Check if we're at a gap in the table and if so, whether we
		 * need to ask for any records.
		 */
		do_req = 0;
		if (!IS_ZERO_LSN(lp->waiting_lsn) &&
		    log_compare(&lp->ready_lsn, &lp->waiting_lsn) != 0) {
			/*
			 * We got a record and processed it, but we may
			 * still be waiting for more records.
			 */
			next_lsn = lp->ready_lsn;
			do_req = ++lp->rcvd_recs >= lp->wait_recs;
			if (do_req) {
				lp->wait_recs = rep->request_gap;
				lp->rcvd_recs = 0;
				if (log_compare(&rp->lsn,
				    &lp->max_wait_lsn) == 0) {
					/*
					 * This single record was requested
					 * so ask for the rest of the gap.
					 */
					lp->max_wait_lsn = lp->waiting_lsn;
					memset(&max_lsn_dbt,
					    0, sizeof(max_lsn_dbt));
					max_lsn_dbt.data = &lp->waiting_lsn;
					max_lsn_dbt.size =
					    sizeof(lp->waiting_lsn);
					max_lsn_dbtp = &max_lsn_dbt;
				}
			}
		} else {
			lp->wait_recs = 0;
			ZERO_LSN(lp->max_wait_lsn);
		}

		if (dbc != NULL)
			if ((ret = __db_c_close(dbc)) != 0)
				goto err;
		dbc = NULL;

		if (do_req) {
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			eid = db_rep->region->master_id;
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			if (eid != DB_EID_INVALID) {
				rep->stat.st_log_requested++;
				(void)__rep_send_message(dbenv, eid,
				    REP_LOG_REQ, &next_lsn, max_lsn_dbtp, 0);
			}
		}
	} else if (cmp > 0) {
		/*
		 * The LSN is higher than the one we were waiting for.
		 * This record isn't in sequence; add it to the temporary
		 * database, update waiting_lsn if necessary, and perform
		 * calculations to determine if we should issue requests
		 * for new records.
		 */
		memset(&key_dbt, 0, sizeof(key_dbt));
		key_dbt.data = rp;
		key_dbt.size = sizeof(*rp);
		R_LOCK(dbenv, &dblp->reginfo);
		next_lsn = lp->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		do_req = 0;
		if (lp->wait_recs == 0) {
			/*
			 * This is a new gap. Initialize the number of
			 * records that we should wait before requesting
			 * that it be resent.  We grab the limits out of
			 * the rep without the mutex.
			 */
			lp->wait_recs = rep->request_gap;
			lp->rcvd_recs = 0;
			ZERO_LSN(lp->max_wait_lsn);
		}

		if (++lp->rcvd_recs >= lp->wait_recs) {
			/*
			 * If we've waited long enough, request the record
			 * (or set of records) and double the wait interval.
			 */
			do_req = 1;
			lp->rcvd_recs = 0;
			lp->wait_recs *= 2;
			if (lp->wait_recs > rep->max_gap)
				lp->wait_recs = rep->max_gap;

			/*
			 * If we've never requested this record, then request
			 * everything between it and the first record we have.
			 * If we have requested this record, then only request
			 * this record, not the entire gap.
			 */
			if (IS_ZERO_LSN(lp->max_wait_lsn)) {
				lp->max_wait_lsn = lp->waiting_lsn;
				memset(&max_lsn_dbt, 0, sizeof(max_lsn_dbt));
				max_lsn_dbt.data = &lp->waiting_lsn;
				max_lsn_dbt.size = sizeof(lp->waiting_lsn);
				max_lsn_dbtp = &max_lsn_dbt;
			} else {
				max_lsn_dbtp = NULL;
				lp->max_wait_lsn = next_lsn;
			}
		}

		ret = __db_put(dbp, NULL, &key_dbt, rec, 0);
		rep->stat.st_log_queued++;
		rep->stat.st_log_queued_total++;
		if (rep->stat.st_log_queued_max < rep->stat.st_log_queued)
			rep->stat.st_log_queued_max = rep->stat.st_log_queued;

		if (ret != 0)
			goto done;

		if (IS_ZERO_LSN(lp->waiting_lsn) ||
		    log_compare(&rp->lsn, &lp->waiting_lsn) < 0)
			lp->waiting_lsn = rp->lsn;

		if (do_req) {
			/* Request the LSN we are still waiting for. */
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			eid = db_rep->region->master_id;

			/*
			 * If the master_id is invalid, this means that since
			 * the last record was sent, somebody declared an
			 * election and we may not have a master to request
			 * things of.
			 *
			 * This is not an error;  when we find a new master,
			 * we'll re-negotiate where the end of the log is and
			 * try to to bring ourselves up to date again anyway.
			 */
			if (eid != DB_EID_INVALID) {
				rep->stat.st_log_requested++;
				MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
				(void)__rep_send_message(dbenv, eid,
				    REP_LOG_REQ, &next_lsn, max_lsn_dbtp, 0);
			} else {
				MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
				(void)__rep_send_message(dbenv,
				    DB_EID_BROADCAST, REP_MASTER_REQ,
				    NULL, NULL, 0);
			}
		}

		/*
		 * If this is permanent; let the caller know that we have
		 * not yet written it to disk, but we've accepted it.
		 */
		if (ret == 0 && F_ISSET(rp, DB_LOG_PERM)) {
			if (ret_lsnp != NULL)
				*ret_lsnp = rp->lsn;
			ret = DB_REP_NOTPERM;
		}
		goto done;
	} else {
		/*
		 * We may miscount if we race, since we
		 * don't currently hold the rep mutex.
		 */
		rep->stat.st_log_duplicated++;
		goto done;
	}
	if (ret != 0 || cmp < 0 || (cmp == 0 && IS_SIMPLE(rectype)))
		goto done;

	/*
	 * If we got here, then we've got a log record in rp and rec that
	 * we need to process.
	 */
	switch (rectype) {
	case DB___dbreg_register:
		/*
		 * DB opens occur in the context of a transaction, so we can
		 * simply handle them when we process the transaction.  Closes,
		 * however, are not transaction-protected, so we have to
		 * handle them here.
		 *
		 * Note that it should be unsafe for the master to do a close
		 * of a file that was opened in an active transaction, so we
		 * should be guaranteed to get the ordering right.
		 */
		memcpy(&txnid, (u_int8_t *)rec->data +
		    ((u_int8_t *)&dbreg_args.txnid - (u_int8_t *)&dbreg_args),
		    sizeof(u_int32_t));
		if (txnid == TXN_INVALID &&
		    !F_ISSET(rep, REP_F_LOGSONLY))
			ret = __db_dispatch(dbenv, dbenv->recover_dtab,
			    dbenv->recover_dtab_size, rec, &rp->lsn,
			    DB_TXN_APPLY, NULL);
		break;
	case DB___txn_ckp:
		/* Sync the memory pool. */
		memcpy(&ckp_lsn, (u_int8_t *)rec->data +
		    ((u_int8_t *)&ckp_args.ckp_lsn - (u_int8_t *)&ckp_args),
		    sizeof(DB_LSN));
		if (!F_ISSET(rep, REP_F_LOGSONLY))
			ret = __memp_sync(dbenv, &ckp_lsn);
		else
			/*
			 * We ought to make sure the logs on a logs-only
			 * replica get flushed now and again.
			 */
			ret = __log_flush(dbenv, &ckp_lsn);
		/* Update the last_ckp in the txn region. */
		if (ret == 0)
			__txn_updateckp(dbenv, &rp->lsn);
		else {
			__db_err(dbenv, "Error syncing ckp [%lu][%lu]",
			    (u_long)ckp_lsn.file, (u_long)ckp_lsn.offset);
			__db_panic(dbenv, ret);
		}
		break;
	case DB___txn_regop:
		if (!F_ISSET(rep, REP_F_LOGSONLY))
			do {
				/*
				 * If an application is doing app-specific
				 * recovery and acquires locks while applying
				 * a transaction, it can deadlock.  Any other
				 * locks held by this thread should have been
				 * discarded in the __rep_process_txn error
				 * path, so if we simply retry, we should
				 * eventually succeed.
				 */
				ret = __rep_process_txn(dbenv, rec);
			} while (ret == DB_LOCK_DEADLOCK);

		/* Now flush the log unless we're running TXN_NOSYNC. */
		if (ret == 0 && !F_ISSET(dbenv, DB_ENV_TXN_NOSYNC))
			ret = __log_flush(dbenv, NULL);
		if (ret != 0) {
			__db_err(dbenv, "Error processing txn [%lu][%lu]",
			    (u_long)rp->lsn.file, (u_long)rp->lsn.offset);
			__db_panic(dbenv, ret);
		}
		break;
	case DB___txn_xa_regop:
		ret = __log_flush(dbenv, NULL);
		break;
	default:
		goto err;
	}

	/* Check if we need to go back into the table. */
	if (ret == 0) {
		if (log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0)
			goto gap_check;
	}

done:
err:	if (dbc != NULL && (t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	if (ret == 0 && F_ISSET(dbenv, DB_ENV_LOG_AUTOREMOVE) &&
	    rp->rectype == REP_NEWFILE)
		__log_autoremove(dbenv);
	if (control_dbt.data != NULL)
		__os_ufree(dbenv, control_dbt.data);
	if (rec_dbt.data != NULL)
		__os_ufree(dbenv, rec_dbt.data);
	if (ret == 0 && gap) {
		if (ret_lsnp != NULL)
			*ret_lsnp = max_lsn;
		ret = DB_REP_ISPERM;
	}
#ifdef DIAGNOSTIC
	if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {
		if (ret == DB_REP_ISPERM)
			__db_err(dbenv, "Returning ISPERM [%lu][%lu]",
			    (u_long)ret_lsnp->file, (u_long)ret_lsnp->offset);
		else if (ret == DB_REP_NOTPERM)
			__db_err(dbenv, "Returning NOTPERM [%lu][%lu]",
			    (u_long)ret_lsnp->file, (u_long)ret_lsnp->offset);
		else if (ret != 0)
			__db_err(dbenv, "Returning %d [%lu][%lu]", ret,
			    (u_long)ret_lsnp->file, (u_long)ret_lsnp->offset);
	}
#endif
	return (ret);
}

/*
 * __rep_process_txn --
 *
 * This is the routine that actually gets a transaction ready for
 * processing.
 *
 * PUBLIC: int __rep_process_txn __P((DB_ENV *, DBT *));
 */
int
__rep_process_txn(dbenv, rec)
	DB_ENV *dbenv;
	DBT *rec;
{
	DBT data_dbt, *lock_dbt;
	DB_LOCKREQ req, *lvp;
	DB_LOGC *logc;
	DB_LSN prev_lsn, *lsnp;
	DB_REP *db_rep;
	LSN_COLLECTION lc;
	REP *rep;
	__txn_regop_args *txn_args;
	__txn_xa_regop_args *prep_args;
	u_int32_t lockid, rectype;
	int i, ret, t_ret;
	void *txninfo;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	logc = NULL;
	txninfo = NULL;
	memset(&data_dbt, 0, sizeof(data_dbt));
	if (F_ISSET(dbenv, DB_ENV_THREAD))
		F_SET(&data_dbt, DB_DBT_REALLOC);

	/*
	 * There are two phases:  First, we have to traverse
	 * backwards through the log records gathering the list
	 * of all LSNs in the transaction.  Once we have this information,
	 * we can loop through and then apply it.
	 */

	/*
	 * We may be passed a prepare (if we're restoring a prepare
	 * on upgrade) instead of a commit (the common case).
	 * Check which and behave appropriately.
	 */
	memcpy(&rectype, rec->data, sizeof(rectype));
	memset(&lc, 0, sizeof(lc));
	if (rectype == DB___txn_regop) {
		/*
		 * We're the end of a transaction.  Make sure this is
		 * really a commit and not an abort!
		 */
		if ((ret = __txn_regop_read(dbenv, rec->data, &txn_args)) != 0)
			return (ret);
		if (txn_args->opcode != TXN_COMMIT) {
			__os_free(dbenv, txn_args);
			return (0);
		}
		prev_lsn = txn_args->prev_lsn;
		lock_dbt = &txn_args->locks;
	} else {
		/* We're a prepare. */
		DB_ASSERT(rectype == DB___txn_xa_regop);

		if ((ret =
		    __txn_xa_regop_read(dbenv, rec->data, &prep_args)) != 0)
			return (ret);
		prev_lsn = prep_args->prev_lsn;
		lock_dbt = &prep_args->locks;
	}

	/* Get locks. */
	if ((ret = __lock_id(dbenv, &lockid)) != 0)
		goto err1;

	if ((ret =
	      __lock_get_list(dbenv, lockid, 0, DB_LOCK_WRITE, lock_dbt)) != 0)
		goto err;

	/* Phase 1.  Get a list of the LSNs in this transaction, and sort it. */
	if ((ret = __rep_collect_txn(dbenv, &prev_lsn, &lc)) != 0)
		goto err;
	qsort(lc.array, lc.nlsns, sizeof(DB_LSN), __rep_lsn_cmp);

	/*
	 * The set of records for a transaction may include dbreg_register
	 * records.  Create a txnlist so that they can keep track of file
	 * state between records.
	 */
	if ((ret = __db_txnlist_init(dbenv, 0, 0, NULL, &txninfo)) != 0)
		goto err;

	/* Phase 2: Apply updates. */
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		goto err;
	for (lsnp = &lc.array[0], i = 0; i < lc.nlsns; i++, lsnp++) {
		if ((ret = __log_c_get(logc, lsnp, &data_dbt, DB_SET)) != 0) {
			__db_err(dbenv, "failed to read the log at [%lu][%lu]",
			    (u_long)lsnp->file, (u_long)lsnp->offset);
			goto err;
		}
		if ((ret = __db_dispatch(dbenv, dbenv->recover_dtab,
		    dbenv->recover_dtab_size, &data_dbt, lsnp,
		    DB_TXN_APPLY, txninfo)) != 0) {
			__db_err(dbenv, "transaction failed at [%lu][%lu]",
			    (u_long)lsnp->file, (u_long)lsnp->offset);
			goto err;
		}
	}

err:	memset(&req, 0, sizeof(req));
	req.op = DB_LOCK_PUT_ALL;
	if ((t_ret =
	     __lock_vec(dbenv, lockid, 0, &req, 1, &lvp)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret = __lock_id_free(dbenv, lockid)) != 0 && ret == 0)
		ret = t_ret;

err1: 	if (rectype == DB___txn_regop) 
		__os_free(dbenv, txn_args);
	else
		__os_free(dbenv, prep_args);
	if (lc.nalloc != 0)
		__os_free(dbenv, lc.array);

	if (logc != NULL && (t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;

	if (txninfo != NULL)
		__db_txnlist_end(dbenv, txninfo);

	if (F_ISSET(&data_dbt, DB_DBT_REALLOC) && data_dbt.data != NULL)
		__os_ufree(dbenv, data_dbt.data);

	if (ret == 0)
		/*
		 * We don't hold the rep mutex, and could miscount if we race.
		 */
		rep->stat.st_txns_applied++;

	return (ret);
}

/*
 * __rep_collect_txn
 *	Recursive function that will let us visit every entry in a transaction
 *	chain including all child transactions so that we can then apply
 *	the entire transaction family at once.
 */
static int
__rep_collect_txn(dbenv, lsnp, lc)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	LSN_COLLECTION *lc;
{
	__txn_child_args *argp;
	DB_LOGC *logc;
	DB_LSN c_lsn;
	DBT data;
	u_int32_t rectype;
	int nalloc, ret, t_ret;

	memset(&data, 0, sizeof(data));
	F_SET(&data, DB_DBT_REALLOC);

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	while (!IS_ZERO_LSN(*lsnp) &&
	    (ret = __log_c_get(logc, lsnp, &data, DB_SET)) == 0) {
		memcpy(&rectype, data.data, sizeof(rectype));
		if (rectype == DB___txn_child) {
			if ((ret = __txn_child_read(dbenv,
			    data.data, &argp)) != 0)
				goto err;
			c_lsn = argp->c_lsn;
			*lsnp = argp->prev_lsn;
			__os_free(dbenv, argp);
			ret = __rep_collect_txn(dbenv, &c_lsn, lc);
		} else {
			if (lc->nalloc < lc->nlsns + 1) {
				nalloc = lc->nalloc == 0 ? 20 : lc->nalloc * 2;
				if ((ret = __os_realloc(dbenv,
				    nalloc * sizeof(DB_LSN), &lc->array)) != 0)
					goto err;
				lc->nalloc = nalloc;
			}
			lc->array[lc->nlsns++] = *lsnp;

			/*
			 * Explicitly copy the previous lsn.  The record
			 * starts with a u_int32_t record type, a u_int32_t
			 * txn id, and then the DB_LSN (prev_lsn) that we
			 * want.  We copy explicitly because we have no idea
			 * what kind of record this is.
			 */
			memcpy(lsnp, (u_int8_t *)data.data +
			    sizeof(u_int32_t) + sizeof(u_int32_t),
			    sizeof(DB_LSN));
		}

		if (ret != 0)
			goto err;
	} 
	if (ret != 0)
		__db_err(dbenv, "collect failed at: [%lu][%lu]",
		    (u_long)lsnp->file, (u_long)lsnp->offset);

err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	if (data.data != NULL)
		__os_ufree(dbenv, data.data);
	return (ret);
}

/*
 * __rep_lsn_cmp --
 *	qsort-type-compatible wrapper for log_compare.
 */
static int
__rep_lsn_cmp(lsn1, lsn2)
	const void *lsn1, *lsn2;
{

	return (log_compare((DB_LSN *)lsn1, (DB_LSN *)lsn2));
}

/*
 * __rep_newfile --
 *	NEWFILE messages have the LSN of the last record in the previous
 * log file.  When applying a NEWFILE message, make sure we haven't already
 * swapped files.
 */
static int
__rep_newfile(dbenv, rc, lsnp)
	DB_ENV *dbenv;
	REP_CONTROL *rc;
	DB_LSN *lsnp;
{
	DB_LOG *dblp;
	LOG *lp;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	if (rc->lsn.file + 1 > lp->lsn.file)
		return (__log_newfile(dblp, lsnp));
	else {
		/* We've already applied this NEWFILE.  Just ignore it. */
		*lsnp = lp->lsn;
		return (0);
	}
}

/*
 * __rep_tally --
 * PUBLIC: int __rep_tally __P((DB_ENV *, REP *, int, int *,
 * PUBLIC:    u_int32_t, u_int32_t));
 *
 * Handle incoming vote1 message on a client.  Called with the db_rep
 * mutex held.  This function will return 0 if we successfully tally
 * the vote and non-zero if the vote is ignored.  This will record
 * both VOTE1 and VOTE2 records, depending on which region offset the
 * caller passed in.
 */
int
__rep_tally(dbenv, rep, eid, countp, egen, vtoff)
	DB_ENV *dbenv;
	REP *rep;
	int eid, *countp;
	u_int32_t egen, vtoff;
{
	REP_VTALLY *tally, *vtp;
	int i;

#ifndef DIAGNOSTIC
	COMPQUIET(rep, NULL);
#endif

	tally = R_ADDR((REGINFO *)dbenv->reginfo, vtoff);
	i = 0;
	vtp = &tally[i];
	while (i < *countp) {
		/*
		 * Ignore votes from earlier elections (i.e. we've heard
		 * from this site in this election, but its vote from an
		 * earlier election got delayed and we received it now).
		 * However, if we happened to hear from an earlier vote
		 * and we recorded it and we're now hearing from a later
		 * election we want to keep the updated one.  Note that
		 * updating the entry will not increase the count.
		 * Also ignore votes that are duplicates.
		 */
		if (vtp->eid == eid) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
			    "Tally found[%d] (%d, %lu), this vote (%d, %lu)",
				    i, vtp->eid, (u_long)vtp->egen,
				    eid, (u_long)egen);
#endif
			if (vtp->egen >= egen)
				return (1);
			else {
				vtp->egen = egen;
				return (0);
			}
		}
		i++;
		vtp = &tally[i];
	}
	/*
	 * If we get here, we have a new voter we haven't
	 * seen before.  Tally this vote.
	 */
#ifdef DIAGNOSTIC
	if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {
		if (vtoff == rep->tally_off)
			__db_err(dbenv, "Tallying VOTE1[%d] (%d, %lu)",
			    i, eid, (u_long)egen);
		else
			__db_err(dbenv, "Tallying VOTE2[%d] (%d, %lu)",
			    i, eid, (u_long)egen);
	}
#endif
	vtp->eid = eid;
	vtp->egen = egen;
	(*countp)++;
	return (0);
}

/*
 * __rep_cmp_vote --
 * PUBLIC: void __rep_cmp_vote __P((DB_ENV *, REP *, int *, DB_LSN *,
 * PUBLIC:     int, int, int));
 *
 * Compare incoming vote1 message on a client.  Called with the db_rep
 * mutex held.
 */
void
__rep_cmp_vote(dbenv, rep, eidp, lsnp, priority, gen, tiebreaker)
	DB_ENV *dbenv;
	REP *rep;
	int *eidp;
	DB_LSN *lsnp;
	int priority, gen, tiebreaker;
{
	int cmp;

#ifndef DIAGNOSTIC
	COMPQUIET(dbenv, NULL);
#endif
	cmp = log_compare(lsnp, &rep->w_lsn);
	/*
	 * If we've seen more than one, compare us to the best so far.
	 * If we're the first, make ourselves the winner to start.
	 */
	if (rep->sites > 1 && priority != 0) {
		/*
		 * LSN is primary determinant. Then priority if LSNs
		 * are equal, then tiebreaker if both are equal.
		 */
		if (cmp > 0 ||
		    (cmp == 0 && (priority > rep->w_priority ||
		    (priority == rep->w_priority &&
		    (tiebreaker > rep->w_tiebreaker))))) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Accepting new vote");
#endif
			rep->winner = *eidp;
			rep->w_priority = priority;
			rep->w_lsn = *lsnp;
			rep->w_gen = gen;
			rep->w_tiebreaker = tiebreaker;
		}
	} else if (rep->sites == 1) {
		if (priority != 0) {
			/* Make ourselves the winner to start. */
			rep->winner = *eidp;
			rep->w_priority = priority;
			rep->w_gen = gen;
			rep->w_lsn = *lsnp;
			rep->w_tiebreaker = tiebreaker;
		} else {
			rep->winner = DB_EID_INVALID;
			rep->w_priority = 0;
			rep->w_gen = 0;
			ZERO_LSN(rep->w_lsn);
			rep->w_tiebreaker = 0;
		}
	}
	return;
}

/*
 * __rep_cmp_vote2 --
 * PUBLIC: int __rep_cmp_vote2 __P((DB_ENV *, REP *, int, u_int32_t));
 *
 * Compare incoming vote2 message with vote1's we've recorded.  Called
 * with the db_rep mutex held.  We return 0 if the VOTE2 is from a
 * site we've heard from and it is from this election.  Otherwise we return 1.
 */
int
__rep_cmp_vote2(dbenv, rep, eid, egen)
	DB_ENV *dbenv;
	REP *rep;
	int eid;
	u_int32_t egen;
{
	int i;
	REP_VTALLY *tally, *vtp;

	tally = R_ADDR((REGINFO *)dbenv->reginfo, rep->tally_off);
	i = 0;
	vtp = &tally[i];
	for (i = 0; i < rep->sites; i++) {
		vtp = &tally[i];
		if (vtp->eid == eid && vtp->egen == egen) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
			    "Found matching vote1 (%d, %lu), at %d of %d",
				    eid, (u_long)egen, i, rep->sites);
#endif
			return (0);
		}
	}
#ifdef DIAGNOSTIC
	if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
		__db_err(dbenv, "Did not find vote1 for eid %d, egen %lu",
		    eid, (u_long)egen);
#endif
	return (1);
}

static int
__rep_dorecovery(dbenv, lsnp, trunclsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp, *trunclsnp;
{
	DB_LSN lsn;
	DBT mylog;
	DB_LOGC *logc;
	int ret, t_ret, undo;
	u_int32_t rectype;
	__txn_regop_args *txnrec;

	/* Figure out if we are backing out any commited transactions. */
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	memset(&mylog, 0, sizeof(mylog));
	undo = 0;
	while (undo == 0 &&
	    (ret = __log_c_get(logc, &lsn, &mylog, DB_PREV)) == 0 &&
	    log_compare(&lsn, lsnp) > 0) {
		memcpy(&rectype, mylog.data, sizeof(rectype));
		if (rectype == DB___txn_regop) {
			if ((ret =
			    __txn_regop_read(dbenv, mylog.data, &txnrec)) != 0)
				goto err;
			if (txnrec->opcode != TXN_ABORT) {
				undo = 1;
			}
			__os_free(dbenv, txnrec);
		}
	}

	ret = __db_apprec(dbenv, lsnp, trunclsnp, undo, 0);

err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __rep_verify_match --
 *	We have just received a matching log record during verification.
 * Figure out if we're going to need to run recovery. If so, wait until
 * everything else has exited the library.  If not, set up the world
 * correctly and move forward.
 */
static int
__rep_verify_match(dbenv, rp, savetime)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	time_t savetime;
{
	DB_LOG *dblp;
	DB_LSN ckplsn, trunclsn;
	DB_REP *db_rep;
	LOG *lp;
	REP *rep;
	int done, master, ret, wait_cnt;
	u_int32_t unused;

	dblp = dbenv->lg_handle;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	lp = dblp->reginfo.primary;
	ret = 0;

	/*
	 * Check if the savetime is different than our current time stamp.
	 * If it is, then we're racing with another thread trying to recover
	 * and we lost.  We must give up.
	 */
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	done = savetime != rep->timestamp;
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	if (done) {
		MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
		return (0);
	}

	ZERO_LSN(lp->verify_lsn);

	/* Check if we our log is already up to date. */
	R_LOCK(dbenv, &dblp->reginfo);
	done = rp->lsn.file == lp->lsn.file &&
	    rp->lsn.offset + lp->len == lp->lsn.offset;
	if (done) {
		lp->ready_lsn = lp->lsn;
		ZERO_LSN(lp->waiting_lsn);
	}
	R_UNLOCK(dbenv, &dblp->reginfo);
	if (done)
		goto finish;	/* Yes, holding the mutex. */
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	if (F_ISSET(rep, REP_F_LOGSONLY)) {
		/*
		 * If we're a logs-only client, we can simply truncate
		 * the log to the point where it last agreed with the
		 * master's.
		 */
		INIT_LSN(ckplsn);
		if ((ret = __log_flush(dbenv, &rp->lsn)) != 0 || (ret =
		    __log_vtruncate(dbenv, &rp->lsn, &ckplsn, &trunclsn)) != 0)
			return (ret);
	} else {
		/*
		 * Make sure the world hasn't changed while we tried to get
		 * the lock.  If it hasn't then it's time for us to kick all
		 * operations out of DB and run recovery.
		 */
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		if (F_ISSET(rep, REP_F_READY) || rep->in_recovery != 0) {
			rep->stat.st_msgs_recover++;
			goto errunlock;
		}

		/* Phase 1: set REP_F_READY and wait for op_cnt to go to 0. */
		F_SET(rep, REP_F_READY);
		for (wait_cnt = 0; rep->op_cnt != 0;) {
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			__os_sleep(dbenv, 1, 0);
#ifdef DIAGNOSTIC
			if (++wait_cnt % 60 == 0)
				__db_err(dbenv,
	"Waiting for txn_cnt to run replication recovery for %d minutes",
				wait_cnt / 60);
#endif
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		}

		/*
		 * Phase 2: set in_recovery and wait for handle count to go
		 * to 0 and for the number of threads in __rep_process_message
		 * to go to 1 (us).
		 */
		rep->in_recovery = 1;
		for (wait_cnt = 0; rep->handle_cnt != 0 || rep->msg_th > 1;) {
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			__os_sleep(dbenv, 1, 0);
#ifdef DIAGNOSTIC
			if (++wait_cnt % 60 == 0)
				__db_err(dbenv,
"Waiting for handle/thread count to run replication recovery for %d minutes",
				wait_cnt / 60);
#endif
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		}

		/* OK, everyone is out, we can now run recovery. */
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

		if ((ret = __rep_dorecovery(dbenv, &rp->lsn, &trunclsn)) != 0) {
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			rep->in_recovery = 0;
			F_CLR(rep, REP_F_READY);
			goto errunlock;
		}
	}

	/*
	 * The log has been truncated (either directly by us or by __db_apprec)
	 * We want to make sure we're waiting for the LSN at the new end-of-log,
	 * not some later point.
	 */
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	lp->ready_lsn = trunclsn;
finish:	ZERO_LSN(lp->waiting_lsn);
	lp->wait_recs = 0;
	lp->rcvd_recs = 0;
	ZERO_LSN(lp->verify_lsn);

	/*
	 * Discard any log records we have queued;  we're about to re-request
	 * them, and can't trust the ones in the queue.  We need to set the
	 * DB_AM_RECOVER bit in this handle, so that the operation doesn't
	 * deadlock.
	 */
	F_SET(db_rep->rep_db, DB_AM_RECOVER);
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
	ret = __db_truncate(db_rep->rep_db, NULL, &unused, 0);
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	F_CLR(db_rep->rep_db, DB_AM_RECOVER);
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	rep->stat.st_log_queued = 0;
	rep->in_recovery = 0;
	F_CLR(rep, REP_F_NOARCHIVE | REP_F_READY | REP_F_RECOVER);

	if (ret != 0)
		goto errunlock;

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
	 * !!!
	 * We cannot assert the election flags though because
	 * somebody may have declared an election and then
	 * got an error, thus clearing the election flags
	 * but we still have an invalid master_id.
	 */
	master = rep->master_id;
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	if (master == DB_EID_INVALID)
		ret = 0;
	else
		(void)__rep_send_message(dbenv,
		    master, REP_ALL_REQ, &rp->lsn, NULL, 0);
	if (0) {
errunlock:
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	}
	return (ret);
}
