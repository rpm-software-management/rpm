#include "system.h"

#include <rpm/rpmcli.h>
#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>

#include "cliutils.h"

#include "debug.h"

enum modes {
    MODE_UNKNOWN	= 0,
    MODE_QUERY		= (1 <<  0),
    MODE_PARSE		= (1 <<  1),
};

static int mode = MODE_UNKNOWN;
static int source = RPMQV_SPECRPMS;
char *queryformat = NULL;

static struct poptOption specOptsTable[] = {
    { "parse", 'P', POPT_ARG_VAL, &mode, MODE_PARSE,
	N_("parse spec file(s) to stdout"), NULL },
    { "query", 'q', POPT_ARG_VAL, &mode, MODE_QUERY,
	N_("query spec file(s)"), NULL },
    { "rpms", 0, POPT_ARG_VAL, &source, RPMQV_SPECRPMS,
	N_("operate on binary rpms generated by spec (default)"), NULL },
    { "builtrpms", 0, POPT_ARG_VAL, &source, RPMQV_SPECBUILTRPMS,
	N_("operate on binary rpms that would be built from spec"), NULL },
    { "srpm", 0, POPT_ARG_VAL, &source, RPMQV_SPECSRPM,
	N_("operate on source rpm generated by spec"), NULL },
    { "queryformat", 0, POPT_ARG_STRING, &queryformat, 0,
	N_("use the following query format"), "QUERYFORMAT" },
    { "qf", 0, (POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN), &queryformat, 0,
	NULL, NULL },
    POPT_TABLEEND
};

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, specOptsTable, 0,
	N_("Spec options:"), NULL },

    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"), NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

typedef int (*parsecb)(rpmSpec spec);

static int dumpSpec(rpmSpec spec)
{
    fprintf(stdout, "%s", rpmSpecGetSection(spec, RPMBUILD_NONE));
    return 0;
}

static int doSpec(poptContext optCon, parsecb cb)
{
    int ec = 0;
    const char * spath;
    char *target = rpmExpand("%{_target}", NULL);
    if (!poptPeekArg(optCon))
	argerror(_("no arguments given for parse"));

    while ((spath = poptGetArg(optCon)) != NULL) {
	rpmSpec spec = rpmSpecParse(spath, (RPMSPEC_ANYARCH|RPMSPEC_FORCE), NULL);
	if (spec) {
	    ec += cb(spec);
	    rpmSpecFree(spec);
	} else {
	    ec++;
	}
	rpmFreeMacros(NULL);
	rpmReadConfigFiles(rpmcliRcfile, target);
    }
    free(target);
    return ec;
}

int main(int argc, char *argv[])
{
    rpmts ts = NULL;
    QVA_t qva = &rpmQVKArgs;

    poptContext optCon;
    int ec = 0;

    xsetprogname(argv[0]); /* Portability call -- see system.h */

    optCon = rpmcliInit(argc, argv, optionsTable);

    if (rpmcliPipeOutput && initPipe())
	exit(EXIT_FAILURE);
	
    ts = rpmtsCreate();
    switch (mode) {

    case MODE_QUERY:
	if (!poptPeekArg(optCon))
	    argerror(_("no arguments given for query"));

	qva->qva_queryFormat = queryformat;
	qva->qva_source = source;
	qva->qva_specQuery = rpmspecQuery;
	ec = rpmcliQuery(ts, qva, (ARGV_const_t) poptGetArgs(optCon));
	break;

    case MODE_PARSE: {
	ec = doSpec(optCon, dumpSpec);
	break;
    }

    case MODE_UNKNOWN:
	if (poptPeekArg(optCon) != NULL || argc <= 1 || rpmIsVerbose()) {
	    printUsage(optCon, stderr, 0);
	    ec = argc;
	}
	break;
    }

    rpmtsFree(ts);
    if (finishPipe())
	ec = EXIT_FAILURE;

    free(qva->qva_queryFormat);

    rpmcliFini(optCon);

    return RETVAL(ec);
}
