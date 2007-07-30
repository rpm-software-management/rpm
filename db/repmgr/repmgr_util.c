/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: repmgr_util.c,v 1.34 2007/06/11 18:29:34 alanb Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"

/*
 * Schedules a future attempt to re-establish a connection with the given site.
 * Usually, we wait the configured retry_wait period.  But if the "immediate"
 * parameter is given as TRUE, we'll make the wait time 0, and put the request
 * at the _beginning_ of the retry queue.  Note how this allows us to preserve
 * the property that the queue stays in time order simply by appending to the
 * end.
 *
 * PUBLIC: int __repmgr_schedule_connection_attempt __P((DB_ENV *, u_int, int));
 *
 * !!!
 * Caller should hold mutex.
 *
 * Unless an error occurs, we always attempt to wake the main thread;
 * __repmgr_bust_connection relies on this behavior.
 */
int
__repmgr_schedule_connection_attempt(dbenv, eid, immediate)
	DB_ENV *dbenv;
	u_int eid;
	int immediate;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;
	REPMGR_RETRY *retry;
	db_timespec t, v;
	int ret;

	db_rep = dbenv->rep_handle;
	if ((ret = __os_malloc(dbenv, sizeof(*retry), &retry)) != 0)
		return (ret);

	__os_gettime(dbenv, &t);
	if (immediate)
		TAILQ_INSERT_HEAD(&db_rep->retries, retry, entries);
	else {
		DB_TIMEOUT_TO_TIMESPEC(db_rep->connection_retry_wait, &v);
		timespecadd(&t, &v);
		TAILQ_INSERT_TAIL(&db_rep->retries, retry, entries);
	}
	retry->eid = eid;
	retry->time = t;

	site = SITE_FROM_EID(eid);
	site->state = SITE_IDLE;
	site->ref.retry = retry;

	return (__repmgr_wake_main_thread(dbenv));
}

/*
 * Initialize the necessary control structures to begin reading a new input
 * message.
 *
 * PUBLIC: void __repmgr_reset_for_reading __P((REPMGR_CONNECTION *));
 */
void
__repmgr_reset_for_reading(con)
	REPMGR_CONNECTION *con;
{
	con->reading_phase = SIZES_PHASE;
	__repmgr_iovec_init(&con->iovecs);
	__repmgr_add_buffer(&con->iovecs, &con->msg_type,
	    sizeof(con->msg_type));
	__repmgr_add_buffer(&con->iovecs, &con->control_size_buf,
	    sizeof(con->control_size_buf));
	__repmgr_add_buffer(&con->iovecs, &con->rec_size_buf,
	    sizeof(con->rec_size_buf));
}

/*
 * Constructs a DB_REPMGR_CONNECTION structure, and puts it on the main list of
 * connections.  It does not initialize eid, since that isn't needed and/or
 * immediately known in all cases.
 *
 * PUBLIC:  int __repmgr_new_connection __P((DB_ENV *, REPMGR_CONNECTION **,
 * PUBLIC:				   socket_t, u_int32_t));
 */
int
__repmgr_new_connection(dbenv, connp, s, flags)
	DB_ENV *dbenv;
	REPMGR_CONNECTION **connp;
	socket_t s;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REPMGR_CONNECTION *c;
	int ret;

	db_rep = dbenv->rep_handle;
	if ((ret = __os_malloc(dbenv, sizeof(REPMGR_CONNECTION), &c)) != 0)
		return (ret);

	c->fd = s;
	c->flags = flags;

	STAILQ_INIT(&c->outbound_queue);
	c->out_queue_length = 0;

	__repmgr_reset_for_reading(c);
	TAILQ_INSERT_TAIL(&db_rep->connections, c, entries);
	*connp = c;

	return (0);
}

/*
 * PUBLIC: int __repmgr_new_site __P((DB_ENV *, REPMGR_SITE**,
 * PUBLIC:     const repmgr_netaddr_t *, int));
 *
 * !!!
 * Caller must hold mutex.
 */
int
__repmgr_new_site(dbenv, sitep, addr, state)
	DB_ENV *dbenv;
	REPMGR_SITE **sitep;
	const repmgr_netaddr_t *addr;
	int state;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;
	SITE_STRING_BUFFER buffer;
	u_int new_site_max, eid;
	int ret;

	db_rep = dbenv->rep_handle;
	if (db_rep->site_cnt >= db_rep->site_max) {
#define	INITIAL_SITES_ALLOCATION	10		/* Arbitrary guess. */
		new_site_max = db_rep->site_max == 0 ?
		    INITIAL_SITES_ALLOCATION : db_rep->site_max * 2;
		if ((ret = __os_realloc(dbenv,
		     sizeof(REPMGR_SITE) * new_site_max, &db_rep->sites)) != 0)
			 return (ret);
		db_rep->site_max = new_site_max;
	}
	eid = db_rep->site_cnt++;

	site = &db_rep->sites[eid];

	memcpy(&site->net_addr, addr, sizeof(*addr));
	ZERO_LSN(site->max_ack);
	site->priority = -1;	/* OOB value indicates we don't yet know. */
	site->state = state;

	RPRINT(dbenv, (dbenv, "EID %u is assigned for %s", eid,
	    __repmgr_format_site_loc(site, buffer)));
	*sitep = site;
	return (0);
}

/*
 * Destructor for a repmgr_netaddr_t, cleans up any allocated memory pointed to
 * by the addr.
 *
 * PUBLIC: void __repmgr_cleanup_netaddr __P((DB_ENV *, repmgr_netaddr_t *));
 */
void
__repmgr_cleanup_netaddr(dbenv, addr)
	DB_ENV *dbenv;
	repmgr_netaddr_t *addr;
{
	if (addr->address_list != NULL) {
		__db_freeaddrinfo(dbenv, addr->address_list);
		addr->address_list = addr->current = NULL;
	}
	if (addr->host != NULL) {
		__os_free(dbenv, addr->host);
		addr->host = NULL;
	}
}

/*
 * PUBLIC: void __repmgr_iovec_init __P((REPMGR_IOVECS *));
 */
void
__repmgr_iovec_init(v)
	REPMGR_IOVECS *v;
{
	v->offset = v->count = 0;
	v->total_bytes = 0;
}

/*
 * PUBLIC: void __repmgr_add_buffer __P((REPMGR_IOVECS *, void *, size_t));
 *
 * !!!
 * There is no checking for overflow of the vectors[5] array.
 */
void
__repmgr_add_buffer(v, address, length)
	REPMGR_IOVECS *v;
	void *address;
	size_t length;
{
	v->vectors[v->count].iov_base = address;
	v->vectors[v->count++].iov_len = length;
	v->total_bytes += length;
}

/*
 * PUBLIC: void __repmgr_add_dbt __P((REPMGR_IOVECS *, const DBT *));
 */
void
__repmgr_add_dbt(v, dbt)
	REPMGR_IOVECS *v;
	const DBT *dbt;
{
	v->vectors[v->count].iov_base = dbt->data;
	v->vectors[v->count++].iov_len = dbt->size;
	v->total_bytes += dbt->size;
}

/*
 * Update a set of iovecs to reflect the number of bytes transferred in an I/O
 * operation, so that the iovecs can be used to continue transferring where we
 * left off.
 *     Returns TRUE if the set of buffers is now fully consumed, FALSE if more
 * remains.
 *
 * PUBLIC: int __repmgr_update_consumed __P((REPMGR_IOVECS *, size_t));
 */
int
__repmgr_update_consumed(v, byte_count)
	REPMGR_IOVECS *v;
	size_t byte_count;
{
	db_iovec_t *iov;
	int i;

	for (i = v->offset; ; i++) {
		DB_ASSERT(NULL, i < v->count && byte_count > 0);
		iov = &v->vectors[i];
		if (byte_count > iov->iov_len) {
			/*
			 * We've consumed (more than) this vector's worth.
			 * Adjust count and continue.
			 */
			byte_count -= iov->iov_len;
		} else {
			/* Adjust length of remaining portion of vector. */
			iov->iov_len -= byte_count;
			if (iov->iov_len > 0) {
				/*
				 * Still some left in this vector.  Adjust base
				 * address too, and leave offset pointing here.
				 */
				iov->iov_base = (void *)
				    ((u_int8_t *)iov->iov_base + byte_count);
				v->offset = i;
			} else {
				/*
				 * Consumed exactly to a vector boundary.
				 * Advance to next vector for next time.
				 */
				v->offset = i+1;
			}
			/*
			 * If offset has reached count, the entire thing is
			 * consumed.
			 */
			return (v->offset >= v->count);
		}
	}
}

/*
 * Builds a buffer containing our network address information, suitable for
 * publishing as cdata via a call to rep_start, and sets up the given DBT to
 * point to it.  The buffer is dynamically allocated memory, and the caller must
 * assume responsibility for it.
 *
 * PUBLIC: int __repmgr_prepare_my_addr __P((DB_ENV *, DBT *));
 */
int
__repmgr_prepare_my_addr(dbenv, dbt)
	DB_ENV *dbenv;
	DBT *dbt;
{
	DB_REP *db_rep;
	size_t size, hlen;
	u_int16_t port_buffer;
	u_int8_t *ptr;
	int ret;

	db_rep = dbenv->rep_handle;

	/*
	 * The cdata message consists of the 2-byte port number, in network byte
	 * order, followed by the null-terminated host name string.
	 */
	port_buffer = htons(db_rep->my_addr.port);
	size = sizeof(port_buffer) +
	    (hlen = strlen(db_rep->my_addr.host) + 1);
	if ((ret = __os_malloc(dbenv, size, &ptr)) != 0)
		return (ret);

	DB_INIT_DBT(*dbt, ptr, size);

	memcpy(ptr, &port_buffer, sizeof(port_buffer));
	ptr = &ptr[sizeof(port_buffer)];
	memcpy(ptr, db_rep->my_addr.host, hlen);

	return (0);
}

/*
 * Provide the appropriate value for nsites, the number of sites in the
 * replication group.  If the application has specified a value, use that.
 * Otherwise, just use the number of sites we know of.
 *
 * PUBLIC: u_int __repmgr_get_nsites __P((DB_REP *));
 */
u_int
__repmgr_get_nsites(db_rep)
	DB_REP *db_rep;
{
	if (db_rep->config_nsites > 0)
		return ((u_int)db_rep->config_nsites);

	/*
	 * The number of other sites in our table, plus 1 to count ourself.
	 */
	return (db_rep->site_cnt + 1);
}

/*
 * PUBLIC: void __repmgr_thread_failure __P((DB_ENV *, int));
 */
void
__repmgr_thread_failure(dbenv, why)
	DB_ENV *dbenv;
	int why;
{
	(void)__repmgr_stop_threads(dbenv);
	(void)__db_panic(dbenv, why);
}

/*
 * Format a printable representation of a site location, suitable for inclusion
 * in an error message.  The buffer must be at least as big as
 * MAX_SITE_LOC_STRING.
 *
 * PUBLIC: char *__repmgr_format_eid_loc __P((DB_REP *, int, char *));
 */
char *
__repmgr_format_eid_loc(db_rep, eid, buffer)
	DB_REP *db_rep;
	int eid;
	char *buffer;
{
	if (IS_VALID_EID(eid))
		return (__repmgr_format_site_loc(SITE_FROM_EID(eid), buffer));

	snprintf(buffer, MAX_SITE_LOC_STRING, "(unidentified site)");
	return (buffer);
}

/*
 * PUBLIC: char *__repmgr_format_site_loc __P((REPMGR_SITE *, char *));
 */
char *
__repmgr_format_site_loc(site, buffer)
	REPMGR_SITE *site;
	char *buffer;
{
	snprintf(buffer, MAX_SITE_LOC_STRING, "site %s:%lu",
	    site->net_addr.host, (u_long)site->net_addr.port);
	return (buffer);
}

/*
 * __repmgr_timespec_diff_now --
 *	Calculate the time duration from now til "when".
 *
 * PUBLIC: void __repmgr_timespec_diff_now
 * PUBLIC:    __P((DB_ENV *, db_timespec *, db_timespec *));
 */
void
__repmgr_timespec_diff_now(dbenv, when, result)
	DB_ENV *dbenv;
	db_timespec *when, *result;
{
	db_timespec now;

	__os_gettime(dbenv, &now);
	if (timespeccmp(&now, when, >=))
		timespecclear(result);
	else {
		*result = *when;
		timespecsub(result, &now);
	}
}

/*
 * PUBLIC: int __repmgr_repstart __P((DB_ENV *, u_int32_t));
 */
int
__repmgr_repstart(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DBT my_addr;
	int ret;

	if ((ret = __repmgr_prepare_my_addr(dbenv, &my_addr)) != 0)
		return (ret);
	ret = __rep_start(dbenv, &my_addr, flags);
	__os_free(dbenv, my_addr.data);
	if (ret != 0)
		__db_err(dbenv, ret, "rep_start");
	return (ret);
}
