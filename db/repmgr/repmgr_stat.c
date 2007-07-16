/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: repmgr_stat.c,v 1.28 2006/09/08 19:22:42 bostic Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"

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

/*
 * PUBLIC: int __repmgr_print_stats __P((DB_ENV *));
 */
int
__repmgr_print_stats(dbenv)
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
