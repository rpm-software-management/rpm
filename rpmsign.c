#include "system.h"
#include <errno.h>
#include <sys/wait.h>

#include <popt.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmsign.h>
#include "cliutils.h"
#include "lib/rpmsignfiles.h"
#include "debug.h"

#if !defined(__GLIBC__) && !defined(__APPLE__)
char ** environ = NULL;
#endif

enum modes {
    MODE_ADDSIGN = (1 << 0),
    MODE_RESIGN  = (1 << 1),
    MODE_DELSIGN = (1 << 2),
};

static int mode = 0;

static int signfiles = 0, fskpass = 0;
static char * fileSigningKey = NULL;
static char * fileSigningKeyPassword = NULL;

static struct poptOption signOptsTable[] = {
    { "addsign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_ADDSIGN,
	N_("sign package(s)"), NULL },
    { "resign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_RESIGN,
	N_("sign package(s) (identical to --addsign)"), NULL },
    { "delsign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_DELSIGN,
	N_("delete package signatures"), NULL },
    { "signfiles", '\0', POPT_ARG_NONE, &signfiles, 0,
	N_("sign package(s) files"), NULL},
    { "fskpath", '\0', POPT_ARG_STRING, &fileSigningKey, 0,
	N_("use file signing key <key>"),
	N_("<key>") },
    { "fskpass", '\0', POPT_ARG_NONE, &fskpass, 0,
	N_("prompt for file signing key password"), NULL},
    POPT_TABLEEND
};

static struct poptOption optionsTable[] = {
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, signOptsTable, 0,
	N_("Signature options:"), NULL },
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"), NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

/* TODO: permit overriding macro setup on the command line */
static int doSign(poptContext optCon)
{
    int rc = EXIT_FAILURE;
    char * passPhrase = NULL;
    char * name = rpmExpand("%{?_gpg_name}", NULL);
    struct rpmSignArgs sig = {NULL, 0, 0};

    if (rstreq(name, "")) {
	fprintf(stderr, _("You must set \"%%_gpg_name\" in your macro file\n"));
	goto exit;
    }

    if (fileSigningKey) {
	addMacro(NULL, "_file_signing_key", NULL, fileSigningKey, RMIL_GLOBAL);
    }

    if (signfiles) {
	const char *key = rpmExpand("%{?_file_signing_key}", NULL);
	if (rstreq(key, "")) {
	    fprintf(stderr, _("You must set \"$$_file_signing_key\" in your macro file or on the command line with --fskpath\n"));
	    goto exit;
	}

	if (fskpass) {
#ifndef WITH_IMAEVM
	    argerror(_("--fskpass may only be specified when signing files"));
#else
	    fileSigningKeyPassword = get_fskpass();
#endif
	}

	addMacro(NULL, "_file_signing_key_password", NULL,
	    fileSigningKeyPassword, RMIL_CMDLINE);
	if (fileSigningKeyPassword) {
	    memset(fileSigningKeyPassword, 0, strlen(fileSigningKeyPassword));
	    free(fileSigningKeyPassword);
	}

	sig.signfiles = 1;
    }

    const char *arg;
    rc = 0;
    while ((arg = poptGetArg(optCon)) != NULL) {
	rc += rpmPkgSign(arg, &sig);
    }

exit:
    free(passPhrase);
    free(name);
    return rc;
}

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);
    const char *arg;
    
    if (argc <= 1) {
	printUsage(optCon, stderr, 0);
	goto exit;
    }

    if (poptPeekArg(optCon) == NULL) {
	argerror(_("no arguments given"));
    }

    if (fileSigningKey && !signfiles) {
	argerror(_("--fskpath may only be specified when signing files"));
    }

    switch (mode) {
    case MODE_ADDSIGN:
    case MODE_RESIGN:
	ec = doSign(optCon);
	break;
    case MODE_DELSIGN:
	ec = 0;
	while ((arg = poptGetArg(optCon)) != NULL) {
	    ec += rpmPkgDelSign(arg);
	}
	break;
    default:
	argerror(_("only one major mode may be specified"));
	break;
    }

exit:
    rpmcliFini(optCon);
    return ec;
}
