#include "system.h"

#include <popt.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <rpm/rpmcli.h>
#include "rpmlua.h"
#include "rpmio_internal.h"
#include "debug.h"

static char *opts = NULL;
static char *execute = NULL;
static int interactive = 0;

static struct poptOption optionsTable[] = {

    { "opts", 'o', POPT_ARG_STRING, &opts, 0,
	N_("getopt string for scripts options"), NULL },
    { "execute", 'e', POPT_ARG_STRING, &execute, 0,
	N_("execute statement"), NULL },
    { "interactive", 'i', POPT_ARG_VAL, &interactive, 1,
	N_("interactive session"), NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

#ifdef HAVE_READLINE
static char *myreadline(const char *prompt)
{
    char *line = readline(prompt);
    if (line && *line)
	add_history(line);
    return line;
}
#endif

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = NULL;
    ARGV_t args = NULL;
    int nargs = 0;
    char *buf = NULL;
    ssize_t blen = 0;
#ifdef HAVE_READLINE
    rpmluarl rlcb = myreadline;
#else
    rpmluarl rlcb = NULL;
#endif

    optCon = rpmcliInit(argc, argv, optionsTable);

    if (optCon == NULL) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    args = (ARGV_t)poptGetArgs(optCon);
    nargs = argvCount(args);

    if (execute)
	ec = rpmluaRunScript(NULL, execute, "<execute>", opts, args);

    if (nargs >= 1) {
	if (rpmioSlurp(args[0], (uint8_t **) &buf, &blen)) {
	    fprintf(stderr, "reading %s failed: %s\n",
		args[0], strerror(errno));
	    goto exit;
	}
	ec = rpmluaRunScript(NULL, buf, args[0], opts, args);
    }

    /* Mimic Lua standalone interpreter behavior */
    if (interactive || (nargs == 0 && execute == 0))
	rpmluaInteractive(NULL, rlcb);

exit:
    free(buf);
    rpmcliFini(optCon);
    return ec;
}
