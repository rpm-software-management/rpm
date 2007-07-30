m4_ignore([dnl
#include <sys/types.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <db.h>

#define	DATABASE1	"foo"
#define	DATABASE2	"bar"

int db_setup(char *, char *, FILE *, char *);

int
main()
{
	return (db_setup("/tmp/__dbenv__", "test", stderr, "test_app"));
}
int
db_setup(home, data_dir, errfp, progname)
	char *home, *data_dir, *progname;
	FILE *errfp;
{])
m4_indent([dnl
	DB_ENV *dbenv;
	DB *dbp1, *dbp2;
	int ret;
m4_blank
	dbenv = NULL;
	dbp1 = dbp2 = NULL;
m4_blank
	/*
	 * Create an environment and initialize it for additional error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (ret);
	}
m4_blank
	dbenv-__GT__set_errfile(dbenv, errfp);
	dbenv-__GT__set_errpfx(dbenv, progname);
m4_blank
	/* Open an environment with just a memory pool. */
	if ((ret =
	    dbenv-__GT__open(dbenv, home, DB_CREATE | DB_INIT_MPOOL, 0)) != 0) {
		dbenv-__GT__err(dbenv, ret, "environment open: %s", home);
		goto err;
	}
m4_blank
	/* Open database #1. */
	if ((ret = db_create(&dbp1, dbenv, 0)) != 0) {
		dbenv-__GT__err(dbenv, ret, "database create");
		goto err;
	}
	if ((ret = dbp1-__GT__open(dbp1,
	    NULL, DATABASE1, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		dbenv-__GT__err(dbenv, ret, "DB-__GT__open: %s", DATABASE1);
		goto err;
	}
m4_blank
	/* Open database #2. */
	if ((ret = db_create(&dbp2, dbenv, 0)) != 0) {
		dbenv-__GT__err(dbenv, ret, "database create");
		goto err;
	}
	if ((ret = dbp2-__GT__open(dbp2,
	    NULL, DATABASE2, NULL, DB_HASH, DB_CREATE, 0664)) != 0) {
		dbenv-__GT__err(dbenv, ret, "DB-__GT__open: %s", DATABASE2);
		goto err;
	}
m4_blank
	return (0);
m4_blank
err:	if (dbp2 != NULL)
		(void)dbp2-__GT__close(dbp2, 0);
	if (dbp1 != NULL)
		(void)dbp2-__GT__close(dbp1, 0);
	(void)dbenv-__GT__close(dbenv, 0);
	return (1);
}])
