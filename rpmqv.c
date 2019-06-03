#include "system.h"

#include <rpm/rpmcli.h>
#include <rpm/rpmlib.h>			/* RPMSIGTAG, rpmReadPackageFile .. */
#include <rpm/rpmlog.h>
#include <rpm/rpmps.h>
#include <rpm/rpmts.h>

#include "cliutils.h"

#include "debug.h"

enum modes {

    MODE_QUERY		= (1 <<  0),
    MODE_VERIFY		= (1 <<  3),
#define	MODES_QV (MODE_QUERY | MODE_VERIFY)

    MODE_INSTALL	= (1 <<  1),
    MODE_ERASE		= (1 <<  2),
#define	MODES_IE (MODE_INSTALL | MODE_ERASE)

    MODE_UNKNOWN	= 0
};

#define	MODES_FOR_NODEPS	(MODES_IE | MODE_VERIFY)
#define	MODES_FOR_TEST		(MODES_IE)

static int quiet;

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQVSourcePoptTable, 0,
        N_("Query/Verify package selection options:"),
        NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQVFilePoptTable, 0,
        N_("Query/Verify file selection options:"),
        NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQueryPoptTable, 0,
	N_("Query options (with -q or --query):"),
	NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmVerifyPoptTable, 0,
	N_("Verify options (with -V or --verify):"),
	NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
	N_("Install/Upgrade/Erase options:"),
	NULL },

 { "quiet", '\0', POPT_ARGFLAG_DOC_HIDDEN, &quiet, 0, NULL, NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int main(int argc, char *argv[])
{
    rpmts ts = NULL;
    enum modes bigMode = MODE_UNKNOWN;
    QVA_t qva = &rpmQVKArgs;
    struct rpmInstallArguments_s * ia = &rpmIArgs;
    poptContext optCon;
    int ec = 0;
    int i;

    xsetprogname(argv[0]); /* Portability call -- see system.h */

    optCon = rpmcliInit(argc, argv, optionsTable);

    /* Set the major mode based on argv[0] */
    if (rstreq(xgetprogname(), "rpmquery"))	bigMode = MODE_QUERY;
    if (rstreq(xgetprogname(), "rpmverify")) bigMode = MODE_VERIFY;

    /* Jumpstart option from argv[0] if necessary. */
    switch (bigMode) {
    case MODE_QUERY:	qva->qva_mode = 'q';	break;
    case MODE_VERIFY:	qva->qva_mode = 'V';	break;
    case MODE_INSTALL:
    case MODE_ERASE:
    case MODE_UNKNOWN:
    default:
	break;
    }

  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_QV)) {
    switch (qva->qva_mode) {
    case 'q':	bigMode = MODE_QUERY;		break;
    case 'V':	bigMode = MODE_VERIFY;		break;
    }

    if (qva->qva_sourceCount) {
	if (qva->qva_sourceCount > 1)
	    argerror(_("one type of query/verify may be performed at a "
			"time"));
    }
    if (qva->qva_flags && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query flags"));

    if (qva->qva_queryFormat && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query format"));

    if (qva->qva_source != RPMQV_PACKAGE && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query source"));
  }

  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_IE))
    {	int iflags = (ia->installInterfaceFlags &
			(INSTALL_UPGRADE|INSTALL_FRESHEN|
			 INSTALL_INSTALL|INSTALL_REINSTALL));
	int eflags = (ia->installInterfaceFlags & INSTALL_ERASE);

	if (iflags & eflags)
	    argerror(_("only one major mode may be specified"));
	else if (iflags)
	    bigMode = MODE_INSTALL;
	else if (eflags)
	    bigMode = MODE_ERASE;
    }

    if (!( bigMode == MODE_INSTALL ) &&
(ia->probFilter & (RPMPROB_FILTER_REPLACEPKG | RPMPROB_FILTER_OLDPACKAGE)))
	argerror(_("only installation and upgrading may be forced"));
    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_FORCERELOCATE))
	argerror(_("files may only be relocated during package installation"));

    if (ia->relocations && ia->prefix)
	argerror(_("cannot use --prefix with --relocate or --excludepath"));

    if (bigMode != MODE_INSTALL && ia->relocations)
	argerror(_("--relocate and --excludepath may only be used when installing new packages"));

    if (bigMode != MODE_INSTALL && ia->prefix)
	argerror(_("--prefix may only be used when installing new packages"));

    if (ia->prefix && ia->prefix[0] != '/') 
	argerror(_("arguments to --prefix must begin with a /"));

    if (!(bigMode & MODES_IE) && (ia->installInterfaceFlags & INSTALL_HASH))
	argerror(_("--hash (-h) may only be specified during package "
			"installation and erasure"));

    if (!(bigMode & MODES_IE) && (ia->installInterfaceFlags & INSTALL_PERCENT))
	argerror(_("--percent may only be specified during package "
			"installation and erasure"));

    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_REPLACEPKG))
	argerror(_("--replacepkgs may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && (ia->transFlags & RPMTRANS_FLAG_NODOCS))
	argerror(_("--excludedocs may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && ia->incldocs)
	argerror(_("--includedocs may only be specified during package "
		   "installation"));

    if (ia->incldocs && (ia->transFlags & RPMTRANS_FLAG_NODOCS))
	argerror(_("only one of --excludedocs and --includedocs may be "
		 "specified"));
  
    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_IGNOREARCH))
	argerror(_("--ignorearch may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_IGNOREOS))
	argerror(_("--ignoreos may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE &&
	(ia->probFilter & (RPMPROB_FILTER_DISKSPACE|RPMPROB_FILTER_DISKNODES)))
	argerror(_("--ignoresize may only be specified during package "
		   "installation"));

    if ((ia->installInterfaceFlags & UNINSTALL_ALLMATCHES) && bigMode != MODE_ERASE)
	argerror(_("--allmatches may only be specified during package "
		   "erasure"));

    if ((ia->transFlags & RPMTRANS_FLAG_ALLFILES) && bigMode != MODE_INSTALL)
	argerror(_("--allfiles may only be specified during package "
		   "installation"));

    if ((ia->transFlags & RPMTRANS_FLAG_JUSTDB) &&
	bigMode != MODE_INSTALL && bigMode != MODE_ERASE)
	argerror(_("--justdb may only be specified during package "
		   "installation and erasure"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE && bigMode != MODE_VERIFY &&
	(ia->transFlags & (RPMTRANS_FLAG_NOSCRIPTS | _noTransScripts | _noTransTriggers)))
	argerror(_("script disabling options may only be specified during "
		   "package installation and erasure"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE && bigMode != MODE_VERIFY &&
	(ia->transFlags & (RPMTRANS_FLAG_NOTRIGGERS | _noTransTriggers)))
	argerror(_("trigger disabling options may only be specified during "
		   "package installation and erasure"));

    if (ia->noDeps & (bigMode & ~MODES_FOR_NODEPS))
	argerror(_("--nodeps may only be specified during package "
		   "installation, erasure, and verification"));

    if ((ia->transFlags & RPMTRANS_FLAG_TEST) && (bigMode & ~MODES_FOR_TEST))
	argerror(_("--test may only be specified during package installation "
		 "and erasure"));

    if (rpmcliRootDir && rpmcliRootDir[0] != '/') {
	argerror(_("arguments to --root (-r) must begin with a /"));
    }

    if (quiet)
	rpmSetVerbosity(RPMLOG_WARNING);

    if (rpmcliPipeOutput && initPipe())
	exit(EXIT_FAILURE);
	
    ts = rpmtsCreate();
    (void) rpmtsSetRootDir(ts, rpmcliRootDir);
    switch (bigMode) {
    case MODE_ERASE:
	if (ia->noDeps) ia->installInterfaceFlags |= UNINSTALL_NODEPS;

	if (!poptPeekArg(optCon)) {
	    argerror(_("no packages given for erase"));
	} else {
	    ec += rpmErase(ts, ia, (ARGV_const_t) poptGetArgs(optCon));
	}
	break;

    case MODE_INSTALL:

	/* RPMTRANS_FLAG_KEEPOBSOLETE */

	if (!ia->incldocs) {
	    if (ia->transFlags & RPMTRANS_FLAG_NODOCS) {
		;
	    } else if (rpmExpandNumeric("%{_excludedocs}"))
		ia->transFlags |= RPMTRANS_FLAG_NODOCS;
	}

	if (ia->noDeps) ia->installInterfaceFlags |= INSTALL_NODEPS;

	/* we've already ensured !(!ia->prefix && !ia->relocations) */
	if (ia->prefix) {
	    ia->relocations = xmalloc(2 * sizeof(*ia->relocations));
	    ia->relocations[0].oldPath = NULL;   /* special case magic */
	    ia->relocations[0].newPath = ia->prefix;
	    ia->relocations[1].oldPath = NULL;
	    ia->relocations[1].newPath = NULL;
	} else if (ia->relocations) {
	    ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
	    ia->relocations[ia->numRelocations].oldPath = NULL;
	    ia->relocations[ia->numRelocations].newPath = NULL;
	}

	if (!poptPeekArg(optCon)) {
	    argerror(_("no packages given for install"));
	} else {
	    /* FIX: ia->relocations[0].newPath undefined */
	    ec += rpmInstall(ts, ia, (ARGV_t) poptGetArgs(optCon));
	}
	break;

    case MODE_QUERY:
	if (!poptPeekArg(optCon) && !(qva->qva_source == RPMQV_ALL))
	    argerror(_("no arguments given for query"));

	ec = rpmcliQuery(ts, qva, (ARGV_const_t) poptGetArgs(optCon));
	break;

    case MODE_VERIFY:
    {	rpmVerifyFlags verifyFlags = VERIFY_ALL;

	verifyFlags &= ~qva->qva_flags;
	qva->qva_flags = (rpmQueryFlags) verifyFlags;

	rpmtsSetFlags(ts, rpmtsFlags(ts) | (ia->transFlags & RPMTRANS_FLAG_NOPLUGINS));

	if (!poptPeekArg(optCon) && !(qva->qva_source == RPMQV_ALL))
	    argerror(_("no arguments given for verify"));
	ec = rpmcliVerify(ts, qva, (ARGV_const_t) poptGetArgs(optCon));
    }	break;

    case MODE_UNKNOWN:
	if (poptPeekArg(optCon) != NULL || argc <= 1) {
	    printUsage(optCon, stderr, 0);
	    ec = argc;
	}
	break;
    }

    rpmtsFree(ts);
    if (finishPipe())
	ec = EXIT_FAILURE;

    free(qva->qva_queryFormat);

    if (ia->relocations != NULL) {
	for (i = 0; i < ia->numRelocations; i++)
	    free(ia->relocations[i].oldPath);
	free(ia->relocations);
    }

    rpmcliFini(optCon);

    rpmlog(RPMLOG_DEBUG, "Exit status: %d\n", ec);

    return RETVAL(ec);
}
