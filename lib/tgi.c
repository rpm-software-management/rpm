#include "system.h"

#include <rpmio_internal.h>
#include <rpmgi.h>
#include <rpmcli.h>

#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static int gitag = RPMGI_FTSWALK;
static int ftsOpts = 0;

static const char * queryFormat = NULL;

/*@only@*/ /*@null@*/
static const char * rpmgiPathOrQF(const rpmgi gi)
	/*@*/
{
    const char * fmt = ((queryFormat != NULL)
	? queryFormat : "%{name}-%{version}-%{release}.%{arch}");
    const char * val = NULL;
    Header h = rpmgiHeader(gi);

    if (h != NULL)
	val = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, NULL);
    else {
	const char * fn = rpmgiHdrPath(gi);
	val = (fn != NULL ? xstrdup(fn) : NULL);
    }

    return val;
}

static struct poptOption optionsTable[] = {
 { "rpmgidebug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmgi_debug, -1,
	N_("debug generalized iterator"), NULL},

 { "rpmdb", '\0', POPT_ARG_VAL, &gitag, RPMGI_RPMDB,
	N_("iterate rpmdb"), NULL },
 { "hdlist", '\0', POPT_ARG_VAL, &gitag, RPMGI_HDLIST,
	N_("iterate hdlist"), NULL },
 { "arglist", '\0', POPT_ARG_VAL, &gitag, RPMGI_ARGLIST,
	N_("iterate arglist"), NULL },
 { "ftswalk", '\0', POPT_ARG_VAL, &gitag, RPMGI_FTSWALK,
	N_("iterate fts(3) walk"), NULL },

 { "qf", '\0', POPT_ARG_STRING, &queryFormat, 0,
        N_("use the following query format"), "QUERYFORMAT" },
 { "queryformat", '\0', POPT_ARG_STRING, &queryFormat, 0,
        N_("use the following query format"), "QUERYFORMAT" },

 { "comfollow", '\0', POPT_BIT_SET,	&ftsOpts, FTS_COMFOLLOW,
	N_("follow command line symlinks"), NULL },
 { "logical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_LOGICAL,
	N_("logical walk"), NULL },
 { "nochdir", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOCHDIR,
	N_("don't change directories"), NULL },
 { "nostat", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOSTAT,
	N_("don't get stat info"), NULL },
 { "physical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_PHYSICAL,
	N_("physical walk"), NULL },
 { "seedot", '\0', POPT_BIT_SET,	&ftsOpts, FTS_SEEDOT,
	N_("return dot and dot-dot"), NULL },
 { "xdev", '\0', POPT_BIT_SET,		&ftsOpts, FTS_XDEV,
	N_("don't cross devices"), NULL },
 { "whiteout", '\0', POPT_BIT_SET,	&ftsOpts, FTS_WHITEOUT,
	N_("return whiteout information"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options for all rpm modes and executables:"),
        NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon;
    rpmts ts = NULL;
    rpmVSFlags vsflags;
    rpmgi gi = NULL;
    const char ** av;
    int ac;
    int rc = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
        exit(EXIT_FAILURE);

    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    ts = rpmtsCreate();
    vsflags = rpmExpandNumeric("%{?_vsflags_query}");
    if (rpmcliQueryFlags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (rpmcliQueryFlags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (rpmcliQueryFlags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    (void) rpmtsSetVSFlags(ts, vsflags);

    {   int_32 tid = (int_32) time(NULL);
	(void) rpmtsSetTid(ts, tid);
    }

    av = poptGetArgs(optCon);
    gi = rpmgiNew(ts, gitag, av, ftsOpts);

    ac = 0;
    while (rpmgiNext(gi) == RPMRC_OK) {
	const char * arg = rpmgiPathOrQF(gi);

	fprintf(stderr, "%5d %s\n", ac, arg);
	arg = _free(arg);
	ac++;
    }

    gi = rpmgiFree(gi);
    ts = rpmtsFree(ts);
    optCon = rpmcliFini(optCon);

    return rc;
}
