#include "system.h"

#define	_AUTOHELP

#if defined(IAM_RPM) || defined(__LCLINT__)
#define	IAM_RPMBT
#define	IAM_RPMDB
#define	IAM_RPMEIU
#define	IAM_RPMQV
#define	IAM_RPMK
#endif

#include <rpmcli.h>
#include <rpmbuild.h>
#include <rpmurl.h>

#ifdef	IAM_RPMBT
#include "build.h"
#define GETOPT_REBUILD		1003
#define GETOPT_RECOMPILE	1004
#endif

#ifdef	DYING
#ifdef	IAM_RPMDB
#define GETOPT_REBUILDDB	1013
#define GETOPT_VERIFYDB		1023
static int initdb = 0;
#endif
#endif

#ifdef	DYING
#ifdef	IAM_RPMEIU
#define GETOPT_INSTALL		1014
#define GETOPT_RELOCATE		1016
#define GETOPT_EXCLUDEPATH	1019
static int incldocs = 0;
/*@null@*/ static const char * prefix = NULL;
#endif	/* IAM_RPMEIU */
#endif

#ifdef	DYING
#ifdef	IAM_RPMK
#define GETOPT_ADDSIGN		1005
#define GETOPT_RESIGN		1006
static int noGpg = 0;
static int noPgp = 0;
#endif	/* IAM_RPMK */
#endif

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
#include "signature.h"
#endif

#include "debug.h"

#define GETOPT_DBPATH		1010
#define GETOPT_SHOWRC		1018
#define	GETOPT_DEFINEMACRO	1020
#define	GETOPT_EVALMACRO	1021
#ifdef	NOTYET
#define	GETOPT_RCFILE		1022
#endif

enum modes {

    MODE_QUERY		= (1 <<  0),
    MODE_VERIFY		= (1 <<  3),
    MODE_QUERYTAGS	= (1 <<  9),
#define	MODES_QV (MODE_QUERY | MODE_VERIFY)

    MODE_INSTALL	= (1 <<  1),
    MODE_ERASE		= (1 <<  2),
#define	MODES_IE (MODE_INSTALL | MODE_ERASE)

    MODE_BUILD		= (1 <<  4),
    MODE_REBUILD	= (1 <<  5),
    MODE_RECOMPILE	= (1 <<  8),
    MODE_TARBUILD	= (1 << 11),
#define	MODES_BT (MODE_BUILD | MODE_TARBUILD | MODE_REBUILD | MODE_RECOMPILE)

    MODE_CHECKSIG	= (1 <<  6),
    MODE_RESIGN		= (1 <<  7),
#define	MODES_K	 (MODE_CHECKSIG | MODES_RESIGN)

    MODE_INITDB		= (1 << 10),
    MODE_REBUILDDB	= (1 << 12),
    MODE_VERIFYDB	= (1 << 13),
#define	MODES_DB (MODE_INITDB | MODE_REBUILDDB | MODE_VERIFYDB)

    MODE_UNKNOWN	= 0
};

#define	MODES_FOR_DBPATH	(MODES_BT | MODES_IE | MODES_QV | MODES_DB)
#define	MODES_FOR_NODEPS	(MODES_BT | MODES_IE | MODE_VERIFY)
#define	MODES_FOR_TEST		(MODES_BT | MODES_IE)
#define	MODES_FOR_ROOT		(MODES_BT | MODES_IE | MODES_QV | MODES_DB)

/*@-exportheadervar@*/
extern int _ftp_debug;
extern int noLibio;
extern int _rpmio_debug;
extern int _url_debug;
#ifdef	DYING
extern int _noDirTokens;
#endif

/*@-varuse@*/
/*@observer@*/ extern const char * rpmNAME;
/*@=varuse@*/
/*@observer@*/ extern const char * rpmEVR;
/*@-varuse@*/
extern int rpmFLAGS;
/*@=varuse@*/

extern struct MacroContext_s rpmCLIMacroContext;
/*@=exportheadervar@*/

/* options for all executables */

static int help = 0;
static int noUsageMsg = 0;
/*@observer@*/ /*@null@*/ static const char * pipeOutput = NULL;
static int quiet = 0;
/*@observer@*/ /*@null@*/ static const char * rcfile = NULL;
/*@observer@*/ /*@null@*/ static char * rootdir = "/";
static int showrc = 0;
static int showVersion = 0;

#ifdef	DYING
#if defined(IAM_RPMQV) || defined(IAM_RPMK)
static int noMd5 = 0;
#endif
#endif

#ifdef	DYING
#if defined(IAM_RPMEIU)
static int noDeps = 0;
static int force = 0;
#endif
#endif

static struct poptOption rpmAllPoptTable[] = {
 { "version", '\0', 0, &showVersion, 0,
	N_("print the version of rpm being used"),
	NULL },
 { "quiet", '\0', 0, &quiet, 0,
	N_("provide less detailed output"), NULL},
 { "verbose", 'v', 0, 0, 'v',
	N_("provide more detailed output"), NULL},
 { "define", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, 0, GETOPT_DEFINEMACRO,
	N_("define macro <name> with value <body>"),
	N_("'<name> <body>'") },
 { "eval", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, 0, GETOPT_EVALMACRO,
	N_("print macro expansion of <expr>+"),
	N_("<expr>+") },
 { "pipe", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &pipeOutput, 0,
	N_("send stdout to <cmd>"),
	N_("<cmd>") },
 { "root", 'r', POPT_ARG_STRING, &rootdir, 0,
	N_("use <dir> as the top level directory"),
	N_("<dir>") },
 { "macros", '\0', POPT_ARG_STRING, &macrofiles, 0,
	N_("read <file:...> instead of default macro file(s)"),
	N_("<file:...>") },
#if !defined(GETOPT_RCFILE)
 { "rcfile", '\0', POPT_ARG_STRING, &rcfile, 0,
	N_("read <file:...> instead of default rpmrc file(s)"),
	N_("<file:...>") },
#else
 { "rcfile", '\0', 0, 0, GETOPT_RCFILE,	
	N_("read <file:...> instead of default rpmrc file(s)"),
	N_("<file:...>") },
#endif
 { "showrc", '\0', 0, &showrc, GETOPT_SHOWRC,
	N_("display final rpmrc and macro configuration"),
	NULL },

#if HAVE_LIBIO_H && defined(_G_IO_IO_FILE_VERSION)
 { "nolibio", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noLibio, 1,
	N_("disable use of libio(3) API"), NULL},
#endif
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},

   POPT_TABLEEND
};

#ifdef	DYING
#ifdef	IAM_RPMDB
static struct poptOption rpmDatabasePoptTable[] = {
 { "initdb", '\0', 0, &initdb, 0,
	N_("initialize database"), NULL},
 { "rebuilddb", '\0', 0, 0, GETOPT_REBUILDDB,
	N_("rebuild database inverted lists from installed package headers"),
	NULL},
 { "verifydb", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, GETOPT_VERIFYDB,
	N_("verify database files"),
	NULL},
 { "nodirtokens", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_noDirTokens, 1,
	N_("generate headers compatible with (legacy) rpm[23] packaging"),
	NULL},
 { "dirtokens", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_noDirTokens, 0,
	N_("generate headers compatible with rpm4 packaging"), NULL},

   POPT_TABLEEND
};
#endif	/* IAM_RPMDB */
#endif

#ifdef	DYING
#ifdef	IAM_RPMK
static struct poptOption rpmSignPoptTable[] = {
 { "addsign", '\0', 0, 0, GETOPT_ADDSIGN,
	N_("add a signature to a package"), NULL },
 { "resign", '\0', 0, 0, GETOPT_RESIGN,
	N_("sign a package (discard current signature)"), NULL },
 { "sign", '\0', 0, &signIt, 0,
	N_("generate GPG/PGP signature"), NULL },
 { "checksig", 'K', 0, 0, 'K',
	N_("verify package signature"), NULL },
 { "nogpg", '\0', 0, &noGpg, 0,
	N_("skip any GPG signatures"), NULL },
 { "nopgp", '\0', POPT_ARGFLAG_DOC_HIDDEN, &noPgp, 0,
	N_("skip any PGP signatures"), NULL },
 { "nomd5", '\0', 0, &noMd5, 0,
	N_("do not verify file md5 checksums"), NULL },

   POPT_TABLEEND
};
#endif	/* IAM_RPMK */
#endif

#ifdef	DYING
#ifdef	IAM_RPMEIU
static rpmtransFlags transFlags = RPMTRANS_FLAG_NONE;
static rpmprobFilterFlags probFilter = RPMPROB_FILTER_NONE;
static rpmInstallInterfaceFlags installInterfaceFlags = INSTALL_NONE;
static rpmEraseInterfaceFlags eraseInterfaceFlags = UNINSTALL_NONE;

static struct poptOption rpmInstallPoptTable[] = {
 { "allfiles", '\0', POPT_BIT_SET, &transFlags, RPMTRANS_FLAG_ALLFILES,
  N_("install all files, even configurations which might otherwise be skipped"),
	NULL},
 { "allmatches", '\0', POPT_BIT_SET, &eraseInterfaceFlags, UNINSTALL_ALLMATCHES,
	N_("remove all packages which match <package> (normally an error is generated if <package> specified multiple packages)"),
	NULL},

 { "apply", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	(_noTransScripts|_noTransTriggers|
		RPMTRANS_FLAG_APPLYONLY|RPMTRANS_FLAG_PKGCOMMIT),
	N_("do not execute package scriptlet(s)"), NULL },

 { "badreloc", '\0', POPT_BIT_SET, &probFilter, RPMPROB_FILTER_FORCERELOCATE,
	N_("relocate files in non-relocateable package"), NULL},
 { "dirstash", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
		RPMTRANS_FLAG_DIRSTASH,
	N_("save erased package files by renaming into sub-directory"), NULL},
 { "erase", 'e', 0, 0, 'e',
	N_("erase (uninstall) package"), N_("<package>+") },
 { "excludedocs", '\0', POPT_BIT_SET, &transFlags, RPMTRANS_FLAG_NODOCS,
	N_("do not install documentation"), NULL},
 { "excludepath", '\0', POPT_ARG_STRING, 0, GETOPT_EXCLUDEPATH,
	N_("skip files with leading component <path> "),
	N_("<path>") },
 { "force", '\0', 0, &force, 0,
	N_("short hand for --replacepkgs --replacefiles"), NULL},
 { "freshen", 'F', POPT_BIT_SET, &installInterfaceFlags,
	(INSTALL_UPGRADE|INSTALL_FRESHEN),
	N_("upgrade package(s) if already installed"),
	N_("<packagefile>+") },
 { "hash", 'h', POPT_BIT_SET, &installInterfaceFlags, INSTALL_HASH,
	N_("print hash marks as package installs (good with -v)"), NULL},
 { "ignorearch", '\0', POPT_BIT_SET, &probFilter, RPMPROB_FILTER_IGNOREARCH,
	N_("don't verify package architecture"), NULL},
 { "ignoreos", '\0', POPT_BIT_SET, &probFilter, RPMPROB_FILTER_IGNOREOS,
	N_("don't verify package operating system"), NULL},
 { "ignoresize", '\0', POPT_BIT_SET, &probFilter,
	(RPMPROB_FILTER_DISKSPACE|RPMPROB_FILTER_DISKNODES),
	N_("don't check disk space before installing"), NULL},
 { "includedocs", '\0', 0, &incldocs, 0,
	N_("install documentation"), NULL},
 { "install", '\0', 0, 0, GETOPT_INSTALL,
	N_("install package"), N_("<packagefile>+") },
 { "justdb", '\0', POPT_BIT_SET, &transFlags, RPMTRANS_FLAG_JUSTDB,
	N_("update the database, but do not modify the filesystem"), NULL},
 { "nodeps", '\0', 0, &noDeps, 0,
	N_("do not verify package dependencies"), NULL },
 { "noorder", '\0', POPT_BIT_SET, &installInterfaceFlags, INSTALL_NOORDER,
	N_("do not reorder package installation to satisfy dependencies"),
	NULL},

 { "noscripts", '\0', POPT_BIT_SET, &transFlags,
	(_noTransScripts|_noTransTriggers),
	N_("do not execute package scriptlet(s)"), NULL },
 { "nopre", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	RPMTRANS_FLAG_NOPRE,
	N_("do not execute %%pre scriptlet (if any)"), NULL },
 { "nopost", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	RPMTRANS_FLAG_NOPOST,
	N_("do not execute %%post scriptlet (if any)"), NULL },
 { "nopreun", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	RPMTRANS_FLAG_NOPREUN,
	N_("do not execute %%preun scriptlet (if any)"), NULL },
 { "nopostun", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	RPMTRANS_FLAG_NOPOSTUN,
	N_("do not execute %%postun scriptlet (if any)"), NULL },

 { "notriggers", '\0', POPT_BIT_SET, &transFlags,
	_noTransTriggers,
	N_("do not execute any scriptlet(s) triggered by this package"), NULL},
 { "notriggerprein", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	RPMTRANS_FLAG_NOTRIGGERPREIN,
	N_("do not execute any %%triggerprein scriptlet(s)"), NULL},
 { "notriggerin", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	RPMTRANS_FLAG_NOTRIGGERIN,
	N_("do not execute any %%triggerin scriptlet(s)"), NULL},
 { "notriggerun", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	RPMTRANS_FLAG_NOTRIGGERUN,
	N_("do not execute any %%triggerun scriptlet(s)"), NULL},
 { "notriggerpostun", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &transFlags,
	RPMTRANS_FLAG_NOTRIGGERPOSTUN,
	N_("do not execute any %%triggerpostun scriptlet(s)"), NULL},

 { "oldpackage", '\0', POPT_BIT_SET, &probFilter, RPMPROB_FILTER_OLDPACKAGE,
	N_("upgrade to an old version of the package (--force on upgrades does this automatically)"),
	NULL},
 { "percent", '\0', POPT_BIT_SET, &installInterfaceFlags, INSTALL_PERCENT,
	N_("print percentages as package installs"), NULL},
 { "prefix", '\0', POPT_ARG_STRING, &prefix, 0,
	N_("relocate the package to <dir>, if relocatable"),
	N_("<dir>") },
 { "relocate", '\0', POPT_ARG_STRING, 0, GETOPT_RELOCATE,
	N_("relocate files from path <old> to <new>"),
	N_("<old>=<new>") },
 { "repackage", '\0', POPT_BIT_SET, &transFlags, RPMTRANS_FLAG_REPACKAGE,
	N_("save erased package files by repackaging"), NULL},
 { "replacefiles", '\0', POPT_BIT_SET, &probFilter,
	(RPMPROB_FILTER_REPLACEOLDFILES | RPMPROB_FILTER_REPLACENEWFILES),
	N_("install even if the package replaces installed files"), NULL},
 { "replacepkgs", '\0', POPT_BIT_SET, &probFilter, RPMPROB_FILTER_REPLACEPKG,
	N_("reinstall if the package is already present"), NULL},
 { "test", '\0', POPT_BIT_SET, &transFlags, RPMTRANS_FLAG_TEST,
	N_("don't install, but tell if it would work or not"), NULL},
 { "upgrade", 'U', POPT_BIT_SET, &installInterfaceFlags, INSTALL_UPGRADE,
	N_("upgrade package(s)"),
	N_("<packagefile>+") },

   POPT_TABLEEND
};
#endif	/* IAM_RPMEIU */
#endif	/* DYING */

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {
#if !defined(_AUTOHELP)
 { "help", '\0', 0, &help, 0,			NULL, NULL},
#endif

 /* XXX colliding options */
#if defined(IAM_RPMQV) || defined(IAM_RPMEIU) || defined(IAM_RPMBT)
 {  NULL, 'i', POPT_ARGFLAG_DOC_HIDDEN, 0, 'i',			NULL, NULL},
#endif

#ifdef	IAM_RPMQV
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQueryPoptTable, 0,
	N_("Query options (with -q or --query):"),
	NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmVerifyPoptTable, 0,
	N_("Verify options (with -V or --verify):"),
	NULL },
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMK
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmSignPoptTable, 0,
	N_("Signature options:"),
	NULL },
#endif	/* IAM_RPMK */

#ifdef	IAM_RPMDB
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmDatabasePoptTable, 0,
	N_("Database options:"),
	NULL },
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMBT
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmBuildPoptTable, 0,
	N_("Build options with [ <specfile> | <tarball> | <source package> ]:"),
	NULL },
#endif	/* IAM_RPMBT */

#ifdef	IAM_RPMEIU
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
	N_("Install/Upgrade/Erase options:"),
	NULL },
#endif	/* IAM_RPMEIU */

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmAllPoptTable, 0,
	N_("Common options for all rpm modes:"),
	NULL },

   POPT_AUTOHELP
   POPT_TABLEEND
};

#ifdef __MINT__
/* MiNT cannot dynamically increase the stack.  */
long _stksize = 64 * 1024L;
#endif

/*@exits@*/ static void argerror(const char * desc)
	/*@modifies fileSystem @*/
{
    fprintf(stderr, _("%s: %s\n"), __progname, desc);
    exit(EXIT_FAILURE);
}

static void printVersion(void)
	/*@modifies fileSystem @*/
{
    fprintf(stdout, _("RPM version %s\n"), rpmEVR);
}

static void printBanner(void)
	/*@modifies fileSystem @*/
{
    (void) puts(_("Copyright (C) 1998-2000 - Red Hat, Inc."));
    (void) puts(_("This program may be freely redistributed under the terms of the GNU GPL"));
}

static void printUsage(void)
	/*@modifies fileSystem @*/
{
    FILE * fp;
    printVersion();
    printBanner();
    (void) puts("");

    fp = stdout;

    fprintf(fp, _("Usage: %s {--help}\n"), __progname);
    fprintf(fp,  ("       %s {--version}\n"), __progname);

#ifdef	IAM_RPMEIU
#ifdef	DYING
--dbpath	all
--ftpproxy etc	all
--force		alias for --replacepkgs --replacefiles
--includedocs	handle as option in table
		--erase forbids many options
#endif
#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMQV
#ifdef	DYING	/* XXX popt glue needing --help doco. */
--dbpath	all
--ftpproxy etc	all
-i,--info	Q
-R,--requires	Q
-P,--provides	Q
--scripts	Q
--triggeredby	Q
--changelog	Q
--triggers	Q
--querytags	!V
--setperms	V
--setugids	V
#endif	/* DYING */
#endif	/* IAM_RPMQV */

}

#ifdef	DYING
static void printHelpLine(char * prefix, char * help)
	/*@modifies fileSystem @*/
{
    int indentLength = strlen(prefix) + 3;
    int lineLength = 79 - indentLength;
    int helpLength = strlen(help);
    char * ch;
    char format[10];

    fprintf(stdout, "%s - ", prefix);

    while (helpLength > lineLength) {
	ch = help + lineLength - 1;
	while (ch > help && !isspace(*ch)) ch--;
	if (ch == help) break;		/* give up */
	while (ch > (help + 1) && isspace(*ch)) ch--;
	ch++;

	sprintf(format, "%%.%ds\n%%%ds", (int) (ch - help), indentLength);
	fprintf(stdout, format, help, " ");
	help = ch;
	while (isspace(*help) && *help) help++;
	helpLength = strlen(help);
    }

    if (helpLength) puts(help);
}

static void printHelp(void) {
    printVersion();
    printBanner();
    puts("");

    puts(         _("Usage:"));
    printHelpLine(  "   --help                 ", 
		  _("print this message"));
    printHelpLine(  "   --version              ",
		  _("print the version of rpm being used"));

    puts("");
    puts(         _("  All modes support the following options:"));
    printHelpLine(_("   --define '<name> <body>'"),
		  _("define macro <name> with value <body>"));
    printHelpLine(_("   --eval '<expr>+'       "),
		  _("print the expansion of macro <expr> to stdout"));
    printHelpLine(_("   --pipe <cmd>           "),
		  _("send stdout to <cmd>"));
    printHelpLine(_("   --rcfile <file:...>    "),
		  _("use <file:...> instead of default list of macro files"));
    printHelpLine(  "   --showrc               ",
		  _("display final rpmrc and macro configuration"));
#if defined(IAM_RPMBT) || defined(IAM_RPMDB) || defined(IAM_RPMEIU) || defined(IAM_RPMQV)
    printHelpLine(_("   --dbpath <dir>         "),
		  _("use <dir> as the directory for the database"));
    printHelpLine(_("   --root <dir>           "),
		  _("use <dir> as the top level directory"));
#endif	/* IAM_RPMBT || IAM_RPMDB || IAM_RPMEIU || IAM_RPMQV */
    printHelpLine(  "   -v                     ",
		  _("be a little more verbose"));
    printHelpLine(  "   -vv                    ",
		  _("be incredibly verbose (for debugging)"));

#if defined(IAM_RPMEIU) || defined(IAM_RPMQV)
    puts("");
    puts(         _("  Install, upgrade and query (with -p) modes allow URL's to be used in place"));
    puts(         _("  of file names as well as the following options:"));
    printHelpLine(_("     --ftpproxy <host>    "),
		  _("hostname or IP of ftp proxy"));
    printHelpLine(_("     --ftpport <port>     "),
		  _("port number of ftp server (or proxy)"));
    printHelpLine(_("     --httpproxy <host>   "),
		  _("hostname or IP of http proxy"));
    printHelpLine(_("     --httpport <port>    "),
		  _("port number of http server (or proxy)"));
#endif	/* IAM_RPMEIU || IAM_RPMQV */

#ifdef IAM_RPMQV
    puts("");
    puts(         _("  Package specification options:"));
    printHelpLine(  "     -a, --all            ",
		  _("query/verify all packages"));
    printHelpLine(_("     -f <file>+           "),
		  _("query/verify package owning <file>"));
    printHelpLine(_("     -p <packagefile>+    "),
		  _("query/verify (uninstalled) package <packagefile>"));
    printHelpLine(_("     --triggeredby <pkg>  "),
		  _("query/verify packages triggered by <pkg>"));
    printHelpLine(_("     --whatprovides <cap> "),
		  _("query/verify packages which provide <cap> capability"));
    printHelpLine(_("     --whatrequires <cap> "),
		  _("query/verify packages which require <cap> capability"));
    puts("");
    printHelpLine(  "  -q, --query             ",
		  _("query mode"));
    printHelpLine(_("     --queryformat <qfmt> "),
		  _("use <qfmt> as the header format (implies --info)"));
    puts("");
    puts(         _("    Information selection options:"));
    printHelpLine(  "       -i, --info         ",
		  _("display package information"));
    printHelpLine(  "       --changelog        ",
		  _("display the package's change log"));
    printHelpLine(  "       -l                 ",
		  _("display package file list"));
    printHelpLine(  "       -s                 ",
		  _("show file states (implies -l)"));
    printHelpLine(  "       -d                 ",
		  _("list only documentation files (implies -l)"));
    printHelpLine(  "       -c                 ",
		  _("list only configuration files (implies -l)"));
    printHelpLine(  "       --dump             ",
		  _("show all verifiable information for each file (must be used with -l, -c, or -d)"));
    printHelpLine(  "       --provides         ",
		  _("list capabilities provided by package"));
    printHelpLine(  "       -R, --requires     ",
		  _("list capabilities required by package"));
    printHelpLine(  "       --scripts          ",
		  _("print the various [un]install scriptlets"));
    printHelpLine(  "       --triggers         ",
		  _("show the trigger scriptlets contained in the package"));
    puts("");
    printHelpLine(  "   -V, -y, --verify       ",
		  _("verify a package installation using the same same package specification options as -q"));
    printHelpLine(  "     --nodeps             ",
		  _("do not verify package dependencies"));
    printHelpLine(  "     --nofiles            ",
		  _("do not verify file attributes"));
    printHelpLine(  "     --nomd5              ",
		  _("do not verify file md5 checksums"));
    printHelpLine(  "     --noscripts          ",
		  _("do not execute scripts (if any)"));
    puts("");
    printHelpLine(  "   --querytags            ",
		  _("list the tags that can be used in a query format"));
    printHelpLine(  "   --setperms             ",
		  _("set the file permissions to those in the package database"
		    " using the same package specification options as -q"));
    printHelpLine(  "   --setugids             ",
		  _("set the file owner and group to those in the package "
		    "database using the same package specification options as "
		    "-q"));
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMEIU
    puts("");
    puts(         _("   --install <packagefile>"));
    printHelpLine(_("   -i <packagefile>       "),
		  _("install package"));
    printHelpLine(  "     --allfiles           ",
		  _("install all files, even configurations which might "
		    "otherwise be skipped"));
    printHelpLine(  "     --badreloc           ",
		  _("relocate files in non-relocateable package"));
    printHelpLine(  "     --excludedocs        ",
		  _("do not install documentation"));
    printHelpLine(_("     --excludepath <path> "),
		  _("skip files with leading component <path> "));
    printHelpLine(  "     --force              ",
		  _("short hand for --replacepkgs --replacefiles"));
    printHelpLine(  "     -h, --hash           ",
		  _("print hash marks as package installs (good with -v)"));
    printHelpLine(  "     --ignorearch         ",
		  _("don't verify package architecture"));
    printHelpLine(  "     --ignoresize         ",
		  _("don't check disk space before installing"));
    printHelpLine(  "     --ignoreos           ",
		  _("don't verify package operating system"));
    printHelpLine(  "     --includedocs        ",
		  _("install documentation"));
    printHelpLine(  "     --justdb             ",
		  _("update the database, but do not modify the filesystem"));
    printHelpLine(  "     --nodeps             ",
		  _("do not verify package dependencies"));
    printHelpLine(  "     --noorder            ",
		  _("do not reorder package installation to satisfy dependencies"));
    printHelpLine(  "     --noscripts          ",
		  _("don't execute any installation scriptlets"));
    printHelpLine(  "     --notriggers         ",
		  _("don't execute any scriptlets triggered by this package"));
    printHelpLine(  "     --percent            ",
		  _("print percentages as package installs"));
    printHelpLine(_("     --prefix <dir>       "),
		  _("relocate the package to <dir>, if relocatable"));
    printHelpLine(_("     --relocate <oldpath>=<newpath>"),
		  _("relocate files from <oldpath> to <newpath>"));
    printHelpLine(  "     --replacefiles       ",
		  _("install even if the package replaces installed files"));
    printHelpLine(  "     --replacepkgs        ",
		  _("reinstall if the package is already present"));
    printHelpLine(  "     --test               ",
		  _("don't install, but tell if it would work or not"));
    puts("");
    puts(         _("   --upgrade <packagefile>"));
    printHelpLine(_("   -U <packagefile>       "),
		  _("upgrade package (same options as --install, plus)"));
    printHelpLine(  "     --oldpackage         ",
		  _("upgrade to an old version of the package (--force on upgrades does this automatically)"));
    puts("");
    puts(         _("   --erase <package>"));
    printHelpLine(_("   -e <package>           "),
		  _("erase (uninstall) package"));
    printHelpLine(  "     --allmatches         ",
		  _("remove all packages which match <package> (normally an error is generated if <package> specified multiple packages)"));
    printHelpLine(  "     --justdb             ",
		  _("update the database, but do not modify the filesystem"));
    printHelpLine(  "     --nodeps             ",
		  _("do not verify package dependencies"));
    printHelpLine(  "     --noorder            ",
		  _("do not reorder package installation to satisfy dependencies"));
    printHelpLine(  "     --noscripts          ",
		  _("do not execute any package specific scripts"));
    printHelpLine(  "     --notriggers         ",
		  _("don't execute any scripts triggered by this package"));
#endif	/* IAM_RPMEIU */

#ifdef IAM_RPMK
    puts("");
    printHelpLine(_("   --resign <pkg>+        "),
		  _("sign a package (discard current signature)"));
    printHelpLine(_("   --addsign <pkg>+       "),
		  _("add a signature to a package"));

    puts(         _("   --checksig <pkg>+"));
    printHelpLine(_("   -K <pkg>+             "),
		  _("verify package signature"));
    printHelpLine(  "     --nopgp              ",
		  _("skip any PGP signatures"));
    printHelpLine(  "     --nogpg              ",
		  _("skip any GPG signatures"));
    printHelpLine(  "     --nomd5              ",
		  _("skip any MD5 signatures"));
#endif	/* IAM_RPMK */

#ifdef	IAM_RPMDB
    puts("");
    printHelpLine(  "   --initdb               ",
		  _("initalize database (unnecessary, legacy use)"));
    printHelpLine(  "   --rebuilddb            ",
		  _("rebuild database indices from existing database headers"));
#endif

}
#endif

int main(int argc, const char ** argv)
{
    enum modes bigMode = MODE_UNKNOWN;

#ifdef	IAM_RPMQV
    QVA_t qva = &rpmQVArgs;
#endif

#ifdef	IAM_RPMBT
    BTA_t ba = &rpmBTArgs;
#endif

#ifdef	IAM_RPMEIU
#ifdef	DYING
/*@only@*/ rpmRelocation * relocations = NULL;
    int numRelocations = 0;
#else
   struct rpmInstallArguments_s * ia = &rpmIArgs;
#endif
#endif

#if defined(IAM_RPMDB)
   struct rpmDatabaseArguments_s * da = &rpmDBArgs;
#endif

#if defined(IAM_RPMK)
#ifdef	DYING
    rpmResignFlags addSign = RESIGN_NEW_SIGNATURE;
    rpmCheckSigFlags checksigFlags = CHECKSIG_NONE;
#else
   struct rpmSignArguments_s * ka = &rpmKArgs;
#endif
#endif

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
    char * passPhrase = "";
#endif

    int arg;
    int gotDbpath = 0;

    const char * optArg;
    pid_t pipeChild = 0;
    poptContext optCon;
    int ec = 0;
    int status;
    int p[2];
	
#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();	/* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);	/* Retrofit glibc __progname */

    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }

    /* Set the major mode based on argv[0] */
    /*@-nullpass@*/
#ifdef	IAM_RPMBT
    if (!strcmp(__progname, "rpmb"))	bigMode = MODE_BUILD;
    if (!strcmp(__progname, "rpmt"))	bigMode = MODE_TARBUILD;
    if (!strcmp(__progname, "rpmbuild"))	bigMode = MODE_BUILD;
#endif
#ifdef	IAM_RPMQV
    if (!strcmp(__progname, "rpmq"))	bigMode = MODE_QUERY;
    if (!strcmp(__progname, "rpmv"))	bigMode = MODE_VERIFY;
    if (!strcmp(__progname, "rpmquery"))	bigMode = MODE_QUERY;
    if (!strcmp(__progname, "rpmverify"))	bigMode = MODE_VERIFY;
#endif
#ifdef	RPMEIU
    if (!strcmp(__progname, "rpme"))	bigMode = MODE_ERASE;
    if (!strcmp(__progname, "rpmi"))	bigMode = MODE_INSTALL;
    if (!strcmp(__progname, "rpmu"))	bigMode = MODE_INSTALL;
#endif
    /*@=nullpass@*/

    /* set the defaults for the various command line options */
    _ftp_debug = 0;

#if HAVE_LIBIO_H && defined(_G_IO_IO_FILE_VERSION)
    noLibio = 0;
#else
    noLibio = 1;
#endif
    _rpmio_debug = 0;
    _url_debug = 0;

    /* XXX Eliminate query linkage loop */
    specedit = 0;
    parseSpecVec = parseSpec;
    freeSpecVec = freeSpec;

    /* set up the correct locale */
    (void) setlocale(LC_ALL, "" );

#ifdef	__LCLINT__
#define	LOCALEDIR	"/usr/share/locale"
#endif
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    rpmSetVerbosity(RPMMESS_NORMAL);	/* XXX silly use by showrc */

    /* Make a first pass through the arguments, looking for --rcfile */
    /* We need to handle that before dealing with the rest of the arguments. */
    /*@-nullpass -temptrans@*/
    optCon = poptGetContext(__progname, argc, argv, optionsTable, 0);
    /*@=nullpass =temptrans@*/
    (void) poptReadConfigFile(optCon, LIBRPMALIAS_FILENAME);
    (void) poptReadDefaultConfig(optCon, 1);
    poptSetExecPath(optCon, RPMCONFIGDIR, 1);

    /* reading rcfile early makes it easy to override */
    /* XXX only --rcfile (and --showrc) need this pre-parse */

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	switch(arg) {
	case 'v':
	    rpmIncreaseVerbosity();	/* XXX silly use by showrc */
	    break;
        default:
	    break;
      }
    }

    if (rpmReadConfigFiles(rcfile, NULL))  
	exit(EXIT_FAILURE);

    if (showrc) {
	(void) rpmShowRC(stdout);
	exit(EXIT_SUCCESS);
    }

    rpmSetVerbosity(RPMMESS_NORMAL);	/* XXX silly use by showrc */

    poptResetContext(optCon);

#ifdef	IAM_RPMQV
    qva->qva_queryFormat = _free(qva->qva_queryFormat);
    memset(qva, 0, sizeof(*qva));
    qva->qva_source = RPMQV_PACKAGE;
    qva->qva_mode = ' ';
    qva->qva_char = ' ';
#endif

#ifdef	IAM_RPMBT
    ba->buildRootOverride = _free(ba->buildRootOverride);
    ba->targets = _free(ba->targets);
    memset(ba, 0, sizeof(*ba));
    ba->buildMode = ' ';
    ba->buildChar = ' ';
#endif

#ifdef	IAM_RPMDB
    memset(da, 0, sizeof(*da));
#endif

#ifdef	IAM_RPMK
    memset(ka, 0, sizeof(*ka));
    ka->addSign = RESIGN_CHK_SIGNATURE;
    ka->checksigFlags = CHECKSIG_ALL;
#endif

#ifdef	IAM_RPMEIU
#ifdef DYING
    transFlags = RPMTRANS_FLAG_NONE;
    probFilter = RPMPROB_FILTER_NONE;
    installInterfaceFlags = INSTALL_NONE;
    eraseInterfaceFlags = UNINSTALL_NONE;
#else
    ia->relocations = _free(ia->relocations);
    memset(ia, 0, sizeof(*ia));
    ia->transFlags = RPMTRANS_FLAG_NONE;
    ia->probFilter = RPMPROB_FILTER_NONE;
    ia->installInterfaceFlags = INSTALL_NONE;
    ia->eraseInterfaceFlags = UNINSTALL_NONE;
#endif
#endif

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	switch (arg) {
	    
	case 'v':
	    rpmIncreaseVerbosity();
	    break;

#if defined(IAM_RPMQV) || defined(IAM_RPMEIU) || defined(IAM_RPMBT)
	case 'i':
#ifdef	IAM_RPMQV
	    if (bigMode == MODE_QUERY) {
		/*@-nullassign -readonlytrans@*/
		const char * infoCommand[] = { "--info", NULL };
		/*@=nullassign =readonlytrans@*/
		(void) poptStuffArgs(optCon, infoCommand);
	    }
#endif
#ifdef	IAM_RPMEIU
	    if (bigMode == MODE_INSTALL)
		/*@-ifempty@*/ ;
	    if (bigMode == MODE_UNKNOWN) {
		/*@-nullassign -readonlytrans@*/
		const char * installCommand[] = { "--install", NULL };
		/*@=nullassign =readonlytrans@*/
		(void) poptStuffArgs(optCon, installCommand);
	    }
#endif
	    break;
#endif	/* IAM_RPMQV || IAM_RPMEIU || IAM_RPMBT */

#ifdef	DYING
#ifdef	IAM_RPMEIU
	
	case 'e':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_ERASE)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_ERASE;
	    break;
	
	case GETOPT_INSTALL:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_INSTALL)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_INSTALL;
	    break;

#ifdef	DYING	/* XXX handled by popt */
	case 'U':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_INSTALL)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_INSTALL;
	    break;

	case 'F':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_INSTALL)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_INSTALL;
	    break;
#endif

	case GETOPT_EXCLUDEPATH:
	    if (optArg == NULL || *optArg != '/') 
		argerror(_("exclude paths must begin with a /"));

	    ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
	    /*@-observertrans -dependenttrans@*/	/* FIX: W2DO? */
	    ia->relocations[ia->numRelocations].oldPath = optArg;
	    ia->relocations[ia->numRelocations++].newPath = NULL;
	    /*@=observertrans =dependenttrans@*/
	    break;

	case GETOPT_RELOCATE:
	  { char * newPath = NULL;
	    if (optArg == NULL || *optArg != '/') 
		argerror(_("relocations must begin with a /"));
	    if (!(newPath = strchr(optArg, '=')))
		argerror(_("relocations must contain a ="));
	    *newPath++ = '\0';
	    if (*newPath != '/') 
		argerror(_("relocations must have a / following the ="));
	    ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
	    /*@-observertrans -kepttrans@*/	/* FIX: W2DO? */
	    ia->relocations[ia->numRelocations].oldPath = optArg;
	    ia->relocations[ia->numRelocations++].newPath = newPath;
	    /*@=observertrans =kepttrans@*/
	  } break;
#endif	/* IAM_RPMEIU */
#endif	/* DYING */

#ifdef	DYING
#ifdef	IAM_RPMDB
	case GETOPT_REBUILDDB:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_REBUILDDB)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_REBUILDDB;
	    break;
	case GETOPT_VERIFYDB:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_VERIFYDB)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_VERIFYDB;
	    break;
#endif	/* IAM_RPMDB */
#endif	/* DYING */

#ifdef	DYING
#ifdef	IAM_RPMK
	case 'K':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_CHECKSIG)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_CHECKSIG;
	    break;

	case GETOPT_RESIGN:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_RESIGN)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_RESIGN;
	    addSign = RESIGN_NEW_SIGNATURE;
	    signIt = 1;
	    break;

	case GETOPT_ADDSIGN:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_RESIGN)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_RESIGN;
	    addSign = RESIGN_ADD_SIGNATURE;
	    signIt = 1;
	    break;
#endif	/* IAM_RPMK */
#endif	/* DYING */

	case GETOPT_DEFINEMACRO:
	    if (optArg) {
		(void) rpmDefineMacro(NULL, optArg, RMIL_CMDLINE);
		(void) rpmDefineMacro(&rpmCLIMacroContext, optArg,RMIL_CMDLINE);
	    }
	    noUsageMsg = 1;
	    break;

	case GETOPT_EVALMACRO:
	    if (optArg) {
		const char *val = rpmExpand(optArg, NULL);
		fprintf(stdout, "%s\n", val);
		val = _free(val);
	    }
	    noUsageMsg = 1;
	    break;

#if defined(GETOPT_RCFILE)
	case GETOPT_RCFILE:
	    fprintf(stderr, _("The --rcfile option has been eliminated.\n"));
	    fprintf(stderr, _("Use \"--macros <file:...>\" instead.\n"));
	    exit(EXIT_FAILURE);
	    /*@notreached@*/ break;
#endif

	default:
	    fprintf(stderr, _("Internal error in argument processing (%d) :-(\n"), arg);
	    exit(EXIT_FAILURE);
	}
    }

    if (quiet)
	rpmSetVerbosity(RPMMESS_QUIET);

    if (showVersion) printVersion();

    if (arg < -1) {
	fprintf(stderr, "%s: %s\n", 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(arg));
	exit(EXIT_FAILURE);
    }

#ifdef	IAM_RPMBT
    switch (ba->buildMode) {
    case 'b':	bigMode = MODE_BUILD;		break;
    case 't':	bigMode = MODE_TARBUILD;	break;
    case 'B':	bigMode = MODE_REBUILD;		break;
    case 'C':	bigMode = MODE_RECOMPILE;	break;
    }

    if ((ba->buildAmount & RPMBUILD_RMSOURCE) && bigMode == MODE_UNKNOWN)
	bigMode = MODE_BUILD;

    if ((ba->buildAmount & RPMBUILD_RMSPEC) && bigMode == MODE_UNKNOWN)
	bigMode = MODE_BUILD;

    if (ba->buildRootOverride && bigMode != MODE_BUILD &&
	bigMode != MODE_REBUILD && bigMode != MODE_TARBUILD) {
	argerror("--buildroot may only be used during package builds");
    }
#endif	/* IAM_RPMBT */
    
#ifdef	IAM_RPMDB
    if (da->init) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_INITDB;
    } else
    if (da->rebuild) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_REBUILDDB;
    } else
    if (da->verify) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_VERIFYDB;
    }
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMQV
    switch (qva->qva_mode) {
    case 'q':	bigMode = MODE_QUERY;		break;
    case 'V':	bigMode = MODE_VERIFY;		break;
    case 'Q':	bigMode = MODE_QUERYTAGS;	break;
    }

    if (qva->qva_sourceCount) {
	if (qva->qva_sourceCount > 2)
	    argerror(_("one type of query/verify may be performed at a "
			"time"));
    }
    if (qva->qva_flags && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query flags"));

    if (qva->qva_queryFormat && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query format"));

    if (qva->qva_source != RPMQV_PACKAGE && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query source"));
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMEIU
    {	int iflags = (ia->installInterfaceFlags &
		(INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_NOERASE));
	int eflags = (ia->installInterfaceFlags & INSTALL_UPGRADE);

	if (iflags & eflags)
	    argerror(_("only one major mode may be specified"));
	else if (iflags)
	    bigMode = MODE_INSTALL;
	else if (eflags)
	    bigMode = MODE_ERASE;
    }
#endif	/* IAM_RPMQV */

    if (gotDbpath && (bigMode & ~MODES_FOR_DBPATH))
	argerror(_("--dbpath given for operation that does not use a "
			"database"));

#if defined(IAM_RPMEIU)
    if (!( bigMode == MODE_INSTALL ) &&
(ia->probFilter & (RPMPROB_FILTER_REPLACEPKG | RPMPROB_FILTER_REPLACEOLDFILES | RPMPROB_FILTER_REPLACENEWFILES | RPMPROB_FILTER_OLDPACKAGE)))
	argerror(_("only installation, upgrading, rmsource and rmspec may be forced"));
    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_FORCERELOCATE))
	argerror(_("files may only be relocated during package installation"));

    if (ia->relocations && ia->prefix)
	argerror(_("only one of --prefix or --relocate may be used"));

    if (bigMode != MODE_INSTALL && ia->relocations)
	argerror(_("--relocate and --excludepath may only be used when installing new packages"));

    if (bigMode != MODE_INSTALL && ia->prefix)
	argerror(_("--prefix may only be used when installing new packages"));

    if (ia->prefix && ia->prefix[0] != '/') 
	argerror(_("arguments to --prefix must begin with a /"));

    if (bigMode != MODE_INSTALL && (ia->installInterfaceFlags & INSTALL_HASH))
	argerror(_("--hash (-h) may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && (ia->installInterfaceFlags & INSTALL_PERCENT))
	argerror(_("--percent may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL &&
 (ia->probFilter & (RPMPROB_FILTER_REPLACEOLDFILES|RPMPROB_FILTER_REPLACENEWFILES)))
	argerror(_("--replacefiles may only be specified during package "
			"installation"));

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

    if (bigMode != MODE_INSTALL &&
	(ia->probFilter & (RPMPROB_FILTER_DISKSPACE|RPMPROB_FILTER_DISKNODES)))
	argerror(_("--ignoresize may only be specified during package "
		   "installation"));

    if ((ia->eraseInterfaceFlags & UNINSTALL_ALLMATCHES) && bigMode != MODE_ERASE)
	argerror(_("--allmatches may only be specified during package "
		   "erasure"));

    if ((ia->transFlags & RPMTRANS_FLAG_ALLFILES) && bigMode != MODE_INSTALL)
	argerror(_("--allfiles may only be specified during package "
		   "installation"));

    if ((ia->transFlags & RPMTRANS_FLAG_JUSTDB) &&
	bigMode != MODE_INSTALL && bigMode != MODE_ERASE)
	argerror(_("--justdb may only be specified during package "
		   "installation and erasure"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE &&
	(ia->transFlags & (RPMTRANS_FLAG_NOSCRIPTS | _noTransScripts | _noTransTriggers)))
	argerror(_("script disabling options may only be specified during "
		   "package installation and erasure"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE &&
	(ia->transFlags & (RPMTRANS_FLAG_NOTRIGGERS | _noTransTriggers)))
	argerror(_("trigger disabling options may only be specified during "
		   "package installation and erasure"));

    if (ia->noDeps & (bigMode & ~MODES_FOR_NODEPS))
	argerror(_("--nodeps may only be specified during package "
		   "building, rebuilding, recompilation, installation,"
		   "erasure, and verification"));

    if ((ia->transFlags & RPMTRANS_FLAG_TEST) && (bigMode & ~MODES_FOR_TEST))
	argerror(_("--test may only be specified during package installation, "
		 "erasure, and building"));
#endif	/* IAM_RPMEIU */

    if (rootdir && rootdir[1] && (bigMode & ~MODES_FOR_ROOT))
	argerror(_("--root (-r) may only be specified during "
		 "installation, erasure, querying, and "
		 "database rebuilds"));

    if (rootdir) {
	switch (urlIsURL(rootdir)) {
	default:
	    if (bigMode & MODES_FOR_ROOT)
		break;
	    /*@fallthrough@*/
	case URL_IS_UNKNOWN:
	    if (rootdir[0] != '/')
		argerror(_("arguments to --root (-r) must begin with a /"));
	    break;
	}
    }

#ifdef	DYING
#ifdef	IAM_RPMK
    if (!(ka->checksigFlags & CHECKSIG_PGP) && bigMode != MODE_CHECKSIG)
	argerror(_("--nopgp may only be used during signature checking"));

    if (!(ka->checksigFlags & CHECKSIG_GPG) && bigMode != MODE_CHECKSIG)
	argerror(_("--nogpg may only be used during signature checking"));
#endif

#if defined(IAM_RPMK) || defined(IAM_RPMQV)
    if ((!(ka->checksigFlags & CHECKSIG_MD5) || noMd5) && bigMode != MODE_CHECKSIG && bigMode != MODE_VERIFY)
	argerror(_("--nomd5 may only be used during signature checking and "
		   "package verification"));
#endif
#endif	/* DYING */

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
    if (0
#if defined(IAM_RPMBT)
    || ba->sign 
#endif
#if defined(IAM_RPMK)
    || ka->sign
#endif
    ) {
        if (bigMode == MODE_REBUILD || bigMode == MODE_BUILD ||
	    bigMode == MODE_RESIGN || bigMode == MODE_TARBUILD) {
	    const char ** av;
	    struct stat sb;
	    int errors = 0;

	    if ((av = poptGetArgs(optCon)) == NULL) {
		fprintf(stderr, _("no files to sign\n"));
		errors++;
	    } else
	    while (*av) {
		if (stat(*av, &sb)) {
		    fprintf(stderr, _("cannot access file %s\n"), *av);
		    errors++;
		}
		av++;
	    }

	    if (errors) {
		ec = errors;
		goto exit;
	    }

            if (poptPeekArg(optCon)) {
		int sigTag;
		switch (sigTag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) {
		  case 0:
		    break;
		  case RPMSIGTAG_PGP:
		    if ((sigTag == RPMSIGTAG_PGP || sigTag == RPMSIGTAG_PGP5) &&
		        !rpmDetectPGPVersion(NULL)) {
		        fprintf(stderr, _("pgp not found: "));
			ec = EXIT_FAILURE;
			goto exit;
		    }	/*@fallthrough@*/
		  case RPMSIGTAG_GPG:
		    passPhrase = rpmGetPassPhrase(_("Enter pass phrase: "), sigTag);
		    if (passPhrase == NULL) {
			fprintf(stderr, _("Pass phrase check failed\n"));
			ec = EXIT_FAILURE;
			goto exit;
		    }
		    fprintf(stderr, _("Pass phrase is good.\n"));
		    passPhrase = xstrdup(passPhrase);
		    break;
		  default:
		    fprintf(stderr,
		            _("Invalid %%_signature spec in macro file.\n"));
		    ec = EXIT_FAILURE;
		    goto exit;
		    /*@notreached@*/ break;
		}
	    }
	} else {
	    argerror(_("--sign may only be used during package building"));
	}
    } else {
    	/* Make rpmLookupSignatureType() return 0 ("none") from now on */
        (void) rpmLookupSignatureType(RPMLOOKUPSIG_DISABLE);
    }
#endif	/* IAM_RPMBT || IAM_RPMK */

    if (pipeOutput) {
	(void) pipe(p);

	if (!(pipeChild = fork())) {
	    (void) close(p[1]);
	    (void) dup2(p[0], STDIN_FILENO);
	    (void) close(p[0]);
	    (void) execl("/bin/sh", "/bin/sh", "-c", pipeOutput, NULL);
	    fprintf(stderr, _("exec failed\n"));
	}

	(void) close(p[0]);
	(void) dup2(p[1], STDOUT_FILENO);
	(void) close(p[1]);
    }
	
    switch (bigMode) {
#ifdef	IAM_RPMDB
    case MODE_INITDB:
	(void) rpmdbInit(rootdir, 0644);
	break;

    case MODE_REBUILDDB:
	ec = rpmdbRebuild(rootdir);
	break;
    case MODE_VERIFYDB:
	ec = rpmdbVerify(rootdir);
	break;
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMBT
    case MODE_REBUILD:
    case MODE_RECOMPILE:
      { const char * pkg;
        while (!rpmIsVerbose())
	    rpmIncreaseVerbosity();

	if (!poptPeekArg(optCon))
	    argerror(_("no packages files given for rebuild"));

	ba->buildAmount = RPMBUILD_PREP | RPMBUILD_BUILD | RPMBUILD_INSTALL;
	if (bigMode == MODE_REBUILD) {
	    ba->buildAmount |= RPMBUILD_PACKAGEBINARY;
	    ba->buildAmount |= RPMBUILD_RMSOURCE;
	    ba->buildAmount |= RPMBUILD_RMSPEC;
	    ba->buildAmount |= RPMBUILD_CLEAN;
	    ba->buildAmount |= RPMBUILD_RMBUILD;
	}

	while ((pkg = poptGetArg(optCon))) {
	    const char * specFile = NULL;
	    char * cookie = NULL;

	    ec = rpmInstallSource("", pkg, &specFile, &cookie);
	    if (ec)
		/*@loopbreak@*/ break;

	    ba->rootdir = rootdir;
	    ec = build(specFile, ba, passPhrase, cookie, rcfile);
	    free(cookie);
	    cookie = NULL;
	    free((void *)specFile);
	    specFile = NULL;

	    if (ec)
		/*@loopbreak@*/ break;
	}
      }	break;

      case MODE_BUILD:
      case MODE_TARBUILD:
      { const char * pkg;
        while (!rpmIsVerbose())
	    rpmIncreaseVerbosity();
       
	switch (ba->buildChar) {
	case 'a':
	    ba->buildAmount |= RPMBUILD_PACKAGESOURCE;
	    /*@fallthrough@*/
	case 'b':
	    ba->buildAmount |= RPMBUILD_PACKAGEBINARY;
	    ba->buildAmount |= RPMBUILD_CLEAN;
	    /*@fallthrough@*/
	case 'i':
	    ba->buildAmount |= RPMBUILD_INSTALL;
	    if ((ba->buildChar == 'i') && ba->shortCircuit)
		break;
	    /*@fallthrough@*/
	case 'c':
	    ba->buildAmount |= RPMBUILD_BUILD;
	    if ((ba->buildChar == 'c') && ba->shortCircuit)
		break;
	    /*@fallthrough@*/
	case 'p':
	    ba->buildAmount |= RPMBUILD_PREP;
	    break;
	    
	case 'l':
	    ba->buildAmount |= RPMBUILD_FILECHECK;
	    break;
	case 's':
	    ba->buildAmount |= RPMBUILD_PACKAGESOURCE;
	    break;
	}

	if (!poptPeekArg(optCon)) {
	    if (bigMode == MODE_BUILD)
		argerror(_("no spec files given for build"));
	    else
		argerror(_("no tar files given for build"));
	}

	while ((pkg = poptGetArg(optCon))) {
	    ba->rootdir = rootdir;
	    ec = build(pkg, ba, passPhrase, NULL, rcfile);
	    if (ec)
		/*@loopbreak@*/ break;
	    rpmFreeMacros(NULL);
	    (void) rpmReadConfigFiles(rcfile, NULL);
	}
      }	break;
#endif	/* IAM_RPMBT */

#ifdef	IAM_RPMEIU
    case MODE_ERASE:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for uninstall"));

	if (ia->noDeps) ia->eraseInterfaceFlags |= UNINSTALL_NODEPS;

	ec = rpmErase(rootdir, (const char **)poptGetArgs(optCon), 
			 ia->transFlags, ia->eraseInterfaceFlags);
	break;

    case MODE_INSTALL:
#ifdef	DYING
	if (force) {
	    ia->probFilter |= RPMPROB_FILTER_REPLACEPKG | 
			  RPMPROB_FILTER_REPLACEOLDFILES |
			  RPMPROB_FILTER_REPLACENEWFILES |
			  RPMPROB_FILTER_OLDPACKAGE;
	}
#endif

	/* RPMTRANS_FLAG_BUILD_PROBS */
	/* RPMTRANS_FLAG_KEEPOBSOLETE */

	if (!ia->incldocs) {
	    if (ia->transFlags & RPMTRANS_FLAG_NODOCS)
		;
	    else if (rpmExpandNumeric("%{_excludedocs}"))
		ia->transFlags |= RPMTRANS_FLAG_NODOCS;
	}

	if (ia->noDeps) ia->installInterfaceFlags |= INSTALL_NODEPS;

	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for install"));

	/* we've already ensured !(!ia->prefix && !ia->relocations) */
	if (ia->prefix) {
	    ia->relocations = xmalloc(2 * sizeof(*ia->relocations));
	    ia->relocations[0].oldPath = NULL;   /* special case magic */
	    ia->relocations[0].newPath = ia->prefix;
	    ia->relocations[1].oldPath = ia->relocations[1].newPath = NULL;
	} else if (ia->relocations) {
	    ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
	    ia->relocations[ia->numRelocations].oldPath = NULL;
	    ia->relocations[ia->numRelocations].newPath = NULL;
	}

	ec += rpmInstall(rootdir, (const char **)poptGetArgs(optCon), 
			ia->transFlags, ia->installInterfaceFlags, ia->probFilter,
			ia->relocations);
	break;
#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMQV
    case MODE_QUERY:
      { const char * pkg;

	qva->qva_prefix = rootdir;
	if (qva->qva_source == RPMQV_ALL) {
	    if (poptPeekArg(optCon))
		argerror(_("extra arguments given for query of all packages"));

	    ec = rpmQuery(qva, RPMQV_ALL, NULL);
	} else {
	    if (!poptPeekArg(optCon))
		argerror(_("no arguments given for query"));
	    while ((pkg = poptGetArg(optCon)))
		ec += rpmQuery(qva, qva->qva_source, pkg);
	}
      }	break;

    case MODE_VERIFY:
      { const char * pkg;
	int verifyFlags;

	verifyFlags = (VERIFY_FILES|VERIFY_DEPS|VERIFY_SCRIPT|VERIFY_MD5);
	verifyFlags &= ~qva->qva_flags;

	qva->qva_prefix = rootdir;
	qva->qva_flags = verifyFlags;
	if (qva->qva_source == RPMQV_ALL) {
	    if (poptPeekArg(optCon))
		argerror(_("extra arguments given for verify of all packages"));
	    ec = rpmVerify(qva, RPMQV_ALL, NULL);
	} else {
	    if (!poptPeekArg(optCon))
		argerror(_("no arguments given for verify"));
	    while ((pkg = poptGetArg(optCon)))
		ec += rpmVerify(qva, qva->qva_source, pkg);
	}
      }	break;

    case MODE_QUERYTAGS:
	if (argc != 2)
	    argerror(_("unexpected arguments to --querytags "));

	rpmDisplayQueryTags(stdout);
	break;
#endif	/* IAM_RPMQV */

#ifdef IAM_RPMK
    case MODE_CHECKSIG:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for signature check"));
#ifdef	DYING
	if (!noPgp) checksigFlags |= CHECKSIG_PGP;
	if (!noGpg) checksigFlags |= CHECKSIG_GPG;
	if (!noMd5) checksigFlags |= CHECKSIG_MD5;
#endif
	ec = rpmCheckSig(ka->checksigFlags,
			(const char **)poptGetArgs(optCon));
	/* XXX don't overflow single byte exit status */
	if (ec > 255) ec = 255;
	break;

    case MODE_RESIGN:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for signing"));
	ec = rpmReSign(ka->addSign, passPhrase,
			(const char **)poptGetArgs(optCon));
	/* XXX don't overflow single byte exit status */
	if (ec > 255) ec = 255;
	break;
#endif	/* IAM_RPMK */
	
#if !defined(IAM_RPMQV)
    case MODE_QUERY:
    case MODE_VERIFY:
    case MODE_QUERYTAGS:
#endif
#if !defined(IAM_RPMK)
    case MODE_CHECKSIG:
    case MODE_RESIGN:
#endif
#if !defined(IAM_RPMDB)
    case MODE_INITDB:
    case MODE_REBUILDDB:
    case MODE_VERIFYDB:
#endif
#if !defined(IAM_RPMBT)
    case MODE_BUILD:
    case MODE_REBUILD:
    case MODE_RECOMPILE:
    case MODE_TARBUILD:
#endif
#if !defined(IAM_RPMEIU)
    case MODE_INSTALL:
    case MODE_ERASE:
#endif
    case MODE_UNKNOWN:
	if (!showVersion && !help && !noUsageMsg) printUsage();
	break;
    }

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
exit:
#endif	/* IAM_RPMBT || IAM_RPMK */
    optCon = poptFreeContext(optCon);
    rpmFreeMacros(NULL);
    rpmFreeMacros(&rpmCLIMacroContext);
    rpmFreeRpmrc();

    if (pipeChild) {
	(void) fclose(stdout);
	(void) waitpid(pipeChild, &status, 0);
    }

    /* keeps memory leak checkers quiet */
    freeNames();
    freeFilesystems();
    urlFreeCache();

#ifdef	IAM_RPMQV
    qva->qva_queryFormat = _free(qva->qva_queryFormat);
#endif

#ifdef	IAM_RPMBT
    ba->buildRootOverride = _free(ba->buildRootOverride);
    ba->targets = _free(ba->targets);
#endif

#ifdef	IAM_RPMEIU
    ia->relocations = _free(ia->relocations);
#endif

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    /*@-globstate@*/
    return ec;
    /*@=globstate@*/
}
