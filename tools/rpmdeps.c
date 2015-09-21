#include "system.h"
const char *__progname;

#include <rpm/rpmbuild.h>
#include <rpm/argv.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfc.h>

#include "debug.h"

char *progname;

static int print_provides;

static int print_requires;

static int print_recommends;

static int print_suggests;

static int print_supplements;

static int print_enhances;

static int print_conflicts;

static int print_obsoletes;

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
 { "recommends", '\0', POPT_ARG_VAL, &print_recommends, -1,
        NULL, NULL },
 { "suggests", '\0', POPT_ARG_VAL, &print_suggests, -1,
        NULL, NULL },
 { "supplements", '\0', POPT_ARG_VAL, &print_supplements, -1,
        NULL, NULL },
 { "enhances", '\0', POPT_ARG_VAL, &print_enhances, -1,
        NULL, NULL },
 { "conflicts", '\0', POPT_ARG_VAL, &print_conflicts, -1,
        NULL, NULL },
 { "obsoletes", '\0', POPT_ARG_VAL, &print_obsoletes, -1,
        NULL, NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = NULL;
    ARGV_t av = NULL;
    rpmfc fc = NULL;
    int ec = 1;
    char buf[BUFSIZ];

    if ((progname = strrchr(argv[0], '/')) != NULL)
	progname++;
    else
	progname = argv[0];

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	goto exit;

    /* normally files get passed through stdin but also accept files as args */
    if (poptPeekArg(optCon)) {
	const char *arg;
	while ((arg = poptGetArg(optCon)) != NULL) {
	    argvAdd(&av, arg);
	}
    } else {
	while (fgets(buf, sizeof(buf), stdin) != NULL) {
	    char *be = buf + strlen(buf) - 1;
	    while (strchr("\r\n", *be) != NULL)
		*be-- = '\0';
	    argvAdd(&av, buf);
	}
    }
    /* Make sure file names are sorted. */
    argvSort(av, NULL);

    /* Build file/package class and dependency dictionaries. */
    fc = rpmfcCreate(getenv("RPM_BUILD_ROOT"), 0);
    if (rpmfcClassify(fc, av, NULL) || rpmfcApply(fc))
	goto exit;

    if (_rpmfc_debug)
	rpmfcPrint(buf, fc, NULL);

    if (print_provides)
	rpmdsPrint(NULL, rpmfcProvides(fc), stdout);
    if (print_requires)
	rpmdsPrint(NULL, rpmfcRequires(fc), stdout);
    if (print_recommends)
	rpmdsPrint(NULL, rpmfcRecommends(fc), stdout);
    if (print_suggests)
	rpmdsPrint(NULL, rpmfcSuggests(fc), stdout);
    if (print_supplements)
	rpmdsPrint(NULL, rpmfcSupplements(fc), stdout);
    if (print_enhances)
	rpmdsPrint(NULL, rpmfcEnhances(fc), stdout);
    if (print_conflicts)
	rpmdsPrint(NULL, rpmfcConflicts(fc), stdout);
    if (print_obsoletes)
	rpmdsPrint(NULL, rpmfcObsoletes(fc), stdout);

    ec = 0;

exit:
    argvFree(av);
    rpmfcFree(fc);
    rpmcliFini(optCon);
    return ec;
}
