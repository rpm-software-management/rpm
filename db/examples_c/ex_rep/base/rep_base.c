/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: rep_base.c,v 12.20 2007/05/17 17:29:27 bostic Exp $
 */

#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>

#include "rep_base.h"

/*
 * Process globals (we could put these in the machtab I suppose).
 */
int master_eid;
char *myaddr;
unsigned short myport;

static void event_callback __P((DB_ENV *, u_int32_t, void *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	DB_ENV *dbenv;
	DBT local;
	enum { MASTER, CLIENT, UNKNOWN } whoami;
	all_args aa;
	connect_args ca;
	machtab_t *machtab;
	thread_t all_thr, conn_thr;
	void *astatus, *cstatus;
#ifdef _WIN32
	WSADATA wsaData;
#else
	struct sigaction sigact;
#endif
	repsite_t site, *sitep, self, *selfp;
	int maxsites, nsites, ret, priority, totalsites;
	char *c, ch;
	const char *home, *progname;
	APP_DATA my_app_data;

	master_eid = DB_EID_INVALID;

	my_app_data.elected = 0;
	my_app_data.shared_data.is_master = 0; /* assume start out as client */
	dbenv = NULL;
	whoami = UNKNOWN;
	machtab = NULL;
	selfp = sitep = NULL;
	maxsites = nsites = ret = totalsites = 0;
	priority = 100;
	home = "TESTDIR";
	progname = "ex_rep_base";

	if ((ret = create_env(progname, &dbenv)) != 0)
		goto err;
	dbenv->app_private = &my_app_data;
	(void)dbenv->set_event_notify(dbenv, event_callback);

	while ((ch = getopt(argc, argv, "Ch:Mm:n:o:p:v")) != EOF)
		switch (ch) {
		case 'M':
			whoami = MASTER;
			master_eid = SELF_EID;
			break;
		case 'C':
			whoami = CLIENT;
			break;
		case 'h':
			home = optarg;
			break;
		case 'm':
			if ((myaddr = strdup(optarg)) == NULL) {
				fprintf(stderr,
				    "System error %s\n", strerror(errno));
				goto err;
			}
			self.host = optarg;
			self.host = strtok(self.host, ":");
			if ((c = strtok(NULL, ":")) == NULL) {
				fprintf(stderr, "Bad host specification.\n");
				goto err;
			}
			myport = self.port = (unsigned short)atoi(c);
			selfp = &self;
			break;
		case 'n':
			totalsites = atoi(optarg);
			break;
		case 'o':
			site.host = optarg;
			site.host = strtok(site.host, ":");
			if ((c = strtok(NULL, ":")) == NULL) {
				fprintf(stderr, "Bad host specification.\n");
				goto err;
			}
			site.port = atoi(c);
			if (sitep == NULL || nsites >= maxsites) {
				maxsites = maxsites == 0 ? 10 : 2 * maxsites;
				if ((sitep = realloc(sitep,
				    maxsites * sizeof(repsite_t))) == NULL) {
					fprintf(stderr, "System error %s\n",
					    strerror(errno));
					goto err;
				}
			}
			sitep[nsites++] = site;
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

	/* Error check command line. */
	if (whoami == UNKNOWN) {
		fprintf(stderr, "Must specify -M or -C.\n");
		goto err;
	}

	if (selfp == NULL)
		usage(progname);

	if (home == NULL)
		usage(progname);

	dbenv->rep_set_priority(dbenv, priority);

#ifdef _WIN32
	/* Initialize the Windows sockets DLL. */
	if ((ret = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
		fprintf(stderr,
		    "Unable to initialize Windows sockets: %d\n", ret);
		goto err;
	}
#else
	/*
	 * Turn off SIGPIPE so that we don't kill processes when they
	 * happen to lose a connection at the wrong time.
	 */
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = SIG_IGN;
	if ((ret = sigaction(SIGPIPE, &sigact, NULL)) != 0) {
		fprintf(stderr,
		    "Unable to turn off SIGPIPE: %s\n", strerror(ret));
		goto err;
	}
#endif

	/*
	 * We are hardcoding priorities here that all clients have the
	 * same priority except for a designated master who gets a higher
	 * priority.
	 */
	if ((ret =
	    machtab_init(&machtab, totalsites)) != 0)
		goto err;
	my_app_data.comm_infrastructure = machtab;

	if ((ret = env_init(dbenv, home)) != 0)
		goto err;

	/*
	 * Now sets up comm infrastructure.  There are two phases.  First,
	 * we open our port for listening for incoming connections.  Then
	 * we attempt to connect to every host we know about.
	 */

	(void)dbenv->rep_set_transport(dbenv, SELF_EID, quote_send);

	ca.dbenv = dbenv;
	ca.home = home;
	ca.progname = progname;
	ca.machtab = machtab;
	ca.port = selfp->port;
	if ((ret = thread_create(&conn_thr, NULL, connect_thread, &ca)) != 0) {
		dbenv->errx(dbenv, "can't create connect thread");
		goto err;
	}

	aa.dbenv = dbenv;
	aa.progname = progname;
	aa.home = home;
	aa.machtab = machtab;
	aa.sites = sitep;
	aa.nsites = nsites;
	if ((ret = thread_create(&all_thr, NULL, connect_all, &aa)) != 0) {
		dbenv->errx(dbenv, "can't create connect-all thread");
		goto err;
	}

	/*
	 * We have now got the entire communication infrastructure set up.
	 * It's time to declare ourselves to be a client or master.
	 */
	if (whoami == MASTER) {
		if ((ret = dbenv->rep_start(dbenv, NULL, DB_REP_MASTER)) != 0) {
			dbenv->err(dbenv, ret, "dbenv->rep_start failed");
			goto err;
		}
	} else {
		memset(&local, 0, sizeof(local));
		local.data = myaddr;
		local.size = (u_int32_t)strlen(myaddr) + 1;
		if ((ret =
		    dbenv->rep_start(dbenv, &local, DB_REP_CLIENT)) != 0) {
			dbenv->err(dbenv, ret, "dbenv->rep_start failed");
			goto err;
		}
		/* Sleep to give ourselves time to find a master. */
		sleep(5);
	}
	if ((ret = doloop(dbenv, &my_app_data.shared_data)) != 0) {
		dbenv->err(dbenv, ret, "Main loop failed");
		goto err;
	}

	/* Wait on the connection threads. */
	if (thread_join(all_thr, &astatus) || thread_join(conn_thr, &cstatus)) {
		ret = -1;
		goto err;
	}
	if ((uintptr_t)astatus != EXIT_SUCCESS ||
	    (uintptr_t)cstatus != EXIT_SUCCESS) {
		ret = -1;
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
	if ((ret = dbenv->log_flush(dbenv, NULL)) != 0)
		dbenv->err(dbenv, ret, "log_flush");

err:	if (machtab != NULL)
		free(machtab);
	if (dbenv != NULL)
		(void)dbenv->close(dbenv, 0);
#ifdef _WIN32
	/* Shut down the Windows sockets DLL. */
	(void)WSACleanup();
#endif
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

	switch (which) {
	case DB_EVENT_REP_CLIENT:
		shared->is_master = 0;
		break;

	case DB_EVENT_REP_ELECTED:
		app->elected = 1;
		master_eid = SELF_EID;
		break;

	case DB_EVENT_REP_MASTER:
		shared->is_master = 1;
		break;

	case DB_EVENT_REP_NEWMASTER:
		master_eid = *(int*)info;
		break;

	case DB_EVENT_REP_STARTUPDONE:
		/* I don't care about this, for now. */
		break;

	default:
		dbenv->errx(dbenv, "ignoring event %d", which);
	}
}
