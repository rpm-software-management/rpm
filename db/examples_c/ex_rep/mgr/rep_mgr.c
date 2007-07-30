/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: rep_mgr.c,v 12.21 2007/05/17 17:29:28 bostic Exp $
 */

#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <db.h>

#include "../common/rep_common.h"

typedef struct {
	SHARED_DATA shared_data;
} APP_DATA;

const char *progname = "ex_rep";

#ifdef _WIN32
extern int getopt(int, char * const *, const char *);
#endif

static void event_callback __P((DB_ENV *, u_int32_t, void *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	DB_ENV *dbenv;
	const char *home;
	char ch, *host, *portstr;
	int ret, totalsites, t_ret, got_listen_address, friend;
	u_int16_t port;
	APP_DATA my_app_data;
	u_int32_t start_policy;
	int priority;

	my_app_data.shared_data.is_master = 0; /* assume start out as client */
	dbenv = NULL;
	ret = got_listen_address = 0;
	home = "TESTDIR";

	if ((ret = create_env(progname, &dbenv)) != 0)
		goto err;
	dbenv->app_private = &my_app_data;
	(void)dbenv->set_event_notify(dbenv, event_callback);

	start_policy = DB_REP_ELECTION;	/* default */
	priority = 100;		/* default */

	while ((ch = getopt(argc, argv, "Cf:h:Mm:n:o:p:v")) != EOF) {
		friend = 0;
		switch (ch) {
		case 'C':
			start_policy = DB_REP_CLIENT;
			break;
		case 'h':
			home = optarg;
			break;
		case 'M':
			start_policy = DB_REP_MASTER;
			break;
		case 'm':
			host = strtok(optarg, ":");
			if ((portstr = strtok(NULL, ":")) == NULL) {
				fprintf(stderr, "Bad host specification.\n");
				goto err;
			}
			port = (unsigned short)atoi(portstr);
			if ((ret = dbenv->repmgr_set_local_site(dbenv,
			    host, port, 0)) != 0) {
				fprintf(stderr,
				    "Could not set listen address (%d).\n",
				    ret);
				goto err;
			}
			got_listen_address = 1;
			break;
		case 'n':
			totalsites = atoi(optarg);
			if ((ret =
			    dbenv->rep_set_nsites(dbenv, totalsites)) != 0)
				dbenv->err(dbenv, ret, "set_nsites");
			break;
		case 'f':
			friend = 1; /* FALLTHROUGH */
		case 'o':
			host = strtok(optarg, ":");
			if ((portstr = strtok(NULL, ":")) == NULL) {
				fprintf(stderr, "Bad host specification.\n");
				goto err;
			}
			port = (unsigned short)atoi(portstr);
			if ((ret = dbenv->repmgr_add_remote_site(dbenv, host,
			    port, NULL, friend ? DB_REPMGR_PEER : 0)) != 0) {
				dbenv->err(dbenv, ret,
				    "Could not add site %s:%d", host,
				    (int)port);
				goto err;
			}
			break;
		case 'p':
			priority = atoi(optarg);
			break;
		case 'v':
			if ((ret = dbenv->set_verbose(dbenv,
			    DB_VERB_REPLICATION, 1)) != 0)
				goto err;
			break;
		case '?':
		default:
			usage(progname);
		}
	}

	/* Error check command line. */
	if ((!got_listen_address) || home == NULL)
		usage(progname);

	dbenv->rep_set_priority(dbenv, priority);

	if ((ret = env_init(dbenv, home)) != 0)
		goto err;

	if ((ret = dbenv->repmgr_start(dbenv, 3, start_policy)) != 0)
		goto err;

	if ((ret = doloop(dbenv, &my_app_data.shared_data)) != 0) {
		dbenv->err(dbenv, ret, "Client failed");
		goto err;
	}

	/*
	 * We have used the DB_TXN_NOSYNC environment flag for improved
	 * performance without the usual sacrifice of transactional durability,
	 * as discussed in the "Transactional guarantees" page of the Reference
	 * Guide: if one replication site crashes, we can expect the data to
	 * exist at another site.  However, in case we shut down all sites
	 * gracefully, we push out the end of the log here so that the most
	 * recent transactions don't mysteriously disappear.
	 */
	if ((ret = dbenv->log_flush(dbenv, NULL)) != 0) {
		dbenv->err(dbenv, ret, "log_flush");
		goto err;
	}

err:
	if (dbenv != NULL &&
	    (t_ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr, "failure closing env: %s (%d)\n",
		    db_strerror(t_ret), t_ret);
		if (ret == 0)
			ret = t_ret;
	}

	return (ret);
}

static void
event_callback(dbenv, which, info)
	DB_ENV *dbenv;
	u_int32_t which;
	void *info;
{
	APP_DATA *app = dbenv->app_private;
	SHARED_DATA *shared = &app->shared_data;

	info = NULL;				/* Currently unused. */

	switch (which) {
	case DB_EVENT_REP_CLIENT:
		shared->is_master = 0;
		break;

	case DB_EVENT_REP_MASTER:
		shared->is_master = 1;
		break;

	case DB_EVENT_REP_PERM_FAILED:
		printf("insufficient acks\n");
		break;

	case DB_EVENT_REP_STARTUPDONE: /* FALLTHROUGH */
	case DB_EVENT_REP_NEWMASTER:
		/* I don't care about these, for now. */
		break;

	default:
		dbenv->errx(dbenv, "ignoring event %d", which);
	}
}
