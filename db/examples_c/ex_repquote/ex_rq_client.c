/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 *	Sleepycat Software.  All rights reserved.
 *
 * Id: ex_rq_client.c,v 1.21 2001/10/09 14:45:43 margo Exp 
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

#include "ex_repquote.h"

static void *check_loop __P((void *));
static void *display_loop __P((void *));
static int print_stocks __P((DBC *));

typedef struct {
	char *progname;
	DB_ENV *dbenv;
} disploop_args;

typedef struct {
	DB_ENV *dbenv;
	machtab_t *machtab;
} checkloop_args;

int
doclient(dbenv, progname, machtab)
	DB_ENV *dbenv;
	char *progname;
	machtab_t *machtab;
{
	checkloop_args cargs;
	disploop_args dargs;
	pthread_t check_thr, disp_thr;
	void *cstatus, *dstatus;
	int rval, s;

	rval = EXIT_SUCCESS;
	s = -1;

	memset(&dargs, 0, sizeof(dargs));
	dstatus = (void *)EXIT_FAILURE;

	dargs.progname = progname;
	dargs.dbenv = dbenv;
	if (pthread_create(&disp_thr, NULL, display_loop, (void *)&dargs)) {
		fprintf(stderr, "%s: display_loop pthread_create failed: %s\n",
		    progname, strerror(errno));
		goto err;
	}

	cargs.dbenv = dbenv;
	cargs.machtab = machtab;
	if (pthread_create(&check_thr, NULL, check_loop, (void *)&cargs)) {
		fprintf(stderr, "%s: check_thread pthread_create failed: %s\n",
		    progname, strerror(errno));
		goto err;
	}
	if (pthread_join(disp_thr, &dstatus) ||
	    pthread_join(check_thr, &cstatus)) {
		fprintf(stderr, "%s: pthread_join failed: %s\n",
		    progname, strerror(errno));
		goto err;
	}

	if (0) {
err:		rval = EXIT_FAILURE;
	}
	return (rval);
}

/*
 * Our only job is to check that the master is valid and if it's not
 * for an extended period, to trigger an election.  We do two phases.
 * If we do not have a master, first we send out a request for a master
 * to identify itself (that would be a call to rep_start).  If that fails,
 * we trigger an election.
 */
static void *
check_loop(args)
	void *args;
{
	DB_ENV *dbenv;
	DBT dbt;
	checkloop_args *cargs;
	int count, n, is_me, pri;
	machtab_t *machtab;
	u_int32_t check, elect;

	cargs = (checkloop_args *)args;
	dbenv = cargs->dbenv;
	machtab = cargs->machtab;

#define	IDLE_INTERVAL	1

	count = 0;
	while (1) {
		sleep(IDLE_INTERVAL);

		/* If we become master, shut this loop off. */
		if (master_eid == SELF_EID)
			break;

		if (master_eid == DB_INVALID_EID)
			count++;
		else
			count = 0;

		if (count <= 1)
			continue;

		/*
		 * First, call rep_start (possibly again) to see if we can
		 * find a master.
		 */

		memset(&dbt, 0, sizeof(dbt));
		dbt.data = myaddr;
		dbt.size = strlen(myaddr) + 1;
		(void)dbenv->rep_start(dbenv, &dbt, DB_REP_CLIENT);
		sleep(IDLE_INTERVAL);

		if (master_eid != DB_INVALID_EID) {
			count = 0;
			continue;
		}

		/* Now call for an election */
		machtab_parm(machtab, &n, &pri, &check, &elect);
		if (dbenv->rep_elect(dbenv,
		    n, pri, check, elect, &master_eid, &is_me) != 0)
			continue;

		/* If I'm the new master, I can stop checking for masters. */
		if (is_me) {
			master_eid = SELF_EID;
			break;
		}
	}

	return ((void *)EXIT_SUCCESS);
}

static void *
display_loop(args)
	void *args;
{
	DB *dbp;
	DB_ENV *dbenv;
	DBC *dbc;
	char *progname;
	disploop_args *dargs;
	int ret, rval;

	dargs = (disploop_args *)args;
	progname = dargs->progname;
	dbenv = dargs->dbenv;

	dbc = NULL;
	dbp = NULL;

	for (;;) {
		/* If we become master, shut this loop off. */
		if (master_eid == SELF_EID)
			break;

		if (dbp == NULL) {
			if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
				fprintf(stderr, "%s: db_create: %s\n", progname,
				    db_strerror(ret));
				return ((void *)EXIT_FAILURE);
			}

			if ((ret = dbp->open(dbp,
			    DATABASE, NULL, DB_BTREE, DB_RDONLY, 0)) != 0) {
				if (ret == ENOENT) {
					printf(
				    "No stock database yet available.\n");
					if ((ret = dbp->close(dbp, 0)) != 0) {
						fprintf(stderr,
						    "%s: DB->close: %s",
						    progname,
						    db_strerror(ret));
						goto err;
					}
					dbp = NULL;
					sleep(SLEEPTIME);
					continue;
				}
				fprintf(stderr, "%s: DB->open: %s\n", progname,
				    db_strerror(ret));
				goto err;
			}
		}

		if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0) {
			fprintf(stderr, "%s: DB->cursor: %s\n", progname,
			    db_strerror(ret));
			goto err;
		}

		if ((ret = print_stocks(dbc)) != 0) {
			fprintf(stderr, "%s: database traversal failed: %s\n",
			    progname, db_strerror(ret));
			goto err;
		}

		if ((ret = dbc->c_close(dbc)) != 0) {
			fprintf(stderr, "%s: DB->close: %s\n", progname,
			    db_strerror(ret));
			goto err;
		}

		dbc = NULL;

		sleep(SLEEPTIME);
	}

	rval = EXIT_SUCCESS;

	if (0) {
err:		rval = EXIT_FAILURE;
	}

	if (dbc != NULL && (ret = dbc->c_close(dbc)) != 0) {
		fprintf(stderr, "%s: DB->close: %s\n", progname,
		    db_strerror(ret));
		rval = EXIT_FAILURE;
	}

	if (dbp != NULL && (ret = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr, "%s: DB->close: %s\n", progname,
		    db_strerror(ret));
		return ((void *)EXIT_FAILURE);
	}

	return ((void *)rval);
}

static int
print_stocks(dbc)
	DBC *dbc;
{
	DBT key, data;
#define	MAXKEYSIZE	10
#define	MAXDATASIZE	20
	char keybuf[MAXKEYSIZE + 1], databuf[MAXDATASIZE + 1];
	int ret;
	u_int32_t keysize, datasize;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	printf("\tSymbol\tPrice\n");
	printf("\t======\t=====\n");

	for (ret = dbc->c_get(dbc, &key, &data, DB_FIRST);
	    ret == 0;
	    ret = dbc->c_get(dbc, &key, &data, DB_NEXT)) {
		keysize = key.size > MAXKEYSIZE ? MAXKEYSIZE : key.size;
		memcpy(keybuf, key.data, keysize);
		keybuf[keysize] = '\0';

		datasize = data.size >= MAXDATASIZE ? MAXDATASIZE : data.size;
		memcpy(databuf, data.data, datasize);
		databuf[datasize] = '\0';

		printf("\t%s\t%s\n", keybuf, databuf);
	}
	printf("\n");
	return (ret == DB_NOTFOUND ? 0 : ret);
}
