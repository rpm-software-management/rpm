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
    MODE_EXPORTKEY	= (1 << 4),
    MODE_REBUILD	= (1 << 5),
};

static int mode = 0;
static int test = 0;
static char * from = NULL;

static struct poptOption keyOptsTable[] = {
    { "checksig", 'K', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_CHECKSIG,
	N_("verify package signature(s)"), NULL },
    { "import", 'i', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_IMPORTKEY,
	N_("import an armored public key"), NULL },
    { "export", 'x', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_EXPORTKEY,
	N_("export an public key"), NULL },
    { "test", 't', POPT_ARG_NONE, &test, 0,
	N_("don't import, but tell if it would work or not"), NULL },
    { "delete", 'd', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_DELKEY,
	N_("Erase keys from RPM keyring"), NULL },
    { "erase", 'e', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_DELKEY,
	N_("Erase keys from RPM keyring"), NULL },
    { "list", 'l', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_LISTKEY,
	N_("list keys from RPM keyring"), NULL },
    { "rebuild", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_REBUILD,
	N_("rebuild the keyring - convert to current backend"), NULL },
    { "from", '\0', POPT_ARG_STRING, &from, 0,
	N_("get keys from this backend when rebuilding current backend"), NULL },
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

static int matchingKeys(rpmts ts, ARGV_const_t args, int callback(rpmPubkey, void*), void * userdata = NULL)
{
    int ec = EXIT_SUCCESS;
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
    char * c;

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

	    /* Check for valid hex chars */
	    for (c=*arg; *c; c++) {
		if (strchr("0123456789abcdefABCDEF", *c) == NULL)
		    break;
	    }
	    if (*c) {
		rpmlog(RPMLOG_ERR, ("invalid key id: %s\n"), *arg);
		ec = EXIT_FAILURE;
		continue;
	    }

	    auto iter = rpmKeyringInitIterator(keyring, 0);
	    rpmPubkey key = NULL;
	    while ((key = rpmKeyringIteratorNext(iter))) {
		const char * fp = rpmPubkeyFingerprintAsHex(key);
		const char * keyid = rpmPubkeyKeyIDAsHex(key);
		if (!strcmp(*arg, fp) || !strcmp(*arg, keyid) ||
		    !strcmp(*arg, keyid+8)) {
		    found = true;
		}
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
    rpmKeyringFree(keyring);
    return ec;
}

static int printKey(rpmPubkey key, void * data)
{
    pgpDigParams params = rpmPubkeyPgpDigParams(key);
    rpmlog(RPMLOG_NOTICE, "%s %s public key\n",
		rpmPubkeyFingerprintAsHex(key), pgpDigParamsUserID(params));
    return 0;
}

static int deleteKey(rpmPubkey key, void * data)
{
    rpmtxn txn = (rpmtxn) data;
    rpmtxnDeletePubkey(txn, key);
    return 0;
}

static int exportKey(rpmPubkey key, void * data)
{
    char * armored = rpmPubkeyArmorWrap(key);
    rpmlog(RPMLOG_NOTICE, "%s", armored);
    free(armored);
    return 0;
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

    if (args == NULL && mode != MODE_LISTKEY && mode != MODE_EXPORTKEY &&
	mode != MODE_REBUILD)
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
    case MODE_EXPORTKEY:
    {
	ec = matchingKeys(ts, args, exportKey);
	break;
    }
    case MODE_DELKEY:
    {
	rpmtxn txn = rpmtxnBegin(ts, RPMTXN_WRITE);
	if (txn) {
	    ec = matchingKeys(ts, args, deleteKey, txn);
	    rpmtxnEnd(txn);
	}
	break;
    }
    case MODE_LISTKEY:
    {
	ec = matchingKeys(ts, args, printKey);
	break;
    }
    case MODE_REBUILD:
    {

	rpmtxn txn = rpmtxnBegin(ts, RPMTXN_WRITE);
	if (txn) {
	    ec = rpmtsRebuildKeystore(txn, from);
	    rpmtxnEnd(txn);
	}
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
