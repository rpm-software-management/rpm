#include "system.h"
const char *__progname;

#include <rpm/rpmcli.h>
#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>

#include "cliutils.h"

#include "debug.h"

enum modes {
    MODE_UNKNOWN	= 0,
    MODE_QUERY		= (1 <<  0),
};

static int mode = MODE_UNKNOWN;

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {
    { "query", 'q', POPT_ARG_VAL, &mode, MODE_QUERY,
	N_("Query spec file(s)"), NULL },

    /* XXX FIXME: only queryformat is relevant for spec queries */
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQueryPoptTable, 0,
	N_("Query options (with -q or --query):"), NULL },
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"), NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int main(int argc, char *argv[])
{
    rpmts ts = NULL;
    QVA_t qva = &rpmQVKArgs;

    poptContext optCon;
    int ec = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);

    if (rpmcliPipeOutput && initPipe())
	exit(EXIT_FAILURE);
	
    ts = rpmtsCreate();
    switch (mode) {

    case MODE_QUERY:
	if (!poptPeekArg(optCon))
	    argerror(_("no arguments given for query"));

	qva->qva_source = RPMQV_SPECFILE;
	qva->qva_specQuery = rpmspecQuery;
	ec = rpmcliQuery(ts, qva, (ARGV_const_t) poptGetArgs(optCon));
	break;

    case MODE_UNKNOWN:
	if (poptPeekArg(optCon) != NULL || argc <= 1 || rpmIsVerbose()) {
	    printUsage(optCon, stderr, 0);
	    ec = argc;
	}
	break;
    }

    ts = rpmtsFree(ts);
    finishPipe();

    qva->qva_queryFormat = _free(qva->qva_queryFormat);

    rpmcliFini(optCon);

    return RETVAL(ec);
}
