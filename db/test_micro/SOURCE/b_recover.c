/*
 * $Id: b_recover.c,v 1.8 2007/05/29 17:39:15 bostic Exp $
 */
#include "bench.h"

int usage(void);

int
main(int argc, char *argv[])
{
	DB *dbp;
	DBT key, data;
	DB_ENV *dbenv;
	DB_TXN *txn;
	int ch, i, count;
	char *config;

	cleanup_test_dir();

	/*
	 * Recover was too slow before release 4.0 that it's not worth
	 * running the test.
	 */
#if DB_VERSION_MAJOR < 4
	return (0);
#endif

	count = 1000;
	config = "synchronous";
	while ((ch = getopt(argc, argv, "c:")) != EOF)
		switch (ch) {
		case 'c':
			count = atoi(optarg);
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		return (usage());

	/* Create the environment. */
	DB_BENCH_ASSERT(db_env_create(&dbenv, 0) == 0);
	dbenv->set_errfile(dbenv, stderr);
	DB_BENCH_ASSERT(
	    dbenv->set_cachesize(dbenv, 0, 1048576 /* 1MB */, 0) == 0);

#define	OFLAGS								\
	DB_CREATE | DB_INIT_LOCK |					\
	DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_PRIVATE
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR", NULL, OFLAGS, 0666) == 0);
#endif
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 1
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR", OFLAGS, 0666) == 0);
#endif
#if DB_VERSION_MAJOR > 3 || DB_VERSION_MINOR > 1
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR", OFLAGS, 0666) == 0);
#endif

	/* Create the database. */
	DB_BENCH_ASSERT(db_create(&dbp, dbenv, 0) == 0);
#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
	DB_BENCH_ASSERT(dbp->open(dbp, NULL,
	    "a", NULL, DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, 0666) == 0);
#else
	DB_BENCH_ASSERT(
	    dbp->open(dbp, "a", NULL, DB_BTREE, DB_CREATE, 0666) == 0);
#endif

	/* Initialize the data. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.size = data.size = 20;
	key.data = data.data = "01234567890123456789";

	/* Start/commit a transaction count times. */
	for (i = 0; i < count; ++i) {
#if DB_VERSION_MAJOR < 4
		DB_BENCH_ASSERT(
		    txn_begin(dbenv, NULL, &txn, DB_TXN_NOSYNC) == 0);
		DB_BENCH_ASSERT(dbp->put(dbp, txn, &key, &data, 0) == 0);
		DB_BENCH_ASSERT(txn_commit(txn, 0) == 0);
#else
		DB_BENCH_ASSERT(
		    dbenv->txn_begin(dbenv, NULL, &txn, DB_TXN_NOSYNC) == 0);
		DB_BENCH_ASSERT(dbp->put(dbp, txn, &key, &data, 0) == 0);
		DB_BENCH_ASSERT(txn->commit(txn, 0) == 0);
#endif
	}

	/* Flush the log buffer. */
#if DB_VERSION_MAJOR < 4
	DB_BENCH_ASSERT(log_flush(dbenv, NULL) == 0);
#else
	DB_BENCH_ASSERT(dbenv->log_flush(dbenv, NULL) == 0);
#endif

	/* Create a new DB_ENV handle. */
	DB_BENCH_ASSERT(db_env_create(&dbenv, 0) == 0);
	dbenv->set_errfile(dbenv, stderr);
	DB_BENCH_ASSERT(
	    dbenv->set_cachesize(dbenv, 0, 1048576 /* 1MB */, 0) == 0);

	/* Now run recovery. */
	TIMER_START;
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0
	DB_BENCH_ASSERT(dbenv->open(
	    dbenv, "TESTDIR", NULL, OFLAGS | DB_RECOVER, 0666) == 0);
#endif
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 1
	DB_BENCH_ASSERT(
	    dbenv->open(dbenv, "TESTDIR", OFLAGS | DB_RECOVER, 0666) == 0);
#endif
#if DB_VERSION_MAJOR > 3 || DB_VERSION_MINOR > 1
	DB_BENCH_ASSERT(
	    dbenv->open(dbenv, "TESTDIR", OFLAGS | DB_RECOVER, 0666) == 0);
#endif
	TIMER_STOP;

	printf("# recovery after %d transactions\n", count);
	TIMER_DISPLAY(count);

	return (0);
}

int
usage()
{
	(void)fprintf(stderr, "usage: b_recover [-c count]\n");
	return (EXIT_FAILURE);
}
