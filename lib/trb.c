#include "system.h"
#include <rpmcli.h>
#include <rpmts.h>

#include "debug.h"

#define	UP2DATEGLOB	"/var/spool/up2date/*.rpm"

static struct poptOption optionsTable[] = {
 { "verbose", 'v', 0, 0, 'v',
	N_("provide more detailed output"), NULL},
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
	N_("Install/Upgrade/Erase options:"),
	NULL },
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon;
    struct rpmInstallArguments_s * ia = &rpmIArgs;
    int arg;
    int ec = 0;
    rpmts ts;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);       /* Retrofit glibc __progname */

#if defined(ENABLE_NLS)
    /* set up the correct locale */
    (void) setlocale(LC_ALL, "" );

    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    rpmSetVerbosity(RPMLOG_NOTICE);

    optCon = poptGetContext(argv[0], argc, (const char **) argv, optionsTable, 0);
    (void) poptReadConfigFile(optCon, LIBRPMALIAS_FILENAME);
#if RPM_USES_POPTREADDEFAULTCONFIG
    (void) poptReadDefaultConfig(optCon, 1);
#endif
    poptSetExecPath(optCon, RPMCONFIGDIR, 1);

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	switch(arg) {
	case 'v':
	    rpmIncreaseVerbosity();
	    break;
	default:
	    break;
	}
    }

    if (ia->rbtid == 0) {
	fprintf(stderr, "--rollback <timestamp> is required\n");
	exit(1);
    }

    if (rpmReadConfigFiles(NULL, NULL))
	exit(1);

    ts = rpmtsCreate();
    ec = rpmRollback(ts, ia, NULL);
    ts = rpmtsFree(ts);

    optCon = poptFreeContext(optCon);
    rpmFreeMacros(NULL);

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    return ec;
}
