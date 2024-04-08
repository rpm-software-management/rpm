#include "system.h"

#include <popt.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmstring.h>
#include "cliutils.h"
#include "debug.h"

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
    { "delete", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_DELKEY,
	N_("delete keys from RPM keyring"), NULL },
    { "list", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_LISTKEY,
	N_("list keys from RPM keyring"), NULL },
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

static ARGV_t gpgkeyargs(ARGV_const_t args) {
    ARGV_t gpgargs = NULL;
    for (char * const * arg = args; *arg; arg++) {
	if (strncmp(*arg, "gpg-pubkey-", 11)) {
	    char * gpgarg = NULL;
	    rstrscat(&gpgarg, "gpg-pubkey-", *arg, NULL);
	    argvAdd(&gpgargs, gpgarg);
	    free(gpgarg);
	} else {
	    argvAdd(&gpgargs, *arg);
	}
    }
    return gpgargs;
}

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = NULL;
    rpmts ts = NULL;
    ARGV_const_t args = NULL;

    optCon = rpmcliInit(argc, argv, optionsTable);

    if (argc < 2) {
	printUsage(optCon, stderr, 0);
	goto exit;
    }

    args = (ARGV_const_t) poptGetArgs(optCon);

    if (mode != MODE_LISTKEY && args == NULL)
	argerror(_("no arguments given"));

    ts = rpmtsCreate();
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
    case MODE_DELKEY:
    {
	struct rpmInstallArguments_s * ia = &rpmIArgs;
	ARGV_t gpgargs = gpgkeyargs(args);
	ec = rpmErase(ts, ia, gpgargs);
	argvFree(gpgargs);
	break;
    }
    case MODE_LISTKEY:
    {
	ARGV_t query = NULL;
	if (args != NULL) {
	    query = gpgkeyargs(args);
	} else {
	    argvAdd(&query, "gpg-pubkey");
	}
	QVA_t qva = &rpmQVKArgs;
	rstrcat(&qva->qva_queryFormat, "%{version}-%{release}: %{summary}\n");
	ec = rpmcliQuery(ts, &rpmQVKArgs, (ARGV_const_t) query);
	query = argvFree(query);
	break;
    }
    default:
	argerror(_("only one major mode may be specified"));
    }

exit:
    rpmtsFree(ts);
    rpmcliFini(optCon);
    fflush(stderr);
    fflush(stdout);
    if (ferror(stdout) || ferror(stderr))
	return 255; /* I/O error */
    return ec;
}
