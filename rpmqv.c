#include "system.h"

#if defined(IAM_RPM)
#define	IAM_RPMBT
#define	IAM_RPMDB
#define	IAM_RPMEIU
#define	IAM_RPMQV
#define	IAM_RPMK
#endif

#include <rpmbuild.h>
#include <rpmurl.h>

#ifdef	IAM_RPMBT
#include "build.h"
#define GETOPT_REBUILD		1003
#define GETOPT_RECOMPILE	1004
#endif

#ifdef	IAM_RPMDB
#define GETOPT_REBUILDDB        1013
static int initdb = 0;
#endif

#ifdef	IAM_RPMEIU
#include "install.h"
#define GETOPT_INSTALL		1014
#define GETOPT_RELOCATE		1016
#define GETOPT_EXCLUDEPATH	1019
static int allFiles = 0;
static int allMatches = 0;
static int badReloc = 0;
static int excldocs = 0;
static int ignoreArch = 0;
static int ignoreOs = 0;
static int ignoreSize = 0;
static int incldocs = 0;
static int justdb = 0;
static int noOrder = 0;
static int oldPackage = 0;
static char * prefix = NULL;
static int replaceFiles = 0;
static int replacePackages = 0;
static int showHash = 0;
static int showPercents = 0;
static int noTriggers = 0;
#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMK
#define GETOPT_ADDSIGN		1005
#define GETOPT_RESIGN		1006
static int noGpg = 0;
static int noPgp = 0;
#endif	/* IAM_RPMK */

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
#include "signature.h"
#endif

#define GETOPT_DBPATH		1010
#define GETOPT_SHOWRC		1018
#define	GETOPT_DEFINEMACRO	1020
#define	GETOPT_EVALMACRO	1021

enum modes {

    MODE_QUERY		= (1 <<  0),
    MODE_VERIFY		= (1 <<  3),
    MODE_QUERYTAGS	= (1 <<  9),
#define	MODES_QV (MODE_QUERY | MODE_VERIFY)

    MODE_INSTALL	= (1 <<  1),
    MODE_UNINSTALL	= (1 <<  2),
#define	MODES_IE (MODE_INSTALL | MODE_UNINSTALL)

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
#define	MODES_DB (MODE_INITDB | MODE_REBUILDDB)

    MODE_UNKNOWN	= 0
};

#define	MODES_FOR_DBPATH	(MODES_BT | MODES_IE | MODES_QV | MODES_DB)
#define	MODES_FOR_NODEPS	(MODES_BT | MODES_IE | MODE_VERIFY)
#define	MODES_FOR_TEST		(MODES_BT | MODES_IE)
#define	MODES_FOR_ROOT		(MODES_BT | MODES_IE | MODES_QV | MODES_DB)

extern int _ftp_debug;
extern int noLibio;
extern int _rpmio_debug;
extern int _url_debug;
extern int _noDirTokens;

extern const char * rpmNAME;
extern const char * rpmEVR;
extern int rpmFLAGS;

extern MacroContext rpmCLIMacroContext;

/* options for all executables */

static int help = 0;
static int noUsageMsg = 0;
static char * pipeOutput = NULL;
static int quiet = 0;
static char * rcfile = NULL;
static char * rootdir = "/";
static int showrc = 0;
static int showVersion = 0;

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
static int signIt = 0;
#endif	/* IAM_RPMBT || IAM_RPMK */

#if defined(IAM_RPMQV) || defined(IAM_RPMK)
static int noMd5 = 0;
#endif

#if defined(IAM_RPMQV) || defined(IAM_RPMEIU) || defined(IAM_RPMBT)
static int noDeps = 0;
#endif

#if defined(IAM_RPMQV) || defined(IAM_RPMEIU)
static int noScripts = 0;
#endif

#if defined(IAM_RPMEIU) || defined(IAM_RPMBT)
static int force = 0;
static int test = 0;
#endif

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {
 { "help", '\0', 0, &help, 0,			NULL, NULL},
 { "version", '\0', 0, &showVersion, 0,		NULL, NULL},

 { "quiet", '\0', 0, &quiet, 0,			NULL, NULL},
 { "verbose", 'v', 0, 0, 'v',			NULL, NULL},

 { "define", '\0', POPT_ARG_STRING, 0, GETOPT_DEFINEMACRO,NULL, NULL},
 { "eval", '\0', POPT_ARG_STRING, 0, GETOPT_EVALMACRO, NULL, NULL},

 { "nodirtokens", '\0', POPT_ARG_VAL, &_noDirTokens, 1,	NULL, NULL},
 { "dirtokens", '\0', POPT_ARG_VAL, &_noDirTokens, 0,	NULL, NULL},
#if HAVE_LIBIO_H && defined(_IO_BAD_SEEN)
 { "nolibio", '\0', POPT_ARG_VAL, &noLibio, 1,		NULL, NULL},
#endif
 { "ftpdebug", '\0', POPT_ARG_VAL, &_ftp_debug, -1,		NULL, NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL, &_rpmio_debug, -1,		NULL, NULL},
 { "urldebug", '\0', POPT_ARG_VAL, &_url_debug, -1,		NULL, NULL},

#if defined(IAM_RPMEIU) || defined(IAM_RPMBT)
 { "force", '\0', 0, &force, 0,			NULL, NULL},
 { "test", '\0', 0, &test, 0,			NULL, NULL},
#endif

 /* XXX colliding options */
#if defined(IAM_RPMQV) || defined(IAM_RPMEIU) || defined(IAM_RPMBT)
 {  NULL, 'i', 0, 0, 'i',			NULL, NULL},
#endif

 { "pipe", '\0', POPT_ARG_STRING, &pipeOutput, 0,	NULL, NULL},
 { "root", 'r', POPT_ARG_STRING, &rootdir, 0,	NULL, NULL},

 { "rcfile", '\0', POPT_ARG_STRING, &rcfile, 0,	NULL, NULL},
 { "showrc", '\0', 0, &showrc, GETOPT_SHOWRC,	NULL, NULL},

#if defined(IAM_RPMQV) || defined(IAM_RPMK)
 { "nomd5", '\0', 0, &noMd5, 0,			NULL, NULL},
#endif

#if defined(IAM_RPMQV) || defined(IAM_RPMEIU) || defined(IAM_RPMBT)
 { "nodeps", '\0', 0, &noDeps, 0,		NULL, NULL},
#endif
#if defined(IAM_RPMQV) || defined(IAM_RPMEIU)
 { "noscripts", '\0', 0, &noScripts, 0,		NULL, NULL},
#endif

#ifdef	IAM_RPMK
 { "addsign", '\0', 0, 0, GETOPT_ADDSIGN,	NULL, NULL},
 { "checksig", 'K', 0, 0, 'K',			NULL, NULL},
 { "nogpg", '\0', 0, &noGpg, 0,			NULL, NULL},
 { "nopgp", '\0', 0, &noPgp, 0,			NULL, NULL},
 { "resign", '\0', 0, 0, GETOPT_RESIGN,		NULL, NULL},
#endif

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
 { "sign", '\0', 0, &signIt, 0,			NULL, NULL},
#endif	/* IAM_RPMBT || IAM_RPMK */

#ifdef	IAM_RPMDB
 { "initdb", '\0', 0, &initdb, 0,		NULL, NULL},
 { "rebuilddb", '\0', 0, 0, GETOPT_REBUILDDB,	NULL, NULL},
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMEIU
 { "allfiles", '\0', 0, &allFiles, 0,		NULL, NULL},
 { "allmatches", '\0', 0, &allMatches, 0,	NULL, NULL},
 { "badreloc", '\0', 0, &badReloc, 0,		NULL, NULL},
 { "erase", 'e', 0, 0, 'e',			NULL, NULL},
 { "excludedocs", '\0', 0, &excldocs, 0,	NULL, NULL},
 { "excludepath", '\0', POPT_ARG_STRING, 0, GETOPT_EXCLUDEPATH,	NULL, NULL},
 { "hash", 'h', 0, &showHash, 0,		NULL, NULL},
 { "ignorearch", '\0', 0, &ignoreArch, 0,	NULL, NULL},
 { "ignoreos", '\0', 0, &ignoreOs, 0,		NULL, NULL},
 { "ignoresize", '\0', 0, &ignoreSize, 0,	NULL, NULL},
 { "includedocs", '\0', 0, &incldocs, 0,	NULL, NULL},
/* info and install both using 'i' is dumb */
 { "install", '\0', 0, 0, GETOPT_INSTALL,	NULL, NULL},
 { "justdb", '\0', 0, &justdb, 0,		NULL, NULL},
 { "noorder", '\0', 0, &noOrder, 0,		NULL, NULL},
 { "notriggers", '\0', 0, &noTriggers, 0,	NULL, NULL},
 { "oldpackage", '\0', 0, &oldPackage, 0,	NULL, NULL},
 { "percent", '\0', 0, &showPercents, 0,	NULL, NULL},
 { "prefix", '\0', POPT_ARG_STRING, &prefix, 0,	NULL, NULL},
 { "relocate", '\0', POPT_ARG_STRING, 0, GETOPT_RELOCATE,	NULL, NULL},
 { "replacefiles", '\0', 0, &replaceFiles, 0,	NULL, NULL},
 { "replacepkgs", '\0', 0, &replacePackages, 0,	NULL, NULL},
 { "upgrade", 'U', 0, 0, 'U',			NULL, NULL},
 { "uninstall", 'u', 0, 0, 'u',			NULL, NULL},
#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMQV
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, 
		rpmQVSourcePoptTable, 0,	(void *) &rpmQVArgs, NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, 
		rpmQueryPoptTable, 0,		(void *) &rpmQVArgs, NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, 
		rpmVerifyPoptTable, 0,		(void *) &rpmQVArgs, NULL },
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMBT
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, 
		rpmBuildPoptTable, 0,		(void *) &rpmBTArgs, NULL },
#endif	/* IAM_RPMBT */

 { 0, 0, 0, 0, 0,	NULL, NULL }
};

#ifdef __MINT__
/* MiNT cannot dynamically increase the stack.  */
long _stksize = 64 * 1024L;
#endif

static void argerror(const char * desc) {
    fprintf(stderr, _("rpm: %s\n"), desc);
    exit(EXIT_FAILURE);
}

static void printHelp(void);
static void printVersion(void);
static void printBanner(void);
static void printUsage(void);
static void printHelpLine(char * prefix, char * help);

static void printVersion(void) {
    fprintf(stdout, _("RPM version %s\n"), rpmEVR);
}

static void printBanner(void) {
    puts(_("Copyright (C) 1998 - Red Hat Software"));
    puts(_("This may be freely redistributed under the terms of the GNU GPL"));
}

static void printUsage(void) {
    FILE * fp;
    printVersion();
    printBanner();
    puts("");

    fp = stdout;

    fprintf(fp, _("Usage: %s {--help}\n"), __progname);
    fprintf(fp,  ("       %s {--version}\n"), __progname);

#ifdef	IAM_RPMDB
    fprintf(fp, _("       %s {--initdb}   [--dbpath <dir>]\n"), __progname);
    fprintf(fp, _("       %s {--rebuilddb} [--rcfile <file>] [--dbpath <dir>]\n"), __progname);
#endif

#ifdef	IAM_RPMEIU
    fprintf(fp, _("       %s {--install -i} [-v] [--hash -h] [--percent] [--force] [--test]\n"), __progname);
    puts(_("                        [--replacepkgs] [--replacefiles] [--root <dir>]"));
    puts(_("                        [--excludedocs] [--includedocs] [--noscripts]"));
    puts(_("                        [--rcfile <file>] [--ignorearch] [--dbpath <dir>]"));
    puts(_("                        [--prefix <dir>] [--ignoreos] [--nodeps] [--allfiles]"));
    puts(_("                        [--ftpproxy <host>] [--ftpport <port>] [--justdb]"));
    puts(_("                        [--httpproxy <host>] [--httpport <port>] "));
    puts(_("                        [--noorder] [--relocate oldpath=newpath]"));
    puts(_("                        [--badreloc] [--notriggers] [--excludepath <path>]"));
    puts(_("                        [--ignoresize] file1.rpm ... fileN.rpm"));
    fprintf(fp,  ("       %s {--upgrade -U} [-v] [--hash -h] [--percent] [--force] [--test]\n"), __progname);
    puts(_("                        [--oldpackage] [--root <dir>] [--noscripts]"));
    puts(_("                        [--excludedocs] [--includedocs] [--rcfile <file>]"));
    puts(_("                        [--ignorearch]  [--dbpath <dir>] [--prefix <dir>] "));
    puts(_("                        [--ftpproxy <host>] [--ftpport <port>]"));
    puts(_("                        [--httpproxy <host>] [--httpport <port>] "));
    puts(_("                        [--ignoreos] [--nodeps] [--allfiles] [--justdb]"));
    puts(_("                        [--noorder] [--relocate oldpath=newpath]"));
    puts(_("                        [--badreloc] [--excludepath <path>] [--ignoresize]"));
    puts(_("                        file1.rpm ... fileN.rpm"));
    fprintf(fp, _("       %s {--erase -e} [--root <dir>] [--noscripts] [--rcfile <file>]\n"), __progname);
    puts(_("                        [--dbpath <dir>] [--nodeps] [--allmatches]"));
    puts(_("                        [--justdb] [--notriggers] rpackage1 ... packageN"));
#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMQV
    fprintf(fp,  ("       %s {--query -q} [-afpg] [-i] [-l] [-s] [-d] [-c] [-v] [-R]\n"), __progname);
    puts(_("                        [--scripts] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--whatprovides] [--whatrequires] [--requires]"));
    puts(_("                        [--triggeredby] [--ftpuseport] [--ftpproxy <host>]"));
    puts(_("                        [--httpproxy <host>] [--httpport <port>] "));
    puts(_("                        [--ftpport <port>] [--provides] [--triggers] [--dump]"));
    puts(_("                        [--changelog] [--dbpath <dir>] [targets]"));
    fprintf(fp, _("       %s {--verify -V -y} [-afpg] [--root <dir>] [--rcfile <file>]\n"), __progname);
    puts(_("                        [--dbpath <dir>] [--nodeps] [--nofiles] [--noscripts]"));
    puts(_("                        [--nomd5] [targets]"));
    fprintf(fp,  ("       %s {--querytags}\n"), __progname);
    fprintf(fp, _("       %s {--setperms} [-afpg] [target]\n"), __progname);
    fprintf(fp, _("       %s {--setugids} [-afpg] [target]\n"), __progname);
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMBT
    fprintf(fp, _("       %s {-b|t}[plcibas] [-v] [--short-circuit] [--clean] [--rcfile  <file>]\n"), __progname);
    puts( ("                        [--sign] [--nobuild] ]"));
    puts(_("                        [--target=platform1[,platform2...]]"));
    puts(_("                        [--rmsource] [--rmspec] specfile"));
    fprintf(fp, _("       %s {--rmsource} [--rcfile <file>] [-v] specfile\n"), __progname);
    fprintf(fp, _("       %s {--rebuild} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm\n"), __progname);
    fprintf(fp, _("       %s {--recompile} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm\n"), __progname);
    fprintf(fp, _("       %s {--freshen -F} file1.rpm ... fileN.rpm\n"), __progname);
#endif	/* IAM_RPMBT */

#ifdef	IAM_RPMK
    fprintf(fp, _("       %s {--resign} [--rcfile <file>] package1 package2 ... packageN\n"), __progname);
    fprintf(fp, _("       %s {--addsign} [--rcfile <file>] package1 package2 ... packageN"), __progname);
    fprintf(fp, _("       %s {--checksig -K} [--nopgp] [--nogpg] [--nomd5] [--rcfile <file>]\n"), __progname);
    puts(_("                           package1 ... packageN"));
#endif	/* IAM_RPMK */

}

static void printHelpLine(char * prefix, char * help) {
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
    printHelpLine(_("   --eval '<name>+'       "),
		  _("print the expansion of macro <name> to stdout"));
    printHelpLine(_("   --pipe <cmd>           "),
		  _("send stdout to <cmd>"));
    printHelpLine(_("   --rcfile <file>        "),
		  _("use <file> instead of /etc/rpmrc and $HOME/.rpmrc"));
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
    printHelpLine(  "     -a, --all           ",
		  _("query/verify all packages"));
    printHelpLine(_("     -f <file>+          "),
		  _("query/verify package owning <file>"));
    printHelpLine(_("     -p <packagefile>+   "),
		  _("query/verify (uninstalled) package <packagefile>"));
    printHelpLine(_("     --triggeredby <pkg> "),
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
    printHelpLine(  "     --nomd5              ",
		  _("do not verify file md5 checksums"));
    printHelpLine(  "     --nofiles            ",
		  _("do not verify file attributes"));
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
    printHelpLine(  "   -e <package>           ",
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

#ifdef	IAM_RPMBT
    puts("");
    puts(         _("   -b<stage> <spec>       "));
    printHelpLine(_("   -t<stage> <tarball>    "),
		  _("build package, where <stage> is one of:"));
    printHelpLine(  "         p                ",
		  _("prep (unpack sources and apply patches)"));
    printHelpLine(  "         l                ",
		  _("list check (do some cursory checks on %files)"));
    printHelpLine(  "         c                ",
		  _("compile (prep and compile)"));
    printHelpLine(  "         i                ",
		  _("install (prep, compile, install)"));
    printHelpLine(  "         b                ",
		  _("binary package (prep, compile, install, package)"));
    printHelpLine(  "         a                ",
		  _("bin/src package (prep, compile, install, package)"));
    printHelpLine(  "         s                ",
		  _("package src rpm only"));
    printHelpLine(  "     --short-circuit      ",
		  _("skip straight to specified stage (only for c,i)"));
    printHelpLine(  "     --clean              ",
		  _("remove build tree when done"));
    printHelpLine(  "     --rmsource           ",
		  _("remove sources when done"));
    printHelpLine(  "     --rmspec             ",
		  _("remove spec file when done"));
    printHelpLine(  "     --sign               ",
		  _("generate PGP/GPG signature"));
    printHelpLine(_("     --buildroot <dir>    "),
		  _("use <dir> as the build root"));
    printHelpLine(_("     --target=<platform>+ "),
		  _("build the packages for the build targets platform1...platformN."));
    printHelpLine(  "     --nobuild            ",
		  _("do not execute any stages"));
    puts("");
    printHelpLine(_("   --rebuild <src_pkg>    "),
		  _("install source package, build binary package and remove spec file, sources, patches, and icons."));
    printHelpLine(_("   --recompile <src_pkg>  "),
		  _("like --rebuild, but don't build any package"));
#endif	/* IAM_RPMBT */

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

int main(int argc, const char ** argv)
{
    enum modes bigMode = MODE_UNKNOWN;

#ifdef	IAM_RPMQV
    QVA_t *qva = &rpmQVArgs;
#endif

#ifdef	IAM_RPMBT
    struct rpmBuildArguments *ba = &rpmBTArgs;
#endif

#ifdef	IAM_RPMEIU
    rpmRelocation * relocations = NULL;
    int numRelocations = 0;
    int installFlags = 0, uninstallFlags = 0, interfaceFlags = 0;
    int probFilter = 0;
    int upgrade = 0;
#endif

#if defined(IAM_RPMK)
    int addSign = NEW_SIGNATURE;
    int checksigFlags = 0;
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
#ifdef	IAM_RPMBT
    if (!strcmp(__progname, "rpmb"))	bigMode = MODE_BUILD;
    if (!strcmp(__progname, "rpmt"))	bigMode = MODE_TARBUILD;
#endif
#ifdef	IAM_RPMQV
    if (!strcmp(__progname, "rpmq"))	bigMode = MODE_QUERY;
    if (!strcmp(__progname, "rpmv"))	bigMode = MODE_VERIFY;
#endif
#ifdef	RPMEIU
    if (!strcmp(__progname, "rpme"))	bigMode = MODE_UNINSTALL;
    if (!strcmp(__progname, "rpmi"))	bigMode = MODE_INSTALL;
    if (!strcmp(__progname, "rpmu"))	bigMode = MODE_INSTALL;
#endif

    /* set the defaults for the various command line options */
    _ftp_debug = 0;

#if HAVE_LIBIO_H && defined(_IO_BAD_SEEN)
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
    setlocale(LC_ALL, "" );

#ifdef	__LCLINT__
#define	LOCALEDIR	"/usr/share/locale"
#endif
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    rpmSetVerbosity(RPMMESS_NORMAL);	/* XXX silly use by showrc */

    /* Make a first pass through the arguments, looking for --rcfile */
    /* We need to handle that before dealing with the rest of the arguments. */
    optCon = poptGetContext(__progname, argc, argv, optionsTable, 0);
    poptReadConfigFile(optCon, LIBRPMALIAS_FILENAME);
    poptReadDefaultConfig(optCon, 1);
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
	rpmShowRC(stdout);
	exit(EXIT_SUCCESS);
    }

    rpmSetVerbosity(RPMMESS_NORMAL);	/* XXX silly use by showrc */

    poptResetContext(optCon);

#ifdef	IAM_RPMQV
    if (qva->qva_queryFormat) xfree(qva->qva_queryFormat);
    memset(qva, 0, sizeof(*qva));
    qva->qva_source = RPMQV_PACKAGE;
    qva->qva_mode = ' ';
    qva->qva_char = ' ';
#endif

#ifdef	IAM_RPMBT
    if (ba->buildRootOverride) xfree(ba->buildRootOverride);
    if (ba->targets) free(ba->targets);
    memset(ba, 0, sizeof(*ba));
    ba->buildMode = ' ';
    ba->buildChar = ' ';
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
		const char * infoCommand[] = { "--info", NULL };
		poptStuffArgs(optCon, infoCommand);
	    }
#endif
#ifdef	IAM_RPMEIU
	    if (bigMode == MODE_INSTALL)
		/*@-ifempty@*/ ;
	    if (bigMode == MODE_UNKNOWN) {
		const char * installCommand[] = { "--install", NULL };
		poptStuffArgs(optCon, installCommand);
	    }
#endif
	    break;
#endif	/* IAM_RPMQV || IAM_RPMEIU || IAM_RPMBT */

#ifdef	IAM_RPMEIU
	  case 'u':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_UNINSTALL)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_UNINSTALL;
	    rpmMessage(RPMMESS_ERROR, _("-u and --uninstall are deprecated and no"
		    " longer work.\n"));
	    rpmMessage(RPMMESS_ERROR, _("Use -e or --erase instead.\n"));
	    exit(EXIT_FAILURE);
	
	  case 'e':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_UNINSTALL)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_UNINSTALL;
	    break;
	
	  case GETOPT_INSTALL:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_INSTALL)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_INSTALL;
	    break;

	  case 'U':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_INSTALL)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_INSTALL;
	    upgrade = 1;
	    break;

	  case GETOPT_EXCLUDEPATH:
	    if (*optArg != '/') 
		argerror(_("exclude paths must begin with a /"));

	    relocations = xrealloc(relocations, 
				  sizeof(*relocations) * (numRelocations + 1));
	    relocations[numRelocations].oldPath = optArg;
	    relocations[numRelocations++].newPath = NULL;
	    break;

	  case GETOPT_RELOCATE:
	  { char * errString = NULL;
	    if (*optArg != '/') 
		argerror(_("relocations must begin with a /"));
	    if (!(errString = strchr(optArg, '=')))
		argerror(_("relocations must contain a ="));
	    *errString++ = '\0';
	    if (*errString != '/') 
		argerror(_("relocations must have a / following the ="));
	    relocations = xrealloc(relocations, 
				  sizeof(*relocations) * (numRelocations + 1));
	    relocations[numRelocations].oldPath = optArg;
	    relocations[numRelocations++].newPath = errString;
	  } break;
#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMDB
	  case GETOPT_REBUILDDB:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_REBUILDDB)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_REBUILDDB;
	    break;
#endif

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
	    addSign = NEW_SIGNATURE;
	    signIt = 1;
	    break;

	  case GETOPT_ADDSIGN:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_RESIGN)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_RESIGN;
	    addSign = ADD_SIGNATURE;
	    signIt = 1;
	    break;
#endif	/* IAM_RPMK */

	  case GETOPT_DEFINEMACRO:
	    rpmDefineMacro(NULL, optArg, RMIL_CMDLINE);
	    rpmDefineMacro(&rpmCLIMacroContext, optArg, RMIL_CMDLINE);
	    noUsageMsg = 1;
	    break;

	  case GETOPT_EVALMACRO:
	  { const char *val = rpmExpand(optArg, NULL);
	    fprintf(stdout, "%s\n", val);
	    xfree(val);
	    noUsageMsg = 1;
	  } break;

	  default:
	    fprintf(stderr, _("Internal error in argument processing (%d) :-(\n"), arg);
	    exit(EXIT_FAILURE);
	}
    }

    if (quiet)
	rpmSetVerbosity(RPMMESS_QUIET);

    if (showVersion) printVersion();
    if (help) printHelp();

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
    if (initdb) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_INITDB;
    }
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMQV
    switch (qva->qva_mode) {
    case 'q':	bigMode = MODE_QUERY;		break;
    case 'V':	bigMode = MODE_VERIFY;		break;
    case 'Q':	bigMode = MODE_QUERYTAGS;	break;
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
#endif

    if (gotDbpath && (bigMode & ~MODES_FOR_DBPATH))
	argerror(_("--dbpath given for operation that does not use a "
			"database"));

#if defined(IAM_RPMEIU) || defined(IAM_RPMBT)

    if (!(
#ifdef	IAM_RPMEIU
	 bigMode == MODE_INSTALL ||
#endif
#ifdef	IAM_RPMBT
	 (bigMode==MODE_BUILD && (ba->buildAmount & RPMBUILD_RMSOURCE))||
	 (bigMode==MODE_BUILD && (ba->buildAmount & RPMBUILD_RMSPEC))
#else
	0
#endif
	) && force)
	argerror(_("only installation, upgrading, rmsource and rmspec may be forced"));
#endif	/* IAM_RPMEIU || IAM_RPMBT */

#ifdef	IAM_RPMEIU
    if (bigMode != MODE_INSTALL && badReloc)
	argerror(_("files may only be relocated during package installation"));

    if (relocations && prefix)
	argerror(_("only one of --prefix or --relocate may be used"));

    if (bigMode != MODE_INSTALL && relocations)
	argerror(_("--relocate and --excludepath may only be used when installing new packages"));

    if (bigMode != MODE_INSTALL && prefix)
	argerror(_("--prefix may only be used when installing new packages"));

    if (prefix && prefix[0] != '/') 
	argerror(_("arguments to --prefix must begin with a /"));

    if (bigMode != MODE_INSTALL && showHash)
	argerror(_("--hash (-h) may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && showPercents)
	argerror(_("--percent may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && replaceFiles)
	argerror(_("--replacefiles may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && replacePackages)
	argerror(_("--replacepkgs may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && excldocs)
	argerror(_("--excludedocs may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && incldocs)
	argerror(_("--includedocs may only be specified during package "
		   "installation"));

    if (excldocs && incldocs)
	argerror(_("only one of --excludedocs and --includedocs may be "
		 "specified"));
  
    if (bigMode != MODE_INSTALL && ignoreArch)
	argerror(_("--ignorearch may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && ignoreOs)
	argerror(_("--ignoreos may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && ignoreSize)
	argerror(_("--ignoresize may only be specified during package "
		   "installation"));

    if (allMatches && bigMode != MODE_UNINSTALL)
	argerror(_("--allmatches may only be specified during package "
		   "erasure"));

    if (allFiles && bigMode != MODE_INSTALL)
	argerror(_("--allfiles may only be specified during package "
		   "installation"));

    if (justdb && bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL)
	argerror(_("--justdb may only be specified during package "
		   "installation and erasure"));
#endif	/* IAM_RPMEIU */

#if defined(IAM_RPMQV) || defined(IAM_RPMEIU)
    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_VERIFY && noScripts)
	argerror(_("--noscripts may only be specified during package "
		   "installation, erasure, and verification"));
#endif	/* IAM_RPMQV || IAM_RPMEIU */

#if defined(IAM_RPMEIU)
    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && noTriggers)
	argerror(_("--notriggers may only be specified during package "
		   "installation and erasure"));
#endif	/* IAM_RPMEIU */

#if defined(IAM_RPMBT) || defined(IAM_RPMEIU)
    if (noDeps & (bigMode & ~MODES_FOR_NODEPS))
	argerror(_("--nodeps may only be specified during package "
		   "building, rebuilding, recompilation, installation,"
		   "erasure, and verification"));

    if (test && (bigMode & ~MODES_FOR_TEST))
	argerror(_("--test may only be specified during package installation, "
		 "erasure, and building"));
#endif	/* IAM_RPMBT || IAM_RPMEIU */

    if (rootdir[1] && (bigMode & ~MODES_FOR_ROOT))
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

#ifdef	IAM_RPMEIU
    if (oldPackage && !upgrade)
	argerror(_("--oldpackage may only be used during upgrades"));
#endif

#ifdef	IAM_RPMK
    if (noPgp && bigMode != MODE_CHECKSIG)
	argerror(_("--nopgp may only be used during signature checking"));

    if (noGpg && bigMode != MODE_CHECKSIG)
	argerror(_("--nogpg may only be used during signature checking"));
#endif

#if defined(IAM_RPMK) || defined(IAM_RPMQV)
    if (noMd5 && bigMode != MODE_CHECKSIG && bigMode != MODE_VERIFY)
	argerror(_("--nomd5 may only be used during signature checking and "
		   "package verification"));
#endif

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
    if (signIt) {
        if (bigMode == MODE_REBUILD || bigMode == MODE_BUILD ||
	    bigMode == MODE_RESIGN || bigMode == MODE_TARBUILD) {
	    const char ** argv;
	    struct stat sb;
	    int errors = 0;

	    if ((argv = poptGetArgs(optCon)) == NULL) {
		fprintf(stderr, _("no files to sign\n"));
		errors++;
	    } else
	    while (*argv) {
		if (stat(*argv, &sb)) {
		    fprintf(stderr, _("cannot access file %s\n"), *argv);
		    errors++;
		}
		argv++;
	    }

	    if (errors) return errors;

            if (poptPeekArg(optCon)) {
		int sigTag;
		switch (sigTag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) {
		  case 0:
		    break;
		  case RPMSIGTAG_PGP:
		    if ((sigTag == RPMSIGTAG_PGP || sigTag == RPMSIGTAG_PGP5) &&
		        !rpmDetectPGPVersion(NULL)) {
		        fprintf(stderr, _("pgp not found: "));
			exit(EXIT_FAILURE);
		    }	/*@fallthrough@*/
		  case RPMSIGTAG_GPG:
		    passPhrase = rpmGetPassPhrase(_("Enter pass phrase: "), sigTag);
		    if (passPhrase == NULL) {
			fprintf(stderr, _("Pass phrase check failed\n"));
			exit(EXIT_FAILURE);
		    }
		    fprintf(stderr, _("Pass phrase is good.\n"));
		    passPhrase = xstrdup(passPhrase);
		    break;
		  default:
		    fprintf(stderr,
		            _("Invalid %%_signature spec in macro file.\n"));
		    exit(EXIT_FAILURE);
		    /*@notreached@*/ break;
		}
	    }
	} else {
	    argerror(_("--sign may only be used during package building"));
	}
    } else {
    	/* Make rpmLookupSignatureType() return 0 ("none") from now on */
        rpmLookupSignatureType(RPMLOOKUPSIG_DISABLE);
    }
#endif	/* IAM_RPMBT || IAM_RPMK */

    if (pipeOutput) {
	pipe(p);

	if (!(pipeChild = fork())) {
	    close(p[1]);
	    dup2(p[0], STDIN_FILENO);
	    close(p[0]);
	    execl("/bin/sh", "/bin/sh", "-c", pipeOutput, NULL);
	    fprintf(stderr, _("exec failed\n"));
	}

	close(p[0]);
	dup2(p[1], STDOUT_FILENO);
	close(p[1]);
    }
	
    switch (bigMode) {
#ifdef	IAM_RPMDB
      case MODE_INITDB:
	rpmdbInit(rootdir, 0644);
	break;

      case MODE_REBUILDDB:
	ec = rpmdbRebuild(rootdir);
	break;
      case MODE_QUERY:
      case MODE_VERIFY:
      case MODE_QUERYTAGS:
      case MODE_INSTALL:
      case MODE_UNINSTALL:
      case MODE_BUILD:
      case MODE_REBUILD:
      case MODE_RECOMPILE:
      case MODE_TARBUILD:
      case MODE_CHECKSIG:
      case MODE_RESIGN:
	if (!showVersion && !help && !noUsageMsg) printUsage();
	break;
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMBT
      case MODE_REBUILD:
      case MODE_RECOMPILE:
      { const char * pkg;
        if (rpmGetVerbosity() == RPMMESS_NORMAL)
	    rpmSetVerbosity(RPMMESS_VERBOSE);

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
		break;

	    ba->rootdir = rootdir;
	    ec = build(specFile, ba, passPhrase, 0, cookie, rcfile, force, noDeps);
	    free(cookie);
	    cookie = NULL;
	    xfree(specFile);
	    specFile = NULL;

	    if (ec)
		break;
	}
      }	break;

      case MODE_BUILD:
      case MODE_TARBUILD:
      { const char * pkg;
        if (rpmGetVerbosity() == RPMMESS_NORMAL)
	    rpmSetVerbosity(RPMMESS_VERBOSE);
       
	switch (ba->buildChar) {
	  /* these fallthroughs are intentional */
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
	    ec = build(pkg, ba, passPhrase, bigMode == MODE_TARBUILD,
			NULL, rcfile, force, noDeps);
	    if (ec)
		break;
	    rpmFreeMacros(NULL);
	    rpmReadConfigFiles(rcfile, NULL);
	}
      }	break;

      case MODE_QUERY:
      case MODE_VERIFY:
      case MODE_QUERYTAGS:
      case MODE_INSTALL:
      case MODE_UNINSTALL:
      case MODE_CHECKSIG:
      case MODE_RESIGN:
      case MODE_INITDB:
      case MODE_REBUILDDB:
	if (!showVersion && !help && !noUsageMsg) printUsage();
	break;
#endif	/* IAM_RPMBT */

#ifdef	IAM_RPMEIU
      case MODE_UNINSTALL:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for uninstall"));

	if (noScripts) uninstallFlags |= RPMTRANS_FLAG_NOSCRIPTS;
	if (noTriggers) uninstallFlags |= RPMTRANS_FLAG_NOTRIGGERS;
	if (test) uninstallFlags |= RPMTRANS_FLAG_TEST;
	if (justdb) uninstallFlags |= RPMTRANS_FLAG_JUSTDB;
	if (noDeps) interfaceFlags |= UNINSTALL_NODEPS;
	if (allMatches) interfaceFlags |= UNINSTALL_ALLMATCHES;

	ec = rpmErase(rootdir, (const char **)poptGetArgs(optCon), 
			 uninstallFlags, interfaceFlags);
	break;

      case MODE_INSTALL:
	if (force) {
	    probFilter |= RPMPROB_FILTER_REPLACEPKG | 
			  RPMPROB_FILTER_REPLACEOLDFILES |
			  RPMPROB_FILTER_REPLACENEWFILES |
			  RPMPROB_FILTER_OLDPACKAGE;
	}
	if (replaceFiles) probFilter |= RPMPROB_FILTER_REPLACEOLDFILES |
			                RPMPROB_FILTER_REPLACENEWFILES;
	if (badReloc) probFilter |= RPMPROB_FILTER_FORCERELOCATE;
	if (replacePackages) probFilter |= RPMPROB_FILTER_REPLACEPKG;
	if (oldPackage) probFilter |= RPMPROB_FILTER_OLDPACKAGE;
	if (ignoreArch) probFilter |= RPMPROB_FILTER_IGNOREARCH; 
	if (ignoreOs) probFilter |= RPMPROB_FILTER_IGNOREOS;
	if (ignoreSize) probFilter |= RPMPROB_FILTER_DISKSPACE;

	if (test) installFlags |= RPMTRANS_FLAG_TEST;
	/* RPMTRANS_FLAG_BUILD_PROBS */
	if (noScripts) installFlags |= RPMTRANS_FLAG_NOSCRIPTS;
	if (justdb) installFlags |= RPMTRANS_FLAG_JUSTDB;
	if (noTriggers) installFlags |= RPMTRANS_FLAG_NOTRIGGERS;
	if (!incldocs) {
	    if (excldocs)
		installFlags |= RPMTRANS_FLAG_NODOCS;
	    else if (rpmExpandNumeric("%{_excludedocs}"))
		installFlags |= RPMTRANS_FLAG_NODOCS;
	}
	if (allFiles) installFlags |= RPMTRANS_FLAG_ALLFILES;
	/* RPMTRANS_FLAG_KEEPOBSOLETE */

	if (showPercents) interfaceFlags |= INSTALL_PERCENT;
	if (showHash) interfaceFlags |= INSTALL_HASH;
	if (noDeps) interfaceFlags |= INSTALL_NODEPS;
	if (noOrder) interfaceFlags |= INSTALL_NOORDER;
	if (upgrade) interfaceFlags |= INSTALL_UPGRADE;

	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for install"));

	/* we've already ensured !(!prefix && !relocations) */
	if (prefix) {
	    relocations = alloca(2 * sizeof(*relocations));
	    relocations[0].oldPath = NULL;   /* special case magic */
	    relocations[0].newPath = prefix;
	    relocations[1].oldPath = relocations[1].newPath = NULL;
	} else if (relocations) {
	    relocations = xrealloc(relocations, 
				  sizeof(*relocations) * (numRelocations + 1));
	    relocations[numRelocations].oldPath = NULL;
	    relocations[numRelocations].newPath = NULL;
	}

	ec += rpmInstall(rootdir, (const char **)poptGetArgs(optCon), 
			installFlags, interfaceFlags, probFilter, relocations);
	break;
      case MODE_QUERY:
      case MODE_VERIFY:
      case MODE_QUERYTAGS:
      case MODE_BUILD:
      case MODE_REBUILD:
      case MODE_RECOMPILE:
      case MODE_TARBUILD:
      case MODE_CHECKSIG:
      case MODE_RESIGN:
      case MODE_INITDB:
      case MODE_REBUILDDB:
	if (!showVersion && !help && !noUsageMsg) printUsage();
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
	if (noDeps)	verifyFlags &= ~VERIFY_DEPS;
	if (noScripts)	verifyFlags &= ~VERIFY_SCRIPT;
	if (noMd5)	verifyFlags &= ~VERIFY_MD5;

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

      case MODE_INSTALL:
      case MODE_UNINSTALL:
      case MODE_BUILD:
      case MODE_REBUILD:
      case MODE_RECOMPILE:
      case MODE_TARBUILD:
      case MODE_CHECKSIG:
      case MODE_RESIGN:
      case MODE_INITDB:
      case MODE_REBUILDDB:
	if (!showVersion && !help && !noUsageMsg) printUsage();
	break;
#endif	/* IAM_RPMQV */

#ifdef IAM_RPMK
      case MODE_CHECKSIG:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for signature check"));
	if (!noPgp) checksigFlags |= CHECKSIG_PGP;
	if (!noGpg) checksigFlags |= CHECKSIG_GPG;
	if (!noMd5) checksigFlags |= CHECKSIG_MD5;
	ec = rpmCheckSig(checksigFlags, (const char **)poptGetArgs(optCon));
	/* XXX don't overflow single byte exit status */
	if (ec > 255) ec = 255;
	break;

      case MODE_RESIGN:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for signing"));
	ec = rpmReSign(addSign, passPhrase, (const char **)poptGetArgs(optCon));
	/* XXX don't overflow single byte exit status */
	if (ec > 255) ec = 255;
	break;
      case MODE_QUERY:
      case MODE_VERIFY:
      case MODE_QUERYTAGS:
      case MODE_INSTALL:
      case MODE_UNINSTALL:
      case MODE_BUILD:
      case MODE_REBUILD:
      case MODE_RECOMPILE:
      case MODE_TARBUILD:
      case MODE_INITDB:
      case MODE_REBUILDDB:
	if (!showVersion && !help && !noUsageMsg) printUsage();
	break;
#endif	/* IAM_RPMK */
	
      case MODE_UNKNOWN:
	if (!showVersion && !help && !noUsageMsg) printUsage();
	break;

    }

    poptFreeContext(optCon);
    rpmFreeMacros(NULL);
    rpmFreeMacros(&rpmCLIMacroContext);
    rpmFreeRpmrc();

    if (pipeChild) {
	fclose(stdout);
	(void)waitpid(pipeChild, &status, 0);
    }

    /* keeps memory leak checkers quiet */
    freeNames();
    freeFilesystems();
    urlFreeCache();

#ifdef	IAM_RPMQV
    if (qva->qva_queryFormat) xfree(qva->qva_queryFormat);
#endif

#ifdef	IAM_RPMBT
    if (ba->buildRootOverride) xfree(ba->buildRootOverride);
    if (ba->targets) free(ba->targets);
#endif

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    return ec;
}
