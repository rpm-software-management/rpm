/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2001
 *	Sleepycat Software.  All rights reserved.
 *
 * Id: dbdemo.c,v 1.1 2001/09/11 15:51:16 sue Exp 
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include <db.h>

#include "stdio.h"
#define	ERROR_RETURN	ERROR

int	dbdemo __P((char *));

int
dbdemo(database)
	char *database;
{
	DB *dbp;
	DBC *dbcp;
	DBT key, data;
	u_int32_t len;
	int ret;
	char *p, *t, buf[1024], rbuf[1024];
	const char *progname = "dbdemo";		/* Program name. */

	/* Remove the previous database. */
	(void)remove(database);

	/* Create and initialize database object, open the database. */
	if ((ret = db_create(&dbp, NULL, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_create: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
	dbp->set_errfile(dbp, stderr);
	dbp->set_errpfx(dbp, progname);
	if ((ret = dbp->set_pagesize(dbp, 1024)) != 0) {
		dbp->err(dbp, ret, "set_pagesize");
		goto err1;
	}
	if ((ret = dbp->set_cachesize(dbp, 0, 32 * 1024, 0)) != 0) {
		dbp->err(dbp, ret, "set_cachesize");
		goto err1;
	}
	if ((ret =
	    dbp->open(dbp, database, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "%s: open", database);
		goto err1;
	}

	/*
	 * Insert records into the database, where the key is the user
	 * input and the data is the user input in reverse order.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	for (;;) {
		printf("input> ");
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		if (strncmp(buf, "quit", 4) == 0)
			break;
		if ((len = strlen(buf)) <= 1)
			continue;
		for (t = rbuf, p = buf + (len - 2); p >= buf;)
			*t++ = *p--;
		*t++ = '\0';

		key.data = buf;
		data.data = rbuf;
		data.size = key.size = len - 1;

		switch (ret =
		    dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE)) {
		case 0:
			break;
		default:
			dbp->err(dbp, ret, "DB->put");
			if (ret != DB_KEYEXIST)
				goto err1;
			break;
		}
	}
	printf("\n");

	/* Acquire a cursor for the database. */
	if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->cursor");
		goto err1;
	}

	/* Initialize the key/data pair so the flags aren't set. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Walk through the database and print out the key/data pairs. */
	while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
		printf("%.*s : %.*s\n",
		    (int)key.size, (char *)key.data,
		    (int)data.size, (char *)data.data);
	if (ret != DB_NOTFOUND) {
		dbp->err(dbp, ret, "DBcursor->get");
		goto err2;
	}

	/* Close everything down. */
	if ((ret = dbcp->c_close(dbcp)) != 0) {
		dbp->err(dbp, ret, "DBcursor->close");
		goto err1;
	}
	if ((ret = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr,
		    "%s: DB->close: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
	return (0);

err2:	(void)dbcp->c_close(dbcp);
err1:	(void)dbp->close(dbp, 0);
	return (ERROR_RETURN);
}
