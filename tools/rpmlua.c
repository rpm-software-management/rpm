#include "system.h"

#include <popt.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <rpm/rpmcli.h>
#include "rpmio/rpmlua.h"
#include "rpmio/rpmio_internal.h"
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

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = NULL;
    ARGV_t args = NULL;
    char *buf = NULL;
    ssize_t blen = 0;

    xsetprogname(argv[0]);

    optCon = rpmcliInit(argc, argv, optionsTable);

    if (optCon == NULL) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    args = (ARGV_t)poptGetArgs(optCon);

    if (execute)
	ec = rpmluaRunScript(NULL, execute, "<execute>", opts, args);

    if (argvCount(args) >= 1) {
	if (rpmioSlurp(args[0], (uint8_t **) &buf, &blen)) {
	    fprintf(stderr, "reading %s failed: %s\n",
		args[0], strerror(errno));
	    goto exit;
	}
	ec = rpmluaRunScript(NULL, buf, args[0], opts, args);
    }

    if (interactive)
	rpmluaInteractive(NULL);

exit:
    free(buf);
    rpmcliFini(optCon);
    return ec;
}
