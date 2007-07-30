m4_ignore([dnl
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

void env_open(DB_ENV **);

#define	ENV_DIRECTORY	"/tmp"

int
main()
{
	DB_ENV *dbenv;
	env_open(&dbenv);
	return (0);
}])
m4_indent([dnl
void
env_open(DB_ENV **dbenvp)
{
	DB_ENV *dbenv;
	int ret;
m4_blank
	/* Create the environment handle. */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "txnapp: db_env_create: %s\n", db_strerror(ret));
		exit (1);
	}
m4_blank
	/* Set up error handling. */
	dbenv-__GT__set_errpfx(dbenv, "txnapp");
	dbenv-__GT__set_errfile(dbenv, stderr);
m4_blank
m4_cbold([dnl
	/* Do deadlock detection internally. */
	if ((ret = dbenv-__GT__set_lk_detect(dbenv, DB_LOCK_DEFAULT)) != 0) {
		dbenv-__GT__err(dbenv, ret, "set_lk_detect: DB_LOCK_DEFAULT");
		exit (1);
	}])
m4_blank
	/*
	 * Open a transactional environment:
	 *	create if it doesn't exist
	 *	free-threaded handle
	 *	run recovery
	 *	read/write owner only
	 */
	if ((ret = dbenv-__GT__open(dbenv, ENV_DIRECTORY,
	    DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG |
	    DB_INIT_MPOOL | DB_INIT_TXN | DB_RECOVER | DB_THREAD,
	    S_IRUSR | S_IWUSR)) != 0) {
		dbenv-__GT__err(dbenv, ret, "dbenv-__GT__open: %s", ENV_DIRECTORY);
		exit (1);
	}
m4_blank
	*dbenvp = dbenv;
}])
