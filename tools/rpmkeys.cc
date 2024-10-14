#include "system.h"

#include <popt.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmkeyring.h>
#include <rpm/rpmlog.h>

#include "cliutils.hh"
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

static int matchingKeys(rpmKeyring keyring, ARGV_const_t args, void * userdata, int callback(rpmPubkey, void*))
{
    int ec = EXIT_SUCCESS;
    if (args) {
	for (char * const * arg = args; *arg; arg++) {
	    int found = false;
	    size_t klen = strlen(*arg);

	    /* Allow short keyid while we're transitioning */
	    if (klen != 40 && klen != 16 && klen != 8) {
		rpmlog(RPMLOG_ERR, ("invalid key id: %s\n"), *arg);
		ec = EXIT_FAILURE;
		continue;
	    }

	    auto iter = rpmKeyringInitIterator(keyring, 0);
	    rpmPubkey key = NULL;
	    while ((key = rpmKeyringIteratorNext(iter))) {
		char * fp = rpmPubkeyFingerprintAsHex(key);
		char * keyid = rpmPubkeyKeyIDAsHex(key);
		if (!strcmp(*arg, fp) || !strcmp(*arg, keyid)) {
		    found = true;
		}
		free(fp);
		free(keyid);
		if (found)
		    break;
	    }
	    rpmKeyringIteratorFree(iter);
	    if (found) {
		callback(key, userdata);
	    } else {
		rpmlog(RPMLOG_ERR, ("key not found: %s\n"), *arg);
		ec = EXIT_FAILURE;
	    }
	}
    } else {
	auto iter = rpmKeyringInitIterator(keyring, 0);
	rpmPubkey key = NULL;
	while ((key = rpmKeyringIteratorNext(iter))) {
	    callback(key, userdata);
	}
	rpmKeyringIteratorFree(iter);
    }
    return ec;
}

static int printKey(rpmPubkey key, void * data)
{
    char * fp = rpmPubkeyFingerprintAsHex(key);
    pgpDigParams params = rpmPubkeyPgpDigParams(key);
    rpmlog(RPMLOG_NOTICE, "%s %s public key\n", fp, pgpDigParamsUserID(params));
    free(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = NULL;
    rpmts ts = NULL;
    ARGV_const_t args = NULL;
    rpmKeyring keyring = NULL;

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
    keyring = rpmtsGetKeyring(ts, 1);

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
	rpmtxn txn = rpmtxnBegin(ts, RPMTXN_WRITE);
	if (txn) {
	    int nfail = 0;
	    for (char const * const *arg = args; *arg && **arg; arg++) {
		rpmRC delrc = rpmtxnDeletePubkey(txn, *arg);
		if (delrc) {
		    if (delrc == RPMRC_NOTFOUND)
			rpmlog(RPMLOG_ERR, ("key not found: %s\n"), *arg);
		    else if (delrc == RPMRC_NOKEY)
			rpmlog(RPMLOG_ERR, ("invalid key id: %s\n"), *arg);
		    else if (delrc == RPMRC_FAIL)
			rpmlog(RPMLOG_ERR, ("failed to delete key: %s\n"), *arg);
		    nfail++;
		}
	    }
	    ec = nfail;
	    rpmtxnEnd(txn);
	}
	break;
    }
    case MODE_LISTKEY:
    {
	ec = matchingKeys(keyring, args, NULL, printKey);
	break;
    }
    default:
	argerror(_("only one major mode may be specified"));
    }

exit:
    rpmKeyringFree(keyring);
    rpmtsFree(ts);
    rpmcliFini(optCon);
    fflush(stderr);
    fflush(stdout);
    if (ferror(stdout) || ferror(stderr))
	return 255; /* I/O error */
    return ec;
}
