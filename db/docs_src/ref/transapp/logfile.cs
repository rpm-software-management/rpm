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

void *logfile_thread(void *);

DB_ENV *dbenv;
pthread_t ptid;
int ret;
])
m4_indent([dnl
int
main(int argc, char *argv[])
{
m4_cbold([dnl
	/* Start a logfile removal thread. */
	if ((ret = pthread_create(
	    &ptid, NULL, logfile_thread, (void *)dbenv)) != 0) {
		fprintf(stderr,
		    "txnapp: failed spawning log file removal thread: %s\n",
		    strerror(ret));
		exit (1);
	}])
m4_ignore([return (0);])
}
m4_blank
m4_cbold([dnl
void *
logfile_thread(void *arg)
{
	DB_ENV *dbenv;
	int ret;
	char **begin, **list;
m4_blank
	dbenv = arg;
	dbenv-__GT__errx(dbenv,
	    "Log file removal thread: %lu", (u_long)pthread_self());
m4_blank
	/* Check once every 5 minutes. */
	for (;; sleep(300)) {
		/* Get the list of log files. */
		if ((ret = dbenv-__GT__log_archive(dbenv, &list, DB_ARCH_ABS)) != 0) {
			dbenv-__GT__err(dbenv, ret, "DB_ENV-__GT__log_archive");
			exit (1);
		}
m4_blank
		/* Remove the log files. */
		if (list != NULL) {
			for (begin = list; *list != NULL; ++list)
				if ((ret = remove(*list)) != 0) {
					dbenv-__GT__err(dbenv,
					    ret, "remove %s", *list);
					exit (1);
				}
			free (begin);
		}
	}
	/* NOTREACHED */
}])])
