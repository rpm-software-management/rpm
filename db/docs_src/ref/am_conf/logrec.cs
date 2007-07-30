m4_ignore([dnl
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include <db.h>

#define	DATABASE	"access.db"
#define	progname	"t"

int recno_build(DB *);

int
main()
{
	DB *dbp;
	int ret;

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
	if ((ret = dbp->open(dbp,
	    NULL, DATABASE, NULL, DB_RECNO, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "%s: open", DATABASE);
		return (1);
	}

	(void)recno_build(dbp);

	return (0);
}])

m4_indent([dnl
int
recno_build(dbp)
	DB *dbp;
{
	DBC *dbcp;
	DBT key, data;
	db_recno_t recno;
	u_int32_t len;
	int ret;
	char buf__LB__1024__RB__;
m4_blank
	/* Insert records into the database. */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	for (recno = 1;; ++recno) {
		printf("record #%lu__GT__ ", (u_long)recno);
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		if ((len = strlen(buf)) __LT__= 1)
			continue;
m4_blank
		key.data = &recno;
		key.size = sizeof(recno);
		data.data = buf;
		data.size = len - 1;
m4_blank
		switch (ret = dbp-__GT__put(dbp, NULL, &key, &data, 0)) {
		case 0:
			break;
		default:
			dbp-__GT__err(dbp, ret, "DB-__GT__put");
			break;
		}
	}
	printf("\n");
m4_blank
	/* Acquire a cursor for the database. */
	if ((ret = dbp-__GT__cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp-__GT__err(dbp, ret, "DB-__GT__cursor");
		return (1);
	}
m4_blank
	/* Re-initialize the key/data pair. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
m4_blank
	/* Walk through the database and print out the key/data pairs. */
	while ((ret = dbcp-__GT__c_get(dbcp, &key, &data, DB_NEXT)) == 0)
		printf("%lu : %.*s\n",
		    *(u_long *)key.data, (int)data.size, (char *)data.data);
	if (ret != DB_NOTFOUND)
		dbp-__GT__err(dbp, ret, "DBcursor-__GT__get");
m4_blank
	/* Close the cursor. */
	if ((ret = dbcp-__GT__c_close(dbcp)) != 0) {
		dbp-__GT__err(dbp, ret, "DBcursor-__GT__close");
		return (1);
	}
	return (0);
}])
