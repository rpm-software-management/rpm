m4_ignore([dnl
#include <sys/types.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <db.h>

DB_ENV *db_setup(char *, char *, FILE *, char *);

int
main()
{
	DB_ENV *dbenv;

	system("rm -rf /tmp/__dbenv__; mkdir -p /tmp/__dbenv__/test");
	dbenv = db_setup("/tmp/__dbenv__", "test", stderr, "test_app");
	system("rm -rf /tmp/__dbenv__");
	return (0);
}])
m4_indent([dnl
DB_ENV *
db_setup(home, data_dir, errfp, progname)
	char *home, *data_dir, *progname;
	FILE *errfp;
{
	DB_ENV *dbenv;
	int ret;
m4_blank
	/*
	 * Create an environment and initialize it for additional error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(errfp, "%s: %s\n", progname, db_strerror(ret));
		return (NULL);
	}
	dbenv-__GT__set_errfile(dbenv, errfp);
	dbenv-__GT__set_errpfx(dbenv, progname);
m4_blank
	/*
	 * Specify the shared memory buffer pool cachesize: 5MB.
	 * Databases are in a subdirectory of the environment home.
	 */
	if ((ret = dbenv-__GT__set_cachesize(dbenv, 0, 5 * 1024 * 1024, 0)) != 0) {
		dbenv-__GT__err(dbenv, ret, "set_cachesize");
		goto err;
	}
	if ((ret = dbenv-__GT__set_data_dir(dbenv, data_dir)) != 0) {
		dbenv-__GT__err(dbenv, ret, "set_data_dir: %s", data_dir);
		goto err;
	}
m4_blank
	/* Open the environment with full transactional support. */
	if ((ret = dbenv-__GT__open(dbenv, home, DB_CREATE |
	    DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN, 0)) != 0) {
		dbenv-__GT__err(dbenv, ret, "environment open: %s", home);
		goto err;
	}
m4_blank
	return (dbenv);
m4_blank
err:	(void)dbenv-__GT__close(dbenv, 0);
	return (NULL);
}])
