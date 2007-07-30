/*
 * $Id: b_txn.c,v 1.10 2007/06/21 17:42:10 bostic Exp $
 */
#include "bench.h"

int usage(void);

int
main(int argc, char *argv[])
{
	DB_ENV *dbenv;
	DB_TXN *txn;
	int tabort, ch, i, count;

	cleanup_test_dir();

	count = 1000;
	tabort = 0;
	while ((ch = getopt(argc, argv, "ac:")) != EOF)
		switch (ch) {
		case 'a':
			tabort = 1;
			break;
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
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR < 1
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR",
	    NULL, DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG |
	    DB_INIT_MPOOL | DB_INIT_TXN | DB_PRIVATE, 0666) == 0);
#else
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR",
	    DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG |
	    DB_INIT_MPOOL | DB_INIT_TXN | DB_PRIVATE, 0666) == 0);
#endif

	/* Start and commit/abort a transaction count times. */
	TIMER_START;
	if (tabort)
		for (i = 0; i < count; ++i) {
#if DB_VERSION_MAJOR < 4
			DB_BENCH_ASSERT(txn_begin(dbenv, NULL, &txn, 0) == 0);
			DB_BENCH_ASSERT(txn_abort(txn) == 0);
#else
			DB_BENCH_ASSERT(
			    dbenv->txn_begin(dbenv, NULL, &txn, 0) == 0);
			DB_BENCH_ASSERT(txn->abort(txn) == 0);
#endif
		}
	else
		for (i = 0; i < count; ++i) {
#if DB_VERSION_MAJOR < 4
			DB_BENCH_ASSERT(txn_begin(dbenv, NULL, &txn, 0) == 0);
			DB_BENCH_ASSERT(txn_commit(txn, 0) == 0);
#else
			DB_BENCH_ASSERT(
			    dbenv->txn_begin(dbenv, NULL, &txn, 0) == 0);
			DB_BENCH_ASSERT(txn->commit(txn, 0) == 0);
#endif
		}
	TIMER_STOP;

	printf("# %d empty transaction start/%s pairs\n",
	    count, tabort ? "abort" : "commit");
	TIMER_DISPLAY(count);

	return (0);
}

int
usage()
{
	(void)fprintf(stderr, "usage: b_txn [-c count]\n");
	return (EXIT_FAILURE);
}
