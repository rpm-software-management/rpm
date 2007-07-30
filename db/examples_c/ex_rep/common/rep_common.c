/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2006,2007 Oracle.  All rights reserved.
 *
 * $Id: rep_common.c,v 12.20 2007/05/17 17:29:27 bostic Exp $
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>

#include "rep_common.h"

#define	CACHESIZE	(10 * 1024 * 1024)
#define	DATABASE	"quote.db"
#define	SLEEPTIME	3

#ifdef _WIN32
#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
#define	sleep(s)		Sleep(1000 * (s))
#endif

static int print_stocks __P((DB *));

static int
print_stocks(dbp)
	DB *dbp;
{
	DBC *dbc;
	DBT key, data;
#define	MAXKEYSIZE	10
#define	MAXDATASIZE	20
	char keybuf[MAXKEYSIZE + 1], databuf[MAXDATASIZE + 1];
	int ret, t_ret;
	u_int32_t keysize, datasize;

	if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0) {
		dbp->err(dbp, ret, "can't open cursor");
		return (ret);
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	printf("\tSymbol\tPrice\n");
	printf("\t======\t=====\n");

	for (ret = dbc->get(dbc, &key, &data, DB_FIRST);
	    ret == 0;
	    ret = dbc->get(dbc, &key, &data, DB_NEXT)) {
		keysize = key.size > MAXKEYSIZE ? MAXKEYSIZE : key.size;
		memcpy(keybuf, key.data, keysize);
		keybuf[keysize] = '\0';

		datasize = data.size >= MAXDATASIZE ? MAXDATASIZE : data.size;
		memcpy(databuf, data.data, datasize);
		databuf[datasize] = '\0';

		printf("\t%s\t%s\n", keybuf, databuf);
	}
	printf("\n");
	fflush(stdout);

	if ((t_ret = dbc->close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	switch (ret) {
	case 0:
	case DB_NOTFOUND:
	case DB_LOCK_DEADLOCK:
		return (0);
	default:
		return (ret);
	}
}

#define	BUFSIZE 1024

int
doloop(dbenv, shared_data)
	DB_ENV *dbenv;
	SHARED_DATA *shared_data;
{
	DB *dbp;
	DBT key, data;
	char buf[BUFSIZE], *first, *price;
	u_int32_t flags;
	int ret;

	dbp = NULL;
	ret = 0;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	for (;;) {
		printf("QUOTESERVER%s> ",
		    shared_data->is_master ? "" : " (read-only)");
		fflush(stdout);

		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;

#define	DELIM " \t\n"
		if ((first = strtok(&buf[0], DELIM)) == NULL) {
			/* Blank input line. */
			price = NULL;
		} else if ((price = strtok(NULL, DELIM)) == NULL) {
			/* Just one input token. */
			if (strncmp(buf, "exit", 4) == 0 ||
			    strncmp(buf, "quit", 4) == 0)
				break;
			dbenv->errx(dbenv, "Format: TICKER VALUE");
			continue;
		} else {
			/* Normal two-token input line. */
			if (first != NULL && !shared_data->is_master) {
				dbenv->errx(dbenv, "Can't update at client");
				continue;
			}
		}

		if (dbp == NULL) {
			if ((ret = db_create(&dbp, dbenv, 0)) != 0)
				return (ret);

			/* Set page size small so page allocation is cheap. */
			if ((ret = dbp->set_pagesize(dbp, 512)) != 0)
				goto err;

			flags = DB_AUTO_COMMIT;
			if (shared_data->is_master)
				flags |= DB_CREATE;
			if ((ret = dbp->open(dbp,
			    NULL, DATABASE, NULL, DB_BTREE, flags, 0)) != 0) {
				if (ret == ENOENT) {
					printf(
					  "No stock database yet available.\n");
					if ((ret = dbp->close(dbp, 0)) != 0) {
						dbenv->err(dbenv, ret,
						    "DB->close");
						goto err;
					}
					dbp = NULL;
					continue;
				}
				dbenv->err(dbenv, ret, "DB->open");
				goto err;
			}
		}

		if (first == NULL)
			switch ((ret = print_stocks(dbp))) {
			case 0:
				continue;
			case DB_REP_HANDLE_DEAD:
				(void)dbp->close(dbp, DB_NOSYNC);
				dbp = NULL;
			default:
				dbp->err(dbp, ret, "Error traversing data");
				goto err;
			}
		else {
			key.data = first;
			key.size = (u_int32_t)strlen(first);

			data.data = price;
			data.size = (u_int32_t)strlen(price);

			if ((ret = dbp->put(dbp,
				 NULL, &key, &data, DB_AUTO_COMMIT)) != 0) {
				dbp->err(dbp, ret, "DB->put");
				goto err;
			}
		}
	}

err:	if (dbp != NULL)
		(void)dbp->close(dbp, DB_NOSYNC);

	return (ret);
}

int
create_env(progname, dbenvp)
	const char *progname;
	DB_ENV **dbenvp;
{
	DB_ENV *dbenv;
	int ret;

	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr, "can't create env handle: %s\n",
		    db_strerror(ret));
		return (ret);
	}

	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, progname);

	*dbenvp = dbenv;
	return (0);
}

/* Open and configure an environment. */
int
env_init(dbenv, home)
	DB_ENV *dbenv;
	const char *home;
{
	u_int32_t flags;
	int ret;

	(void)dbenv->set_cachesize(dbenv, 0, CACHESIZE, 0);
	(void)dbenv->set_flags(dbenv, DB_TXN_NOSYNC, 1);

	flags = DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL |
	    DB_INIT_REP | DB_INIT_TXN | DB_RECOVER | DB_THREAD;
	if ((ret = dbenv->open(dbenv, home, flags, 0)) != 0)
		dbenv->err(dbenv, ret, "can't open environment");
	return (ret);
}

/*
 * In this application, we specify all communication via the command line.  In
 * a real application, we would expect that information about the other sites
 * in the system would be maintained in some sort of configuration file.  The
 * critical part of this interface is that we assume at startup that we can
 * find out
 *	1) what host/port we wish to listen on for connections,
 *	2) a (possibly empty) list of other sites we should attempt to connect
 *	to; and
 *	3) what our Berkeley DB home environment is.
 *
 * These pieces of information are expressed by the following flags.
 * -m host:port (required; m stands for me)
 * -o host:port (optional; o stands for other; any number of these may be
 *	specified)
 * -h home directory
 * -n nsites (optional; number of sites in replication group; defaults to 0
 *	in which case we try to dynamically compute the number of sites in
 *	the replication group)
 * -p priority (optional: defaults to 100)
 * -C or -M  start up as client or master (optional)
 */
void
usage(progname)
	const char *progname;
{
	fprintf(stderr, "usage: %s ", progname);
	fprintf(stderr, "[-CM][-h home][-o host:port][-m host:port]%s",
	    "[-n nsites][-p priority]\n");
	exit(EXIT_FAILURE);
}
