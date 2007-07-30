m4_ignore([dnl
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>

#define	progname	"t"
#define	database	"a.db"

int store(DB *);

int
main()
{
	DB *dbp;
	int ret;

	(void)remove(database);

	(void)db_create(&dbp, NULL, 0);
	(void)dbp->set_errfile(dbp, stderr);
	(void)dbp->set_errpfx(dbp, progname);
	(void)dbp->set_flags(dbp, DB_DUP);
	if ((ret = dbp->open(dbp, NULL,
	    database, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "%s: DB->open", database);
		return (1);
	}

	store(dbp);

	(void)dbp->close(dbp, 0);

	return (0);
}])
m4_indent([dnl
int
store(dbp)
	DB *dbp;
{
	DBC *dbcp;
	DBT key, data;
	int ret;
m4_blank
	/*
	 * The DB handle for a Btree database supporting duplicate data
	 * items is the argument; acquire a cursor for the database.
	 */
	if ((ret = dbp-__GT__cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp-__GT__err(dbp, ret, "DB-__GT__cursor");
		goto err;
	}
m4_blank
	/* Initialize the key. */
	memset(&key, 0, sizeof(key));
	key.data = "new key";
	key.size = strlen(key.data) + 1;
m4_blank
	/* Initialize the data to be the first of two duplicate records. */
	memset(&data, 0, sizeof(data));
	data.data = "new key's data: entry #1";
	data.size = strlen(data.data) + 1;
m4_blank
	/* Store the first of the two duplicate records. */
	if ((ret = dbcp-__GT__c_put(dbcp, &key, &data, DB_KEYFIRST)) != 0)
		dbp-__GT__err(dbp, ret, "DB-__GT__cursor");
m4_blank
	/* Initialize the data to be the second of two duplicate records. */
	data.data = "new key's data: entry #2";
	data.size = strlen(data.data) + 1;
m4_blank
	/*
	 * Store the second of the two duplicate records.  No duplicate
	 * record sort function has been specified, so we explicitly
	 * store the record as the last of the duplicate set.
	 */
	if ((ret = dbcp-__GT__c_put(dbcp, &key, &data, DB_KEYLAST)) != 0)
		dbp-__GT__err(dbp, ret, "DB-__GT__cursor");
m4_blank
err:	if ((ret = dbcp-__GT__c_close(dbcp)) != 0)
		dbp-__GT__err(dbp, ret, "DBcursor-__GT__close");
m4_blank
	return (0);
}])
