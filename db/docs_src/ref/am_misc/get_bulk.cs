m4_ignore([dnl
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>

#define	DATABASE "access.db"
#define	WORDLIST "../test/wordlist"

int	rec_display __P((DB *));

int
main()
{
	DB *dbp;
	DBT key, data;
	FILE *fp;
	u_int32_t len;
	int cnt, ret;
	char *p, *t, buf[1024], rbuf[1024];
	const char *progname = "ex_access";		/* Program name. */

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
	dbp->set_errpfx(dbp, progname);
	if ((ret = dbp->set_pagesize(dbp, 1024)) != 0) {
		dbp->err(dbp, ret, "set_pagesize");
		return (1);
	}
	if ((ret = dbp->set_cachesize(dbp, 0, 32 * 1024, 0)) != 0) {
		dbp->err(dbp, ret, "set_cachesize");
		return (1);
	}
	if ((ret = dbp->open(dbp,
	    NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "%s: open", DATABASE);
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
				return (1);
		}
	}

	/* Close the word database. */
	(void)fclose(fp);

	rec_display(dbp);

	return (0);
}])
m4_indent([dnl
int
rec_display(dbp)
	DB *dbp;
{
	DBC *dbcp;
	DBT key, data;
	size_t retklen, retdlen;
	char *retkey, *retdata;
	int ret, t_ret;
	void *p;
m4_blank
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
m4_blank
	/* Review the database in 5MB chunks. */
#define	BUFFER_LENGTH	(5 * 1024 * 1024)
	if ((data.data = malloc(BUFFER_LENGTH)) == NULL)
		return (errno);
	data.ulen = BUFFER_LENGTH;
	data.flags = DB_DBT_USERMEM;
m4_blank
	/* Acquire a cursor for the database. */
	if ((ret = dbp-__GT__cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp-__GT__err(dbp, ret, "DB-__GT__cursor");
		free(data.data);
		return (ret);
	}
m4_blank
	for (;;) {
		/*
		 * Acquire the next set of key/data pairs.  This code does
		 * not handle single key/data pairs that won't fit in a
		 * BUFFER_LENGTH size buffer, instead returning DB_BUFFER_SMALL
		 * to our caller.
		 */
		if ((ret = dbcp-__GT__c_get(dbcp,
		    &key, &data, DB_MULTIPLE_KEY | DB_NEXT)) != 0) {
			if (ret != DB_NOTFOUND)
				dbp-__GT__err(dbp, ret, "DBcursor-__GT__c_get");
			break;
		}
m4_blank
		for (DB_MULTIPLE_INIT(p, &data);;) {
			DB_MULTIPLE_KEY_NEXT(p,
			    &data, retkey, retklen, retdata, retdlen);
			if (p == NULL)
				break;
			printf("key: %.*s, data: %.*s\n",
			    (int)retklen, retkey, (int)retdlen, retdata);
		}
	}
m4_blank
	if ((t_ret = dbcp-__GT__c_close(dbcp)) != 0) {
		dbp-__GT__err(dbp, ret, "DBcursor-__GT__close");
		if (ret == 0)
			ret = t_ret;
	}
m4_blank
	free(data.data);
m4_blank
	return (ret);
}])
