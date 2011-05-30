#include "system.h"

#include <popt.h>
#include <rpm/rpmcli.h>
#include "cliutils.h"
#include "debug.h"

#if !defined(__GLIBC__) && !defined(__APPLE__)
char ** environ = NULL;
#endif

enum modes {
    MODE_INITDB		= (1 << 0),
    MODE_REBUILDDB	= (1 << 1),
    MODE_VERIFYDB	= (1 << 2),
};

static int mode = 0;

static struct poptOption dbOptsTable[] = {
    { "initdb", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_INITDB,
	N_("initialize database"), NULL},
    { "rebuilddb", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_REBUILDDB,
	N_("rebuild database inverted lists from installed package headers"),
	NULL},
    { "verifydb", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR|POPT_ARGFLAG_DOC_HIDDEN),
	&mode, MODE_VERIFYDB, N_("verify database files"), NULL},
    POPT_TABLEEND
};

static struct poptOption optionsTable[] = {
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, dbOptsTable, 0,
	N_("Database options:"), NULL },
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"), NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);
    rpmts ts = NULL;
    
    if (argc < 2 || poptPeekArg(optCon)) {
	printUsage(optCon, stderr, 0);
	goto exit;
    }

    ts = rpmtsCreate();
    rpmtsSetRootDir(ts, rpmcliRootDir);

    switch (mode) {
    case MODE_INITDB:
	ec = rpmtsInitDB(ts, 0644);
	break;
    case MODE_REBUILDDB:
    {   rpmVSFlags vsflags = rpmExpandNumeric("%{_vsflags_rebuilddb}");
	rpmVSFlags ovsflags = rpmtsSetVSFlags(ts, vsflags);
	ec = rpmtsRebuildDB(ts);
	rpmtsSetVSFlags(ts, ovsflags);
    }	break;
    case MODE_VERIFYDB:
	ec = rpmtsVerifyDB(ts);
	break;
    default:
	argerror(_("only one major mode may be specified"));
    }

exit:
    rpmtsFree(ts);
    rpmcliFini(optCon);
    return ec;
}
