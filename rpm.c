#include "system.h"

#include <rpmbuild.h>
#include <rpmurl.h>

#include "build.h"
#include "install.h"
#include "signature.h"

#define GETOPT_ADDSIGN		1005
#define GETOPT_RESIGN		1006
#define GETOPT_DBPATH		1010
#define GETOPT_REBUILDDB        1013
#define GETOPT_INSTALL		1014
#define GETOPT_RELOCATE		1016
#define GETOPT_SHOWRC		1018
#define GETOPT_EXCLUDEPATH	1019
#define	GETOPT_DEFINEMACRO	1020
#define	GETOPT_EVALMACRO	1021

enum modes {
    MODE_UNKNOWN	= 0,
    MODE_QUERY		= (1 <<  0),
    MODE_INSTALL	= (1 <<  1),
    MODE_UNINSTALL	= (1 <<  2),
    MODE_VERIFY		= (1 <<  3),
    MODE_BUILD		= (1 <<  4),
    MODE_REBUILD	= (1 <<  5),
    MODE_CHECKSIG	= (1 <<  6),
    MODE_RESIGN		= (1 <<  7),
    MODE_RECOMPILE	= (1 <<  8),
    MODE_QUERYTAGS	= (1 <<  9),
    MODE_INITDB		= (1 << 10),
    MODE_TARBUILD	= (1 << 11),
    MODE_REBUILDDB	= (1 << 12)
};

#define	MODES_QV (MODE_QUERY | MODE_VERIFY)
#define	MODES_BT (MODE_BUILD | MODE_TARBUILD | MODE_REBUILD | MODE_RECOMPILE)
#define	MODES_IE (MODE_INSTALL | MODE_UNINSTALL)
#define	MODES_DB (MODE_INITDB | MODE_REBUILDDB)
#define	MODES_K	 (MODE_CHECKSIG | MODES_RESIGN)

#define	MODES_FOR_DBPATH	(MODES_BT | MODES_IE | MODES_QV | MODES_DB)
#define	MODES_FOR_NODEPS	(MODES_BT | MODES_IE | MODE_VERIFY)
#define	MODES_FOR_TEST		(MODES_BT | MODES_IE)
#define	MODES_FOR_ROOT		(MODES_BT | MODES_IE | MODES_QV | MODES_DB)

/* the flags for the various options */
static int allFiles;
static int allMatches;
static int badReloc;
static int excldocs;
static int force;
extern int _ftp_debug;
static int showHash;
static int help;
static int ignoreArch;
static int ignoreOs;
static int ignoreSize;
static int incldocs;
static int initdb;
static int justdb;
static int noDeps;
static int noGpg;
extern int noLibio;
static int noMd5;
static int noOrder;
static int noPgp;
static int noScripts;
static int noTriggers;
static int noUsageMsg;
static int oldPackage;
static char * pipeOutput;
static char * prefix;
static int quiet;
static char * rcfile;
static int replaceFiles;
static int replacePackages;
static char * rootdir;
extern int _rpmio_debug;
static int showPercents;
static int showrc;
static int signIt;
static int test;
extern int _url_debug;
extern int _noDirTokens;
extern int _useDbiMajor;

static int showVersion;
extern const char * rpmNAME;
extern const char * rpmEVR;
extern int rpmFLAGS;

extern MacroContext rpmCLIMacroContext;

static struct rpmQVArguments rpmQVArgs;

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {
 { "addsign", '\0', 0, 0, GETOPT_ADDSIGN,	NULL, NULL},
/* all and allmatches both using 'a' is dumb */
 { "all", 'a', 0, 0, 'a',			NULL, NULL},
 { "allfiles", '\0', 0, &allFiles, 0,		NULL, NULL},
 { "allmatches", '\0', 0, &allMatches, 0,	NULL, NULL},
 { "badreloc", '\0', 0, &badReloc, 0,		NULL, NULL},
 { "checksig", 'K', 0, 0, 'K',			NULL, NULL},
 { "define", '\0', POPT_ARG_STRING, 0, GETOPT_DEFINEMACRO,NULL, NULL},
 { "dirtokens", '\0', POPT_ARG_VAL, &_noDirTokens, 0,	NULL, NULL},
 { "erase", 'e', 0, 0, 'e',			NULL, NULL},
 { "eval", '\0', POPT_ARG_STRING, 0, GETOPT_EVALMACRO, NULL, NULL},
 { "excludedocs", '\0', 0, &excldocs, 0,	NULL, NULL},
 { "excludepath", '\0', POPT_ARG_STRING, 0, GETOPT_EXCLUDEPATH,	NULL, NULL},
 { "force", '\0', 0, &force, 0,			NULL, NULL},
 { "ftpdebug", '\0', POPT_ARG_VAL, &_ftp_debug, -1,		NULL, NULL},
 { "hash", 'h', 0, &showHash, 0,		NULL, NULL},
 { "help", '\0', 0, &help, 0,			NULL, NULL},
 {  NULL, 'i', 0, 0, 'i',			NULL, NULL},
 { "ignorearch", '\0', 0, &ignoreArch, 0,	NULL, NULL},
 { "ignoreos", '\0', 0, &ignoreOs, 0,		NULL, NULL},
 { "ignoresize", '\0', 0, &ignoreSize, 0,	NULL, NULL},
 { "includedocs", '\0', 0, &incldocs, 0,	NULL, NULL},
 { "initdb", '\0', 0, &initdb, 0,		NULL, NULL},
/* info and install both using 'i' is dumb */
 { "install", '\0', 0, 0, GETOPT_INSTALL,	NULL, NULL},
 { "justdb", '\0', 0, &justdb, 0,		NULL, NULL},
 { "nodeps", '\0', 0, &noDeps, 0,		NULL, NULL},
 { "nodirtokens", '\0', POPT_ARG_VAL, &_noDirTokens, 1,	NULL, NULL},
 { "nogpg", '\0', 0, &noGpg, 0,			NULL, NULL},
#if HAVE_LIBIO_H && defined(_IO_BAD_SEEN)
 { "nolibio", '\0', POPT_ARG_VAL, &noLibio, 1,		NULL, NULL},
#endif
 { "nomd5", '\0', 0, &noMd5, 0,			NULL, NULL},
 { "noorder", '\0', 0, &noOrder, 0,		NULL, NULL},
 { "nopgp", '\0', 0, &noPgp, 0,			NULL, NULL},
 { "noscripts", '\0', 0, &noScripts, 0,		NULL, NULL},
 { "notriggers", '\0', 0, &noTriggers, 0,	NULL, NULL},
 { "oldpackage", '\0', 0, &oldPackage, 0,	NULL, NULL},
 { "percent", '\0', 0, &showPercents, 0,	NULL, NULL},
 { "pipe", '\0', POPT_ARG_STRING, &pipeOutput, 0,	NULL, NULL},
 { "prefix", '\0', POPT_ARG_STRING, &prefix, 0,	NULL, NULL},
#ifdef	DYING
 { "query", 'q', 0, NULL, 'q',			NULL, NULL},
 { "querytags", '\0', 0, &queryTags, 0,		NULL, NULL},
#endif
 { "quiet", '\0', 0, &quiet, 0,			NULL, NULL},
 { "rcfile", '\0', POPT_ARG_STRING, &rcfile, 0,	NULL, NULL},
 { "rebuilddb", '\0', 0, 0, GETOPT_REBUILDDB,	NULL, NULL},
 { "relocate", '\0', POPT_ARG_STRING, 0, GETOPT_RELOCATE,	NULL, NULL},
 { "replacefiles", '\0', 0, &replaceFiles, 0,	NULL, NULL},
 { "replacepkgs", '\0', 0, &replacePackages, 0,	NULL, NULL},
 { "resign", '\0', 0, 0, GETOPT_RESIGN,		NULL, NULL},
 { "root", 'r', POPT_ARG_STRING, &rootdir, 0,	NULL, NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL, &_rpmio_debug, -1,		NULL, NULL},
 { "showrc", '\0', 0, &showrc, GETOPT_SHOWRC,	NULL, NULL},
 { "sign", '\0', 0, &signIt, 0,			NULL, NULL},
 { "test", '\0', 0, &test, 0,			NULL, NULL},
 { "upgrade", 'U', 0, 0, 'U',			NULL, NULL},
 { "urldebug", '\0', POPT_ARG_VAL, &_url_debug, -1,		NULL, NULL},
 { "uninstall", 'u', 0, 0, 'u',			NULL, NULL},
 { "verbose", 'v', 0, 0, 'v',			NULL, NULL},
#ifdef	DYING
 { "verify", 'V', 0, 0, 'V',			NULL, NULL},
 {  NULL, 'y', 0, 0, 'V',			NULL, NULL},
#endif
 { "version", '\0', 0, &showVersion, 0,		NULL, NULL},

 /* XXX hack to pass build args correctly */
 { "buildroot", '\0', POPT_ARG_STRING, 0,  0,	NULL, NULL},
 { "target", '\0', POPT_ARG_STRING, 0,  0,	NULL, NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, 
		rpmQVSourcePoptTable, 0,	(void *) &rpmQVArgs, NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, 
		rpmQueryPoptTable, 0,		(void *) &rpmQVArgs, NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, 
		rpmVerifyPoptTable, 0,		(void *) &rpmQVArgs, NULL },
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
    printVersion();
    printBanner();
    puts("");

    puts(_("Usage: rpm {--help}"));
    puts(_("       rpm {--version}"));
    puts(_("       rpm {--initdb}   [--dbpath <dir>]"));
    puts(_("       rpm {--install -i} [-v] [--hash -h] [--percent] [--force] [--test]"));
    puts(_("                        [--replacepkgs] [--replacefiles] [--root <dir>]"));
    puts(_("                        [--excludedocs] [--includedocs] [--noscripts]"));
    puts(_("                        [--rcfile <file>] [--ignorearch] [--dbpath <dir>]"));
    puts(_("                        [--prefix <dir>] [--ignoreos] [--nodeps] [--allfiles]"));
    puts(_("                        [--ftpproxy <host>] [--ftpport <port>] [--justdb]"));
    puts(_("                        [--httpproxy <host>] [--httpport <port>] "));
    puts(_("                        [--noorder] [--relocate oldpath=newpath]"));
    puts(_("                        [--badreloc] [--notriggers] [--excludepath <path>]"));
    puts(_("                        [--ignoresize] file1.rpm ... fileN.rpm"));
    puts(_("       rpm {--upgrade -U} [-v] [--hash -h] [--percent] [--force] [--test]"));
    puts(_("                        [--oldpackage] [--root <dir>] [--noscripts]"));
    puts(_("                        [--excludedocs] [--includedocs] [--rcfile <file>]"));
    puts(_("                        [--ignorearch]  [--dbpath <dir>] [--prefix <dir>] "));
    puts(_("                        [--ftpproxy <host>] [--ftpport <port>]"));
    puts(_("                        [--httpproxy <host>] [--httpport <port>] "));
    puts(_("                        [--ignoreos] [--nodeps] [--allfiles] [--justdb]"));
    puts(_("                        [--noorder] [--relocate oldpath=newpath]"));
    puts(_("                        [--badreloc] [--excludepath <path>] [--ignoresize]"));
    puts(_("                        file1.rpm ... fileN.rpm"));
    puts(_("       rpm {--query -q} [-afpg] [-i] [-l] [-s] [-d] [-c] [-v] [-R]"));
    puts(_("                        [--scripts] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--whatprovides] [--whatrequires] [--requires]"));
    puts(_("                        [--triggeredby] [--ftpuseport] [--ftpproxy <host>]"));
    puts(_("                        [--httpproxy <host>] [--httpport <port>] "));
    puts(_("                        [--ftpport <port>] [--provides] [--triggers] [--dump]"));
    puts(_("                        [--changelog] [--dbpath <dir>] [targets]"));
    puts(_("       rpm {--verify -V -y} [-afpg] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--dbpath <dir>] [--nodeps] [--nofiles] [--noscripts]"));
    puts(_("                        [--nomd5] [targets]"));
    puts(_("       rpm {--setperms} [-afpg] [target]"));
    puts(_("       rpm {--setugids} [-afpg] [target]"));
    puts(_("       rpm {--freshen -F} file1.rpm ... fileN.rpm"));
    puts(_("       rpm {--erase -e} [--root <dir>] [--noscripts] [--rcfile <file>]"));
    puts(_("                        [--dbpath <dir>] [--nodeps] [--allmatches]"));
    puts(_("                        [--justdb] [--notriggers] rpackage1 ... packageN"));
    puts(_("       rpm {-b|t}[plcibas] [-v] [--short-circuit] [--clean] [--rcfile  <file>]"));
    puts(_("                        [--sign] [--nobuild] [--timecheck <s>] ]"));
    puts(_("                        [--target=platform1[,platform2...]]"));
    puts(_("                        [--rmsource] [--rmspec] specfile"));
    puts(_("       rpm {--rmsource} [--rcfile <file>] [-v] specfile"));
    puts(_("       rpm {--rebuild} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm"));
    puts(_("       rpm {--recompile} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm"));
    puts(_("       rpm {--resign} [--rcfile <file>] package1 package2 ... packageN"));
    puts(_("       rpm {--addsign} [--rcfile <file>] package1 package2 ... packageN"));
    puts(_("       rpm {--checksig -K} [--nopgp] [--nogpg] [--nomd5] [--rcfile <file>]"));
    puts(_("                           package1 ... packageN"));
    puts(_("       rpm {--rebuilddb} [--rcfile <file>] [--dbpath <dir>]"));
    puts(_("       rpm {--querytags}"));
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
    puts(         _("   All modes support the following arguments:"));
    printHelpLine(_("    --define '<name> <body>'"),
		  _("define macro <name> with value <body>"));
    printHelpLine(_("    --eval '<name>+'      "),
		  _("print the expansion of macro <name> to stdout"));
    printHelpLine(_("    --pipe <cmd>          "),
		  _("send stdout to <cmd>"));
    printHelpLine(_("    --rcfile <file>       "),
		  _("use <file> instead of /etc/rpmrc and $HOME/.rpmrc"));
    printHelpLine(  "    --showrc              ",
		  _("display final rpmrc and macro configuration"));
    printHelpLine(  "     -v                   ",
		  _("be a little more verbose"));
    printHelpLine(  "     -vv                  ",
		  _("be incredibly verbose (for debugging)"));

    puts("");
    puts(         _("   Install, upgrade and query (with -p) allow URL's to be used in place"));
    puts(         _("   of file names as well as the following options:"));
    printHelpLine(_("      --ftpproxy <host>   "),
		  _("hostname or IP of ftp proxy"));
    printHelpLine(_("      --ftpport <port>    "),
		  _("port number of ftp server (or proxy)"));
    printHelpLine(_("      --httpproxy <host>  "),
		  _("hostname or IP of http proxy"));
    printHelpLine(_("      --httpport <port>   "),
		  _("port number of http server (or proxy)"));

    puts("");
    printHelpLine(  "   -q, --query            ",
		  _("query mode"));
    printHelpLine(_("      --dbpath <dir>      "),
		  _("use <dir> as the directory for the database"));
    printHelpLine(_("      --queryformat <qfmt>"),
		  _("use <qfmt> as the header format (implies --info)"));
    printHelpLine(_("      --root <dir>        "),
		  _("use <dir> as the top level directory"));
    puts(         _("      Package specification options:"));
    printHelpLine(  "        -a, --all         ",
		  _("query all packages"));
    printHelpLine(_("        -f <file>+        "),
		  _("query package owning <file>"));
    printHelpLine(_("        -p <packagefile>+ "),
		  _("query (uninstalled) package <packagefile>"));
    printHelpLine(_("        --triggeredby <pkg>"),
		  _("query packages triggered by <pkg>"));
    printHelpLine(_("        --whatprovides <cap>"),
		  _("query packages which provide <cap> capability"));
    printHelpLine(_("        --whatrequires <cap>"),
		  _("query packages which require <cap> capability"));
    puts(         _("      Information selection options:"));
    printHelpLine(  "        -i, --info         ",
		  _("display package information"));
    printHelpLine(  "        --changelog       ",
		  _("display the package's change log"));
    printHelpLine(  "        -l                ",
		  _("display package file list"));
    printHelpLine(  "        -s                ",
		  _("show file states (implies -l)"));
    printHelpLine(  "        -d                ",
		  _("list only documentation files (implies -l)"));
    printHelpLine(  "        -c                ",
		  _("list only configuration files (implies -l)"));
    printHelpLine(  "        --dump            ",
		  _("show all verifiable information for each file (must be used with -l, -c, or -d)"));
    printHelpLine(  "        --provides        ",
		  _("list capabilities package provides"));
    printHelpLine(  "        -R, --requires    ",
		  _("list package dependencies"));
    printHelpLine(  "        --scripts         ",
		  _("print the various [un]install scripts"));
    printHelpLine(  "        --triggers        ",
		  _("show the trigger scripts contained in the package"));

    puts("");
    printHelpLine(  "    -V, -y, --verify      ",
		  _("verify a package installation using the same same package specification options as -q"));
    printHelpLine(_("      --dbpath <dir>      "),
		  _("use <dir> as the directory for the database"));
    printHelpLine(_("      --root <dir>        "),
		  _("use <dir> as the top level directory"));
    printHelpLine(  "      --nodeps            ",
		  _("do not verify package dependencies"));
    printHelpLine(  "      --nomd5             ",
		  _("do not verify file md5 checksums"));
    printHelpLine(  "      --nofiles           ",
		  _("do not verify file attributes"));
    printHelpLine(  "    --querytags           ",
		  _("list the tags that can be used in a query format"));

    puts("");
    puts(         _("    --install <packagefile>"));
    printHelpLine(_("    -i <packagefile>      "),
		  _("install package"));
    printHelpLine(_("      --excludepath <path>"),
		  _("skip files in path <path>"));
    printHelpLine(_("      --relocate <oldpath>=<newpath>"),
		  _("relocate files from <oldpath> to <newpath>"));
    printHelpLine(  "      --badreloc          ",
		  _("relocate files in non-relocateable package"));
    printHelpLine(_("      --prefix <dir>      "),
		  _("relocate the package to <dir>, if relocatable"));
    printHelpLine(_("      --dbpath <dir>      "),
		  _("use <dir> as the directory for the database"));
    printHelpLine(  "      --excludedocs       ",
		  _("do not install documentation"));
    printHelpLine(  "      --force             ",
		  _("short hand for --replacepkgs --replacefiles"));
    printHelpLine(  "      -h, --hash          ",
		  _("print hash marks as package installs (good with -v)"));
    printHelpLine(  "      --allfiles          ",
		  _("install all files, even configurations which might "
		    "otherwise be skipped"));
    printHelpLine(  "      --ignorearch        ",
		  _("don't verify package architecture"));
    printHelpLine(  "      --ignoresize        ",
		  _("don't check disk space before installing"));
    printHelpLine(  "      --ignoreos          ",
		  _("don't verify package operating system"));
    printHelpLine(  "      --includedocs       ",
		  _("install documentation"));
    printHelpLine(  "      --justdb            ",
		  _("update the database, but do not modify the filesystem"));
    printHelpLine(  "      --nodeps            ",
		  _("do not verify package dependencies"));
    printHelpLine(  "      --noorder           ",
		  _("do not reorder package installation to satisfy dependencies"));
    printHelpLine(  "      --noscripts         ",
		  _("don't execute any installation scripts"));
    printHelpLine(  "      --notriggers        ",
		  _("don't execute any scripts triggered by this package"));
    printHelpLine(  "      --percent           ",
		  _("print percentages as package installs"));
    printHelpLine(  "      --replacefiles      ",
		  _("install even if the package replaces installed files"));
    printHelpLine(  "      --replacepkgs       ",
		  _("reinstall if the package is already present"));
    printHelpLine(_("      --root <dir>        "),
		  _("use <dir> as the top level directory"));
    printHelpLine(  "      --test              ",
		  _("don't install, but tell if it would work or not"));

    puts("");
    puts(         _("    --upgrade <packagefile>"));
    printHelpLine(_("    -U <packagefile>      "),
		  _("upgrade package (same options as --install, plus)"));
    printHelpLine(  "      --oldpackage        ",
		  _("upgrade to an old version of the package (--force on upgrades does this automatically)"));
    puts("");
    puts(         _("    --erase <package>"));
    printHelpLine(  "    -e <package>          ",
		  _("erase (uninstall) package"));
    printHelpLine(  "      --allmatches        ",
		  _("remove all packages which match <package> (normally an error is generated if <package> specified multiple packages)"));
    printHelpLine(_("      --dbpath <dir>      "),
		  _("use <dir> as the directory for the database"));
    printHelpLine(  "      --justdb            ",
		  _("update the database, but do not modify the filesystem"));
    printHelpLine(  "      --nodeps            ",
		  _("do not verify package dependencies"));
    printHelpLine(  "      --noorder           ",
		  _("do not reorder package installation to satisfy dependencies"));
    printHelpLine(  "      --noscripts         ",
		  _("do not execute any package specific scripts"));
    printHelpLine(  "      --notriggers        ",
		  _("don't execute any scripts triggered by this package"));
    printHelpLine(_("      --root <dir>        "),
		  _("use <dir> as the top level directory"));
    puts("");
    puts(         _("    -b<stage> <spec>      "));
    printHelpLine(_("    -t<stage> <tarball>   "),
		  _("build package, where <stage> is one of:"));
    printHelpLine(  "          p               ",
		  _("prep (unpack sources and apply patches)"));
    printHelpLine(  "          l               ",
		  _("list check (do some cursory checks on %files)"));
    printHelpLine(  "          c               ",
		  _("compile (prep and compile)"));
    printHelpLine(  "          i               ",
		  _("install (prep, compile, install)"));
    printHelpLine(  "          b               ",
		  _("binary package (prep, compile, install, package)"));
    printHelpLine(  "          a               ",
		  _("bin/src package (prep, compile, install, package)"));
    printHelpLine(  "      --short-circuit     ",
		  _("skip straight to specified stage (only for c,i)"));
    printHelpLine(  "      --clean             ",
		  _("remove build tree when done"));
    printHelpLine(  "      --rmsource          ",
		  _("remove sources when done"));
    printHelpLine(  "      --rmspec            ",
		  _("remove spec file when done"));
    printHelpLine(  "      --sign              ",
		  _("generate PGP/GPG signature"));
    printHelpLine(_("      --buildroot <dir>   "),
		  _("use <dir> as the build root"));
    printHelpLine(_("      --target=<platform>+"),
		  _("build the packages for the build targets platform1...platformN."));
    printHelpLine(  "      --nobuild           ",
		  _("do not execute any stages"));
    printHelpLine(_("      --timecheck <secs>  "),
		  _("set the time check to <secs> seconds (0 disables)"));
    puts("");
    printHelpLine(_("    --rebuild <src_pkg>   "),
		  _("install source package, build binary package and remove spec file, sources, patches, and icons."));
    printHelpLine(_("    --recompile <src_pkg> "),
		  _("like --rebuild, but don't build any package"));

    puts("");
    printHelpLine(_("    --resign <pkg>+       "),
		  _("sign a package (discard current signature)"));
    printHelpLine(_("    --addsign <pkg>+      "),
		  _("add a signature to a package"));
    puts(         _("    --checksig <pkg>+"));
    printHelpLine(_("    -K <pkg>+           "),
		  _("verify package signature"));
    printHelpLine(  "      --nopgp             ",
		  _("skip any PGP signatures"));
    printHelpLine(  "      --nogpg             ",
		  _("skip any GPG signatures"));
    printHelpLine(  "      --nomd5             ",
		  _("skip any MD5 signatures"));

    puts("");
    printHelpLine(  "    --initdb              ",
		  _("make sure a valid database exists"));
    printHelpLine(  "    --rebuilddb           ",
		  _("rebuild database from existing database"));
    printHelpLine(_("      --dbpath <dir>      "),
		  _("use <dir> as the directory for the database"));
    printHelpLine(  "      --root <dir>        ",
		  _("use <dir> as the top level directory"));

    puts("");
    printHelpLine(  "    --setperms            ",
		  _("set the file permissions to those in the package database"
		    " using the same package specification options as -q"));
    printHelpLine(  "    --setugids            ",
		  _("set the file owner and group to those in the package "
		    "database using the same package specification options as "
		    "-q"));
}

int main(int argc, const char ** argv)
{
    enum modes bigMode = MODE_UNKNOWN;
    QVA_t *qva = &rpmQVArgs;
    enum rpmQVSources QVSource = RPMQV_PACKAGE;
    int arg;
    int installFlags = 0, uninstallFlags = 0, interfaceFlags = 0;
    int verifyFlags;
    int checksigFlags = 0;
    int addSign = NEW_SIGNATURE;
    char * passPhrase = "";
    const char * optArg;
    pid_t pipeChild = 0;
    const char * pkg;
    char * errString = NULL;
    poptContext optCon;
    int ec = 0;
    int status;
    int p[2];
    rpmRelocation * relocations = NULL;
    int numRelocations = 0;
    int sigTag;
    int upgrade = 0;
    int probFilter = 0;
	
#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();	/* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);	/* Retrofit glibc __progname */

    /* set the defaults for the various command line options */
    allFiles = 0;
    allMatches = 0;
    badReloc = 0;
    excldocs = 0;
    force = 0;
    _ftp_debug = 0;
    showHash = 0;
    help = 0;
    ignoreArch = 0;
    ignoreOs = 0;
    ignoreSize = 0;
    incldocs = 0;
    initdb = 0;
    justdb = 0;
    noDeps = 0;
    noGpg = 0;
#if HAVE_LIBIO_H && defined(_IO_BAD_SEEN)
    noLibio = 0;
#else
    noLibio = 1;
#endif
    noMd5 = 0;
    noOrder = 0;
    noPgp = 0;
    noScripts = 0;
    noTriggers = 0;
    noUsageMsg = 0;
    oldPackage = 0;
    showPercents = 0;
    pipeOutput = NULL;
    prefix = NULL;
    quiet = 0;
    _rpmio_debug = 0;
    replaceFiles = 0;
    replacePackages = 0;
    rootdir = "/";
    showrc = 0;
    signIt = 0;
    showVersion = 0;
    specedit = 0;
    test = 0;
    _url_debug = 0;

    /* XXX Eliminate query linkage loop */
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
    optCon = poptGetContext("rpm", argc, argv, optionsTable, 0);
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

    if (qva->qva_queryFormat) xfree(qva->qva_queryFormat);
    memset(qva, 0, sizeof(*qva));
    qva->qva_mode = ' ';
    qva->qva_char = ' ';

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	switch (arg) {
	  case 'K':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_CHECKSIG)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_CHECKSIG;
	    break;
	    
	  case 'q':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_QUERY)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_QUERY;
	    break;

	  case 'V':
	  case 'y':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_VERIFY)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_VERIFY;
	    break;

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
	
	  case 'v':
	    rpmIncreaseVerbosity();
	    break;

	  case 'i':
	    if (bigMode == MODE_QUERY) {
		const char * infoCommand[] = { "--info", NULL };
		poptStuffArgs(optCon, infoCommand);
	    } else if (bigMode == MODE_INSTALL)
		/*@-ifempty@*/ ;
	    else if (bigMode == MODE_UNKNOWN) {
		const char * installCommand[] = { "--install", NULL };
		poptStuffArgs(optCon, installCommand);
	    }
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

	  case 'p':
	    if (QVSource != RPMQV_PACKAGE && QVSource != RPMQV_RPM)
		argerror(_("one type of query/verify may be performed at a " "time"));
	    QVSource = RPMQV_RPM;
	    break;

	  case 'g':
	    if (QVSource != RPMQV_PACKAGE && QVSource != RPMQV_GROUP)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    QVSource = RPMQV_GROUP;
	    break;

	  case 'f':
	    if (QVSource != RPMQV_PACKAGE && QVSource != RPMQV_PATH)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    QVSource = RPMQV_PATH;
	    break;

	  case 'a':
	    if (QVSource != RPMQV_PACKAGE && QVSource != RPMQV_ALL)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    QVSource = RPMQV_ALL;
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

	  case GETOPT_REBUILDDB:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_REBUILDDB)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_REBUILDDB;
	    break;

	  case GETOPT_RELOCATE:
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
	    break;

	  case GETOPT_EXCLUDEPATH:
	    if (*optArg != '/') 
		argerror(_("exclude paths must begin with a /"));

	    relocations = xrealloc(relocations, 
				  sizeof(*relocations) * (numRelocations + 1));
	    relocations[numRelocations].oldPath = optArg;
	    relocations[numRelocations++].newPath = NULL;
	    break;

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

    if (bigMode == MODE_UNKNOWN && qva->qva_mode != ' ') {
	switch (qva->qva_mode) {
	case 'q':	bigMode = MODE_QUERY;		break;
	case 'V':	bigMode = MODE_VERIFY;		break;
	case 'Q':	bigMode = MODE_QUERYTAGS;	break;
	}
    }

    if (initdb) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_INITDB;
    }

#ifdef	DYING
    if (queryTags) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_QUERYTAGS;
    }
#endif

    if (qva->qva_sourceCount) {
	if (QVSource != RPMQV_PACKAGE || qva->qva_sourceCount > 1)
	    argerror(_("one type of query/verify may be performed at a "
			"time"));
	QVSource = qva->qva_source;
    }

    if (qva->qva_flags && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query flags"));

    if (qva->qva_queryFormat && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query format"));

    if (QVSource != RPMQV_PACKAGE && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query source"));

    if (!(bigMode == MODE_INSTALL) && force)
	argerror(_("only installation, upgrading, rmsource and rmspec may be forced"));

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

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_VERIFY && noScripts)
	argerror(_("--noscripts may only be specified during package "
		   "installation, erasure, and verification"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && noTriggers)
	argerror(_("--notriggers may only be specified during package "
		   "installation, erasure, and verification"));

    if (noDeps & (bigMode & ~MODES_FOR_NODEPS))
	argerror(_("--nodeps may only be specified during package "
		   "building, rebuilding, recompilation, installation,"
		   "erasure, and verification"));

    if (test && (bigMode & ~MODES_FOR_TEST))
	argerror(_("--test may only be specified during package installation, "
		 "erasure, and building"));

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

    if (oldPackage && !upgrade)
	argerror(_("--oldpackage may only be used during upgrades"));

    if (noPgp && bigMode != MODE_CHECKSIG)
	argerror(_("--nopgp may only be used during signature checking"));

    if (noGpg && bigMode != MODE_CHECKSIG)
	argerror(_("--nogpg may only be used during signature checking"));

    if (noMd5 && bigMode != MODE_CHECKSIG && bigMode != MODE_VERIFY)
	argerror(_("--nomd5 may only be used during signature checking and "
		   "package verification"));

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
      case MODE_UNKNOWN:
	if (!showVersion && !help && !noUsageMsg) printUsage();
	break;

      case MODE_REBUILDDB:
	ec = rpmdbRebuild(rootdir);
	break;

      case MODE_QUERYTAGS:
	if (argc != 2)
	    argerror(_("unexpected arguments to --querytags "));

	rpmDisplayQueryTags(stdout);
	break;

      case MODE_INITDB:
	rpmdbInit(rootdir, 0644);
	break;

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
	
      case MODE_REBUILD:
      case MODE_RECOMPILE:
	break;

      case MODE_BUILD:
      case MODE_TARBUILD:
	break;

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
	qva->qva_prefix = rootdir;
	if (QVSource == RPMQV_ALL) {
	    if (poptPeekArg(optCon))
		argerror(_("extra arguments given for query of all packages"));

	    ec = rpmQuery(qva, RPMQV_ALL, NULL);
	} else {
	    if (!poptPeekArg(optCon))
		argerror(_("no arguments given for query"));
	    while ((pkg = poptGetArg(optCon)))
		ec += rpmQuery(qva, QVSource, pkg);
	}
	break;

      case MODE_VERIFY:
	verifyFlags = (VERIFY_FILES|VERIFY_DEPS|VERIFY_SCRIPT|VERIFY_MD5);
	verifyFlags &= ~qva->qva_flags;
	if (noDeps)	verifyFlags &= ~VERIFY_DEPS;
	if (noScripts)	verifyFlags &= ~VERIFY_SCRIPT;
	if (noMd5)	verifyFlags &= ~VERIFY_MD5;

	qva->qva_prefix = rootdir;
	qva->qva_flags = verifyFlags;
	if (QVSource == RPMQV_ALL) {
	    if (poptPeekArg(optCon))
		argerror(_("extra arguments given for verify of all packages"));
	    ec = rpmVerify(qva, RPMQV_ALL, NULL);
	} else {
	    if (!poptPeekArg(optCon))
		argerror(_("no arguments given for verify"));
	    while ((pkg = poptGetArg(optCon)))
		ec += rpmVerify(qva, QVSource, pkg);
	}
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
    if (qva->qva_queryFormat) xfree(qva->qva_queryFormat);

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    return ec;
}
