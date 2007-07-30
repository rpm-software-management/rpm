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

void log_archlist(DB_ENV *);

int
main()
{
	DB_ENV *dbenv;
	log_archlist(dbenv);
	return (0);
}])
m4_indent([dnl
void
log_archlist(DB_ENV *dbenv)
{
	int ret;
	char **begin, **list;
m4_blank
	/* Get the list of database files. */
	if ((ret = dbenv-__GT__log_archive(dbenv,
	    &list, DB_ARCH_ABS | DB_ARCH_DATA)) != 0) {
		dbenv-__GT__err(dbenv, ret, "DB_ENV-__GT__log_archive: DB_ARCH_DATA");
		exit (1);
	}
	if (list != NULL) {
		for (begin = list; *list != NULL; ++list)
			printf("database file: %s\n", *list);
		free (begin);
	}
m4_blank
	/* Get the list of log files. */
	if ((ret = dbenv-__GT__log_archive(dbenv,
	    &list, DB_ARCH_ABS | DB_ARCH_LOG)) != 0) {
		dbenv-__GT__err(dbenv, ret, "DB_ENV-__GT__log_archive: DB_ARCH_LOG");
		exit (1);
	}
	if (list != NULL) {
		for (begin = list; *list != NULL; ++list)
			printf("log file: %s\n", *list);
		free (begin);
	}
}])
