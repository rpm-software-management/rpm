#include "system.h"

#include <rpm/rpmbuild.h>
#include <rpm/argv.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfc.h>

#include "cliutils.hh"
#include "debug.h"

enum modes {
    MODE_NONE			= 0,
    MODE_PROVIDES		= (1 << 0),
    MODE_REQUIRES		= (1 << 1),
    MODE_RECOMMENDS		= (1 << 2),
    MODE_SUGGESTS		= (1 << 3),
    MODE_SUPPLEMENTS		= (1 << 4),
    MODE_ENHANCES		= (1 << 5),
    MODE_CONFLICTS		= (1 << 6),
    MODE_OBSOLETES		= (1 << 7),
    MODE_ORDERWITHREQUIRES	= (1 << 8),
    MODE_ALLDEPS		= (1 << 9),
};

static int mode = MODE_NONE;

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

 { "provides", 'P', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_PROVIDES,
        NULL, NULL },
 { "requires", 'R', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_REQUIRES,
        NULL, NULL },
 { "recommends", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_RECOMMENDS,
        NULL, NULL },
 { "suggests", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_SUGGESTS,
        NULL, NULL },
 { "supplements", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_SUPPLEMENTS,
        NULL, NULL },
 { "enhances", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_ENHANCES,
        NULL, NULL },
 { "conflicts", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_CONFLICTS,
        NULL, NULL },
 { "obsoletes", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_OBSOLETES,
        NULL, NULL },
 { "orderwithrequires", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR),
        &mode, MODE_ORDERWITHREQUIRES,
        NULL, NULL },
 { "alldeps", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_ALLDEPS,
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

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	goto exit;

    if (mode == MODE_NONE)
	argerror(_("no operation specified"));

    if (mode & (mode - 1))
	argerror(_("only one major mode may be specified"));

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

    if (_rpmfc_debug && mode != MODE_ALLDEPS)
	rpmfcPrint(NULL, fc, NULL);

    switch (mode) {
    case MODE_PROVIDES:
	rpmdsPrint(NULL, rpmfcProvides(fc), stdout);
	break;
    case MODE_REQUIRES:
	rpmdsPrint(NULL, rpmfcRequires(fc), stdout);
	break;
    case MODE_RECOMMENDS:
	rpmdsPrint(NULL, rpmfcRecommends(fc), stdout);
	break;
    case MODE_SUGGESTS:
	rpmdsPrint(NULL, rpmfcSuggests(fc), stdout);
	break;
    case MODE_SUPPLEMENTS:
	rpmdsPrint(NULL, rpmfcSupplements(fc), stdout);
	break;
    case MODE_ENHANCES:
	rpmdsPrint(NULL, rpmfcEnhances(fc), stdout);
	break;
    case MODE_CONFLICTS:
	rpmdsPrint(NULL, rpmfcConflicts(fc), stdout);
	break;
    case MODE_OBSOLETES:
	rpmdsPrint(NULL, rpmfcObsoletes(fc), stdout);
	break;
    case MODE_ORDERWITHREQUIRES:
	rpmdsPrint(NULL, rpmfcOrderWithRequires(fc), stdout);
	break;
    case MODE_ALLDEPS:
	rpmfcPrint(NULL, fc, stdout);
	break;
    }

    ec = 0;

exit:
    argvFree(av);
    rpmfcFree(fc);
    rpmcliFini(optCon);
    return ec;
}
