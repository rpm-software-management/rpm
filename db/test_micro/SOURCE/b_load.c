/*
 * $Id: b_load.c,v 1.12 2007/07/02 09:31:20 moshen Exp $
 */
#include "bench.h"

int usage(void);

int
main(int argc, char *argv[])
{
	DB *dbp;
	DBTYPE type;
	DBT key, data;
	db_recno_t recno;
	int ch, i, count, duplicate;
	char *ts, buf[32];

	cleanup_test_dir();

	duplicate = 0;
	count = 100000;
	ts = "Btree";
	type = DB_BTREE;
	while ((ch = getopt(argc, argv, "c:dt:")) != EOF)
		switch (ch) {
		case 'c':
			count = atoi(optarg);
			break;
		case 'd':
			duplicate = 1;
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

	/* Usage. */
	if (duplicate && (type == DB_QUEUE || type == DB_RECNO)) {
		fprintf(stderr,
		    "b_load: Queue an Recno don't support duplicates\n");
		return (usage());
	}

#if DB_VERSION_MAJOR < 3 || DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR < 1
	/*
	 * DB versions prior to 3.1.17 didn't have off-page duplicates, so
	 * this test can run forever.
	 */
	if (duplicate)
		return (0);
#endif

	/* Create the database. */
	DB_BENCH_ASSERT(db_create(&dbp, NULL, 0) == 0);
	DB_BENCH_ASSERT(dbp->set_cachesize(dbp, 0, 1048576 /* 1MB */, 0) == 0);
	if (duplicate)
		DB_BENCH_ASSERT(dbp->set_flags(dbp, DB_DUP) == 0);
	dbp->set_errfile(dbp, stderr);

	/* Set record length for Queue. */
	if (type == DB_QUEUE)
		DB_BENCH_ASSERT(dbp->set_re_len(dbp, 20) == 0);

#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
	DB_BENCH_ASSERT(
	    dbp->open(dbp, NULL, "a", NULL, type, DB_CREATE, 0666) == 0);
#else
	DB_BENCH_ASSERT(dbp->open(dbp, "a", NULL, type, DB_CREATE, 0666) == 0);
#endif

	/* Initialize the data. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Insert count in-order key/data pairs. */
	TIMER_START;
	if (duplicate) {
		key.size = 10;
		key.data = "01234567890123456789";
		data.data = buf;
		data.size = 20;
		for (i = 0; i < count; ++i) {
			(void)snprintf(buf, sizeof(buf), "%020d", i);
			DB_BENCH_ASSERT(
			    dbp->put(dbp, NULL, &key, &data, 0) == 0);
		}
	} else {
		data.data = buf;
		data.size = 20;
		if (type == DB_BTREE || type == DB_HASH) {
			key.size = 10;
			key.data = buf;
			for (i = 0; i < count; ++i) {
				(void)snprintf(buf, sizeof(buf), "%010d", i);
				DB_BENCH_ASSERT(
				    dbp->put(dbp, NULL, &key, &data, 0) == 0);
			}
		} else {
			key.data = &recno;
			key.size = sizeof(recno);
			for (i = 0, recno = 1; i < count; ++i, ++recno)
				DB_BENCH_ASSERT(
				    dbp->put(dbp, NULL, &key, &data, 0) == 0);
		}
	}

	TIMER_STOP;

	printf("# %d %s database in-order put of 10/20 byte key/data %sitems\n",
	    count, ts, duplicate ? "duplicate " : "");
	TIMER_DISPLAY(count);

	return (0);
}

int
usage()
{
	(void)fprintf(stderr, "usage: b_load [-d] [-c count] [-t type]\n");
	return (EXIT_FAILURE);
}
