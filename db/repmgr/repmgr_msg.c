/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: repmgr_msg.c,v 1.36 2007/06/11 18:29:34 alanb Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"

static int message_loop __P((DB_ENV *));
static int process_message __P((DB_ENV*, DBT*, DBT*, int));
static int handle_newsite __P((DB_ENV *, const DBT *));
static int ack_message __P((DB_ENV *, u_int32_t, DB_LSN *));

/*
 * PUBLIC: void *__repmgr_msg_thread __P((void *));
 */
void *
__repmgr_msg_thread(args)
	void *args;
{
	DB_ENV *dbenv = args;
	int ret;

	if ((ret = message_loop(dbenv)) != 0) {
		__db_err(dbenv, ret, "message thread failed");
		__repmgr_thread_failure(dbenv, ret);
	}
	return (NULL);
}

static int
message_loop(dbenv)
	DB_ENV *dbenv;
{
	REPMGR_MESSAGE *msg;
	int ret;

	while ((ret = __repmgr_queue_get(dbenv, &msg)) == 0) {
		while ((ret = process_message(dbenv, &msg->control, &msg->rec,
		    msg->originating_eid)) == DB_LOCK_DEADLOCK)
			RPRINT(dbenv, (dbenv, "repmgr deadlock retry"));

		__os_free(dbenv, msg);
		if (ret != 0)
			return (ret);
	}

	return (ret == DB_REP_UNAVAIL ? 0 : ret);
}

static int
process_message(dbenv, control, rec, eid)
	DB_ENV *dbenv;
	DBT *control, *rec;
	int eid;
{
	DB_REP *db_rep;
	REP *rep;
	DB_LSN permlsn;
	int ret;
	u_int32_t generation;

	db_rep = dbenv->rep_handle;

	/*
	 * Save initial generation number, in case it changes in a close race
	 * with a NEWMASTER.  See msgdir.10000/10039/msg00086.html.
	 */
	generation = db_rep->generation;

	switch (ret =
	    __rep_process_message(dbenv, control, rec, eid, &permlsn)) {
	case 0:
		if (db_rep->takeover_pending) {
			db_rep->takeover_pending = FALSE;
			return (__repmgr_become_master(dbenv));
		}
		break;

	case DB_REP_NEWSITE:
		return (handle_newsite(dbenv, rec));

	case DB_REP_HOLDELECTION:
		LOCK_MUTEX(db_rep->mutex);
		ret = __repmgr_init_election(dbenv, ELECT_ELECTION);
		UNLOCK_MUTEX(db_rep->mutex);
		if (ret != 0)
			return (ret);
		break;

	case DB_REP_DUPMASTER:
		if ((ret = __repmgr_repstart(dbenv, DB_REP_CLIENT)) != 0)
			return (ret);
		LOCK_MUTEX(db_rep->mutex);
		ret = __repmgr_init_election(dbenv, ELECT_ELECTION);
		UNLOCK_MUTEX(db_rep->mutex);
		if (ret != 0)
			return (ret);
		break;

	case DB_REP_ISPERM:
		/*
		 * Don't bother sending ack if master doesn't care about it.
		 */
		rep = db_rep->region;
		if (db_rep->perm_policy == DB_REPMGR_ACKS_NONE ||
		    (IS_PEER_POLICY(db_rep->perm_policy) &&
		    rep->priority == 0))
			break;

		if ((ret = ack_message(dbenv, generation, &permlsn)) != 0)
			return (ret);

		break;

	case DB_REP_NOTPERM: /* FALLTHROUGH */
	case DB_REP_IGNORE: /* FALLTHROUGH */
	case DB_LOCK_DEADLOCK:
		break;

	default:
		__db_err(dbenv, ret, "DB_ENV->rep_process_message");
		return (ret);
	}
	return (0);
}

/*
 * Handle replication-related events.  Returns only 0 or DB_EVENT_NOT_HANDLED;
 * no other error returns are tolerated.
 *
 * PUBLIC: int __repmgr_handle_event __P((DB_ENV *, u_int32_t, void *));
 */
int
__repmgr_handle_event(dbenv, event, info)
	DB_ENV *dbenv;
	u_int32_t event;
	void *info;
{
	DB_REP *db_rep;

	db_rep = dbenv->rep_handle;

	if (db_rep->selector == NULL) {
		/* Repmgr is not in use, so all events go to application. */
		return (DB_EVENT_NOT_HANDLED);
	}

	switch (event) {
	case DB_EVENT_REP_ELECTED:
		DB_ASSERT(dbenv, info == NULL);

		db_rep->found_master = TRUE;
		db_rep->takeover_pending = TRUE;

		/*
		 * The application doesn't really need to see this, because the
		 * purpose of this event is to tell the winning site that it
		 * should call rep_start(MASTER), and in repmgr we do that
		 * automatically.  Still, they could conceivably be curious, and
		 * it doesn't hurt anything to let them know.
		 */
		break;
	case DB_EVENT_REP_NEWMASTER:
		DB_ASSERT(dbenv, info != NULL);

		db_rep->found_master = TRUE;
		db_rep->master_eid = *(int *)info;
		__repmgr_stash_generation(dbenv);

		/* Application still needs to see this. */
		break;
	default:
		break;
	}
	return (DB_EVENT_NOT_HANDLED);
}

/*
 * Acknowledges a message.
 *
 * !!!
 * Note that this cannot be called from the select() thread, in case we call
 * __repmgr_bust_connection(..., FALSE).
 */
static int
ack_message(dbenv, generation, lsn)
	DB_ENV *dbenv;
	u_int32_t generation;
	DB_LSN *lsn;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;
	REPMGR_CONNECTION *conn;
	DB_REPMGR_ACK ack;
	DBT control2, rec2;
	int ret;

	db_rep = dbenv->rep_handle;
	/*
	 * Regardless of where a message came from, all ack's go to the master
	 * site.  If we're not in touch with the master, we drop it, since
	 * there's not much else we can do.
	 */
	if (!IS_VALID_EID(db_rep->master_eid) ||
	    db_rep->master_eid == SELF_EID) {
		RPRINT(dbenv, (dbenv,
		    "dropping ack with master %d", db_rep->master_eid));
		return (0);
	}

	ret = 0;
	LOCK_MUTEX(db_rep->mutex);
	site = SITE_FROM_EID(db_rep->master_eid);
	if (site->state == SITE_CONNECTED &&
	    !F_ISSET(site->ref.conn, CONN_CONNECTING)) {

		ack.generation = generation;
		memcpy(&ack.lsn, lsn, sizeof(DB_LSN));
		control2.data = &ack;
		control2.size = sizeof(ack);
		rec2.size = 0;

		conn = site->ref.conn;
		if ((ret = __repmgr_send_one(dbenv, conn, REPMGR_ACK,
		    &control2, &rec2)) == DB_REP_UNAVAIL)
			ret = __repmgr_bust_connection(dbenv, conn, FALSE);
	}

	UNLOCK_MUTEX(db_rep->mutex);
	return (ret);
}

/*
 * Does everything necessary to handle the processing of a NEWSITE return.
 */
static int
handle_newsite(dbenv, rec)
	DB_ENV *dbenv;
	const DBT *rec;
{
	ADDRINFO *ai;
	DB_REP *db_rep;
	REPMGR_SITE *site;
	SITE_STRING_BUFFER buffer;
	repmgr_netaddr_t *addr;
	size_t hlen;
	u_int16_t port;
	int ret;
	char *host;

	db_rep = dbenv->rep_handle;
	/*
	 * Check if we got sent connect information and if we did, if
	 * this is me or if we already have a connection to this new
	 * site.  If we don't, establish a new one.
	 *
	 * Unmarshall the cdata: a 2-byte port number, in network byte order,
	 * followed by the host name string, which should already be
	 * null-terminated, but let's make sure.
	 */
	if (rec->size < sizeof(port) + 1) {
		__db_errx(dbenv, "unexpected cdata size, msg ignored");
		return (0);
	}
	memcpy(&port, rec->data, sizeof(port));
	port = ntohs(port);

	host = (char*)((u_int8_t*)rec->data + sizeof(port));
	hlen = (rec->size - sizeof(port)) - 1;
	host[hlen] = '\0';

	/* It's me, do nothing. */
	if (strcmp(host, db_rep->my_addr.host) == 0 &&
	    port == db_rep->my_addr.port) {
		RPRINT(dbenv, (dbenv, "repmgr ignores own NEWSITE info"));
		return (0);
	}

	LOCK_MUTEX(db_rep->mutex);
	if ((ret = __repmgr_add_site(dbenv, host, port, &site)) == EEXIST) {
		RPRINT(dbenv, (dbenv,
		    "NEWSITE info from %s was already known",
		    __repmgr_format_site_loc(site, buffer)));
		/*
		 * In case we already know about this site only because it
		 * first connected to us, we may not yet have had a chance to
		 * look up its addresses.  Even though we don't need them just
		 * now, this is an advantageous opportunity to get them since we
		 * can do so away from the critical select thread.  Give up only
		 * for a disastrous failure.
		 */
		addr = &site->net_addr;
		if (addr->address_list == NULL) {
			if ((ret = __repmgr_getaddr(dbenv,
			    addr->host, addr->port, 0, &ai)) == 0)
				addr->address_list = ai;
			else if (ret != DB_REP_UNAVAIL)
				goto unlock;
		}

		ret = 0;
		if (site->state == SITE_CONNECTED)
			goto unlock; /* Nothing to do. */
	} else {
		if (ret != 0)
			goto unlock;
		RPRINT(dbenv, (dbenv, "NEWSITE info added %s",
		    __repmgr_format_site_loc(site, buffer)));
	}

	/*
	 * Wake up the main thread to connect to the new or reawakened
	 * site.
	 */
	ret = __repmgr_wake_main_thread(dbenv);

unlock: UNLOCK_MUTEX(db_rep->mutex);
	return (ret);
}

/*
 * PUBLIC: void __repmgr_stash_generation __P((DB_ENV *));
 */
void
__repmgr_stash_generation(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	db_rep->generation = rep->gen;
}
