/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2006,2007 Oracle.  All rights reserved.
 *
 * $Id: repmgr_sel.c,v 1.36 2007/06/11 18:29:34 alanb Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"

static int __repmgr_connect __P((DB_ENV*, socket_t *, REPMGR_SITE *));
static int dispatch_phase_completion __P((DB_ENV *, REPMGR_CONNECTION *));
static int notify_handshake __P((DB_ENV *, REPMGR_CONNECTION *));
static int record_ack __P((DB_ENV *, REPMGR_SITE *, DB_REPMGR_ACK *));
static int __repmgr_try_one __P((DB_ENV *, u_int));

/*
 * PUBLIC: void *__repmgr_select_thread __P((void *));
 */
void *
__repmgr_select_thread(args)
	void *args;
{
	DB_ENV *dbenv = args;
	int ret;

	if ((ret = __repmgr_select_loop(dbenv)) != 0) {
		__db_err(dbenv, ret, "select loop failed");
		__repmgr_thread_failure(dbenv, ret);
	}
	return (NULL);
}

/*
 * PUBLIC: int __repmgr_accept __P((DB_ENV *));
 *
 * !!!
 * Only ever called in the select() thread, since we may call
 * __repmgr_bust_connection(..., TRUE).
 */
int
__repmgr_accept(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REPMGR_CONNECTION *conn;
	struct sockaddr_in siaddr;
	socklen_t addrlen;
	socket_t s;
	int ret;
#ifdef DB_WIN32
	WSAEVENT event_obj;
#endif

	db_rep = dbenv->rep_handle;
	addrlen = sizeof(siaddr);
	if ((s = accept(db_rep->listen_fd, (struct sockaddr *)&siaddr,
	    &addrlen)) == -1) {
		/*
		 * Some errors are innocuous and so should be ignored.  MSDN
		 * Library documents the Windows ones; the Unix ones are
		 * advocated in Stevens' UNPv1, section 16.6; and Linux
		 * Application Development, p. 416.
		 */
		switch (ret = net_errno) {
#ifdef DB_WIN32
		case WSAECONNRESET:
		case WSAEWOULDBLOCK:
#else
		case EINTR:
		case EWOULDBLOCK:
		case ECONNABORTED:
		case ENETDOWN:
#ifdef EPROTO
		case EPROTO:
#endif
		case ENOPROTOOPT:
		case EHOSTDOWN:
#ifdef ENONET
		case ENONET:
#endif
		case EHOSTUNREACH:
		case EOPNOTSUPP:
		case ENETUNREACH:
#endif
			RPRINT(dbenv, (dbenv,
			    "accept error %d considered innocuous", ret));
			return (0);
		default:
			__db_err(dbenv, ret, "accept error");
			return (ret);
		}
	}
	RPRINT(dbenv, (dbenv, "accepted a new connection"));

	if ((ret = __repmgr_set_nonblocking(s)) != 0) {
		__db_err(dbenv, ret, "can't set nonblock after accept");
		(void)closesocket(s);
		return (ret);
	}

#ifdef DB_WIN32
	if ((event_obj = WSACreateEvent()) == WSA_INVALID_EVENT) {
		ret = net_errno;
		__db_err(dbenv, ret, "can't create WSA event");
		(void)closesocket(s);
		return (ret);
	}
	if (WSAEventSelect(s, event_obj, FD_READ|FD_CLOSE) == SOCKET_ERROR) {
		ret = net_errno;
		__db_err(dbenv, ret, "can't set desired event bits");
		(void)WSACloseEvent(event_obj);
		(void)closesocket(s);
		return (ret);
	}
#endif
	if ((ret = __repmgr_new_connection(dbenv, &conn, s, 0)) != 0) {
#ifdef DB_WIN32
		(void)WSACloseEvent(event_obj);
#endif
		(void)closesocket(s);
		return (ret);
	}
	conn->eid = -1;
#ifdef DB_WIN32
	conn->event_object = event_obj;
#endif

	switch (ret = __repmgr_send_handshake(dbenv, conn)) {
	case 0:
		return (0);
	case DB_REP_UNAVAIL:
		return (__repmgr_bust_connection(dbenv, conn, TRUE));
	default:
		return (ret);
	}
}

/*
 * Initiates connection attempts for any sites on the idle list whose retry
 * times have expired.
 *
 * PUBLIC: int __repmgr_retry_connections __P((DB_ENV *));
 *
 * !!!
 * Assumes caller holds the mutex.
 */
int
__repmgr_retry_connections(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REPMGR_RETRY *retry;
	db_timespec now;
	u_int eid;
	int ret;

	db_rep = dbenv->rep_handle;
	__os_gettime(dbenv, &now);

	while (!TAILQ_EMPTY(&db_rep->retries)) {
		retry = TAILQ_FIRST(&db_rep->retries);
		if (timespeccmp(&retry->time, &now, >=))
			break;	/* since items are in time order */

		TAILQ_REMOVE(&db_rep->retries, retry, entries);

		eid = retry->eid;
		__os_free(dbenv, retry);

		if ((ret = __repmgr_try_one(dbenv, eid)) != 0)
			return (ret);
	}
	return (0);
}

/*
 * PUBLIC: int __repmgr_first_try_connections __P((DB_ENV *));
 *
 * !!!
 * Assumes caller holds the mutex.
 */
int
__repmgr_first_try_connections(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	u_int eid;
	int ret;

	db_rep = dbenv->rep_handle;
	for (eid=0; eid<db_rep->site_cnt; eid++)
		if ((ret = __repmgr_try_one(dbenv, eid)) != 0)
			return (ret);
	return (0);
}

/*
 * Makes a best-effort attempt to connect to the indicated site.  Returns a
 * non-zero error indication only for disastrous failures.  For re-tryable
 * errors, we will have scheduled another attempt, and that can be considered
 * success enough.
 */
static int
__repmgr_try_one(dbenv, eid)
	DB_ENV *dbenv;
	u_int eid;
{
	DB_REP *db_rep;
	ADDRINFO *list;
	repmgr_netaddr_t *addr;
	int ret;

	db_rep = dbenv->rep_handle;

	/*
	 * If have never yet successfully resolved this site's host name, try to
	 * do so now.
	 *
	 * Throughout all the rest of repmgr, we almost never do any sort of
	 * blocking operation in the select thread.  This is the sole exception
	 * to that rule.  Fortunately, it should rarely happen:
	 *
	 * - for a site that we only learned about because it connected to us:
	 *   not only were we not configured to know about it, but we also never
	 *   got a NEWSITE message about it.  And even then only if the
	 *   connection fails and we want to retry it from this end;
	 *
	 * - if the name look-up system (e.g., DNS) is not working (let's hope
	 *   it's temporary), or the host name is not found.
	 */
	addr = &SITE_FROM_EID(eid)->net_addr;
	if (ADDR_LIST_FIRST(addr) == NULL) {
		if ((ret = __repmgr_getaddr(
		    dbenv, addr->host, addr->port, 0, &list)) == 0) {
			addr->address_list = list;
			(void)ADDR_LIST_FIRST(addr);
		} else if (ret == DB_REP_UNAVAIL)
			return (__repmgr_schedule_connection_attempt(
			    dbenv, eid, FALSE));
		else
			return (ret);
	}

	/* Here, when we have a valid address. */
	return (__repmgr_connect_site(dbenv, eid));
}

/*
 * Tries to establish a connection with the site indicated by the given eid,
 * starting with the "current" element of its address list and trying as many
 * addresses as necessary until the list is exhausted.
 *
 * !!!
 * Only ever called in the select() thread, since we may call
 * __repmgr_bust_connection(..., TRUE).
 *
 * PUBLIC: int __repmgr_connect_site __P((DB_ENV *, u_int eid));
 */
int
__repmgr_connect_site(dbenv, eid)
	DB_ENV *dbenv;
	u_int eid;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;
	REPMGR_CONNECTION *con;
	socket_t s;
	u_int32_t flags;
	int ret;
#ifdef DB_WIN32
	long desired_event;
	WSAEVENT event_obj;
#endif

	db_rep = dbenv->rep_handle;
	site = SITE_FROM_EID(eid);

	flags = 0;
	switch (ret = __repmgr_connect(dbenv, &s, site)) {
	case 0:
		flags = 0;
#ifdef DB_WIN32
		desired_event = FD_READ|FD_CLOSE;
#endif
		break;
	case INPROGRESS:
		flags = CONN_CONNECTING;
#ifdef DB_WIN32
		desired_event = FD_CONNECT;
#endif
		break;
	default:
		STAT(db_rep->region->mstat.st_connect_fail++);
		return (
		    __repmgr_schedule_connection_attempt(dbenv, eid, FALSE));
	}

#ifdef DB_WIN32
	if ((event_obj = WSACreateEvent()) == WSA_INVALID_EVENT) {
		ret = net_errno;
		__db_err(dbenv, ret, "can't create WSA event");
		(void)closesocket(s);
		return (ret);
	}
	if (WSAEventSelect(s, event_obj, desired_event) == SOCKET_ERROR) {
		ret = net_errno;
		__db_err(dbenv, ret, "can't set desired event bits");
		(void)WSACloseEvent(event_obj);
		(void)closesocket(s);
		return (ret);
	}
#endif

	if ((ret = __repmgr_new_connection(dbenv, &con, s, flags))
	    != 0) {
#ifdef DB_WIN32
		(void)WSACloseEvent(event_obj);
#endif
		(void)closesocket(s);
		return (ret);
	}
#ifdef DB_WIN32
	con->event_object = event_obj;
#endif

	if (flags == 0) {
		switch (ret = __repmgr_send_handshake(dbenv, con)) {
		case 0:
			break;
		case DB_REP_UNAVAIL:
			return (__repmgr_bust_connection(dbenv, con, TRUE));
		default:
			return (ret);
		}
	}

	con->eid = (int)eid;

	site->ref.conn = con;
	site->state = SITE_CONNECTED;
	return (0);
}

static int
__repmgr_connect(dbenv, socket_result, site)
	DB_ENV *dbenv;
	socket_t *socket_result;
	REPMGR_SITE *site;
{
	repmgr_netaddr_t *addr;
	ADDRINFO *ai;
	socket_t s;
	char *why;
	int ret;
	SITE_STRING_BUFFER buffer;

	/*
	 * Lint doesn't know about DB_ASSERT, so it can't tell that this
	 * loop will always get executed at least once, giving 'why' a value.
	 */
	COMPQUIET(why, "");
	addr = &site->net_addr;
	ai = ADDR_LIST_CURRENT(addr);
	DB_ASSERT(dbenv, ai != NULL);
	for (; ai != NULL; ai = ADDR_LIST_NEXT(addr)) {

		if ((s = socket(ai->ai_family,
		    ai->ai_socktype, ai->ai_protocol)) == SOCKET_ERROR) {
			why = "can't create socket to connect";
			continue;
		}

		if ((ret = __repmgr_set_nonblocking(s)) != 0) {
			__db_err(dbenv,
			    ret, "can't make nonblock socket to connect");
			(void)closesocket(s);
			return (ret);
		}

		if (connect(s, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0)
			ret = net_errno;

		if (ret == 0 || ret == INPROGRESS) {
			*socket_result = s;
			RPRINT(dbenv, (dbenv,
			    "init connection to %s with result %d",
			    __repmgr_format_site_loc(site, buffer), ret));
			return (ret);
		}

		why = "connection failed";
		(void)closesocket(s);
	}

	/* We've exhausted all possible addresses. */
	ret = net_errno;
	__db_err(dbenv, ret, "%s to %s", why,
	    __repmgr_format_site_loc(site, buffer));
	return (ret);
}

/*
 * PUBLIC: int __repmgr_send_handshake __P((DB_ENV *, REPMGR_CONNECTION *));
 */
int
__repmgr_send_handshake(dbenv, conn)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
{
	DB_REP *db_rep;
	REP *rep;
	repmgr_netaddr_t *my_addr;
	DB_REPMGR_HANDSHAKE buffer;
	DBT cntrl, rec;

	DB_ASSERT(dbenv, !F_ISSET(conn, CONN_CONNECTING | CONN_DEFUNCT));

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	my_addr = &db_rep->my_addr;

	/*
	 * Remind us that we need to fix priority on the rep API not to be an
	 * int, if we're going to pass it across the wire.
	 */
	DB_ASSERT(dbenv, sizeof(u_int32_t) >= sizeof(int));

	buffer.version = DB_REPMGR_VERSION;
	buffer.priority = htonl((u_int32_t)rep->priority);
	buffer.port = my_addr->port;
	cntrl.data = &buffer;
	cntrl.size = sizeof(buffer);

	DB_SET_DBT(rec, my_addr->host, strlen(my_addr->host) + 1);

	return (__repmgr_send_one(dbenv, conn, REPMGR_HANDSHAKE, &cntrl, &rec));
}

/*
 * PUBLIC: int __repmgr_read_from_site __P((DB_ENV *, REPMGR_CONNECTION *));
 *
 * !!!
 * Caller is assumed to hold repmgr->mutex, 'cuz we call queue_put() from here.
 */
int
__repmgr_read_from_site(dbenv, conn)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
{
	SITE_STRING_BUFFER buffer;
	size_t nr;
	int ret;

	/*
	 * Keep reading pieces as long as we're making some progress, or until
	 * we complete the current read phase.
	 */
	for (;;) {
		if ((ret = __repmgr_readv(conn->fd,
		    &conn->iovecs.vectors[conn->iovecs.offset],
		    conn->iovecs.count - conn->iovecs.offset, &nr)) != 0) {
			switch (ret) {
#ifndef DB_WIN32
			case EINTR:
				continue;
#endif
			case WOULDBLOCK:
				return (0);
			default:
				(void)__repmgr_format_eid_loc(dbenv->rep_handle,
				    conn->eid, buffer);
				__db_err(dbenv, ret,
				    "can't read from %s", buffer);
				STAT(dbenv->rep_handle->
				    region->mstat.st_connection_drop++);
				return (DB_REP_UNAVAIL);
			}
		}

		if (nr > 0) {
			if (__repmgr_update_consumed(&conn->iovecs, nr))
				return (dispatch_phase_completion(dbenv,
					    conn));
		} else {
			(void)__repmgr_format_eid_loc(dbenv->rep_handle,
			    conn->eid, buffer);
			__db_errx(dbenv, "EOF on connection from %s", buffer);
			STAT(dbenv->rep_handle->
			    region->mstat.st_connection_drop++);
			return (DB_REP_UNAVAIL);
		}
	}
}

/*
 * Handles whatever needs to be done upon the completion of a reading phase on a
 * given connection.
 */
static int
dispatch_phase_completion(dbenv, conn)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
{
#define	MEM_ALIGN sizeof(double)
	DB_REP *db_rep;
	DB_REPMGR_ACK *ack;
	REPMGR_SITE *site;
	DB_REPMGR_HANDSHAKE *handshake;
	REPMGR_RETRY *retry;
	repmgr_netaddr_t addr;
	DBT *dbt;
	u_int32_t control_size, rec_size;
	size_t memsize, control_offset, rec_offset;
	void *membase;
	char *host;
	u_int port;
	int ret, eid;

	db_rep = dbenv->rep_handle;
	switch (conn->reading_phase) {
	case SIZES_PHASE:
		/*
		 * We've received the header: a message type and the lengths of
		 * the two pieces of the message.  Set up buffers to read the
		 * two pieces.
		 */

		if (conn->msg_type != REPMGR_HANDSHAKE &&
		    !IS_VALID_EID(conn->eid)) {
			__db_errx(dbenv,
	       "expected handshake as first msg from passively connected site");
			return (DB_REP_UNAVAIL);
		}

		__repmgr_iovec_init(&conn->iovecs);
		control_size = ntohl(conn->control_size_buf);
		rec_size = ntohl(conn->rec_size_buf);
		if (conn->msg_type == REPMGR_REP_MESSAGE) {
			/*
			 * Allocate a block of memory large enough to hold a
			 * DB_REPMGR_MESSAGE wrapper, plus the (one or) two DBT
			 * data areas that it points to.  Start by calculating
			 * the total memory needed, rounding up for the start of
			 * each DBT, to ensure possible alignment requirements.
			 */
			memsize = (size_t)
			    DB_ALIGN(sizeof(REPMGR_MESSAGE), MEM_ALIGN);
			control_offset = memsize;
			memsize += control_size;
			if (rec_size > 0) {
				memsize = (size_t)DB_ALIGN(memsize, MEM_ALIGN);
				rec_offset = memsize;
				memsize += rec_size;
			} else
				COMPQUIET(rec_offset, 0);
			if ((ret = __os_malloc(dbenv, memsize, &membase)) != 0)
				return (ret);
			conn->input.rep_message = membase;
			memset(&conn->input.rep_message->control, 0,
			    sizeof(DBT));
			memset(&conn->input.rep_message->rec, 0, sizeof(DBT));
			conn->input.rep_message->originating_eid = conn->eid;
			conn->input.rep_message->control.size = control_size;
			conn->input.rep_message->control.data =
			    (u_int8_t*)membase + control_offset;
			__repmgr_add_buffer(&conn->iovecs,
			    conn->input.rep_message->control.data,
			    control_size);

			conn->input.rep_message->rec.size = rec_size;
			if (rec_size > 0) {
				conn->input.rep_message->rec.data =
				    (u_int8_t*)membase + rec_offset;
				__repmgr_add_buffer(&conn->iovecs,
				    conn->input.rep_message->rec.data,
				    rec_size);
			} else
				conn->input.rep_message->rec.data = NULL;

		} else {
			if (control_size == 0) {
				__db_errx(
				    dbenv, "illegal size for non-rep msg");
				return (DB_REP_UNAVAIL);
			}
			conn->input.repmgr_msg.cntrl.size = control_size;
			conn->input.repmgr_msg.rec.size = rec_size;

			dbt = &conn->input.repmgr_msg.cntrl;
			dbt->size = control_size;
			if ((ret = __os_malloc(dbenv, control_size,
			    &dbt->data)) != 0)
				return (ret);
			__repmgr_add_dbt(&conn->iovecs, dbt);

			dbt = &conn->input.repmgr_msg.rec;
			if ((dbt->size = rec_size) > 0) {
				if ((ret = __os_malloc(dbenv, rec_size,
				     &dbt->data)) != 0) {
					__os_free(dbenv,
					    conn->input.repmgr_msg.cntrl.data);
					return (ret);
				}
				__repmgr_add_dbt(&conn->iovecs, dbt);
			}
		}

		conn->reading_phase = DATA_PHASE;
		break;

	case DATA_PHASE:
		/*
		 * We have a complete message, so process it.  Acks and
		 * handshakes get processed here, in line.  Regular rep messages
		 * get posted to a queue, to be handled by a thread from the
		 * message thread pool.
		 */
		switch (conn->msg_type) {
		case REPMGR_ACK:
			/*
			 * Extract the LSN.  Save it only if it is an
			 * improvement over what the site has already ack'ed.
			 */
			ack = conn->input.repmgr_msg.cntrl.data;
			if (conn->input.repmgr_msg.cntrl.size != sizeof(*ack) ||
			    conn->input.repmgr_msg.rec.size != 0) {
				__db_errx(dbenv, "bad ack msg size");
				return (DB_REP_UNAVAIL);
			}
			if ((ret = record_ack(dbenv, SITE_FROM_EID(conn->eid),
			    ack)) != 0)
				return (ret);
			__os_free(dbenv, conn->input.repmgr_msg.cntrl.data);
			break;

		case REPMGR_HANDSHAKE:
			handshake = conn->input.repmgr_msg.cntrl.data;
			if (conn->input.repmgr_msg.cntrl.size >=
			    sizeof(handshake->version) &&
			    handshake->version != DB_REPMGR_VERSION) {
				__db_errx(dbenv,
			   "mismatched repmgr message protocol version (%lu)",
				    (u_long)handshake->version);
				return (DB_REP_UNAVAIL);
			}
			if (conn->input.repmgr_msg.cntrl.size !=
			    sizeof(*handshake) ||
			    conn->input.repmgr_msg.rec.size == 0) {
				__db_errx(dbenv, "bad handshake msg size");
				return (DB_REP_UNAVAIL);
			}

			port = handshake->port;
			host = conn->input.repmgr_msg.rec.data;
			host[conn->input.repmgr_msg.rec.size-1] = '\0';

			RPRINT(dbenv, (dbenv,
				   "got handshake %s:%u, pri %lu", host, port,
				   (u_long)ntohl(handshake->priority)));

			if (IS_VALID_EID(conn->eid)) {
				/*
				 * We must have initiated this as an outgoing
				 * connection, since we already know the EID.
				 * All we need from the handshake is the
				 * priority.
				 */
				site = SITE_FROM_EID(conn->eid);
				RPRINT(dbenv, (dbenv,
				    "handshake from connection to %s:%lu",
				    site->net_addr.host,
				    (u_long)site->net_addr.port));
			} else {
				if (IS_VALID_EID(eid =
				    __repmgr_find_site(dbenv, host, port))) {
					site = SITE_FROM_EID(eid);
					if (site->state == SITE_IDLE) {
						RPRINT(dbenv, (dbenv,
					"handshake from previously idle site"));
						retry = site->ref.retry;
						TAILQ_REMOVE(&db_rep->retries,
						    retry, entries);
						__os_free(dbenv, retry);

						conn->eid = eid;
						site->state = SITE_CONNECTED;
						site->ref.conn = conn;
					} else {
						/*
						 * We got an incoming connection
						 * for a site we were already
						 * connected to; discard it.
						 */
						__db_errx(dbenv,
			       "redundant incoming connection will be ignored");
						return (DB_REP_UNAVAIL);
					}
				} else {
					RPRINT(dbenv, (dbenv,
					  "handshake introduces unknown site"));
					if ((ret = __repmgr_pack_netaddr(
					    dbenv, host, port, NULL,
					    &addr)) != 0)
						return (ret);
					if ((ret = __repmgr_new_site(
					    dbenv, &site, &addr,
					    SITE_CONNECTED)) != 0) {
						__repmgr_cleanup_netaddr(dbenv,
						    &addr);
						return (ret);
					}
					conn->eid = EID_FROM_SITE(site);
					site->ref.conn = conn;
				}
			}

			/* TODO: change priority to be u_int32_t. */
			DB_ASSERT(dbenv, sizeof(int) == sizeof(u_int32_t));
			site->priority = (int)ntohl(handshake->priority);

			if ((ret = notify_handshake(dbenv, conn)) != 0)
				return (ret);

			__os_free(dbenv, conn->input.repmgr_msg.cntrl.data);
			__os_free(dbenv, conn->input.repmgr_msg.rec.data);
			break;

		case REPMGR_REP_MESSAGE:
			if ((ret = __repmgr_queue_put(dbenv,
			    conn->input.rep_message)) != 0)
				return (ret);
			/*
			 * The queue has taken over responsibility for the
			 * rep_message buffer, and will free it later.
			 */
			break;

		default:
			__db_errx(dbenv, "unknown msg type rcvd: %d",
			    (int)conn->msg_type);
			return (DB_REP_UNAVAIL);
		}

		__repmgr_reset_for_reading(conn);
		break;

	default:
		DB_ASSERT(dbenv, FALSE);
	}

	return (0);
}

/*
 * Performs any processing needed upon the receipt of a handshake.
 *
 * !!!
 * Caller must hold mutex.
 */
static int
notify_handshake(dbenv, conn)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
{
	DB_REP *db_rep;

	COMPQUIET(conn, NULL);

	db_rep = dbenv->rep_handle;
	/*
	 * If we're moping around wishing we knew who the master was, then
	 * getting in touch with another site might finally provide sufficient
	 * connectivity to find out.  But just do this once, because otherwise
	 * we get messages while the subsequent rep_start operations are going
	 * on, and rep tosses them in that case.
	 */
	if (db_rep->master_eid == DB_EID_INVALID && !db_rep->done_one) {
		db_rep->done_one = TRUE;
		RPRINT(dbenv, (dbenv,
		    "handshake with no known master to wake election thread"));
		return (__repmgr_init_election(dbenv, ELECT_REPSTART));
	}
	return (0);
}

static int
record_ack(dbenv, site, ack)
	DB_ENV *dbenv;
	REPMGR_SITE *site;
	DB_REPMGR_ACK *ack;
{
	DB_REP *db_rep;
	SITE_STRING_BUFFER buffer;
	int ret;

	db_rep = dbenv->rep_handle;

	/* Ignore stale acks. */
	if (ack->generation < db_rep->generation) {
		RPRINT(dbenv, (dbenv,
		    "ignoring stale ack (%lu<%lu), from %s",
		     (u_long)ack->generation, (u_long)db_rep->generation,
		     __repmgr_format_site_loc(site, buffer)));
		return (0);
	}
	RPRINT(dbenv, (dbenv,
	    "got ack [%lu][%lu](%lu) from %s", (u_long)ack->lsn.file,
	    (u_long)ack->lsn.offset, (u_long)ack->generation,
	    __repmgr_format_site_loc(site, buffer)));

	if (ack->generation == db_rep->generation &&
	    log_compare(&ack->lsn, &site->max_ack) == 1) {
		memcpy(&site->max_ack, &ack->lsn, sizeof(DB_LSN));
		if ((ret = __repmgr_wake_waiting_senders(dbenv)) != 0)
			return (ret);
	}
	return (0);
}

/*
 * PUBLIC: int __repmgr_write_some __P((DB_ENV *, REPMGR_CONNECTION *));
 */
int
__repmgr_write_some(dbenv, conn)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
{
	REPMGR_FLAT *msg;
	QUEUED_OUTPUT *output;
	int bytes, ret;

	while (!STAILQ_EMPTY(&conn->outbound_queue)) {
		output = STAILQ_FIRST(&conn->outbound_queue);
		msg = output->msg;
		if ((bytes = send(conn->fd, &msg->data[output->offset],
		    (size_t)msg->length - output->offset, 0)) == SOCKET_ERROR) {
			if ((ret = net_errno) == WOULDBLOCK)
				return (0);
			else {
				__db_err(dbenv, ret, "writing data");
				STAT(dbenv->rep_handle->
				    region->mstat.st_connection_drop++);
				return (DB_REP_UNAVAIL);
			}
		}

		if ((output->offset += (size_t)bytes) >= msg->length) {
			STAILQ_REMOVE_HEAD(&conn->outbound_queue, entries);
			__os_free(dbenv, output);
			conn->out_queue_length--;
			if (--msg->ref_count <= 0)
				__os_free(dbenv, msg);
		}
	}

#ifdef DB_WIN32
	/*
	 * With the queue now empty, it's time to relinquish ownership of this
	 * connection again, so that the next call to send() can write the
	 * message in line, instead of posting it to the queue for us.
	 */
	if (WSAEventSelect(conn->fd, conn->event_object, FD_READ|FD_CLOSE)
	    == SOCKET_ERROR) {
		ret = net_errno;
		__db_err(dbenv, ret, "can't remove FD_WRITE event bit");
		return (ret);
	}
#endif

	return (0);
}
