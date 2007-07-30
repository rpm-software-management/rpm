m4_ignore([dnl
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

#define	MAXIMUM_RETRY	5

int add_fruit(DB_ENV *, DB *, char *, char *);

void  db_open(DB_ENV *a, DB **b, char *c, int d) {
	a = NULL; b = NULL; c = NULL; d = 0;
}
void usage(){}
void env_open(DB_ENV **a) { a = NULL; }
void env_dir_create() {}
])
m4_indent([dnl
int
main(int argc, char *argv[])
{
	extern int optind;
	DB *db_cats, *db_color, *db_fruit;
	DB_ENV *dbenv;
	int ch;
m4_blank
	while ((ch = getopt(argc, argv, "")) != EOF)
		switch (ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
m4_blank
	env_dir_create();
	env_open(&dbenv);
m4_blank
	/* Open database: Key is fruit class; Data is specific type. */
	db_open(dbenv, &db_fruit, "fruit", 0);
m4_blank
	/* Open database: Key is a color; Data is an integer. */
	db_open(dbenv, &db_color, "color", 0);
m4_blank
	/*
	 * Open database:
	 *	Key is a name; Data is: company name, cat breeds.
	 */
	db_open(dbenv, &db_cats, "cats", 1);
m4_blank
m4_cbold([dnl
	add_fruit(dbenv, db_fruit, "apple", "yellow delicious");])
m4_blank
	return (0);
}
m4_blank
m4_cbold([dnl
int
add_fruit(DB_ENV *dbenv, DB *db, char *fruit, char *name)
{
	DBT key, data;
	DB_TXN *tid;
	int fail, ret, t_ret;
m4_blank
	/* Initialization. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = fruit;
	key.size = strlen(fruit);
	data.data = name;
	data.size = strlen(name);
m4_blank
	for (fail = 0;;) {
		/* Begin the transaction. */
		if ((ret = dbenv-__GT__txn_begin(dbenv, NULL, &tid, 0)) != 0) {
			dbenv-__GT__err(dbenv, ret, "DB_ENV-__GT__txn_begin");
			exit (1);
		}
m4_blank
		/* Store the value. */
		switch (ret = db-__GT__put(db, tid, &key, &data, 0)) {
		case 0:
			/* Success: commit the change. */
			if ((ret = tid-__GT__commit(tid, 0)) != 0) {
				dbenv-__GT__err(dbenv, ret, "DB_TXN-__GT__commit");
				exit (1);
			}
			return (0);
		case DB_LOCK_DEADLOCK:
		default:
			/* Retry the operation. */
			if ((t_ret = tid-__GT__abort(tid)) != 0) {
				dbenv-__GT__err(dbenv, t_ret, "DB_TXN-__GT__abort");
				exit (1);
			}
			if (fail++ == MAXIMUM_RETRY)
				return (ret);
			break;
		}
	}
}])])
