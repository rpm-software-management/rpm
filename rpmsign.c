#include "system.h"
#include <errno.h>
#include <sys/wait.h>
#include <termios.h>

#include <popt.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmsign.h>
#include "cliutils.h"
#include "debug.h"

enum modes {
    MODE_NONE    = 0,
    MODE_ADDSIGN = (1 << 0),
    MODE_RESIGN  = (1 << 1),
    MODE_DELSIGN = (1 << 2),
    MODE_DELFILESIGN = (1 << 3),
};

static int mode = MODE_NONE;

#if defined(WITH_IMAEVM) || defined(WITH_FSVERITY)
static int fskpass = 0;
static char * fileSigningKey = NULL;
#endif
#ifdef WITH_FSVERITY
static char * fileSigningCert = NULL;
static char * verityAlgorithm = NULL;
#endif

static struct rpmSignArgs sargs = {NULL, 0, 0};

static struct poptOption signOptsTable[] = {
    { "addsign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_ADDSIGN,
	N_("sign package(s)"), NULL },
    { "resign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_RESIGN,
	N_("sign package(s) (identical to --addsign)"), NULL },
    { "delsign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_DELSIGN,
	N_("delete package signatures"), NULL },
#if defined(WITH_IMAEVM) || defined(WITH_FSVERITY)
    { "delfilesign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode,
      MODE_DELFILESIGN,	N_("delete IMA and fsverity file signatures"), NULL },
#endif
    { "rpmv3", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR),
	&sargs.signflags, RPMSIGN_FLAG_RPMV3,
	N_("create rpm v3 header+payload signatures") },
#ifdef WITH_IMAEVM
    { "signfiles", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR),
	&sargs.signflags, RPMSIGN_FLAG_IMA,
	N_("sign package(s) files"), NULL},
#endif
#ifdef WITH_FSVERITY
    { "signverity", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR),
	&sargs.signflags, RPMSIGN_FLAG_FSVERITY,
	N_("generate fsverity signatures for package(s) files"), NULL},
    { "verityalgo", '\0', POPT_ARG_STRING, &verityAlgorithm, 0,
	N_("algorithm to use for verity signatures, default sha256"),
	N_("<algorithm>") },
    { "certpath", '\0', POPT_ARG_STRING, &fileSigningCert, 0,
	N_("use file signing cert <cert>"),
	N_("<cert>") },
#endif
#if defined(WITH_IMAEVM) || defined(WITH_FSVERITY)
    { "fskpath", '\0', POPT_ARG_STRING, &fileSigningKey, 0,
	N_("use file signing key <key>"),
	N_("<key>") },
    { "fskpass", '\0', POPT_ARG_NONE, &fskpass, 0,
	N_("prompt for file signing key password"), NULL},
#endif
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

#if defined(WITH_IMAEVM) || defined(WITH_FSVERITY)
static int flags_sign_files(int flags)
{
	return (flags & (RPMSIGN_FLAG_IMA | RPMSIGN_FLAG_FSVERITY) ? 1 : 0);
}

static char *get_fskpass(void)
{
    struct termios flags, tmp_flags;
    int passlen = 64;
    char *password = xmalloc(passlen);
    char *pwd = NULL;

    tcgetattr(fileno(stdin), &flags);
    tmp_flags = flags;
    tmp_flags.c_lflag &= ~ECHO;
    tmp_flags.c_lflag |= ECHONL;

    if (tcsetattr(fileno(stdin), TCSANOW, &tmp_flags) != 0) {
	perror("tcsetattr");
	goto exit;
    }

    printf("PEM password: ");
    pwd = fgets(password, passlen, stdin);

    if (tcsetattr(fileno(stdin), TCSANOW, &flags) != 0) {
	perror("tcsetattr");
	pwd = NULL;
	goto exit;
    }

exit:
    if (pwd)
	pwd[strlen(pwd) - 1] = '\0';  /* remove newline */
    else
	free(password);
    return pwd;
}
#endif

/* TODO: permit overriding macro setup on the command line */
static int doSign(poptContext optCon, struct rpmSignArgs *sargs)
{
    int rc = EXIT_FAILURE;
    char * name = rpmExpand("%{?_gpg_name}", NULL);

    if (rstreq(name, "")) {
	fprintf(stderr, _("You must set \"%%_gpg_name\" in your macro file\n"));
	goto exit;
    }

#if defined(WITH_IMAEVM) || defined(WITH_FSVERITY)
    if (fileSigningKey) {
	rpmPushMacro(NULL, "_file_signing_key", NULL, fileSigningKey, RMIL_GLOBAL);
    }

#ifdef WITH_FSVERITY
    if (fileSigningCert) {
	rpmPushMacro(NULL, "_file_signing_cert", NULL, fileSigningCert, RMIL_GLOBAL);
    }
    if (verityAlgorithm) {
	rpmPushMacro(NULL, "_verity_algorithm", NULL, verityAlgorithm, RMIL_GLOBAL);
    }
#endif

    if (flags_sign_files(sargs->signflags)) {
	char *fileSigningKeyPassword = NULL;
	char *key = rpmExpand("%{?_file_signing_key}", NULL);
	if (rstreq(key, "")) {
	    fprintf(stderr, _("You must set \"%%_file_signing_key\" in your macro file or on the command line with --fskpath\n"));
	    goto exit;
	}

	if (fskpass) {
	    fileSigningKeyPassword = get_fskpass();
	}

	if (fileSigningKeyPassword) {
	    rpmPushMacro(NULL, "_file_signing_key_password", NULL,
			fileSigningKeyPassword, RMIL_CMDLINE);
	    memset(fileSigningKeyPassword, 0, strlen(fileSigningKeyPassword));
	    free(fileSigningKeyPassword);
	}

	free(key);
    }
#endif

    const char *arg;
    rc = 0;
    while ((arg = poptGetArg(optCon)) != NULL) {
	if (rpmPkgSign(arg, sargs) < 0)
	    rc++;
    }

exit:
    free(name);
    return rc;
}

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = NULL;
    const char *arg;
    
    xsetprogname(argv[0]); /* Portability call -- see system.h */

    optCon = rpmcliInit(argc, argv, optionsTable);

    if (argc <= 1) {
	printUsage(optCon, stderr, 0);
	goto exit;
    }

    if (poptPeekArg(optCon) == NULL) {
	argerror(_("no arguments given"));
    }

#if defined(WITH_IMAEVM) || defined(WITH_FSVERITY)
    if (fileSigningKey && !(flags_sign_files(sargs.signflags))) {
	argerror(_("--fskpath may only be specified when signing files"));
    }
#endif

    switch (mode) {
    case MODE_ADDSIGN:
    case MODE_RESIGN:
	ec = doSign(optCon, &sargs);
	break;
    case MODE_DELSIGN:
	ec = 0;
	while ((arg = poptGetArg(optCon)) != NULL) {
	    if (rpmPkgDelSign(arg, &sargs) < 0)
		ec++;
	}
	break;
    case MODE_DELFILESIGN:
	ec = 0;
	while ((arg = poptGetArg(optCon)) != NULL) {
	    if (rpmPkgDelFileSign(arg, &sargs) < 0)
		ec++;
	}
	break;
    case MODE_NONE:
	printUsage(optCon, stderr, 0);
	break;
    default:
	argerror(_("only one major mode may be specified"));
	break;
    }

exit:
    rpmcliFini(optCon);
    return RETVAL(ec);
}
