/*
 * $Id: b_txn_write.c,v 1.12 2007/06/07 14:06:36 moshen Exp $
 */
#include "bench.h"

int usage(void);

/*
 * send --
 *	A stubbed-out replication message function.
 */
int
send(dbenv, control, rec, lsn, eid, flags)
	DB_ENV *dbenv;
	const DBT *control, *rec;
	const DB_LSN *lsn;
	int eid;
	u_int32_t flags;
{
	return (0);
}

int
main(int argc, char *argv[])
{
	DB *dbp;
	DBT key, data;
	DB_ENV *dbenv;
	DB_TXN *txn;
	u_int32_t flags, oflags;
	int ch, i, count, rep_stub;
	char *config;

	cleanup_test_dir();

	count = 1000;
	oflags = flags = 0;
	rep_stub = 0;
	config = "synchronous";
	while ((ch = getopt(argc, argv, "ac:rw")) != EOF)
		switch (ch) {
		case 'a':
			config = "nosync";
			flags = DB_TXN_NOSYNC;
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case 'r':
#ifdef DB_INIT_REP
			rep_stub = 1;
#else
			exit(0);
#endif
			break;
		case 'w':
			config = "write-nosync";
#ifdef DB_TXN_WRITE_NOSYNC
			flags = DB_TXN_WRITE_NOSYNC;
#else
			exit(0);
#endif
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

#ifdef DB_INIT_REP
	if (rep_stub) {
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 5 || DB_VERSION_MAJOR > 4
		DB_BENCH_ASSERT(dbenv->rep_set_transport(dbenv, 1, send) == 0);
#else
		DB_BENCH_ASSERT(dbenv->set_rep_transport(dbenv, 1, send) == 0);
#endif
		oflags |= DB_INIT_REP;
	}
#endif
	oflags |= DB_CREATE | DB_INIT_LOCK |
	    DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_PRIVATE;
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0
	DB_BENCH_ASSERT(
	    dbenv->open(dbenv, "TESTDIR", NULL, flags | oflags, 0666) == 0);
#endif
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 1
	DB_BENCH_ASSERT(
	    dbenv->open(dbenv, "TESTDIR", flags | oflags, 0666) == 0);
#endif
#if DB_VERSION_MAJOR > 3 || DB_VERSION_MINOR > 1
	if (flags != 0)
		DB_BENCH_ASSERT(dbenv->set_flags(dbenv, flags, 1) == 0);
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR", oflags, 0666) == 0);
#endif

#ifdef DB_INIT_REP
	if (rep_stub)
		DB_BENCH_ASSERT(
		    dbenv->rep_start(dbenv, NULL, DB_REP_MASTER) == 0);
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
	TIMER_START;
	for (i = 0; i < count; ++i) {
#if DB_VERSION_MAJOR < 4
		DB_BENCH_ASSERT(txn_begin(dbenv, NULL, &txn, 0) == 0);
		DB_BENCH_ASSERT(dbp->put(dbp, txn, &key, &data, 0) == 0);
		DB_BENCH_ASSERT(txn_commit(txn, 0) == 0);
#else
		DB_BENCH_ASSERT(dbenv->txn_begin(dbenv, NULL, &txn, 0) == 0);
		DB_BENCH_ASSERT(dbp->put(dbp, txn, &key, &data, 0) == 0);
		DB_BENCH_ASSERT(txn->commit(txn, 0) == 0);
#endif
	}
	TIMER_STOP;

	printf("# %d %stransactions write %s commit pairs\n",
	    count, rep_stub ? "replicated ": "", config);
	TIMER_DISPLAY(count);

	return (0);
}

int
usage()
{
	(void)fprintf(stderr, "usage: b_txn_write [-arw] [-c count]\n");
	return (EXIT_FAILURE);
}
