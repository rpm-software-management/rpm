#include "system.h"

#include <popt.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmdb.h>
#include "cliutils.h"
#include "debug.h"

enum modes {
    MODE_INITDB		= (1 << 0),
    MODE_REBUILDDB	= (1 << 1),
    MODE_VERIFYDB	= (1 << 2),
    MODE_EXPORTDB	= (1 << 3),
    MODE_IMPORTDB	= (1 << 4),
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
    { "exportdb", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_EXPORTDB,
	N_("export database to stdout header list"),
	NULL},
    { "importdb", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_IMPORTDB,
	N_("import database from stdin header list"),
	NULL},
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

static int exportDB(rpmts ts)
{
    FD_t fd = fdDup(STDOUT_FILENO);
    rpmtxn txn = rpmtxnBegin(ts, RPMTXN_READ);
    int rc = 0;

    if (txn && fd) {
	Header h;
	rpmdbMatchIterator mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi))) {
	    rc += headerWrite(fd, h, HEADER_MAGIC_YES);
	}
	rpmdbFreeIterator(mi);
    } else {
	rc = -1;
    }
    Fclose(fd);
    rpmtxnEnd(txn);
    return rc;
}

/* XXX: only allow this on empty db? */
static int importDB(rpmts ts)
{
    FD_t fd = fdDup(STDIN_FILENO);
    rpmtxn txn = rpmtxnBegin(ts, RPMTXN_WRITE);
    int rc = 0;

    if (txn && fd) {
	Header h;
	while ((h = headerRead(fd, HEADER_MAGIC_YES))) {
	    rc += rpmtsImportHeader(txn, h, 0);
	}
    } else {
	rc = -1;
    }
    rpmtxnEnd(txn);
    Fclose(fd);
    return rc;
}

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = NULL;
    rpmts ts = NULL;

    xsetprogname(argv[0]); /* Portability call -- see system.h */

    optCon = rpmcliInit(argc, argv, optionsTable);

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
    case MODE_EXPORTDB:
	ec = exportDB(ts);
	break;
    case MODE_IMPORTDB:
	ec = importDB(ts);
	break;
    default:
	argerror(_("only one major mode may be specified"));
    }

exit:
    rpmtsFree(ts);
    rpmcliFini(optCon);
    return ec;
}
