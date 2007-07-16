/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: rep_mgr.c,v 12.15 2006/09/08 20:32:06 bostic Exp $
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

const char *progname = "ex_rep";

#ifdef _WIN32
extern int getopt(int, char * const *, const char *);
#endif


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

	my_app_data.is_master = 0; /* assume I start out as client */
	dbenv = NULL;
	ret = got_listen_address = 0;
	home = "TESTDIR";

	if ((ret = create_env(progname, &dbenv)) != 0)
		goto err;
	dbenv->app_private = &my_app_data;

	start_policy = DB_REP_ELECTION;	/* default */
	priority = 100;		/* default */

	while ((ch = getopt(argc, argv, "CFf:h:Mm:n:o:p:v")) != EOF) {
		friend = 0;
		switch (ch) {
		case 'C':
			start_policy = DB_REP_CLIENT;
			break;
		case 'F':
			start_policy = DB_REP_FULL_ELECTION;
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
				    "Could not set listen address (%d).\n", ret);
				goto err;
			}
			got_listen_address = 1;
			break;
		case 'n':
			totalsites = atoi(optarg);
			if ((ret = dbenv->rep_set_nsites(dbenv, totalsites)) != 0)
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

	if ((ret = doloop(dbenv, &my_app_data)) != 0) {
		dbenv->err(dbenv, ret, "Client failed");
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
