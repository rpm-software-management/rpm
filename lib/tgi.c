#include "system.h"

#include <rpmio_internal.h>
#include <rpmgi.h>
#include <rpmcli.h>

#include <rpmte.h>

#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static const char * gitagstr = "packages";
static const char * gikeystr = NULL;
static rpmtransFlags transFlags = 0;
#ifdef	DYING
static rpmgiFlags giFlags = 0;
#endif

static const char * queryFormat = NULL;
static const char * defaultQueryFormat =
	"%{name}-%{version}-%{release}.%|SOURCERPM?{%{arch}.rpm}:{%|ARCH?{src.rpm}:{pubkey}|}|";

/*@only@*/ /*@null@*/
static const char * rpmgiPathOrQF(const rpmgi gi)
	/*@*/
{
    const char * fmt = ((queryFormat != NULL)
	? queryFormat : defaultQueryFormat);
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

 { "tag", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &gitagstr, 0,
	N_("iterate tag index"), NULL },
 { "key", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &gikeystr, 0,
	N_("tag value key"), NULL },

 { "anaconda", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
 	&transFlags, RPMTRANS_FLAG_ANACONDA|RPMTRANS_FLAG_DEPLOOPS,
	N_("use anaconda \"presentation order\""), NULL},

 { "transaction", 'T', POPT_BIT_SET, &giFlags, (RPMGI_TSADD|RPMGI_TSORDER),
	N_("create transaction set"), NULL},
 { "noorder", '\0', POPT_BIT_CLR, &giFlags, RPMGI_TSORDER,
	N_("do not order transaction set"), NULL},
 { "noglob", '\0', POPT_BIT_SET, &giFlags, RPMGI_NOGLOB,
	N_("do not glob arguments"), NULL},
 { "nomanifest", '\0', POPT_BIT_SET, &giFlags, RPMGI_NOMANIFEST,
	N_("do not process non-package files as manifests"), NULL},
 { "noheader", '\0', POPT_BIT_SET, &giFlags, RPMGI_NOHEADER,
	N_("do not read headers"), NULL},

 { "qf", '\0', POPT_ARG_STRING, &queryFormat, 0,
        N_("use the following query format"), "QUERYFORMAT" },
 { "queryformat", '\0', POPT_ARG_STRING, &queryFormat, 0,
        N_("use the following query format"), "QUERYFORMAT" },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliFtsPoptTable, 0,
        N_("File tree walk options for fts(3):"),
        NULL },

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
    int gitag = RPMDBI_PACKAGES;
    const char ** av;
    int ac;
    int rc = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
        exit(EXIT_FAILURE);

    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    if (gitagstr != NULL) {
	gitag = tagValue(gitagstr);
	if (gitag < 0) {
	    fprintf(stderr, _("unknown --tag argument: %s\n"), gitagstr);
	    exit(EXIT_FAILURE);
	}
    }

    /* XXX ftswalk segfault with no args. */

    ts = rpmtsCreate();
    (void) rpmtsSetFlags(ts, transFlags);

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

    gi = rpmgiNew(ts, gitag, gikeystr, 0);

    av = poptGetArgs(optCon);
    (void) rpmgiSetArgs(gi, av, ftsOpts, giFlags);

    ac = 0;
    while (rpmgiNext(gi) == RPMRC_OK) {
	if (!(giFlags & RPMGI_TSADD)) {
	    const char * arg = rpmgiPathOrQF(gi);

	    fprintf(stdout, "%5d %s\n", ac, arg);
	    arg = _free(arg);
	}
	ac++;
    }

    if (giFlags & RPMGI_TSORDER) {
	rpmtsi tsi;
	rpmte q;
	int i;
	
fprintf(stdout, "======================= %d transaction elements\n\
    # Tree Depth Degree Package\n\
=======================\n", rpmtsNElements(ts));

	i = 0;
	tsi = rpmtsiInit(ts);
	while((q = rpmtsiNext(tsi, 0)) != NULL) {
	    char deptypechar;

	    if (i == rpmtsUnorderedSuccessors(ts, -1))
		fprintf(stdout, "======================= leaf nodes only:\n");

	    deptypechar = (rpmteType(q) == TR_REMOVED ? '-' : '+');
	    fprintf(stdout, "%5d%5d%6d%7d %*s%c%s\n",
		i, rpmteTree(q), rpmteDepth(q), rpmteDegree(q),
		(2 * rpmteDepth(q)), "",
		deptypechar, rpmteNEVRA(q));
	    i++;
	}
	tsi = rpmtsiFree(tsi);
    }

    gi = rpmgiFree(gi);
    ts = rpmtsFree(ts);
    optCon = rpmcliFini(optCon);

    return rc;
}
