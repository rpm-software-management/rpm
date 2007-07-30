/*
 * $Id: b_put.c,v 1.11 2007/05/29 17:39:15 bostic Exp $
 */
#include "bench.h"

int usage(void);
int s(DB *, const DBT *, const DBT *, DBT *);

int
main(int argc, char *argv[])
{
	DB_ENV *dbenv = NULL;
	DB *dbp, **second;
	DBTYPE type;
	DBT key, data;
	db_recno_t recno;
	size_t dsize;
	int ch, i, count, secondaries;
	char *ts, buf[64];

	cleanup_test_dir();

	count = 100000;
	ts = "Btree";
	type = DB_BTREE;
	dsize = 20;
	secondaries = 0;
	while ((ch = getopt(argc, argv, "c:d:s:t:")) != EOF)
		switch (ch) {
		case 'c':
			count = atoi(optarg);
			break;
		case 'd':
			dsize = atoi(optarg);
			break;
		case 's':
			secondaries = atoi(optarg);
			break;
		case 't':
			switch (optarg[0]) {
			case 'B': case 'b':
				ts = "Btree";
				type = DB_BTREE;
				break;
			case 'H': case 'h':
				ts = "Hash";
				type = DB_HASH;
				break;
			case 'Q': case 'q':
				ts = "Queue";
				type = DB_QUEUE;
				break;
			case 'R': case 'r':
				ts = "Recno";
				type = DB_RECNO;
				break;
			default:
				return (usage());
			}
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		return (usage());

#if DB_VERSION_MAJOR < 3 || DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR < 3
	/*
	 * Secondaries were added after DB 3.2.9.
	 */
	if (secondaries)
		return (0);
#endif

	/* Create the environment. */
	DB_BENCH_ASSERT(db_env_create(&dbenv, 0) == 0);
	dbenv->set_errfile(dbenv, stderr);
	DB_BENCH_ASSERT(
	    dbenv->set_cachesize(dbenv, 0, 1048576 /* 1MB */, 0) == 0);
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR < 1
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR",
	    NULL, DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE, 0666) == 0);
#else
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR",
	    DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE, 0666) == 0);
#endif

	/*
	 * Create the database.
	 * Optionally set the record length for Queue.
	 */
	DB_BENCH_ASSERT(db_create(&dbp, dbenv, 0) == 0);
	if (type == DB_QUEUE)
		DB_BENCH_ASSERT(dbp->set_re_len(dbp, dsize) == 0);
#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
	DB_BENCH_ASSERT(
	    dbp->open(dbp, NULL, "a", NULL, type, DB_CREATE, 0666) == 0);
#else
	DB_BENCH_ASSERT(dbp->open(dbp, "a", NULL, type, DB_CREATE, 0666) == 0);
#endif

	/* Optionally create the secondaries. */
	if (secondaries != 0) {
		DB_BENCH_ASSERT(
		    (second = calloc(sizeof(DB *), secondaries)) != NULL);
		for (i = 0; i < secondaries; ++i) {
			DB_BENCH_ASSERT(db_create(&second[i], dbenv, 0) == 0);
			snprintf(buf, sizeof(buf), "%d.db", i);
#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
			DB_BENCH_ASSERT(second[i]->open(second[i], NULL,
			    buf, NULL, DB_BTREE, DB_CREATE, 0600) == 0);
#else
			DB_BENCH_ASSERT(second[i]->open(second[i],
			    buf, NULL, DB_BTREE, DB_CREATE, 0600) == 0);
#endif
#if DB_VERSION_MAJOR > 3 || DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3
#if DB_VERSION_MAJOR > 3 && DB_VERSION_MINOR > 0
			/*
			 * The DB_TXN argument to Db.associate was added in
			 * 4.1.25.
			 */
			DB_BENCH_ASSERT(
			    dbp->associate(dbp, NULL, second[i], s, 0) == 0);
#else
			DB_BENCH_ASSERT(
			    dbp->associate(dbp, second[i], s, 0) == 0);
#endif
#endif
		}
	}

	/* Store a key/data pair. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	switch (type) {
	case DB_BTREE:
	case DB_HASH:
		key.data = "01234567890123456789";
		key.size = 10;
		break;
	case DB_QUEUE:
	case DB_RECNO:
		recno = 1;
		key.data = &recno;
		key.size = sizeof(recno);
		break;
	case DB_UNKNOWN:
		abort();
		break;
	}

	DB_BENCH_ASSERT((data.data = malloc(data.size = dsize)) != NULL);

	/* Store the key/data pair count times. */
	TIMER_START;
	for (i = 0; i < count; ++i)
		DB_BENCH_ASSERT(dbp->put(dbp, NULL, &key, &data, 0) == 0);
	TIMER_STOP;

	if (type == DB_BTREE || type == DB_HASH)
		printf(
		    "# %d %s database put of 10 byte key, %lu byte data",
		    count, ts, (u_long)dsize);
	else
		printf("# %d %s database put of key, %lu byte data",
		    count, ts, (u_long)dsize);
	if (secondaries)
		printf(" with %d secondaries", secondaries);
	printf("\n");
	TIMER_DISPLAY(count);

	return (0);
}

int
s(dbp, pkey, pdata, skey)
	DB *dbp;
	const DBT *pkey, *pdata;
	DBT *skey;
{
	skey->data = pkey->data;
	skey->size = pkey->size;
	return (0);
}

int
usage()
{
	(void)fprintf(stderr,
	    "usage: b_put [-c count] [-d bytes] [-s secondaries] [-t type]\n");
	return (EXIT_FAILURE);
}
