/*
 * $Id: b_curwalk.c,v 1.10 2007/05/29 17:39:15 bostic Exp $
 */
#include "bench.h"

int usage(void);

int
main(int argc, char *argv[])
{
	DB *dbp;
	DBTYPE type;
	DBC *dbc;
	DBT key, data;
	db_recno_t recno;
	u_int32_t walkflags;
	int ch, i, count, dupcount, j;
	int prev, ret, skipdupwalk, sorted, walkcount;
	char *ts, dbuf[32], kbuf[32];

	cleanup_test_dir();

	count = 100000;
	dupcount = prev = skipdupwalk = sorted = 0;
	walkcount = 1000;
	ts = "Btree";
	type = DB_BTREE;
	while ((ch = getopt(argc, argv, "c:d:pSst:w:")) != EOF)
		switch (ch) {
		case 'c':
			count = atoi(optarg);
			break;
		case 'd':
			dupcount = atoi(optarg);
			break;
		case 'p':
			prev = 1;
			break;
		case 'S':
			skipdupwalk = 1;
			break;
		case 's':
			sorted = 1;
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
		case 'w':
			walkcount = atoi(optarg);
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		return (usage());

	/*
	 * Queue and Recno don't support duplicates.
	 */
	if (dupcount != 0 && (type == DB_QUEUE || type == DB_RECNO)) {
		fprintf(stderr,
		    "b_curwalk: Queue and Recno don't support duplicates\n");
		return (usage());
	}

#if DB_VERSION_MAJOR < 3 || DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0
#define	DB_PREV_NODUP	0
	/*
	 * DB_PREV_NODUP wasn't available until after 3.0.55.
	 *
	 * For some reason, testing sorted duplicates doesn't work either.
	 * I don't really care about 3.0.55 any more, just ignore it.
	 */
	return (0);
#endif

	/* Create the database. */
	DB_BENCH_ASSERT(db_create(&dbp, NULL, 0) == 0);
	DB_BENCH_ASSERT(
	    dbp->set_cachesize(dbp, 0, 10 * 1024 * 1024 /* 10MB */, 0) == 0);
	DB_BENCH_ASSERT(dbp->set_pagesize(dbp, 16 * 1024 /* 16KB */) == 0);
	dbp->set_errfile(dbp, stderr);

	/* Set record length for Queue. */
	if (type == DB_QUEUE)
		DB_BENCH_ASSERT(dbp->set_re_len(dbp, 20) == 0);

	/* Set duplicates flag. */
	if (dupcount != 0)
		DB_BENCH_ASSERT(
		    dbp->set_flags(dbp, sorted ? DB_DUPSORT : DB_DUP) == 0);

#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
	DB_BENCH_ASSERT(dbp->open(
	    dbp, NULL, "a", NULL, type, DB_CREATE, 0666) == 0);
#else
	DB_BENCH_ASSERT(dbp->open(
	    dbp, "a", NULL, type, DB_CREATE, 0666) == 0);
#endif

	/* Initialize the data. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Insert count in-order key/data pairs. */
	data.data = dbuf;
	data.size = 20;
	if (type == DB_BTREE || type == DB_HASH) {
		key.size = 10;
		key.data = kbuf;
		for (i = 0; i < count; ++i) {
			(void)snprintf(kbuf, sizeof(kbuf), "%010d", i);
			for (j = 0; j <= dupcount; ++j) {
				(void)snprintf(dbuf, sizeof(dbuf), "%020d", j);
				DB_BENCH_ASSERT(
				    dbp->put(dbp, NULL, &key, &data, 0) == 0);
			}
		}
	} else {
		key.data = &recno;
		key.size = sizeof(recno);
		for (i = 0, recno = 1; i < count; ++i, ++recno)
			DB_BENCH_ASSERT(
			    dbp->put(dbp, NULL, &key, &data, 0) == 0);
	}

	walkflags = prev ?
	    (skipdupwalk ? DB_PREV_NODUP : DB_PREV) :
	    (skipdupwalk ? DB_NEXT_NODUP : DB_NEXT);

	/* Walk the cursor through the tree N times. */
	TIMER_START;
	for (i = 0; i < walkcount; ++i) {
		DB_BENCH_ASSERT(dbp->cursor(dbp, NULL, &dbc, 0) == 0);
		while ((ret = dbc->c_get(dbc, &key, &data, walkflags)) == 0);
		DB_BENCH_ASSERT(ret == DB_NOTFOUND);
		DB_BENCH_ASSERT(dbc->c_close(dbc) == 0);
	}
	TIMER_STOP;

	printf("# %d %s %s cursor of %d 10/20 byte key/data items",
	    walkcount, ts, prev ?
	    (skipdupwalk ? "DB_PREV_NODUP" : "DB_PREV") :
	    (skipdupwalk ? "DB_NEXT_NODUP" : "DB_NEXT"),
	    count);
	if (dupcount != 0)
		printf(" with %d dups", dupcount);
	printf("\n");
	TIMER_DISPLAY(count);

	return (EXIT_SUCCESS);
}

int
usage()
{
	(void)fprintf(stderr,
    "usage: b_curwalk [-pSs] [-c cnt] [-d dupcnt] [-t type] [-w walkcnt]\n");
	return (EXIT_FAILURE);
}
