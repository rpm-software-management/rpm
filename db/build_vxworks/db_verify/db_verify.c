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
    "Id: db_verify.c,v 1.25 2001/10/09 18:20:32 bostic Exp ";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "clib_ext.h"

int db_verify_main __P((int, char *[]));
int db_verify_usage __P((void));
int db_verify_version_check __P((const char *));

int
db_verify(args)
	char *args;
{
	int argc;
	char **argv;

	__db_util_arg("db_verify", args, &argc, &argv);
	return (db_verify_main(argc, argv) ? EXIT_FAILURE : EXIT_SUCCESS);
}

#include <stdio.h>
#define	ERROR_RETURN	ERROR

int
db_verify_main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind, __db_getopt_reset;
	DB *dbp;
	DB_ENV *dbenv;
	const char *progname = "db_verify";
	int ch, e_close, exitval, nflag, quiet, ret, t_ret;
	char *home;

	if ((ret = db_verify_version_check(progname)) != 0)
		return (ret);

	dbenv = NULL;
	e_close = exitval = nflag = quiet = 0;
	home = NULL;
	__db_getopt_reset = 1;
	while ((ch = getopt(argc, argv, "h:NqV")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
			break;
		case 'N':
			nflag = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case '?':
		default:
			return (db_verify_usage());
		}
	argc -= optind;
	argv += optind;

	if (argc <= 0)
		return (db_verify_usage());

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr, "%s: db_env_create: %s\n",
		    progname, db_strerror(ret));
		goto shutdown;
	}
	e_close = 1;

	/*
	 * XXX
	 * We'd prefer to have error output configured while calling
	 * db_env_create, but there's no way to turn it off once it's
	 * turned on.
	 */
	if (!quiet) {
		dbenv->set_errfile(dbenv, stderr);
		dbenv->set_errpfx(dbenv, progname);
	}

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
	 * Attach to an mpool if it exists, but if that fails, attach to a
	 * private region.  In the latter case, declare a reasonably large
	 * cache so that we don't fail when verifying large databases.
	 */
	if ((ret =
	    dbenv->open(dbenv, home, DB_INIT_MPOOL | DB_USE_ENVIRON, 0)) != 0) {
		if ((ret = dbenv->set_cachesize(dbenv, 0, MEGABYTE, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_cachesize");
			goto shutdown;
		}
		if ((ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON, 0)) != 0) {
			dbenv->err(dbenv, ret, "open");
			goto shutdown;
		}
	}

	for (; !__db_util_interrupted() && argv[0] != NULL; ++argv) {
		if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
			fprintf(stderr,
			    "%s: db_create: %s\n", progname, db_strerror(ret));
			goto shutdown;
		}
		if (!quiet) {
			dbp->set_errfile(dbp, stderr);
			dbp->set_errpfx(dbp, progname);
		}
		if ((ret = dbp->verify(dbp, argv[0], NULL, NULL, 0)) != 0)
			dbp->err(dbp, ret, "DB->verify: %s", argv[0]);
		if ((t_ret = dbp->close(dbp, 0)) != 0 && ret == 0) {
			dbenv->err(dbenv, ret, "DB->close: %s", argv[0]);
			ret = t_ret;
		}
		if (ret != 0)
			goto shutdown;
	}

	if (0) {
shutdown:	exitval = 1;
	}
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
db_verify_usage()
{
	fprintf(stderr, "usage: db_verify [-NqV] [-h home] db_file ...\n");
	return (EXIT_FAILURE);
}

int
db_verify_version_check(progname)
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
