m4_indent([dnl
m4_include(sys/types.h)
m4_include(sys/stat.h)
m4_blank
m4_include(errno.h)
m4_include(stdarg.h)
m4_include(stdlib.h)
m4_include(string.h)
m4_include(unistd.h)
m4_blank
m4_include(db.h)
m4_blank
#define	ENV_DIRECTORY	"TXNAPP"
m4_blank
void  env_dir_create(void);
void  env_open(DB_ENV **);
m4_blank
m4_ignore([void usage(){}])
int
main(int argc, char *argv[])
{
	extern int optind;
	DB *db_cats, *db_color, *db_fruit;
	DB_ENV *dbenv;
	int ch;
m4_ignore([db_fruit = NULL; db_color = NULL; db_cats = NULL;])
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
	return (0);
}
m4_blank
void
env_dir_create()
{
	struct stat sb;
m4_blank
	/*
	 * If the directory exists, we're done.  We do not further check
	 * the type of the file, DB will fail appropriately if it's the
	 * wrong type.
	 */
	if (stat(ENV_DIRECTORY, &sb) == 0)
		return;
m4_blank
	/* Create the directory, read/write/access owner only. */
	if (mkdir(ENV_DIRECTORY, S_IRWXU) != 0) {
		fprintf(stderr,
		    "txnapp: mkdir: %s: %s\n", ENV_DIRECTORY, strerror(errno));
		exit (1);
	}
}
m4_blank
void
env_open(DB_ENV **dbenvp)
{
	DB_ENV *dbenv;
	int ret;
m4_blank
	/* Create the environment handle. */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "txnapp: db_env_create: %s\n", db_strerror(ret));
		exit (1);
	}
m4_blank
	/* Set up error handling. */
	dbenv-__GT__set_errpfx(dbenv, "txnapp");
	dbenv-__GT__set_errfile(dbenv, stderr);
m4_blank
	/*
	 * Open a transactional environment:
	 *	create if it doesn't exist
	 *	free-threaded handle
	 *	run recovery
	 *	read/write owner only
	 */
	if ((ret = dbenv-__GT__open(dbenv, ENV_DIRECTORY,
	    DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG |
	    DB_INIT_MPOOL | DB_INIT_TXN | DB_RECOVER | DB_THREAD,
	    S_IRUSR | S_IWUSR)) != 0) {
		(void)dbenv-__GT__close(dbenv, 0);
		fprintf(stderr, "dbenv-__GT__open: %s: %s\n",
		    ENV_DIRECTORY, db_strerror(ret));
		exit (1);
	}
m4_blank
	*dbenvp = dbenv;
}])
