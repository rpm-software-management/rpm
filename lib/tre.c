#include "system.h"

#define	_RPMSX_INTERNAL
#include <rpmsx.h>
#include <popt.h>

#include "debug.h"

static int add_assoc = 1;

/*
 * Command-line options.
 */
static int change = 1;
static int quiet = 0;
#define QPRINTF(args...) do { if (!quiet) printf(args); } while (0)
static int use_stdin = 0;
static int verbose = 0;
static int warn_no_match = 0;
static char *rootpath = NULL;
static int rootpathlen = 0;

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL, &_rpmsx_debug, -1,
        N_("show what specification matched each file"), NULL },
 { "nochange", 'n', POPT_ARG_VAL, &change, 0,
        N_("do not change any file labels"), NULL },
 { "quiet", 'q', POPT_ARG_VAL, &quiet, 1,
        N_("be quiet (suppress non-error output)"), NULL },
 { "root", 'r', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &rootpath, 0,
        N_("use an alternate root path"), N_("ROOT") },
 { "stdin", 's', POPT_ARG_VAL, &use_stdin, 1,
        N_("use stdin for a list of files instead of searching a partition"), NULL },
 { "verbose", 'v', POPT_ARG_VAL, &warn_no_match, 1,
        N_("show changes in file labels"), NULL },
 { "warn", 'W', POPT_ARG_VAL, &warn_no_match, 1,
        N_("warn about entries that have no matching file"), NULL },

   POPT_AUTOHELP
   POPT_TABLEEND
};

int main(int argc, char **argv)
{
    poptContext optCon;
    const char ** av;
    const char * arg;
    rpmsx sx;
    int ec = EXIT_FAILURE;	/* assume failure. */
    int rc;
    int i;

#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-noeffect@*/
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif

    setprogname(argv[0]);       /* Retrofit glibc __progname */
    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }
                                                                                
    (void) setlocale(LC_ALL, "" );
    (void) bindtextdomain(PACKAGE, LOCALEDIR);
    (void) textdomain(PACKAGE);

    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	default:
/*@-nullpass@*/
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		__progname, rc);
/*@=nullpass@*/
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
        }
    }

    if (rc < -1) {
/*@-nullpass@*/
	fprintf(stderr, "%s: %s: %s\n", __progname,
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(rc));
/*@=nullpass@*/
	goto exit;
    }

    /* trim trailing /, if present */
    if (rootpath != NULL) {
	rootpathlen = strlen(rootpath);
	while (rootpath[rootpathlen - 1] == '/')
	    rootpath[--rootpathlen] = 0;
    }

    av = poptGetArgs(optCon);

    /* Parse the specification file. */
    sx = rpmsxNew(NULL);

    if (_rpmsx_debug) {
	sx = rpmsxInit(sx, 1);
	if (sx != NULL)
	while ((i = rpmsxNext(sx)) >= 0) {
	    const char * pattern = rpmsxPattern(sx);
	    const char * type = rpmsxType(sx);
	    const char * context = rpmsxContext(sx);

	    fprintf(stderr, "%5d: %s\t%s\t%s\n", i,
		pattern, (type ? type : ""), context);
	}
    }

    if (av != NULL)
    while ((arg = *av++) != NULL) {
	const char * context = rpmsxFContext(sx, arg, S_IFREG);
	fprintf(stderr, "%s: %s\n", arg, context);
    }

    sx = rpmsxFree(sx);

    /*
     * Apply the specifications to the file systems.
     */
    ec = 0;

exit:
    optCon = poptFreeContext(optCon);

#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-noeffect@*/
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif

    return ec;
}
