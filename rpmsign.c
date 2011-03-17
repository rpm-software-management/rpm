#include "system.h"
#include <errno.h>
#include <sys/wait.h>

#include <popt.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmsign.h>
#include "cliutils.h"
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

static struct poptOption signOptsTable[] = {
    { "addsign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_ADDSIGN,
	N_("sign package(s)"), NULL },
    { "resign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_RESIGN,
	N_("sign package(s) (identical to --addsign)"), NULL },
    { "delsign", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &mode, MODE_DELSIGN,
	N_("delete package signatures"), NULL },
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

static int checkPassPhrase(const char * passPhrase)
{
    int passPhrasePipe[2];
    int pid, status;
    int rc;
    int xx;

    if (passPhrase == NULL)
	return -1;

    passPhrasePipe[0] = passPhrasePipe[1] = 0;
    xx = pipe(passPhrasePipe);
    if (!(pid = fork())) {
	char * cmd, * gpg_path;
	char *const *av;
	int fdno;

	xx = close(STDIN_FILENO);
	xx = close(STDOUT_FILENO);
	xx = close(passPhrasePipe[1]);
	if ((fdno = open("/dev/null", O_RDONLY)) != STDIN_FILENO) {
	    xx = dup2(fdno, STDIN_FILENO);
	    xx = close(fdno);
	}
	if ((fdno = open("/dev/null", O_WRONLY)) != STDOUT_FILENO) {
	    xx = dup2(fdno, STDOUT_FILENO);
	    xx = close(fdno);
	}
	xx = dup2(passPhrasePipe[0], 3);

	unsetenv("MALLOC_CHECK_");
	gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	if (!rstreq(gpg_path, ""))
	    setenv("GNUPGHOME", gpg_path, 1);
	
	cmd = rpmExpand("%{?__gpg_check_password_cmd}", NULL);
	rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
	if (!rc)
	    rc = execve(av[0], av+1, environ);

	fprintf(stderr, _("Could not exec %s: %s\n"), "gpg",
		    strerror(errno));
	_exit(EXIT_FAILURE);
    }

    xx = close(passPhrasePipe[0]);
    xx = write(passPhrasePipe[1], passPhrase, strlen(passPhrase));
    xx = write(passPhrasePipe[1], "\n", 1);
    xx = close(passPhrasePipe[1]);

    (void) waitpid(pid, &status, 0);

    return ((WIFEXITED(status) && WEXITSTATUS(status) == 0)) ? 0 : 1;
}

/* TODO: permit overriding macro setup on the command line */
static int doSign(poptContext optCon)
{
    int rc = EXIT_FAILURE;
    char * passPhrase = NULL;
    char * name = rpmExpand("%{?_gpg_name}", NULL);

    if (rstreq(name, "")) {
	fprintf(stderr, _("You must set \"%%_gpg_name\" in your macro file\n"));
	goto exit;
    }

    /* XXX FIXME: eliminate obsolete getpass() usage */
    passPhrase = getpass(_("Enter pass phrase: "));
    passPhrase = (passPhrase != NULL) ? rstrdup(passPhrase) : NULL;
    if (checkPassPhrase(passPhrase) == 0) {
	const char *arg;
	fprintf(stderr, _("Pass phrase is good.\n"));
	rc = 0;
	while ((arg = poptGetArg(optCon)) != NULL) {
	    rc += rpmPkgSign(arg, NULL, passPhrase);
	}
    } else {
	fprintf(stderr, _("Pass phrase check failed\n"));
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
