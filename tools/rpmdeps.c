#include "system.h"
const char *__progname;

#include <rpmbuild.h>
#include <argv.h>
#include <rpmds.h>
#include <rpmfc.h>

#include "debug.h"

/*@unchecked@*/
char *progname;

/*@unchecked@*/
static int print_provides;

/*@unchecked@*/
static int print_requires;

static void rpmdsPrint(const char * msg, rpmds ds, FILE * fp)
{
    if (fp == NULL) fp = stderr;

    if (msg)
	fprintf(fp, "===================================== %s\n", msg);

    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0)
	fprintf(fp, "%s\n", rpmdsDNEVR(ds)+2);
}

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL }, 

 { "provides", 'P', POPT_ARG_VAL, &print_provides, -1,
        NULL, NULL },
 { "requires", 'R', POPT_ARG_VAL, &print_requires, -1,
        NULL, NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon;
    ARGV_t av = NULL;
    rpmfc fc;
    int ac = 0;
    int ec = 1;
    int xx;
char buf[BUFSIZ];

/*@-modobserver@*/
    if ((progname = strrchr(argv[0], '/')) != NULL)
	progname++;
    else
	progname = argv[0];
/*@=modobserver@*/

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	goto exit;

    av = poptGetArgs(optCon);
    ac = argvCount(av);

    if (ac == 0) {
	char * b, * be;
	av = NULL;
	while ((b = fgets(buf, sizeof(buf), stdin)) != NULL) {
	    buf[sizeof(buf)-1] = '\0';
	    be = b + strlen(buf) - 1;
	    while (strchr("\r\n", *be) != NULL)
		*be-- = '\0';
	    xx = argvAdd(&av, b);
	}
	ac = argvCount(av);
    }

    /* Make sure file names are sorted. */
    xx = argvSort(av, NULL);

    /* Build file class dictionary. */
    fc = rpmfcNew();
    xx = rpmfcClassify(fc, av, NULL);

    /* Build file/package dependency dictionary. */
    xx = rpmfcApply(fc);

if (_rpmfc_debug) {
sprintf(buf, "final: files %d cdict[%d] %d%% ddictx[%d]", fc->nfiles, argvCount(fc->cdict), ((100 * fc->fknown)/fc->nfiles), argiCount(fc->ddictx));
rpmfcPrint(buf, fc, NULL);
}

    if (print_provides)
	rpmdsPrint(NULL, fc->provides, stdout);
    if (print_requires)
	rpmdsPrint(NULL, fc->requires, stdout);

    fc = rpmfcFree(fc);

    ec = 0;

exit:
    optCon = rpmcliFini(optCon);
    return ec;
}
