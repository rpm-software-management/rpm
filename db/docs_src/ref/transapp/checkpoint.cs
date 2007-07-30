m4_ignore([dnl
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

void *checkpoint_thread(void *);

int add_cat(DB_ENV *a, DB *b, char *c, ...) {
	a = NULL; b = NULL; c = NULL;
	return (0);
}
int add_color(DB_ENV *a, DB *b, char *c, int d) {
	a = NULL; b = NULL; c = NULL; d = 0;
	return (0);
}
int add_fruit(DB_ENV *a, DB *b, char *c, char *d) {
	a = NULL; b = NULL; c = NULL; d = 0;
	return (0);
}
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
	pthread_t ptid;
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
	/* Start a checkpoint thread. */
	if ((errno = pthread_create(
	    &ptid, NULL, checkpoint_thread, (void *)dbenv)) != 0) {
		fprintf(stderr,
		    "txnapp: failed spawning checkpoint thread: %s\n",
		    strerror(errno));
		exit (1);
	}])
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
	add_fruit(dbenv, db_fruit, "apple", "yellow delicious");
m4_blank
	add_color(dbenv, db_color, "blue", 0);
	add_color(dbenv, db_color, "blue", 3);
m4_blank
	add_cat(dbenv, db_cats,
		"Amy Adams",
		"Oracle",
		"abyssinian",
		"bengal",
		"chartreaux",
		NULL);
m4_blank
	return (0);
}
m4_blank
m4_cbold([dnl
void *
checkpoint_thread(void *arg)
{
	DB_ENV *dbenv;
	int ret;
m4_blank
	dbenv = arg;
	dbenv-__GT__errx(dbenv, "Checkpoint thread: %lu", (u_long)pthread_self());
m4_blank
	/* Checkpoint once a minute. */
	for (;; sleep(60))
		if ((ret = dbenv-__GT__txn_checkpoint(dbenv, 0, 0, 0)) != 0) {
			dbenv-__GT__err(dbenv, ret, "checkpoint thread");
			exit (1);
		}
m4_blank
	/* NOTREACHED */
}])])
