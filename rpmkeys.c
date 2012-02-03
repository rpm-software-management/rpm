#include "system.h"

#include <popt.h>
#include <rpm/rpmcli.h>
#include "cliutils.h"
#include "debug.h"

#if !defined(__GLIBC__) && !defined(__APPLE__)
char ** environ = NULL;
#endif

enum modes {
    MODE_CHECKSIG	= (1 << 0),
    MODE_IMPORTKEY	= (1 << 1),
    MODE_DELKEY		= (1 << 2),
    MODE_LISTKEY	= (1 << 3),
};

static int mode = 0;
static int test = 0;

static struct poptOption keyOptsTable[] = {
    { "checksig", 'K', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_CHECKSIG,
	N_("verify package signature(s)"), NULL },
    { "import", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_IMPORTKEY,
	N_("import an armored public key"), NULL },
    { "test", '\0', POPT_ARG_NONE, &test, 0,
	N_("don't import, but tell if it would work or not"), NULL },
#if 0
    { "delete-key", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_DELKEY,
	N_("list keys from RPM keyring"), NULL },
    { "list-keys", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_LISTKEY,
	N_("list keys from RPM keyring"), NULL },
#endif
    POPT_TABLEEND
};

static struct poptOption optionsTable[] = {
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, keyOptsTable, 0,
	N_("Keyring options:"), NULL },
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
    rpmts ts = rpmtsCreate();
    ARGV_const_t args = NULL;
    
    if (argc < 2) {
	printUsage(optCon, stderr, 0);
	goto exit;
    }

    args = (ARGV_const_t) poptGetArgs(optCon);

    if (mode != MODE_LISTKEY && args == NULL)
	argerror(_("no arguments given"));

    rpmtsSetRootDir(ts, rpmcliRootDir);

    switch (mode) {
    case MODE_CHECKSIG:
	ec = rpmcliVerifySignatures(ts, args);
	break;
    case MODE_IMPORTKEY:
	if (test)
	    rpmtsSetFlags(ts, (rpmtsFlags(ts)|RPMTRANS_FLAG_TEST));
	ec = rpmcliImportPubkeys(ts, args);
	break;
    /* XXX TODO: actually implement these... */
    case MODE_DELKEY:
    case MODE_LISTKEY:
	break;
    default:
	argerror(_("only one major mode may be specified"));
    }

exit:
    rpmtsFree(ts);
    rpmcliFini(optCon);
    return ec;
}
