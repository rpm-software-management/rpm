/*
 * $Id: b_get.c,v 1.10 2007/05/29 18:53:43 bostic Exp $
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
	int ch, i, count;
	char *ts;

	dbp = NULL;
	cleanup_test_dir();

	count = 100000;
	ts = "Btree";
	type = DB_BTREE;

	while ((ch = getopt(argc, argv, "c:t:")) != EOF)
		switch (ch) {
		case 'c':
			count = atoi(optarg);
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

	/* Create the database. */
	DB_BENCH_ASSERT(db_create(&dbp, NULL, 0) == 0);
	DB_BENCH_ASSERT(dbp->set_cachesize(dbp, 0, 1048576 /* 1MB */, 0) == 0);
	dbp->set_errfile(dbp, stderr);

	/* Set record length for Queue. */
	if (type == DB_QUEUE)
		DB_BENCH_ASSERT(dbp->set_re_len(dbp, 10) == 0);

#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
	DB_BENCH_ASSERT(
	    dbp->open(dbp, NULL, "a", NULL, type, DB_CREATE, 0666) == 0);
#else
	DB_BENCH_ASSERT(dbp->open(dbp, "a", NULL, type, DB_CREATE, 0666) == 0);
#endif

	/* Store a key/data pair. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	switch (type) {
	case DB_BTREE:
	case DB_HASH:
		key.data = "aaaaa";
		key.size = 5;
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
	data.data = "bbbbb";
	data.size = 5;

	DB_BENCH_ASSERT(dbp->put(dbp, NULL, &key, &data, 0) == 0);

	/* Retrieve the key/data pair count times. */
	TIMER_START;
	for (i = 0; i < count; ++i)
		DB_BENCH_ASSERT(dbp->get(dbp, NULL, &key, &data, 0) == 0);
	TIMER_STOP;

	printf("# %d %s database get of cached key/data item\n", count, ts);
	TIMER_DISPLAY(count);

	return (0);
}

int
usage()
{
	(void)fprintf(stderr, "usage: b_get [-c count] [-t type]\n");
	return (EXIT_FAILURE);
}
