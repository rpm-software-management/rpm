/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 *	Sleepycat Software.  All rights reserved.
 */

#ifndef _REP_H_
#define	_REP_H_

#define	REP_ALIVE	1	/* I am alive message. */
#define	REP_ALIVE_REQ	2	/* Request for alive messages. */
#define	REP_ALL_REQ	3	/* Request all log records greater than LSN. */
#define	REP_ELECT	4	/* Indicates that all listeners should */
				/* begin master election */
#define	REP_FILE	6	/* Page of a database file. */
#define	REP_FILE_REQ	7	/* Request for a database file. */
#define	REP_LOG		8	/* Log record. */
#define	REP_LOG_REQ	9	/* Request for a log record. */
#define	REP_MASTER_REQ	10	/* Who is the master */
#define	REP_NEWCLIENT	11	/* Announces the presence of a new client. */
#define	REP_NEWFILE	12	/* Announce a log file change. */
#define	REP_NEWMASTER	13	/* Announces who the master is. */
#define	REP_NEWSITE	14	/* Announces that a site has heard from a new
				 * site; like NEWCLIENT, but indirect.  A
				 * NEWCLIENT message comes directly from the new
				 * client while a NEWSITE comes indirectly from
				 * someone who heard about a NEWSITE.
				 */
#define	REP_PAGE	15	/* Database page. */
#define	REP_PAGE_REQ	16	/* Request for a database page. */
#define	REP_PLIST	17	/* Database page list. */
#define	REP_PLIST_REQ	18	/* Request for a page list. */
#define	REP_VERIFY	19	/* A log record for verification. */
#define	REP_VERIFY_FAIL	20	/* The client is outdated. */
#define	REP_VERIFY_REQ	21	/* Request for a log record to verify. */
#define	REP_VOTE1	22	/* Send out your information for an election. */
#define	REP_VOTE2	23	/* Send a "you are master" vote. */

/* Used to consistently designate which messages ought to be received where. */
#define	MASTER_ONLY(dbenv)	\
	if (!F_ISSET(dbenv, DB_ENV_REP_MASTER)) return (EINVAL)

#define	CLIENT_ONLY(dbenv)	\
	if (!F_ISSET(dbenv, DB_ENV_REP_CLIENT)) return (EINVAL)

#define	ANYSITE(dbenv)

/* Shared replication structure. */

typedef struct __rep {
	DB_MUTEX	mutex;		/* Region lock. */
	u_int32_t	tally_off;	/* Offset of the tally region. */
	int		eid;		/* Environment id. */
	int		master_id;	/* ID of the master site. */
	u_int32_t	gen;		/* Replication generation number */
	int		asites;		/* Space allocated for sites. */
	int		nsites;		/* Number of sites in group. */
	int		priority;	/* My priority in an election. */

	/* Vote tallying information. */
	int		sites;		/* Sites heard from. */
	int		winner;		/* Current winner. */
	int		w_priority;	/* Winner priority. */
	u_int32_t	w_gen;		/* Winner generation. */
	DB_LSN		w_lsn;		/* Winner LSN. */
	int		votes;		/* Number of votes for this site. */

#define	REP_F_EPHASE1	0x01		/* In phase 1 of election. */
#define	REP_F_EPHASE2	0x02		/* In phase 2 of election. */
#define	REP_F_LOGSONLY	0x04		/* Log-site only; cannot be upgraded. */
#define	REP_F_MASTER	0x08		/* Master replica. */
#define	REP_F_RECOVER	0x10
#define	REP_F_UPGRADE	0x20		/* Upgradeable replica. */
#define	REP_ISCLIENT	(REP_F_UPGRADE | REP_F_LOGSONLY)
	u_int32_t	flags;
} REP;

#define	IN_ELECTION(R)		F_ISSET((R), REP_F_EPHASE1 | REP_F_EPHASE2)
#define	ELECTION_DONE(R)	F_CLR((R), REP_F_EPHASE1 | REP_F_EPHASE2)

/*
 * Per-process replication structure.
 */
struct __db_rep {
	DB_MUTEX	*mutexp;
	DB		*rep_db;	/* Bookkeeping database. */
	REP		*region;	/* In memory structure. */
	int		(*rep_send)	/* Send function. */
			    __P((DB_ENV *,
			    const DBT *, const DBT *, int, u_int32_t));
};

/*
 * Control structure for replication communication infrastructure.
 *
 * Note that the version information should be at the beginning of the
 * structure, so that we can rearrange the rest of it while letting the
 * version checks continue to work.  DB_REPVERSION should be revved any time
 * the rest of the structure changes.
 */
typedef struct __rep_control {
#define	DB_REPVERSION	1
	u_int32_t	rep_version;	/* Replication version number. */
	u_int32_t	log_version;	/* Log version number. */

	DB_LSN		lsn;		/* Log sequence number. */
	u_int32_t	rectype;	/* Message type. */
	u_int32_t	gen;		/* Generation number. */
	u_int32_t	flags;		/* log_put flag value. */
} REP_CONTROL;

/* Election vote information. */
typedef struct __rep_vote {
	int	priority;		/* My site's priority. */
	int	nsites;			/* Number of sites I've been in
					 * communication with. */
} REP_VOTE_INFO;

/*
 * This structure takes care of representing a transaction.
 * It holds all the records, sorted by page number so that
 * we can obtain locks and apply updates in a deadlock free
 * order.
 */
typedef struct __lsn_page {
	DB_LSN		lsn;
	u_int32_t	fid;
	DB_LOCK_ILOCK	pgdesc;
#define	LSN_PAGE_NOLOCK		0x0001	/* No lock necessary for log rec. */
	u_int32_t	flags;
} LSN_PAGE;

typedef struct __txn_recs {
	int		npages;
	int		nalloc;
	LSN_PAGE	*array;
	u_int32_t	txnid;
	u_int32_t	lockid;
} TXN_RECS;

/*
 * This is used by the page-prep routines to do the lock_vec call to
 * apply the updates for a single transaction or a collection of
 * transactions.
 */
typedef struct _linfo {
	int		n;
	DB_LOCKREQ	*reqs;
	DBT		*objs;
} linfo_t;

#include "rep_ext.h"
#endif	/* _REP_H_ */
