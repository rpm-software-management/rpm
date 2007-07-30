m4_ignore([dnl
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>

#define	progname	"t"

int display (char *);

int
main()
{
	display("TESTDIR/test001.db");
	return (0);
}])
m4_indent([dnl
int
display(database)
	char *database;
{
	DB *dbp;
	DBC *dbcp;
	DBT key, data;
	int close_db, close_dbc, ret;
m4_blank
	close_db = close_dbc = 0;
m4_blank
	/* Open the database. */
	if ((ret = db_create(&dbp, NULL, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_create: %s\n", progname, db_strerror(ret));
		return (1);
	}
	close_db = 1;
m4_blank
	/* Turn on additional error output. */
	dbp-__GT__set_errfile(dbp, stderr);
	dbp-__GT__set_errpfx(dbp, progname);
m4_blank
	/* Open the database. */
	if ((ret =
	    dbp-__GT__open(dbp, NULL, database, NULL, DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
		dbp-__GT__err(dbp, ret, "%s: DB-__GT__open", database);
		goto err;
	}
m4_blank
	/* Acquire a cursor for the database. */
	if ((ret = dbp-__GT__cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp-__GT__err(dbp, ret, "DB-__GT__cursor");
		goto err;
	}
	close_dbc = 1;
m4_blank
	/* Initialize the key/data return pair. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
m4_blank
	/* Walk through the database and print out the key/data pairs. */
	while ((ret = dbcp-__GT__c_get(dbcp, &key, &data, DB_NEXT)) == 0)
		printf("%.*s : %.*s\n",
		    (int)key.size, (char *)key.data,
		    (int)data.size, (char *)data.data);
	if (ret != DB_NOTFOUND) {
		dbp-__GT__err(dbp, ret, "DBcursor-__GT__get");
		goto err;
	}
m4_blank
err:	if (close_dbc && (ret = dbcp-__GT__c_close(dbcp)) != 0)
		dbp-__GT__err(dbp, ret, "DBcursor-__GT__close");
	if (close_db && (ret = dbp-__GT__close(dbp, 0)) != 0)
		fprintf(stderr,
		    "%s: DB-__GT__close: %s\n", progname, db_strerror(ret));
	return (0);
}])
