m4_ignore([dnl
#include <sys/types.h>
#include <stdlib.h>

#include <db.h>

int foo();

int
main()
{
	return(foo());
}

int
foo()
{
#define	MAXIMUM_RETRY	5
	DB_ENV *dbenv;
	DB *dbp;
	DBT key, data;
	DB_TXN *tid;
	int fail, ret, t_ret;
])
m4_indent([dnl
for (fail = 0;;) {
	/* Begin the transaction. */
	if ((ret = dbenv-__GT__txn_begin(dbenv, NULL, &tid, 0)) != 0) {
		dbenv-__GT__err(dbenv, ret, "dbenv-__GT__txn_begin");
		exit (1);
	}
m4_blank
	/* Store the key. */
	switch (ret = dbp-__GT__put(dbp, tid, &key, &data, 0)) {
	case 0:
		/* Success: commit the change. */
		printf("db: %s: key stored.\n", (char *)key.data);
		if ((ret = tid-__GT__commit(tid, 0)) != 0) {
			dbenv-__GT__err(dbenv, ret, "DB_TXN-__GT__commit");
			exit (1);
		}
		return (0);
	case DB_LOCK_DEADLOCK:
	default:
		/* Failure: retry the operation. */
		if ((t_ret = tid-__GT__abort(tid)) != 0) {
			dbenv-__GT__err(dbenv, t_ret, "DB_TXN-__GT__abort");
			exit (1);
		}
		if (fail++ == MAXIMUM_RETRY)
			return (ret);
		continue;
	}
}])
m4_ignore([}])
