/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: repmgr_stat.c,v 1.35 2007/06/22 18:26:52 bostic Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"

#ifdef HAVE_STATISTICS
static int __repmgr_print_all __P((DB_ENV *, u_int32_t));
static int __repmgr_print_sites __P((DB_ENV *));
static int __repmgr_print_stats __P((DB_ENV *, u_int32_t));
static int __repmgr_stat __P((DB_ENV *, DB_REPMGR_STAT **, u_int32_t));
static int __repmgr_stat_print __P((DB_ENV *, u_int32_t));

/*
 * __repmgr_stat_pp --
 *	DB_ENV->repmgr_stat pre/post processing.
 *
 * PUBLIC: int __repmgr_stat_pp __P((DB_ENV *, DB_REPMGR_STAT **, u_int32_t));
 */
int
__repmgr_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_REPMGR_STAT **statp;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG_XX(
	    dbenv, rep_handle, "DB_ENV->repmgr_stat", DB_INIT_REP);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->repmgr_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	ret = __repmgr_stat(dbenv, statp, flags);
	ENV_LEAVE(dbenv, ip);

	return (ret);
}

/*
 * __repmgr_stat --
 *	DB_ENV->repmgr_stat.
 */
static int
__repmgr_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_REPMGR_STAT **statp;
	u_int32_t flags;
{
	DB_REP *db_rep;
	DB_REPMGR_STAT *stats;
	int ret;

	db_rep = dbenv->rep_handle;

	*statp = NULL;

	/* Allocate a stat struct to return to the user. */
	if ((ret = __os_umalloc(dbenv, sizeof(DB_REPMGR_STAT), &stats)) != 0)
		return (ret);

	memcpy(stats, &db_rep->region->mstat, sizeof(*stats));
	if (LF_ISSET(DB_STAT_CLEAR))
		memset(&db_rep->region->mstat, 0, sizeof(DB_REPMGR_STAT));

	*statp = stats;
	return (0);
}

/*
 * __repmgr_stat_print_pp --
 *	DB_ENV->repmgr_stat_print pre/post processing.
 *
 * PUBLIC: int __repmgr_stat_print_pp __P((DB_ENV *, u_int32_t));
 */
int
__repmgr_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG_XX(
	    dbenv, rep_handle, "DB_ENV->repmgr_stat_print", DB_INIT_REP);

	if ((ret = __db_fchk(dbenv, "DB_ENV->repmgr_stat_print",
	    flags, DB_STAT_ALL | DB_STAT_CLEAR)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	ret = __repmgr_stat_print(dbenv, flags);
	ENV_LEAVE(dbenv, ip);

	return (ret);
}

int
__repmgr_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	u_int32_t orig_flags;
	int ret;

	orig_flags = flags;
	LF_CLR(DB_STAT_CLEAR | DB_STAT_SUBSYSTEM);
	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		if ((ret = __repmgr_print_stats(dbenv, orig_flags)) == 0)
			ret = __repmgr_print_sites(dbenv);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL) &&
	    (ret = __repmgr_print_all(dbenv, orig_flags)) != 0)
		return (ret);

	return (0);
}

static int
__repmgr_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_REPMGR_STAT *sp;
	int ret;

	if ((ret = __repmgr_stat(dbenv, &sp, flags)) != 0)
		return (ret);

	__db_dl(dbenv, "Number of PERM messages not acknowledged",
	    (u_long)sp->st_perm_failed);
	__db_dl(dbenv, "Number of messages queued due to network delay",
	    (u_long)sp->st_msgs_queued);
	__db_dl(dbenv, "Number of messages discarded due to queue length",
	    (u_long)sp->st_msgs_dropped);
	__db_dl(dbenv, "Number of existing connections dropped",
	    (u_long)sp->st_connection_drop);
	__db_dl(dbenv, "Number of failed new connection attempts",
	    (u_long)sp->st_connect_fail);

	__os_ufree(dbenv, sp);

	return (0);
}

static int
__repmgr_print_sites(dbenv)
	DB_ENV *dbenv;
{
	DB_REPMGR_SITE *list;
	u_int count, i;
	int ret;

	if ((ret = __repmgr_site_list(dbenv, &count, &list)) != 0)
		return (ret);

	if (count == 0)
		return (0);

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_REPMGR site information:");

	for (i = 0; i < count; ++i) {
		__db_msg(dbenv, "%s (eid: %d, port: %u, %sconnected)",
		    list[i].host, list[i].eid, list[i].port,
		    list[i].status == DB_REPMGR_CONNECTED ? "" : "dis");
	}

	__os_ufree(dbenv, list);

	return (0);
}

/*
 * __repmgr_print_all --
 *	Display debugging replication manager statistics.
 */
static int
__repmgr_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(flags, 0);
	return (0);
}

#else /* !HAVE_STATISTICS */

int
__repmgr_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_REPMGR_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}

int
__repmgr_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif

/*
 * PUBLIC: int __repmgr_site_list __P((DB_ENV *, u_int *, DB_REPMGR_SITE **));
 */
int
__repmgr_site_list(dbenv, countp, listp)
	DB_ENV *dbenv;
	u_int *countp;
	DB_REPMGR_SITE **listp;
{
	DB_REP *db_rep;
	DB_REPMGR_SITE *status;
	REPMGR_SITE *site;
	size_t array_size, total_size;
	u_int count, i;
	int locked, ret;
	char *name;

	db_rep = dbenv->rep_handle;
	if (REPMGR_SYNC_INITED(db_rep)) {
		LOCK_MUTEX(db_rep->mutex);
		locked = TRUE;
	} else
		locked = FALSE;

	/* Initialize for empty list or error return. */
	ret = 0;
	*countp = 0;
	*listp = NULL;

	/* First, add up how much memory we need for the host names. */
	if ((count = db_rep->site_cnt) == 0)
		goto err;

	array_size = sizeof(DB_REPMGR_SITE) * count;
	total_size = array_size;
	for (i = 0; i < count; i++) {
		site = &db_rep->sites[i];

		/* Make room for the NUL terminating byte. */
		total_size += strlen(site->net_addr.host) + 1;
	}

	if ((ret = __os_umalloc(dbenv, total_size, &status)) != 0)
		goto err;

	/*
	 * Put the storage for the host names after the array of structs.  This
	 * way, the caller can free the whole thing in one single operation.
	 */
	name = (char *)((u_int8_t *)status + array_size);
	for (i = 0; i < count; i++) {
		site = &db_rep->sites[i];

		status[i].eid = EID_FROM_SITE(site);

		status[i].host = name;
		(void)strcpy(name, site->net_addr.host);
		name += strlen(name) + 1;

		status[i].port = site->net_addr.port;
		status[i].status = site->state == SITE_CONNECTED ?
		    DB_REPMGR_CONNECTED : DB_REPMGR_DISCONNECTED;
	}

	*countp = count;
	*listp = status;

err:	if (locked)
		UNLOCK_MUTEX(db_rep->mutex);
	return (ret);
}
