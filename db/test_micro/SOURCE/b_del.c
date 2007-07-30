/*
 * $Id: b_del.c,v 1.10 2007/05/29 17:39:15 bostic Exp $
 */
#include "bench.h"

int usage(void);

int
main(int argc, char *argv[])
{
	DB *dbp;
	DBC *dbc;
	DBT key, data;
	DBTYPE type;
	db_recno_t recno;
	int ch, i, count, ret, use_cursor;
	char *ts, buf[32];

	cleanup_test_dir();

	type = DB_BTREE;
	count = 100000;
	use_cursor = 0;
	ts = "Btree";
	while ((ch = getopt(argc, argv, "Cc:t:")) != EOF)
		switch (ch) {
		case 'C':
			use_cursor = 1;
			break;
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
	data.data = "01234567890123456789";
	data.size = 20;

	/* Store a key/data pair. */
	switch (type) {
	case DB_BTREE:
	case DB_HASH:
		key.data = buf;
		key.size = 10;
		break;
	case DB_QUEUE:
	case DB_RECNO:
		key.data = &recno;
		key.size = sizeof(recno);
		break;
	case DB_UNKNOWN:
		abort();
		break;
	}

	/* Insert count in-order key/data pairs. */
	if (type == DB_BTREE || type == DB_HASH)
		for (i = 0; i < count; ++i) {
			(void)snprintf(buf, sizeof(buf), "%010d", i);
			DB_BENCH_ASSERT(
			    dbp->put(dbp, NULL, &key, &data, 0) == 0);
		}
	else
		for (i = 0, recno = 1; i < count; ++i, ++recno)
			DB_BENCH_ASSERT(
			    dbp->put(dbp, NULL, &key, &data, 0) == 0);

	/* Delete the records. */
	TIMER_START;
	if (use_cursor) {
		DB_BENCH_ASSERT(dbp->cursor(dbp, NULL, &dbc, 0) == 0);
		while ((ret = dbc->c_get(dbc, &key, &data, DB_NEXT)) == 0)
			DB_BENCH_ASSERT(dbc->c_del(dbc, 0) == 0);
		DB_BENCH_ASSERT (ret == DB_NOTFOUND);
	} else
		if (type == DB_BTREE || type == DB_HASH)
			for (i = 0; i < count; ++i) {
				(void)snprintf(buf, sizeof(buf), "%010d", i);
				DB_BENCH_ASSERT(
				    dbp->del(dbp, NULL, &key, 0) == 0);
			}
		else
			for (i = 0, recno = 1; i < count; ++i, ++recno)
				DB_BENCH_ASSERT(
				    dbp->del(dbp, NULL, &key, 0) == 0);

	TIMER_STOP;

	printf(
    "# %d %s database in-order delete of 10/20 byte key/data pairs using %s\n",
	    count, ts, use_cursor ? "a cursor" : "the key");
	TIMER_DISPLAY(count);

	return (0);
}

int
usage()
{
	(void)fprintf(stderr, "usage: b_del [-C] [-c count] [-t type]\n");
	return (EXIT_FAILURE);
}
