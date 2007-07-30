m4_ignore([dnl
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

int  db_open(DB_ENV *, DB **, char *, int);

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
m4_cbold([dnl
	/* Open database: Key is fruit class; Data is specific type. */
	if (db_open(dbenv, &db_fruit, "fruit", 0))
		return (1);
m4_blank
	/* Open database: Key is a color; Data is an integer. */
	if (db_open(dbenv, &db_color, "color", 0))
		return (1);
m4_blank
	/*
	 * Open database:
	 *	Key is a name; Data is: company name, cat breeds.
	 */
	if (db_open(dbenv, &db_cats, "cats", 1))
		return (1);])
m4_blank
	return (0);
}
m4_blank
m4_cbold([dnl
int
db_open(DB_ENV *dbenv, DB **dbp, char *name, int dups)
{
	DB *db;
	int ret;
m4_blank
	/* Create the database handle. */
	if ((ret = db_create(&db, dbenv, 0)) != 0) {
		dbenv-__GT__err(dbenv, ret, "db_create");
		return (1);
	}
m4_blank
	/* Optionally, turn on duplicate data items. */
	if (dups && (ret = db-__GT__set_flags(db, DB_DUP)) != 0) {
		(void)db-__GT__close(db, 0);
		dbenv-__GT__err(dbenv, ret, "db-__GT__set_flags: DB_DUP");
		return (1);
	}
m4_blank
	/*
	 * Open a database in the environment:
	 *	create if it doesn't exist
	 *	free-threaded handle
	 *	read/write owner only
	 */
	if ((ret = db-__GT__open(db, NULL, name, NULL, DB_BTREE,
	    DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, S_IRUSR | S_IWUSR)) != 0) {
		(void)db-__GT__close(db, 0);
		dbenv-__GT__err(dbenv, ret, "db-__GT__open: %s", name);
		return (1);
	}
m4_blank
	*dbp = db;
	return (0);
}])])
