/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: repmgr_net.c,v 1.55 2007/06/11 18:29:34 alanb Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"
#include "dbinc/mp.h"

/*
 * The functions in this module implement a simple wire protocol for
 * transmitting messages, both replication messages and our own internal control
 * messages.  The protocol is as follows:
 *
 *      1 byte          - message type  (defined in repmgr_int.h)
 *      4 bytes         - size of control
 *      4 bytes         - size of rec
 *      ? bytes         - control
 *      ? bytes         - rec
 *
 * where both sizes are 32-bit binary integers in network byte order.
 * Either control or rec can have zero length, but even in this case the
 * 4-byte length will be present.
 *     Putting both lengths right up at the front allows us to read in fewer
 * phases, and allows us to allocate buffer space for both parts (plus a wrapper
 * struct) at once.
 */

/*
 * In sending a message, we first try to send it in-line, in the sending thread,
 * and without first copying the message, by using scatter/gather I/O, using
 * iovecs to point to the various pieces of the message.  If that all works
 * without blocking, that's optimal.
 *     If we find that, for a particular connection, we can't send without
 * blocking, then we must copy the message for sending later in the select()
 * thread.  In the course of doing that, we might as well "flatten" the message,
 * forming one single buffer, to simplify life.  Not only that, once we've gone
 * to the trouble of doing that, other sites to which we also want to send the
 * message (in the case of a broadcast), may as well take advantage of the
 * simplified structure also.
 *     This structure holds it all.  Note that this structure, and the
 * "flat_msg" structure, are allocated separately, because (1) the flat_msg
 * version is usually not needed; and (2) when it is needed, it will need to
 * live longer than the wrapping sending_msg structure.
 *     Note that, for the broadcast case, where we're going to use this
 * repeatedly, the iovecs is a template that must be copied, since in normal use
 * the iovecs pointers and lengths get adjusted after every partial write.
 */
struct sending_msg {
	REPMGR_IOVECS iovecs;
	u_int8_t type;
	u_int32_t control_size_buf, rec_size_buf;
	REPMGR_FLAT *fmsg;
};

static int __repmgr_send_broadcast
    __P((DB_ENV *, const DBT *, const DBT *, u_int *, u_int *));
static void setup_sending_msg
    __P((struct sending_msg *, u_int, const DBT *, const DBT *));
static int __repmgr_send_internal
    __P((DB_ENV *, REPMGR_CONNECTION *, struct sending_msg *));
static int enqueue_msg
    __P((DB_ENV *, REPMGR_CONNECTION *, struct sending_msg *, size_t));
static int flatten __P((DB_ENV *, struct sending_msg *));
static REPMGR_SITE *__repmgr_available_site __P((DB_ENV *, int));

/*
 * __repmgr_send --
 *	The send function for DB_ENV->rep_set_transport.
 *
 * !!!
 * This is only ever called as the replication transport call-back, which means
 * it's either on one of our message processing threads or an application
 * thread.  It mustn't be called from the select() thread, because we might call
 * __repmgr_bust_connection(..., FALSE) here, and that's not allowed in the
 * select() thread.
 *
 * PUBLIC: int __repmgr_send __P((DB_ENV *, const DBT *, const DBT *,
 * PUBLIC:     const DB_LSN *, int, u_int32_t));
 */
int
__repmgr_send(dbenv, control, rec, lsnp, eid, flags)
	DB_ENV *dbenv;
	const DBT *control, *rec;
	const DB_LSN *lsnp;
	int eid;
	u_int32_t flags;
{
	DB_REP *db_rep;
	u_int nsites, npeers, available, needed;
	int ret, t_ret;
	REPMGR_SITE *site;
	REPMGR_CONNECTION *conn;

	db_rep = dbenv->rep_handle;

	LOCK_MUTEX(db_rep->mutex);
	if (eid == DB_EID_BROADCAST) {
		if ((ret = __repmgr_send_broadcast(dbenv, control, rec,
			 &nsites, &npeers)) != 0)
			goto out;
	} else {
		/*
		 * If this is a request that can be sent anywhere, then see if
		 * we can send it to our peer (to save load on the master), but
		 * not if it's a rerequest, 'cuz that likely means we tried this
		 * already and failed.
		 */
		if ((flags & (DB_REP_ANYWHERE | DB_REP_REREQUEST)) ==
		    DB_REP_ANYWHERE &&
		    IS_VALID_EID(db_rep->peer) &&
		    (site = __repmgr_available_site(dbenv, db_rep->peer)) !=
		    NULL) {
			RPRINT(dbenv, (dbenv, "sending request to peer"));
		} else if ((site = __repmgr_available_site(dbenv, eid)) ==
		    NULL) {
			RPRINT(dbenv, (dbenv,
			    "ignoring message sent to unavailable site"));
			ret = DB_REP_UNAVAIL;
			goto out;
		}

		conn = site->ref.conn;
		if ((ret = __repmgr_send_one(dbenv, conn, REPMGR_REP_MESSAGE,
		    control, rec)) == DB_REP_UNAVAIL &&
		    (t_ret = __repmgr_bust_connection(dbenv, conn, FALSE)) != 0)
			ret = t_ret;
		if (ret != 0)
			goto out;

		nsites = 1;
		npeers = site->priority > 0 ? 1 : 0;
	}
	/*
	 * Right now, nsites and npeers represent the (maximum) number of sites
	 * we've attempted to begin sending the message to.  Of course we
	 * haven't really received any ack's yet.  But since we've only sent to
	 * nsites/npeers other sites, that's the maximum number of ack's we
	 * could possibly expect.  If even that number fails to satisfy our PERM
	 * policy, there's no point waiting for something that will never
	 * happen.
	 */
	if (LF_ISSET(DB_REP_PERMANENT)) {
		switch (db_rep->perm_policy) {
		case DB_REPMGR_ACKS_NONE:
			goto out;

		case DB_REPMGR_ACKS_ONE:
			needed = 1;
			available = nsites;
			break;

		case DB_REPMGR_ACKS_ALL:
			/* Number of sites in the group besides myself. */
			needed = __repmgr_get_nsites(db_rep) - 1;
			available = nsites;
			break;

		case DB_REPMGR_ACKS_ONE_PEER:
			needed = 1;
			available = npeers;
			break;

		case DB_REPMGR_ACKS_ALL_PEERS:
			/*
			 * Too hard to figure out "needed", since we're not
			 * keeping track of how many peers we have; so just skip
			 * the optimization in this case.
			 */
			needed = 1;
			available = npeers;
			break;

		case DB_REPMGR_ACKS_QUORUM:
			/*
			 * The minimum number of acks necessary to ensure that
			 * the transaction is durable if an election is held.
			 */
			needed = (__repmgr_get_nsites(db_rep) - 1) / 2;
			available = npeers;
			break;

		default:
			COMPQUIET(available, 0);
			COMPQUIET(needed, 0);
			(void)__db_unknown_path(dbenv, "__repmgr_send");
			break;
		}
		if (available < needed) {
			ret = DB_REP_UNAVAIL;
			goto out;
		}
		/* In ALL_PEERS case, display of "needed" might be confusing. */
		RPRINT(dbenv, (dbenv,
		    "will await acknowledgement: need %u", needed));
		ret = __repmgr_await_ack(dbenv, lsnp);
	}

out:	UNLOCK_MUTEX(db_rep->mutex);
	if (ret != 0 && LF_ISSET(DB_REP_PERMANENT)) {
		STAT(db_rep->region->mstat.st_perm_failed++);
		DB_EVENT(dbenv, DB_EVENT_REP_PERM_FAILED, NULL);
	}
	return (ret);
}

static REPMGR_SITE *
__repmgr_available_site(dbenv, eid)
	DB_ENV *dbenv;
	int eid;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;

	db_rep = dbenv->rep_handle;
	site = SITE_FROM_EID(eid);
	if (site->state != SITE_CONNECTED)
		return (NULL);

	if (F_ISSET(site->ref.conn, CONN_CONNECTING))
		return (NULL);
	return (site);
}

/*
 * Sends message to all sites with which we currently have an active
 * connection.  Sets result parameters according to how many sites we attempted
 * to begin sending to, even if we did nothing more than queue it for later
 * delivery.
 *
 * !!!
 * Caller must hold dbenv->mutex.
 *
 * !!!
 * Note that this cannot be called from the select() thread, in case we call
 * __repmgr_bust_connection(..., FALSE).
 */
static int
__repmgr_send_broadcast(dbenv, control, rec, nsitesp, npeersp)
	DB_ENV *dbenv;
	const DBT *control, *rec;
	u_int *nsitesp, *npeersp;
{
	DB_REP *db_rep;
	struct sending_msg msg;
	REPMGR_CONNECTION *conn;
	REPMGR_SITE *site;
	u_int nsites, npeers;
	int ret;

	db_rep = dbenv->rep_handle;

	setup_sending_msg(&msg, REPMGR_REP_MESSAGE, control, rec);
	nsites = npeers = 0;

	/*
	 * Traverse the connections list.  Here, even in bust_connection, we
	 * don't unlink the current list entry, so we can use the TAILQ_FOREACH
	 * macro.
	 */
	TAILQ_FOREACH(conn, &db_rep->connections, entries) {
		if (F_ISSET(conn, CONN_CONNECTING | CONN_DEFUNCT) ||
		    !IS_VALID_EID(conn->eid))
			continue;

		if ((ret = __repmgr_send_internal(dbenv, conn, &msg)) == 0) {
			site = SITE_FROM_EID(conn->eid);
			nsites++;
			if (site->priority > 0)
				npeers++;
		} else if (ret == DB_REP_UNAVAIL) {
			if ((ret = __repmgr_bust_connection(
			     dbenv, conn, FALSE)) != 0)
				return (ret);
		} else
			return (ret);
	}

	*nsitesp = nsites;
	*npeersp = npeers;
	return (0);
}

/*
 * __repmgr_send_one --
 *	Send a message to a site, or if you can't just yet, make a copy of it
 * and arrange to have it sent later.  'rec' may be NULL, in which case we send
 * a zero length and no data.
 *
 * If we get an error, we take care of cleaning up the connection (calling
 * __repmgr_bust_connection()), so that the caller needn't do so.
 *
 * !!!
 * Note that the mutex should be held through this call.
 * It doubles as a synchronizer to make sure that two threads don't
 * intersperse writes that are part of two single messages.
 *
 * PUBLIC: int __repmgr_send_one __P((DB_ENV *, REPMGR_CONNECTION *,
 * PUBLIC:    u_int, const DBT *, const DBT *));
 */
int
__repmgr_send_one(dbenv, conn, msg_type, control, rec)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
	u_int msg_type;
	const DBT *control, *rec;
{
	struct sending_msg msg;

	setup_sending_msg(&msg, msg_type, control, rec);
	return (__repmgr_send_internal(dbenv, conn, &msg));
}

/*
 * Attempts a "best effort" to send a message on the given site.  If there is an
 * excessive backlog of message already queued on the connection, we simply drop
 * this message, and still return 0 even in this case.
 */
static int
__repmgr_send_internal(dbenv, conn, msg)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
	struct sending_msg *msg;
{
#define	OUT_QUEUE_LIMIT 10	/* arbitrary, for now */
	REPMGR_IOVECS iovecs;
	SITE_STRING_BUFFER buffer;
	int ret;
	size_t nw;
	size_t total_written;

	DB_ASSERT(dbenv, !F_ISSET(conn, CONN_CONNECTING));
	if (!STAILQ_EMPTY(&conn->outbound_queue)) {
		/*
		 * Output to this site is currently owned by the select()
		 * thread, so we can't try sending in-line here.  We can only
		 * queue the msg for later.
		 */
		RPRINT(dbenv, (dbenv, "msg to %s to be queued",
		    __repmgr_format_eid_loc(dbenv->rep_handle,
		    conn->eid, buffer)));
		if (conn->out_queue_length < OUT_QUEUE_LIMIT)
			return (enqueue_msg(dbenv, conn, msg, 0));
		else {
			RPRINT(dbenv, (dbenv, "queue limit exceeded"));
			STAT(dbenv->rep_handle->
			    region->mstat.st_msgs_dropped++);
			return (0);
		}
	}

	/*
	 * Send as much data to the site as we can, without blocking.  Keep
	 * writing as long as we're making some progress.  Make a scratch copy
	 * of iovecs for our use, since we destroy it in the process of
	 * adjusting pointers after each partial I/O.
	 */
	memcpy(&iovecs, &msg->iovecs, sizeof(iovecs));
	total_written = 0;
	while ((ret = __repmgr_writev(conn->fd, &iovecs.vectors[iovecs.offset],
	    iovecs.count-iovecs.offset, &nw)) == 0) {
		total_written += nw;
		if (__repmgr_update_consumed(&iovecs, nw)) /* all written */
			return (0);
	}

	if (ret != WOULDBLOCK) {
		__db_err(dbenv, ret, "socket writing failure");
		return (DB_REP_UNAVAIL);
	}

	RPRINT(dbenv, (dbenv, "wrote only %lu bytes to %s",
	    (u_long)total_written,
	    __repmgr_format_eid_loc(dbenv->rep_handle, conn->eid, buffer)));
	/*
	 * We can't send any more without blocking: queue (a pointer to) a
	 * "flattened" copy of the message, so that the select() thread will
	 * finish sending it later.
	 */
	if ((ret = enqueue_msg(dbenv, conn, msg, total_written)) != 0)
		return (ret);

	STAT(dbenv->rep_handle->region->mstat.st_msgs_queued++);

	/*
	 * Wake the main select thread so that it can discover that it has
	 * received ownership of this connection.  Note that we didn't have to
	 * do this in the previous case (above), because the non-empty queue
	 * implies that the select() thread is already managing ownership of
	 * this connection.
	 */
#ifdef DB_WIN32
	if (WSAEventSelect(conn->fd, conn->event_object,
	    FD_READ|FD_WRITE|FD_CLOSE) == SOCKET_ERROR) {
		ret = net_errno;
		__db_err(dbenv, ret, "can't add FD_WRITE event bit");
		return (ret);
	}
#endif
	return (__repmgr_wake_main_thread(dbenv));
}

/*
 * PUBLIC: int __repmgr_is_permanent __P((DB_ENV *, const DB_LSN *));
 *
 * Count up how many sites have ack'ed the given LSN.  Returns TRUE if enough
 * sites have ack'ed; FALSE otherwise.
 *
 * !!!
 * Caller must hold the mutex.
 */
int
__repmgr_is_permanent(dbenv, lsnp)
	DB_ENV *dbenv;
	const DB_LSN *lsnp;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;
	u_int eid, nsites, npeers;
	int is_perm, has_missing_peer;

	db_rep = dbenv->rep_handle;

	if (db_rep->perm_policy == DB_REPMGR_ACKS_NONE)
		return (TRUE);

	nsites = npeers = 0;
	has_missing_peer = FALSE;
	for (eid = 0; eid < db_rep->site_cnt; eid++) {
		site = SITE_FROM_EID(eid);
		if (site->priority == -1) {
			/*
			 * Never connected to this site: since we can't know
			 * whether it's a peer, assume the worst.
			 */
			has_missing_peer = TRUE;
			continue;
		}

		if (log_compare(&site->max_ack, lsnp) >= 0) {
			nsites++;
			if (site->priority > 0)
				npeers++;
		} else {
			/* This site hasn't ack'ed the message. */
			if (site->priority > 0)
				has_missing_peer = TRUE;
		}
	}

	switch (db_rep->perm_policy) {
	case DB_REPMGR_ACKS_ONE:
		is_perm = (nsites >= 1);
		break;
	case DB_REPMGR_ACKS_ONE_PEER:
		is_perm = (npeers >= 1);
		break;
	case DB_REPMGR_ACKS_QUORUM:
		/*
		 * The minimum number of acks necessary to ensure that the
		 * transaction is durable if an election is held (given that we
		 * always conduct elections according to the standard,
		 * recommended practice of requiring votes from a majority of
		 * sites).
		 */
		if (__repmgr_get_nsites(db_rep) == 2) {
			/*
			 * A group of 2 sites is, as always, a special case.
			 * For a transaction to be durable the other site has to
			 * have received it.
			 */
			is_perm = (npeers >= 1);
		} else
			is_perm = (npeers >= (__repmgr_get_nsites(db_rep)-1)/2);
		break;
	case DB_REPMGR_ACKS_ALL:
		/* Adjust by 1, since get_nsites includes local site. */
		is_perm = (nsites >= __repmgr_get_nsites(db_rep) - 1);
		break;
	case DB_REPMGR_ACKS_ALL_PEERS:
		if (db_rep->site_cnt < __repmgr_get_nsites(db_rep) - 1) {
			/* Assume missing site might be a peer. */
			has_missing_peer = TRUE;
		}
		is_perm = !has_missing_peer;
		break;
	default:
		is_perm = FALSE;
		(void)__db_unknown_path(dbenv, "__repmgr_is_permanent");
	}
	return (is_perm);
}

/*
 * Abandons a connection, to recover from an error.  Upon entry the conn struct
 * must be on the connections list.
 *
 * If the 'do_close' flag is true, we do the whole job; the clean-up includes
 * removing the struct from the list and freeing all its memory, so upon return
 * the caller must not refer to it any further.  Otherwise, we merely mark the
 * connection for clean-up later by the main thread.
 *
 * PUBLIC: int __repmgr_bust_connection __P((DB_ENV *,
 * PUBLIC:     REPMGR_CONNECTION *, int));
 *
 * !!!
 * Caller holds mutex.
 */
int
__repmgr_bust_connection(dbenv, conn, do_close)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
	int do_close;
{
	DB_REP *db_rep;
	int connecting, ret, eid;

	db_rep = dbenv->rep_handle;
	ret = 0;

	DB_ASSERT(dbenv, !TAILQ_EMPTY(&db_rep->connections));
	eid = conn->eid;
	connecting = F_ISSET(conn, CONN_CONNECTING);
	if (do_close)
		__repmgr_cleanup_connection(dbenv, conn);
	else {
		F_SET(conn, CONN_DEFUNCT);
		conn->eid = -1;
	}

	/*
	 * When we first accepted the incoming connection, we set conn->eid to
	 * -1 to indicate that we didn't yet know what site it might be from.
	 * If we then get here because we later decide it was a redundant
	 * connection, the following scary stuff will correctly not happen.
	 */
	if (IS_VALID_EID(eid)) {
		/* schedule_connection_attempt wakes the main thread. */
		if ((ret = __repmgr_schedule_connection_attempt(
		    dbenv, (u_int)eid, FALSE)) != 0)
			return (ret);

		/*
		 * If this connection had gotten no further than the CONNECTING
		 * state, this can't count as a loss of connection to the
		 * master.
		 */
		if (!connecting && eid == db_rep->master_eid) {
			(void)__memp_set_config(
			    dbenv, DB_MEMP_SYNC_INTERRUPT, 1);
			if ((ret = __repmgr_init_election(
			    dbenv, ELECT_FAILURE_ELECTION)) != 0)
				return (ret);
		}
	} else if (!do_close) {
		/*
		 * One way or another, make sure the main thread is poked, so
		 * that we do the deferred clean-up.
		 */
		ret = __repmgr_wake_main_thread(dbenv);
	}
	return (ret);
}

/*
 * PUBLIC: void __repmgr_cleanup_connection
 * PUBLIC:    __P((DB_ENV *, REPMGR_CONNECTION *));
 */
void
__repmgr_cleanup_connection(dbenv, conn)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
{
	DB_REP *db_rep;
	QUEUED_OUTPUT *out;
	REPMGR_FLAT *msg;
	DBT *dbt;

	db_rep = dbenv->rep_handle;

	TAILQ_REMOVE(&db_rep->connections, conn, entries);
	if (conn->fd != INVALID_SOCKET) {
		(void)closesocket(conn->fd);
#ifdef DB_WIN32
		(void)WSACloseEvent(conn->event_object);
#endif
	}

	/*
	 * Deallocate any input and output buffers we may have.
	 */
	if (conn->reading_phase == DATA_PHASE) {
		if (conn->msg_type == REPMGR_REP_MESSAGE)
			__os_free(dbenv, conn->input.rep_message);
		else {
			dbt = &conn->input.repmgr_msg.cntrl;
			__os_free(dbenv, dbt->data);
			dbt = &conn->input.repmgr_msg.rec;
			if (dbt->size > 0)
				__os_free(dbenv, dbt->data);
		}
	}
	while (!STAILQ_EMPTY(&conn->outbound_queue)) {
		out = STAILQ_FIRST(&conn->outbound_queue);
		STAILQ_REMOVE_HEAD(&conn->outbound_queue, entries);
		msg = out->msg;
		if (--msg->ref_count <= 0)
			__os_free(dbenv, msg);
		__os_free(dbenv, out);
	}

	__os_free(dbenv, conn);
}

static int
enqueue_msg(dbenv, conn, msg, offset)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
	struct sending_msg *msg;
	size_t offset;
{
	QUEUED_OUTPUT *q_element;
	int ret;

	if (msg->fmsg == NULL && ((ret = flatten(dbenv, msg)) != 0))
		return (ret);
	if ((ret = __os_malloc(dbenv, sizeof(QUEUED_OUTPUT), &q_element)) != 0)
		return (ret);
	q_element->msg = msg->fmsg;
	msg->fmsg->ref_count++;	/* encapsulation would be sweeter */
	q_element->offset = offset;

	/* Put it on the connection's outbound queue. */
	STAILQ_INSERT_TAIL(&conn->outbound_queue, q_element, entries);
	conn->out_queue_length++;
	return (0);
}

/*
 * The 'rec' DBT can be NULL, in which case we treat it like a zero-length DBT.
 * But 'control' is always present.
 */
static void
setup_sending_msg(msg, type, control, rec)
	struct sending_msg *msg;
	u_int type;
	const DBT *control, *rec;
{
	u_int32_t rec_size;

	/*
	 * The wire protocol is documented in a comment at the top of this
	 * module.
	 */
	__repmgr_iovec_init(&msg->iovecs);
	msg->type = type;
	__repmgr_add_buffer(&msg->iovecs, &msg->type, sizeof(msg->type));

	msg->control_size_buf = htonl(control->size);
	__repmgr_add_buffer(&msg->iovecs,
	    &msg->control_size_buf, sizeof(msg->control_size_buf));

	rec_size = rec == NULL ? 0 : rec->size;
	msg->rec_size_buf = htonl(rec_size);
	__repmgr_add_buffer(
	    &msg->iovecs, &msg->rec_size_buf, sizeof(msg->rec_size_buf));

	if (control->size > 0)
		__repmgr_add_dbt(&msg->iovecs, control);

	if (rec_size > 0)
		__repmgr_add_dbt(&msg->iovecs, rec);

	msg->fmsg = NULL;
}

/*
 * Convert a message stored as iovec pointers to various pieces, into flattened
 * form, by copying all the pieces, and then make the iovec just point to the
 * new simplified form.
 */
static int
flatten(dbenv, msg)
	DB_ENV *dbenv;
	struct sending_msg *msg;
{
	u_int8_t *p;
	size_t msg_size;
	int i, ret;

	DB_ASSERT(dbenv, msg->fmsg == NULL);

	msg_size = msg->iovecs.total_bytes;
	if ((ret = __os_malloc(dbenv, sizeof(*msg->fmsg) + msg_size,
	    &msg->fmsg)) != 0)
		return (ret);
	msg->fmsg->length = msg_size;
	msg->fmsg->ref_count = 0;
	p = &msg->fmsg->data[0];

	for (i = 0; i < msg->iovecs.count; i++) {
		memcpy(p, msg->iovecs.vectors[i].iov_base,
		    msg->iovecs.vectors[i].iov_len);
		p = &p[msg->iovecs.vectors[i].iov_len];
	}
	__repmgr_iovec_init(&msg->iovecs);
	__repmgr_add_buffer(&msg->iovecs, &msg->fmsg->data[0], msg_size);
	return (0);
}

/*
 * PUBLIC: int __repmgr_find_site __P((DB_ENV *, const char *, u_int));
 */
int
__repmgr_find_site(dbenv, host, port)
	DB_ENV *dbenv;
	const char *host;
	u_int port;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;
	u_int i;

	db_rep = dbenv->rep_handle;
	for (i = 0; i < db_rep->site_cnt; i++) {
		site = &db_rep->sites[i];

		if (strcmp(site->net_addr.host, host) == 0 &&
		    site->net_addr.port == port)
			return ((int)i);
	}

	return (-1);
}

/*
 * Stash a copy of the given host name and port number into a convenient data
 * structure so that we can save it permanently.  This is kind of like a
 * constructor for a netaddr object, except that the caller supplies the memory
 * for the base struct (though not the subordinate attachments).
 *
 * All inputs are assumed to have been already validated.
 *
 * PUBLIC: int __repmgr_pack_netaddr __P((DB_ENV *, const char *,
 * PUBLIC:     u_int, ADDRINFO *, repmgr_netaddr_t *));
 */
int
__repmgr_pack_netaddr(dbenv, host, port, list, addr)
	DB_ENV *dbenv;
	const char *host;
	u_int port;
	ADDRINFO *list;
	repmgr_netaddr_t *addr;
{
	int ret;

	DB_ASSERT(dbenv, host != NULL);

	if ((ret = __os_strdup(dbenv, host, &addr->host)) != 0)
		return (ret);
	addr->port = (u_int16_t)port;
	addr->address_list = list;
	addr->current = NULL;
	return (0);
}

/*
 * PUBLIC: int __repmgr_getaddr __P((DB_ENV *,
 * PUBLIC:     const char *, u_int, int, ADDRINFO **));
 */
int
__repmgr_getaddr(dbenv, host, port, flags, result)
	DB_ENV *dbenv;
	const char *host;
	u_int port;
	int flags;    /* Matches struct addrinfo declaration. */
	ADDRINFO **result;
{
	ADDRINFO *answer, hints;
	char buffer[10];		/* 2**16 fits in 5 digits. */
#ifdef DB_WIN32
	int ret;
#endif

	/*
	 * Ports are really 16-bit unsigned values, but it's too painful to
	 * push that type through the API.
	 */
	if (port > UINT16_MAX) {
		__db_errx(dbenv, "port %u larger than max port %u",
		    port, UINT16_MAX);
		return (EINVAL);
	}

#ifdef DB_WIN32
	if (!dbenv->rep_handle->wsa_inited &&
	    (ret = __repmgr_wsa_init(dbenv)) != 0)
		return (ret);
#endif

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = flags;
	(void)snprintf(buffer, sizeof(buffer), "%u", port);

	/*
	 * Although it's generally bad to discard error information, the return
	 * code from __db_getaddrinfo is undependable.  Our callers at least
	 * would like to be able to distinguish errors in getaddrinfo (which we
	 * want to consider to be re-tryable), from other failure (e.g., EINVAL,
	 * above).
	 */
	if (__db_getaddrinfo(dbenv, host, port, buffer, &hints, &answer) != 0)
		return (DB_REP_UNAVAIL);
	*result = answer;

	return (0);
}

/*
 * Adds a new site to our array of known sites (unless it already exists),
 * and schedules it for immediate connection attempt.  Whether it exists or not,
 * we set newsitep, either to the already existing site, or to the newly created
 * site.  Unless newsitep is passed in as NULL, which is allowed.
 *
 * PUBLIC: int __repmgr_add_site
 * PUBLIC:     __P((DB_ENV *, const char *, u_int, REPMGR_SITE **));
 *
 * !!!
 * Caller is expected to hold the mutex.
 */
int
__repmgr_add_site(dbenv, host, port, newsitep)
	DB_ENV *dbenv;
	const char *host;
	u_int port;
	REPMGR_SITE **newsitep;
{
	DB_REP *db_rep;
	ADDRINFO *address_list;
	repmgr_netaddr_t addr;
	REPMGR_SITE *site;
	int ret, eid;

	ret = 0;
	db_rep = dbenv->rep_handle;

	if (IS_VALID_EID(eid = __repmgr_find_site(dbenv, host, port))) {
		site = SITE_FROM_EID(eid);
		ret = EEXIST;
		goto out;
	}

	if ((ret = __repmgr_getaddr(
	    dbenv, host, port, 0, &address_list)) == DB_REP_UNAVAIL) {
		/* Allow re-tryable errors.  We'll try again later. */
		address_list = NULL;
	} else if (ret != 0)
		return (ret);

	if ((ret = __repmgr_pack_netaddr(
	    dbenv, host, port, address_list, &addr)) != 0) {
		__db_freeaddrinfo(dbenv, address_list);
		return (ret);
	}

	if ((ret = __repmgr_new_site(dbenv, &site, &addr, SITE_IDLE)) != 0) {
		__repmgr_cleanup_netaddr(dbenv, &addr);
		return (ret);
	}

	if (db_rep->selector != NULL &&
	    (ret = __repmgr_schedule_connection_attempt(
	    dbenv, (u_int)EID_FROM_SITE(site), TRUE)) != 0)
		return (ret);

	/* Note that we should only come here for success and EEXIST. */
out:
	if (newsitep != NULL)
		*newsitep = site;
	return (ret);
}

/*
 * Initializes net-related memory in the db_rep handle.
 *
 * PUBLIC: int __repmgr_net_create __P((DB_ENV *, DB_REP *));
 */
int
__repmgr_net_create(dbenv, db_rep)
	DB_ENV *dbenv;
	DB_REP *db_rep;
{
	COMPQUIET(dbenv, NULL);

	db_rep->listen_fd = INVALID_SOCKET;
	db_rep->master_eid = DB_EID_INVALID;

	TAILQ_INIT(&db_rep->connections);
	TAILQ_INIT(&db_rep->retries);

	return (0);
}

/*
 * listen_socket_init --
 *	Initialize a socket for listening.  Sets
 *	a file descriptor for the socket, ready for an accept() call
 *	in a thread that we're happy to let block.
 *
 * PUBLIC:  int __repmgr_listen __P((DB_ENV *));
 */
int
__repmgr_listen(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	ADDRINFO *ai;
	char *why;
	int sockopt, ret;
	socket_t s;

	db_rep = dbenv->rep_handle;

	/* Use OOB value as sentinel to show no socket open. */
	s = INVALID_SOCKET;
	ai = ADDR_LIST_FIRST(&db_rep->my_addr);

	/*
	 * Given the assert is correct, we execute the loop at least once, which
	 * means 'why' will have been set by the time it's needed.  But I guess
	 * lint doesn't know about DB_ASSERT.
	 */
	COMPQUIET(why, "");
	DB_ASSERT(dbenv, ai != NULL);
	for (; ai != NULL; ai = ADDR_LIST_NEXT(&db_rep->my_addr)) {

		if ((s = socket(ai->ai_family,
		    ai->ai_socktype, ai->ai_protocol)) == INVALID_SOCKET) {
			why = "can't create listen socket";
			continue;
		}

		/*
		 * When testing, it's common to kill and restart regularly.  On
		 * some systems, this causes bind to fail with "address in use"
		 * errors unless this option is set.
		 */
		sockopt = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (sockopt_t)&sockopt,
		    sizeof(sockopt)) != 0) {
			why = "can't set REUSEADDR socket option";
			break;
		}

		if (bind(s, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0) {
			why = "can't bind socket to listening address";
			(void)closesocket(s);
			s = INVALID_SOCKET;
			continue;
		}

		if (listen(s, 5) != 0) {
			why = "listen()";
			break;
		}

		if ((ret = __repmgr_set_nonblocking(s)) != 0) {
			__db_err(dbenv, ret, "can't unblock listen socket");
			goto clean;
		}

		db_rep->listen_fd = s;
		return (0);
	}

	ret = net_errno;
	__db_err(dbenv, ret, why);
clean:	if (s != INVALID_SOCKET)
		(void)closesocket(s);
	return (ret);
}

/*
 * PUBLIC: int __repmgr_net_close __P((DB_ENV *));
 */
int
__repmgr_net_close(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REPMGR_CONNECTION *conn;
#ifndef DB_WIN32
	struct sigaction sigact;
#endif
	int ret;

	db_rep = dbenv->rep_handle;
	if (db_rep->listen_fd == INVALID_SOCKET)
		return (0);

	TAILQ_FOREACH(conn, &db_rep->connections, entries) {
		if (conn->fd != INVALID_SOCKET) {
			(void)closesocket(conn->fd);
			conn->fd = INVALID_SOCKET;
#ifdef DB_WIN32
			(void)WSACloseEvent(conn->event_object);
#endif
		}
	}

	ret = 0;
	if (closesocket(db_rep->listen_fd) == SOCKET_ERROR)
		ret = net_errno;

#ifdef DB_WIN32
	/* Shut down the Windows sockets DLL. */
	if (WSACleanup() == SOCKET_ERROR && ret == 0)
		ret = net_errno;
	db_rep->wsa_inited = FALSE;
#else
	/* Restore original SIGPIPE handling configuration. */
	if (db_rep->chg_sig_handler) {
		memset(&sigact, 0, sizeof(sigact));
		sigact.sa_handler = SIG_DFL;
		if (sigaction(SIGPIPE, &sigact, NULL) == -1 && ret == 0)
			ret = errno;
	}
#endif
	db_rep->listen_fd = INVALID_SOCKET;
	return (ret);
}

/*
 * PUBLIC: void __repmgr_net_destroy __P((DB_ENV *, DB_REP *));
 */
void
__repmgr_net_destroy(dbenv, db_rep)
	DB_ENV *dbenv;
	DB_REP *db_rep;
{
	REPMGR_CONNECTION *conn;
	REPMGR_RETRY *retry;
	REPMGR_SITE *site;
	u_int i;

	__repmgr_cleanup_netaddr(dbenv, &db_rep->my_addr);

	if (db_rep->sites == NULL)
		return;

	while (!TAILQ_EMPTY(&db_rep->retries)) {
		retry = TAILQ_FIRST(&db_rep->retries);
		TAILQ_REMOVE(&db_rep->retries, retry, entries);
		__os_free(dbenv, retry);
	}

	while (!TAILQ_EMPTY(&db_rep->connections)) {
		conn = TAILQ_FIRST(&db_rep->connections);
		__repmgr_cleanup_connection(dbenv, conn);
	}

	for (i = 0; i < db_rep->site_cnt; i++) {
		site = &db_rep->sites[i];
		__repmgr_cleanup_netaddr(dbenv, &site->net_addr);
	}
	__os_free(dbenv, db_rep->sites);
	db_rep->sites = NULL;
}
