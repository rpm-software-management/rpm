/*
 * $Id: b_curalloc.c,v 1.8 2007/05/29 17:39:15 bostic Exp $
 */
#include "bench.h"

int usage(void);

int
main(int argc, char *argv[])
{
	DB *dbp;
	DBC *curp;
	int ch, i, count;

	cleanup_test_dir();

	count = 100000;
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

	/* Create the database. */
	DB_BENCH_ASSERT(db_create(&dbp, NULL, 0) == 0);
	dbp->set_errfile(dbp, stderr);

#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
	DB_BENCH_ASSERT(
	    dbp->open(dbp, NULL, "a", NULL, DB_BTREE, DB_CREATE, 0666) == 0);
#else
	DB_BENCH_ASSERT(
	    dbp->open(dbp, "a", NULL, DB_BTREE, DB_CREATE, 0666) == 0);
#endif

	/* Allocate a cursor count times. */
	TIMER_START;
	for (i = 0; i < count; ++i) {
		DB_BENCH_ASSERT(dbp->cursor(dbp, NULL, &curp, 0) == 0);
		DB_BENCH_ASSERT(curp->c_close(curp) == 0);
	}
	TIMER_STOP;

	printf("# %d cursor allocations\n", count);
	TIMER_DISPLAY(count);

	return (0);
}

int
usage()
{
	(void)fprintf(stderr, "usage: b_curalloc [-c count]\n");
	return (EXIT_FAILURE);
}
