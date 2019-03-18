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
};

static int mode = MODE_NONE;

#ifdef WITH_IMAEVM
static int signfiles = 0, fskpass = 0;
static char * fileSigningKey = NULL;
#endif

static struct rpmSignArgs sargs = {NULL, 0, 0};

static struct poptOption signOptsTable[] = {
    { "addsign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_ADDSIGN,
	N_("sign package(s)"), NULL },
    { "resign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_RESIGN,
	N_("sign package(s) (identical to --addsign)"), NULL },
    { "delsign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_DELSIGN,
	N_("delete package signatures"), NULL },
#ifdef WITH_IMAEVM
    { "signfiles", '\0', POPT_ARG_NONE, &signfiles, 0,
	N_("sign package(s) files"), NULL},
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

#ifdef WITH_IMAEVM
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

#ifdef WITH_IMAEVM
    if (fileSigningKey) {
	rpmPushMacro(NULL, "_file_signing_key", NULL, fileSigningKey, RMIL_GLOBAL);
    }

    if (signfiles) {
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

	sargs->signfiles = 1;
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

#ifdef WITH_IMAEVM
    if (fileSigningKey && !signfiles) {
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
