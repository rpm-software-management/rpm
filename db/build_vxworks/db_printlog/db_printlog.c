/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2001
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2001\nSleepycat Software Inc.  All rights reserved.\n";
static const char revid[] =
    "Id: db_printlog.c,v 11.36 2001/10/11 22:46:27 ubell Exp ";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"
#include "db_am.h"
#include "hash.h"
#include "log.h"
#include "qam.h"
#include "txn.h"
#include "clib_ext.h"

int db_printlog_main __P((int, char *[]));
int db_printlog_usage __P((void));
int db_printlog_version_check __P((const char *));

int
db_printlog(args)
	char *args;
{
	int argc;
	char **argv;

	__db_util_arg("db_printlog", args, &argc, &argv);
	return (db_printlog_main(argc, argv) ? EXIT_FAILURE : EXIT_SUCCESS);
}

#include <stdio.h>
#define	ERROR_RETURN	ERROR

int
db_printlog_main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind, __db_getopt_reset;
	const char *progname = "db_printlog";
	DB_ENV	*dbenv;
	DB_LOGC *logc;
	int (**dtab) __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t dtabsize;
	DBT data;
	DB_LSN key;
	int ch, e_close, exitval, nflag, rflag, ret;
	char *home;

	if ((ret = db_printlog_version_check(progname)) != 0)
		return (ret);

	logc = NULL;
	e_close = exitval = nflag = rflag = 0;
	home = NULL;
	dtabsize = 0;
	dtab = NULL;
	__db_getopt_reset = 1;
	while ((ch = getopt(argc, argv, "h:NrV")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
			break;
		case 'N':
			nflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case '?':
		default:
			return (db_printlog_usage());
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		return (db_printlog_usage());

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		goto shutdown;
	}
	e_close = 1;

	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, progname);

	if (nflag) {
		if ((ret = dbenv->set_flags(dbenv, DB_NOLOCKING, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOLOCKING");
			goto shutdown;
		}
		if ((ret = dbenv->set_flags(dbenv, DB_NOPANIC, 0)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOPANIC");
			goto shutdown;
		}
	}

	/*
	 * An environment is required, but as all we're doing is reading log
	 * files, we create one if it doesn't already exist.  If we create
	 * it, create it private so it automatically goes away when we're done.
	 */
	if ((ret = dbenv->open(dbenv, home,
	    DB_JOINENV | DB_USE_ENVIRON, 0)) != 0 &&
	    (ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_LOG | DB_PRIVATE | DB_USE_ENVIRON, 0)) != 0) {
		dbenv->err(dbenv, ret, "open");
		goto shutdown;
	}

	/* Initialize print callbacks. */
	if ((ret = __bam_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __crdel_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __db_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __qam_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __ham_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __log_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __txn_init_print(dbenv, &dtab, &dtabsize)) != 0) {
		dbenv->err(dbenv, ret, "callback: initialization");
		goto shutdown;
	}

	/* Allocate a log cursor. */
	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB_ENV->log_cursor");
		goto shutdown;
	}

	memset(&data, 0, sizeof(data));
	while (!__db_util_interrupted()) {
		if ((ret = logc->get(
		    logc, &key, &data, rflag ? DB_PREV : DB_NEXT)) != 0) {
			if (ret == DB_NOTFOUND)
				break;
			dbenv->err(dbenv, ret, "DB_LOGC->get");
			goto shutdown;
		}

		/*
		 * XXX
		 * We use DB_TXN_ABORT as our op because that's the only op
		 * that calls the underlying recovery function without any
		 * consideration as to the contents of the transaction list.
		 */
		ret =
		    __db_dispatch(dbenv, dtab, &data, &key, DB_TXN_ABORT, NULL);

		/*
		 * XXX
		 * Just in case the underlying routines don't flush.
		 */
		(void)fflush(stdout);

		if (ret != 0) {
			dbenv->err(dbenv, ret, "tx: dispatch");
			goto shutdown;
		}
	}

	if (0) {
shutdown:	exitval = 1;
	}
	if (logc != NULL && (ret = logc->close(logc, 0)) != 0)
		exitval = 1;

	if (dtab != NULL)
		__os_free(dbenv, dtab, 0);
	if (e_close && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	/* Resend any caught signal. */
	__db_util_sigresend();

	return (exitval == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

int
db_printlog_usage()
{
	fprintf(stderr, "usage: db_printlog [-NrV] [-h home]\n");
	return (EXIT_FAILURE);
}

int
db_printlog_version_check(progname)
	const char *progname;
{
	int v_major, v_minor, v_patch;

	/* Make sure we're loaded with the right version of the DB library. */
	(void)db_version(&v_major, &v_minor, &v_patch);
	if (v_major != DB_VERSION_MAJOR ||
	    v_minor != DB_VERSION_MINOR || v_patch != DB_VERSION_PATCH) {
		fprintf(stderr,
	"%s: version %d.%d.%d doesn't match library version %d.%d.%d\n",
		    progname, DB_VERSION_MAJOR, DB_VERSION_MINOR,
		    DB_VERSION_PATCH, v_major, v_minor, v_patch);
		return (EXIT_FAILURE);
	}
	return (0);
}
