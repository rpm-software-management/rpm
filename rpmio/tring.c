#include "system.h"
#include "rpmio_internal.h"
#include "popt.h"
#include "debug.h"

static int printing = 1;
static int _debug = 0;
int noNeon;

static struct poptOption optionsTable[] = {
 { "print", 'p', POPT_ARG_VAL,	&printing, 1,		NULL, NULL },
 { "noprint", 'n', POPT_ARG_VAL, &printing, 0,		NULL, NULL },
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "noneon", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noNeon, 1,
	N_("disable use of libneon for HTTP"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main (int argc, const char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    pgpDig dig;
    const byte * pkt = NULL;
    ssize_t pktlen;
    const char ** args;
    const char * fn;
    int rc, ec = 0;

    while ((rc = poptGetNextOpt(optCon)) > 0)
	;

    if ((args = poptGetArgs(optCon)) != NULL)
    while ((fn = *args++) != NULL) {
	pgpArmor pa;

	pa = pgpReadPkts(fn, &pkt, &pktlen);
	if (pa == PGPARMOR_ERROR
	 || pa == PGPARMOR_NONE
	 || pkt == NULL || pktlen <= 0)
	{
	    ec++;
	    continue;
	}

fprintf(stderr, "===================== %s\n", fn);
	dig = xcalloc(1, sizeof(*dig));
	(void) pgpPrtPkts(pkt, pktlen, dig, printing);
	free((void *)pkt);
	pkt = NULL;
	free((void *)dig);
	dig = NULL;
    }

    return ec;
}
