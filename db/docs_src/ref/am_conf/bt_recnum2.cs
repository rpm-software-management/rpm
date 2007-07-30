m4_ignore([
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>

#define DATABASE        "access.db"
#define WORDLIST        "../test/wordlist"

int rec_display __P((DB *, db_recno_t));
int recno_display __P((DB *, char *));

int
main()
{
	DB *dbp;
	DBT key, data;
	DB_BTREE_STAT *statp;
	FILE *fp;
	db_recno_t recno;
	u_int32_t len;
	int cnt, ret;
	char *p, *t, buf[1024], rbuf[1024];
	const char *progname = "ex_btrec";		/* Program name. */

	/* Open the word database. */
	if ((fp = fopen(WORDLIST, "r")) == NULL) {
		fprintf(stderr, "%s: open %s: %s\n",
		    progname, WORDLIST, db_strerror(errno));
		return (1);
	}

	/* Remove the previous database. */
	(void)remove(DATABASE);

	/* Create and initialize database object, open the database. */
	if ((ret = db_create(&dbp, NULL, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_create: %s\n", progname, db_strerror(ret));
		return (1);
	}
	dbp->set_errfile(dbp, stderr);
	dbp->set_errpfx(dbp, progname);			/* 1K page sizes. */
	if ((ret = dbp->set_pagesize(dbp, 1024)) != 0) {
		dbp->err(dbp, ret, "set_pagesize");
		return (1);
	}						/* Record numbers. */
	if ((ret = dbp->set_flags(dbp, DB_RECNUM)) != 0) {
		dbp->err(dbp, ret, "set_flags: DB_RECNUM");
		return (1);
	}
	if ((ret = dbp->open(dbp,
	    NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "open: %s", DATABASE);
		return (1);
	}

	/*
	 * Insert records into the database, where the key is the word
	 * preceded by its record number, and the data is the same, but
	 * in reverse order.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	for (cnt = 1; cnt <= 1000; ++cnt) {
		(void)sprintf(buf, "%04d_", cnt);
		if (fgets(buf + 4, sizeof(buf) - 4, fp) == NULL)
			break;
		len = strlen(buf);
		for (t = rbuf, p = buf + (len - 2); p >= buf;)
			*t++ = *p--;
		*t++ = '\0';

		key.data = buf;
		data.data = rbuf;
		data.size = key.size = len - 1;

		if ((ret =
		    dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE)) != 0) {
			dbp->err(dbp, ret, "DB->put");
			if (ret != DB_KEYEXIST)
				goto err1;
		}
	}

	/* Close the word database. */
	(void)fclose(fp);

	/* Print out the number of records in the database. */
	if ((ret = dbp->stat(dbp, NULL, &statp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->stat");
		goto err1;
	}
	printf("%s: database contains %lu records\n",
	    progname, (u_long)statp->bt_ndata);
	free(statp);

	/*
	 * Prompt the user for a record number, then retrieve and display
	 * that record.
	 */
	for (;;) {
		/* Get a record number. */
		printf("recno #> ");
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		recno = atoi(buf);
		rec_display(dbp, recno);
	}

	recno_display(dbp, "0007atoned");

	if ((ret = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr,
		    "%s: DB->close: %s\n", progname, db_strerror(ret));
		return (1);
	}

	return (0);

err1:	(void)dbp->close(dbp, 0);
	return (ret);

}
int
rec_display(dbp, recno)
	DB *dbp;
	db_recno_t recno;
{
	DBT key, data;
	int ret;

	memset(&key, 0, sizeof(key));
	key.data = &recno;
	key.size = sizeof(recno);
	memset(&data, 0, sizeof(data));

	if ((ret = dbp->get(dbp, NULL, &key, &data, DB_SET_RECNO)) != 0)
		return (ret);
	printf("data for %lu: %.*s\n",
	    (u_long)recno, (int)data.size, (char *)data.data);
	return (0);
}])
m4_indent([dnl
int
recno_display(dbp, keyvalue)
	DB *dbp;
	char *keyvalue;
{
	DBC *dbcp;
	DBT key, data;
	db_recno_t recno;
	int ret, t_ret;
m4_blank
	/* Acquire a cursor for the database. */
	if ((ret = dbp-__GT__cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp-__GT__err(dbp, ret, "DB-__GT__cursor");
		goto err;
	}
m4_blank
	/* Position the cursor. */
	memset(&key, 0, sizeof(key));
	key.data = keyvalue;
	key.size = strlen(keyvalue);
	memset(&data, 0, sizeof(data));
	if ((ret = dbcp-__GT__c_get(dbcp, &key, &data, DB_SET)) != 0) {
		dbp-__GT__err(dbp, ret, "DBC-__GT__c_get(DB_SET): %s", keyvalue);
		goto err;
	}
m4_blank
	/*
	 * Request the record number, and store it into appropriately
	 * sized and aligned local memory.
	 */
	memset(&data, 0, sizeof(data));
	data.data = &recno;
	data.ulen = sizeof(recno);
	data.flags = DB_DBT_USERMEM;
	if ((ret = dbcp-__GT__c_get(dbcp, &key, &data, DB_GET_RECNO)) != 0) {
		dbp-__GT__err(dbp, ret, "DBC-__GT__c_get(DB_GET_RECNO)");
		goto err;
	}
m4_blank
	printf("key for requested key was %lu\n", (u_long)recno);
m4_blank
err:	/* Close the cursor. */
	if ((t_ret = dbcp-__GT__c_close(dbcp)) != 0) {
		if (ret == 0)
			ret = t_ret;
		dbp-__GT__err(dbp, ret, "DBC-__GT__close");
	}
	return (ret);
}])
