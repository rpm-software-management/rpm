#include "system.h"

#include <rpmbuild.h>
#include <argv.h>
#include <rpmfc.h>

#include "debug.h"

static int print_provides;
static int print_requires;

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL }, 

 { "rpmfcdebug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmfc_debug, -1,
        NULL, NULL },

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
    static const char * av_file[] = { "%{?__file_z_n}", NULL };
    poptContext optCon;
    StringBuf sb_stdin = NULL;
    StringBuf sb_stdout;
    int failnonzero = 1;
    ARGV_t xav;
    ARGV_t av = NULL;
    rpmfc fc;
    int ac = 0;
    int ec = 1;
    int xx;
char buf[BUFSIZ];

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	goto exit;

    av = poptGetArgs(optCon);
    ac = argvCount(av);

    if (ac == 0) {
	char * b, * be;
	sb_stdin = newStringBuf();
	av = NULL;
	while ((b = fgets(buf, sizeof(buf), stdin)) != NULL) {
	    buf[sizeof(buf)-1] = '\0';
	    be = b + strlen(buf) - 1;
	    while (strchr("\r\n", *be) != NULL)
		*be-- = '\0';
	    xx = argvAdd(&av, b);
	    appendLineStringBuf(sb_stdin, b);
	}
	ac = argvCount(av);
    }
	
    xav = NULL;
    xx = argvAppend(&xav, av_file);

    sb_stdout = NULL;
    if (sb_stdin != NULL) {
fprintf(stderr, "========\n%s\n==========\n", getStringBuf(sb_stdin));
	xx = rpmfcExec(xav, sb_stdin, &sb_stdout, failnonzero);
	xx = argvAppend(&xav, av);
    } else {
	xx = argvAppend(&xav, av);
	xx = rpmfcExec(xav, sb_stdin, &sb_stdout, failnonzero);
    }
    sb_stdin = freeStringBuf(sb_stdin);

    xav = argvFree(xav);

    xx = argvSplit(&xav, getStringBuf(sb_stdout), "\n");
    sb_stdout = freeStringBuf(sb_stdout);

    xx = argvSort(xav, NULL);

    /* Build file class dictionary. */
    fc = rpmfcNew();
    xx = rpmfcClassify(fc, xav);

    /* Build file/package dependency dictionary. */
    xx = rpmfcApply(fc);

if (_rpmfc_debug) {
sprintf(buf, "final: files %d cdict[%d] %d%% ddictx[%d]", fc->nfiles, argvCount(fc->cdict), ((100 * fc->fknown)/fc->nfiles), argiCount(fc->ddictx));
rpmfcPrint(buf, fc, NULL);
}

    if (print_provides)
	argvPrint(NULL, fc->provides, stdout);
    if (print_requires)
	argvPrint(NULL, fc->requires, stdout);

    fc = rpmfcFree(fc);

    xav = argvFree(xav);

exit:
    optCon = rpmcliFini(optCon);
    return ec;
}
