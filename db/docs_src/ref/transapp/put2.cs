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
int
main(int argc, char *argv[])
{
	extern int optind;
	DB *db_cats, *db_color, *db_fruit;
	DB_ENV *dbenv;
	int ch;

	while ((ch = getopt(argc, argv, "")) != EOF)
		switch (ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	env_dir_create();
	env_open(&dbenv);

	/* Open database: Key is fruit class; Data is specific type. */
	db_open(dbenv, &db_fruit, "fruit", 0);

	/* Open database: Key is a color; Data is an integer. */
	db_open(dbenv, &db_color, "color", 0);

	/*
	 * Open database:
	 *	Key is a name; Data is: company name, cat breeds.
	 */
	db_open(dbenv, &db_cats, "cats", 1);

	add_fruit(dbenv, db_fruit, "apple", "yellow delicious");

	return (0);
}

int
add_fruit(DB_ENV *dbenv, DB *db, char *fruit, char *name)
{
	DBT key, data;
	int fail, ret;

	/* Initialization. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = fruit;
	key.size = strlen(fruit);
	data.data = name;
	data.size = strlen(name);
])
m4_indent([m4_cbold([dnl
	for (fail = 0; fail++ __LT__= MAXIMUM_RETRY &&
	    (ret = db-__GT__put(db, NULL, &key, &data, 0)) == DB_LOCK_DEADLOCK;)
		;
	return (ret == 0 ? 0 : 1);])])
m4_ignore([
}])
