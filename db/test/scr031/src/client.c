#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tx.h>
#include <atmi.h>
#include <fml32.h>
#include <fml1632.h>

#include <db.h>

#include "datafml.h"
#include "hdbrec.h"
#include "hcommonxa.h"
#include "htimestampxa.h"

#define	HOME	"../data"
#define	TABLE1	"../data/table1.db"
#define	TABLE3	"../data/table3.db"

#ifdef VERBOSE
static int verbose = 1;				/* Debugging output. */
#else
static int verbose = 0;
#endif

DB_ENV *dbenv;
char *progname;					/* Client run-time name. */

int   check_data(DB *);
char *db_buf(DBT *);
int   usage(void);

int
main(int argc, char* argv[])
{
	DB *dbp3;
	DBT key, data;
	FBFR *buf, *replyBuf;
	HDbRec rec;
	TPINIT *initBuf;
	long len, replyLen, seqNo;
	int ch, cnt, cnt_abort, cnt_commit, cnt_server1, i, ret;
	char *target;

	progname = argv[0];

	dbp3 = NULL;
	buf = replyBuf = NULL;
	initBuf = NULL;
	cnt = 1000;
	cnt_abort = cnt_commit = cnt_server1 = 0;

	while ((ch = getopt(argc, argv, "n:v")) != EOF)
		switch (ch) {
		case 'n':
			cnt = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	if (verbose)
		printf("%s: called\n", progname);

	/* Seed random number generator. */
	srand((u_int)(time(NULL) | getpid()));

	if (tpinit((TPINIT *)NULL) == -1)
		goto tuxedo_err;
	if (verbose)
		printf("%s: tpinit() OK\n", progname);

	/* Allocate init buffer */
	if ((initBuf = (TPINIT *)tpalloc("TPINIT", NULL, TPINITNEED(0))) == 0)
		goto tuxedo_err;
	if (verbose)
		printf("%s: tpalloc(\"TPINIT\") OK\n", progname);

	/* Join the DB environment. */
	if ((ret = db_env_create(&dbenv, 0)) != 0 ||
	    (ret = dbenv->open(dbenv, HOME, DB_JOINENV, 0)) != 0) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, HOME, db_strerror(ret));
		goto err;
	}
	dbenv->set_errfile(dbenv, stderr);
	if (verbose)
		printf("%s: opened %s OK\n", progname, HOME);

	/* Open table #3 -- truncate it for each new run. */
	if ((ret = db_create(&dbp3, dbenv, 0)) != 0 ||
	    (ret = dbp3->open(dbp3,
	    NULL, TABLE3, NULL, DB_BTREE, DB_CREATE, 0660)) != 0) {
		fprintf(stderr,
		    "%s: %s %s\n", progname, TABLE3, db_strerror(ret));
		goto err;
	}
	if (verbose)
		printf("%s: opened/truncated %s OK\n", progname, TABLE3);

	/* Allocate send buffer. */
	len = Fneeded(1, 3 * sizeof(long));
	if ((buf = (FBFR*)tpalloc("FML32", NULL, len)) == 0)
		goto tuxedo_err;
	if (verbose)
		printf("%s: tpalloc(\"FML32\"), send buffer OK\n", progname);

	/* Allocate reply buffer. */
	replyLen = 1024;
	if ((replyBuf = (FBFR*)tpalloc("FML32", NULL, replyLen)) == NULL)
		goto tuxedo_err;
	if (verbose)
		printf("%s: tpalloc(\"FML32\"), reply buffer OK\n", progname);

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	for (rec.SeqNo = 1, i = 0; i < cnt; ++i, ++rec.SeqNo) {
		GetTime(&rec.Ts);

		if (Fchg(buf, SEQ_NO, 0, (char *)&rec.SeqNo, 0) == -1)
			goto tuxedo_fml_err;
		if (verbose)
			printf("%s: Fchg(), sequence number OK\n", progname);
		if (Fchg(buf, TS_SEC, 0, (char *)&rec.Ts.Sec, 0) == -1)
			goto tuxedo_fml_err;
		if (verbose)
			printf("%s: Fchg(), seconds OK\n", progname);
		if (Fchg(buf, TS_USEC, 0, (char *)&rec.Ts.Usec, 0) == -1)
			goto tuxedo_fml_err;
		if (verbose)
			printf("%s: Fchg(), microseconds OK\n", progname);

		if (tpbegin(60L, 0L) == -1)
			goto tuxedo_err;
		if (verbose)
			printf("%s: tpbegin() OK\n", progname);

		/* Randomly send half of our requests to each server. */
		if (rand() % 2 > 0) {
			++cnt_server1;
			target = "TestTxn1";
		} else
			target = "TestTxn2";
		if (tpcall(target, (char *)buf,
		    0L, (char **)&replyBuf, &replyLen, TPSIGRSTRT) == -1)
			goto tuxedo_err;

		/* Commit for a return value of 0, otherwise abort. */
		if (tpurcode == 0) {
			++cnt_commit;
			if (verbose)
				printf("%s: txn success\n", progname);

			if (tpcommit(0L) == -1)
				goto tuxedo_err;
			if (verbose)
				printf("%s: tpcommit() OK\n", progname);

			/*
			 * Store a copy of the key/data pair into table #3
			 * on success, we'll compare table #1 and table #3
			 * after the run finishes.
			 */
			seqNo = rec.SeqNo;
			key.data = &seqNo;
			key.size = sizeof(seqNo);
			data.data = &rec;
			data.size = sizeof(rec);
			if ((ret =
			    dbp3->put(dbp3, NULL, &key, &data, 0)) != 0) {
				fprintf(stderr, "%s: DB->put: %s %s\n",
				    progname, TABLE3, db_strerror(ret));
				goto err;
			}
		} else {
			++cnt_abort;
			if (verbose)
				printf("%s: txn failure\n", progname);

			if (tpabort(0L) == -1)
				goto tuxedo_err;
			if (verbose)
				printf("%s: tpabort() OK\n", progname);
		}
	}

	printf("%s: %d requests: %d committed, %d aborted\n",
	    progname, cnt, cnt_commit, cnt_abort);
	printf("%s: %d sent to server #1, %d sent to server #2\n",
	    progname, cnt_server1, cnt - cnt_server1);

	ret = check_data(dbp3);

	if (0) {
tuxedo_err:	fprintf(stderr, "%s: TUXEDO ERROR: %s (code %d)\n",
		    progname, tpstrerror(tperrno), tperrno);
		goto err;
	}
	if (0) {
tuxedo_fml_err:	fprintf(stderr, "%s: FML ERROR: %s (code %d)\n",
		    progname, Fstrerror(Ferror), Ferror);
	}
	if (0) {
err:		ret = EXIT_FAILURE;
	}

	if (replyBuf != NULL)
		tpfree((char *)replyBuf);
	if (buf != NULL)
		tpfree((char *)buf);
	if (initBuf != NULL)
		tpfree((char *)initBuf);
	if (dbp3 != NULL)
		(void)dbp3->close(dbp3, 0);
	if (dbenv != NULL)
		(void)dbenv->close(dbenv, 0);

	tpterm();
	if (verbose)
		printf("%s: tpterm() OK\n", progname);

	return (ret);
}

/*
 * check_data --
 *	Compare committed data with our local copy, stored in table3.
 */
int
check_data(dbp3)
	DB *dbp3;
{
	DB *dbp1;
	DBC *dbc1, *dbc3;
	DBT key1, data1, key3, data3;
	int ret, ret1, ret3;

	dbp1 = NULL;
	dbc1 = dbc3 = NULL;

	/* Open table #1. */
	if ((ret = db_create(&dbp1, dbenv, 0)) != 0 ||
	    (ret = dbp1->open(
	    dbp1, NULL, TABLE1, NULL, DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, TABLE1, db_strerror(ret));
		goto err;
	}
	if (verbose)
		printf("%s: opened %s OK\n", progname, TABLE1);

	/* Open cursors. */
	if ((ret = dbp1->cursor(dbp1, NULL, &dbc1, 0)) != 0 ||
	    (ret = dbp3->cursor(dbp3, NULL, &dbc3, 0)) != 0) {
		fprintf(stderr,
		    "%s: DB->cursor: %s\n", progname, db_strerror(ret));
		goto err;
	}
	if (verbose)
		printf("%s: opened cursors OK\n", progname);

	/* Compare the two databases. */
	memset(&key1, 0, sizeof(key1));
	memset(&data1, 0, sizeof(data1));
	memset(&key3, 0, sizeof(key3));
	memset(&data3, 0, sizeof(data3));
	for (;;) {
		ret1 = dbc1->c_get(dbc1, &key1, &data1, DB_NEXT);
		ret3 = dbc3->c_get(dbc3, &key3, &data3, DB_NEXT);
		if (verbose) {
			printf("get: key1: %s\n", db_buf(&key1));
			printf("get: key3: %s\n", db_buf(&key3));
			printf("get: data1: %s\n", db_buf(&data1));
			printf("get: data3: %s\n", db_buf(&data3));
		}
		if (ret1 != 0 || ret3 != 0)
			break;
		/*
		 * Only compare the first N bytes, the saved message chunks
		 * are different.
		 */
		if (key1.size != key3.size ||
		    memcmp(key1.data, key3.data, key1.size) != 0 ||
		    data1.size != data3.size ||
		    memcmp(data1.data, data3.data,
		    sizeof(long) + sizeof(HTimestampData)) != 0)
			goto mismatch;
	}
	if (ret1 != DB_NOTFOUND || ret3 != DB_NOTFOUND) {
mismatch:	fprintf(stderr,
		    "%s: DB_ERROR: databases 1 and 3 weren't identical\n",
		    progname);
		ret = 1;
	}

err:	if (dbc1 != NULL)
		(void)dbc1->c_close(dbc1);
	if (dbc3 != NULL)
		(void)dbc3->c_close(dbc3);
	if (dbp1 != NULL)
		(void)dbp1->close(dbp1, 0);

	return (ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

char *
db_buf(dbt)
	DBT *dbt;
{
	static u_char buf[1024];
	size_t len;
	u_char *p, *b;

	for (p = dbt->data, len = dbt->size, b = buf; len > 0; ++p, --len)
		if (isprint(*p))
			b += sprintf((char *)b, "%c", *p);
		else
			b += sprintf((char *)b, "%#o", *p);
	return ((char *)buf);
}

int
usage()
{
	fprintf(stderr, "usage: %s [-v] [-n txn]\n", progname);
	return (EXIT_FAILURE);
}
