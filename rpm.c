#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "build/build.h"
#include "checksig.h"
#include "install.h"
#include "intl.h"
#include "lib/messages.h"
#include "lib/signature.h"
#include "misc/popt.h"
#include "query.h"
#include "rpmlib.h"
#include "verify.h"

#define GETOPT_QUERYFORMAT	1000
#define GETOPT_WHATREQUIRES	1001
#define GETOPT_WHATPROVIDES	1002
#define GETOPT_REBUILD		1003
#define GETOPT_RECOMPILE	1004
#define GETOPT_ADDSIGN		1005
#define GETOPT_RESIGN		1006
#define GETOPT_BUILDROOT 	1007
#define GETOPT_QUERYBYNUMBER	1009
#define GETOPT_DBPATH		1010
#define GETOPT_TIMECHECK        1012
#define GETOPT_REBUILDDB        1013
#define GETOPT_INSTALL		1014

char * version = VERSION;

enum modes { MODE_QUERY, MODE_INSTALL, MODE_UNINSTALL, MODE_VERIFY,
	     MODE_BUILD, MODE_REBUILD, MODE_CHECKSIG, MODE_RESIGN,
	     MODE_RECOMPILE, MODE_QUERYTAGS, MODE_INITDB, MODE_TARBUILD,
	     MODE_REBUILDDB, MODE_UNKNOWN };

static void argerror(char * desc);

static void argerror(char * desc) {
    fprintf(stderr, "rpm: %s\n", desc);
    exit(1);
}

static void printHelp(void);
static void printVersion(void);
static void printBanner(void);
static void printUsage(void);
static void printHelpLine(char * prefix, char * help);
static int build(char *arg, int buildAmount, char *passPhrase,
	         char *buildRootOverride, int fromTarball);

static void printVersion(void) {
    printf(_("RPM version %s\n"), version);
}

static void printBanner(void) {
    puts(_("Copyright (C) 1997 - Red Hat Software"));
    puts(_("This may be freely redistributed under the terms of the GNU "
	   "Public License"));
}

static void printUsage(void) {
    printVersion();
    printBanner();
    puts("");

    puts(_("usage: rpm {--help}"));
    puts(_("       rpm {--version}"));
    puts(_("       rpm {--initdb}   [--dbpath <dir>]"));
    puts(_("       rpm {--install -i} [-v] [--hash -h] [--percent] [--force] [--test]"));
    puts(_("                        [--replacepkgs] [--replacefiles] [--root <dir>]"));
    puts(_("                        [--excludedocs] [--includedocs] [--noscripts]"));
    puts(_("                        [--rcfile <file>] [--ignorearch] [--dbpath <dir>]"));
    puts(_("                        [--prefix <dir>] [--ignoreos] [--nodeps]"));
    puts(_("                        [--ftpproxy <host>] [--ftpport <port>]"));
    puts(_("                        file1.rpm ... fileN.rpm"));
    puts(_("       rpm {--upgrade -U} [-v] [--hash -h] [--percent] [--force] [--test]"));
    puts(_("                        [--oldpackage] [--root <dir>] [--noscripts]"));
    puts(_("                        [--excludedocs] [--includedocs] [--rcfile <file>]"));
    puts(_("                        [--ignorearch]  [--dbpath <dir>] [--prefix <dir>] "));
    puts(_("                        [--ftpproxy <host>] [--ftpport <port>]"));
    puts(_("                        [--ignoreos] [--nodeps] file1.rpm ... fileN.rpm"));
    puts(_("       rpm {--query -q} [-afpg] [-i] [-l] [-s] [-d] [-c] [-v] [-R]"));
    puts(_("                        [--scripts] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--whatprovides] [--whatrequires] [--requires]"));
    puts(_("                        [--ftpuseport] [--ftpproxy <host>] [--ftpport <port>]"));
    puts(_("                        [--provides] [--dump] [--dbpath <dir>] [targets]"));
    puts(_("       rpm {--verify -V -y} [-afpg] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--dbpath <dir>] [--nodeps] [--nofiles] [--noscripts]"));
    puts(_("                        [--nomd5] [targets]"));
    puts(_("       rpm {--setperms} [-afpg] [target]"));
    puts(_("       rpm {--setugids} [-afpg] [target]"));
    puts(_("       rpm {--erase -e] [--root <dir>] [--noscripts] [--rcfile <file>]"));
    puts(_("                        [--dbpath <dir>] [--nodeps] package1 ... packageN"));
    puts(_("       rpm {-b|t}[plciba] [-v] [--short-circuit] [--clean] [--rcfile  <file>]"));
    puts(_("                        [--sign] [--test] [--timecheck <s>] specfile"));
    puts(_("       rpm {--rebuild} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm"));
    puts(_("       rpm {--recompile} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm"));
    puts(_("       rpm {--resign} [--rcfile <file>] package1 package2 ... packageN"));
    puts(_("       rpm {--addsign} [--rcfile <file>] package1 package2 ... packageN"));
    puts(_("       rpm {--checksig -K} [--nopgp] [--nomd5] [--rcfile <file>]"));
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

    printf("%s - ", prefix);

    while (helpLength > lineLength) {
	ch = help + lineLength - 1;
	while (ch > help && !isspace(*ch)) ch--;
	if (ch == help) break;		/* give up */
	while (ch > (help + 1) && isspace(*ch)) ch--;
	ch++;

	sprintf(format, "%%.%ds\n%%%ds", ch - help, indentLength);
	printf(format, help, " ");
	help = ch;
	while (isspace(*help) && *help) help++;
	helpLength = strlen(help);
    }

    if (helpLength) puts(help);
}

static void printHelp(void) {
    printVersion();
    printBanner();
    puts(_(""));

    puts(_("usage:"));
    printHelpLine("   --help                 ", 
		  _("print this message"));
    printHelpLine("   --version              ",
		  _("print the version of rpm being used"));
    puts(_("   all modes support the following arguments:"));
    printHelpLine("      --rcfile <file>     ",
		  _("use <file> instead of /etc/rpmrc and $HOME/.rpmrc"));
    printHelpLine("     -v                   ",
		  _("be a little more verbose"));
    printHelpLine("     -vv                  ",
		  _("be incredibly verbose (for debugging)"));
    printHelpLine("   -q                     ",
		  _("query mode"));
    printHelpLine("      --root <dir>        ",
		  _("use <dir> as the top level directory"));
    printHelpLine("      --dbpath <dir>      ",
		  _("use <dir> as the directory for the database"));
    printHelpLine("      --queryformat <s>   ",
		  _("use s as the header format (implies -i)"));
    puts(_("   install, upgrade and query (with -p) allow ftp URL's to be used in place"));
    puts(_("   of file names as well as the following options:\n"));
    printHelpLine("      --ftpproxy <host>   ",
		  _("hostname or IP of ftp proxy"));
    printHelpLine("      --ftpport <port>    ",
		  _("port number of ftp server (or proxy)"));
    puts(       _("      Package specification options:"));
    printHelpLine("        -a                ",
		  _("query all packages"));
    printHelpLine("        -f <file>+        ",
		  _("query package owning <file>"));
    printHelpLine("        -p <packagefile>+ ",
		  _("query (uninstalled) package <packagefile>"));
    printHelpLine("        --whatprovides <i>",
		  _("query packages which provide <i> capability"));
    printHelpLine("        --whatrequires <i>",
		  _("query packages which require <i> capability"));
    puts(_("      Information selection options:"));
    printHelpLine("        -i                ",
		  _("display package information"));
    printHelpLine("        -l                ",
		  _("display package file list"));
    printHelpLine("        -s                ",
		  _("show file states (implies -l)"));
    printHelpLine("        -d                ",
		  _("list only documentation files (implies -l)"));
    printHelpLine("        -c                ",
		  _("list only configuration files (implies -l)"));
    printHelpLine("        --dump            ",
		  _("show all verifiable information for each file (must be used with -l, -c, or -d)"));
    printHelpLine("        --provides        ",
		  _("list capabilbities package provides"));
    puts(       _("        --requires"));
    printHelpLine("        -R                ",
		  _("list package dependencies"));
    printHelpLine("        --scripts         ",
		  _("print the various [un]install scripts"));
    puts(         "");
    puts(         "    -V");
    puts(         "    -y");
    printHelpLine("    --pipe <cmd>        ",
		  _("send stdout to <cmd>"));
    printHelpLine("    --verify              ",
		  _("verify a package installation using the same same package specification options as -q"));
    printHelpLine("      --dbpath <dir>      ",
		  _("use <dir> as the directory for the database"));
    printHelpLine("      --root <dir>        ",
		  _("use <dir> as the top level directory"));
    printHelpLine("      --nodeps            ",
		  _("do not verify package dependencies"));
    printHelpLine("      --nomd5             ",
		  _("do not verify file md5 checksums"));
    printHelpLine("      --nofiles           ",
		  _("do not verify file attributes"));
    puts("");
    printHelpLine("    --setperms            ",
		  _("set the file permissions to those in the package database"
		    " using the same package specification options as -q"));
    printHelpLine("    --setugids            ",
		  _("set the file owner and group to those in the package "
		    "database using the same package specification options as "
		    "-q"));
    puts("");
    puts(         "    --install <packagefile>");
    printHelpLine("    -i <packagefile>      ",
		  _("install package"));
    printHelpLine("      --prefix <dir>      ",
		  _("relocate the package to <dir>, if relocatable"));
    printHelpLine("      --dbpath <dir>      ",
		  _("use <dir> as the directory for the database"));
    printHelpLine("      --excludedocs       ",
		  _("do not install documentation"));
    printHelpLine("      --force             ",
		  _("short hand for --replacepkgs --replacefiles"));
    puts(         "      -h");
    printHelpLine("      --hash              ",
		  _("print hash marks as package installs (good with -v)"));
    printHelpLine("      --ignorearch        ",
		  _("don't verify package architecure"));
    printHelpLine("      --ignoreos          ",
		  _("don't verify package operating system"));
    printHelpLine("      --includedocs       ",
		  _("install documentation"));
    printHelpLine("      --nodeps            ",
		  _("do not verify package dependencies"));
    printHelpLine("      --noscripts         ",
		  _("don't execute any installation scripts"));
    printHelpLine("      --percent           ",
		  _("print percentages as package installs"));
    printHelpLine("      --replacefiles      ",
		  _("install even if the package replaces installed files"));
    printHelpLine("      --replacepkgs       ",
		  _("reinstall if the package is already present"));
    printHelpLine("      --root <dir>        ",
		  _("use <dir> as the top level directory"));
    printHelpLine("      --test              ",
		  _("don't install, but tell if it would work or not"));
    puts("");
    puts(         "    --upgrade <packagefile>");
    printHelpLine("    -U <packagefile>      ",
		  _("upgrade package (same options as --install, plus)"));
    printHelpLine("      --oldpackage        ",
		  _("upgrade to an old version of the package (--force on upgrades does this automatically)"));
    puts("");
    puts(         "    --erase <package>");
    printHelpLine("    -e <package>          ",
		  _("erase (uninstall) package"));
    printHelpLine("      --dbpath <dir>      ",
		  _("use <dir> as the directory for the database"));
    printHelpLine("      --nodeps            ",
		  _("do not verify package dependencies"));
    printHelpLine("      --noscripts         ",
		  _("do not execute any package specific scripts"));
    printHelpLine("      --root <dir>        ",
		  _("use <dir> as the top level directory"));
    puts(_(""));
    puts(_("    -b<stage> <spec>      "));
    printHelpLine("    -t<stage> <tarball>      ",
		  _("build package, where <stage> is one of:"));
    printHelpLine("          p               ",
		  _("prep (unpack sources and apply patches)"));
    printHelpLine("          l               ",
		  _("list check (do some cursory checks on %files)"));
    printHelpLine("          c               ",
		  _("compile (prep and compile)"));
    printHelpLine("          i               ",
		  _("install (prep, compile, install)"));
    printHelpLine("          b               ",
		  _("binary package (prep, compile, install, package)"));
    printHelpLine("          a               ",
		  _("bin/src package (prep, compile, install, package)"));
    printHelpLine("      --short-circuit     ",
		  _("skip straight to specified stage (only for c,i)"));
    printHelpLine("      --clean             ",
		  _("remove build tree when done"));
    printHelpLine("      --sign              ",
		  _("generate PGP signature"));
    printHelpLine("      --buildroot <s>     ",
		  _("use s as the build root"));
    printHelpLine("      --test              ",
		  _("do not execute any stages"));
    printHelpLine("      --timecheck <s>     ",
		  _("set the time check to S seconds (0 disables it)"));
    puts("");
    printHelpLine("    --rebuild <src_pkg>   ",
		  _("install source package, build binary package and remove spec file, sources, patches, and icons."));
    printHelpLine("    --recompile <src_pkg> ",
		  _("like --rebuild, but don't build any package"));
    printHelpLine("    --resign <pkg>+       ",
		  _("sign a package (discard current signature)"));
    printHelpLine("    --addsign <pkg>+      ",
		  _("add a signature to a package"));
    puts(         "    -K");
    printHelpLine("    --checksig <pkg>+     ",
		  _("verify package signature"));
    printHelpLine("      --nopgp             ",
		  _("skip any PGP signatures"));
    printHelpLine("      --nomd5             ",
		  _("skip any MD5 signatures"));
    printHelpLine("    --querytags           ",
		  _("list the tags that can be used in a query format"));
    printHelpLine("    --initdb              ",
		  _("make sure a valid database exists"));
    printHelpLine("    --rebuilddb           ",
		  _("rebuild database from existing database"));
    printHelpLine("      --dbpath <dir>      ",
		  _("use <dir> as the directory for the database"));
    printHelpLine("      --root <dir>        ",
		  _("use <dir> as the top level directory"));
}

static int build(char *arg, int buildAmount, char *passPhrase,
	         char *buildRootOverride, int fromTarball) {
    FILE *f;
    Spec s;
    char * specfile;
    int res = 0;
    struct stat statbuf;
    char * specDir;
    char * tmpSpecFile;
    char * cmd;
    char buf[1024];

    if (fromTarball) {
	specDir = rpmGetVar(RPMVAR_SPECDIR);
	tmpSpecFile = alloca(1024);
	sprintf(tmpSpecFile, "%s/rpm-spec-file-%d", specDir, getpid());

	cmd = alloca(strlen(arg) + 50 + strlen(tmpSpecFile));
	sprintf(cmd, "gunzip < %s | tar xOvf - \\*.spec 2>&1 > %s", arg,
			tmpSpecFile);
	if (!(f = popen(cmd, "r"))) {
	    fprintf(stderr, _("Failed to open tar pipe: %s\n"), 
			strerror(errno));
	    return 1;
	}
	if (!fgets(buf, sizeof(buf) - 1, f)) {
	    fprintf(stderr, _("Failed to read spec file from %s\n"), arg);
	    unlink(tmpSpecFile);
	    return 1;
	}
	fclose(f);

	cmd = specfile = buf;
	while (*cmd) {
	    if (*cmd == '/') specfile = cmd + 1;
	    cmd++;
	}

	cmd = specfile;

	/* remove trailing \n */
	specfile = cmd + strlen(cmd) - 1;
	*specfile = '\0';

	specfile = alloca(strlen(specDir) + strlen(cmd) + 5);
	sprintf(specfile, "%s/%s", specDir, cmd);
	
	if (rename(tmpSpecFile, specfile)) {
	    fprintf(stderr, "Failed to rename %s to %s: %s\n", tmpSpecFile,
			specfile, strerror(errno));
	    unlink(tmpSpecFile);
	    return 1;
	}

	/* Make the directory which contains the tarball the source 
	   directory for this run */

	/* XXX this is broken if PWD is near 1024 */
	getcwd(buf, 1024);
	strcat(buf, "/");
	strcat(buf, arg);

	cmd = buf + strlen(buf) - 1;
	while (*cmd != '/') cmd--;
	*cmd = '\0';

	rpmSetVar(RPMVAR_SOURCEDIR, buf);
    } else if (arg[0] == '/') {
	specfile = arg;
    } else {
	/* XXX this is broken if PWD is near 1024 */
	specfile = alloca(1024);
	getcwd(specfile, 1024);
	strcat(specfile, "/");
	strcat(specfile, arg);
    }

    stat(specfile, &statbuf);
    if (! S_ISREG(statbuf.st_mode)) {
	rpmError(RPMERR_BADSPEC, "File is not a regular file: %s\n", specfile);
	return 1;
    }
    
    if (!(f = fopen(specfile, "r"))) {
	fprintf(stderr, _("unable to open: %s\n"), specfile);
	return 1;
    }
    s = parseSpec(f, specfile, buildRootOverride);
    fclose(f);
    if (s) {
	if (verifySpec(s)) {
	    fprintf(stderr, "\n%cSpec file check failed!!\n", 7);
	    fprintf(stderr,
		    "Tell rpm-list@redhat.com if this is incorrect.\n\n");
	    res = 1;
	} else {
	    if (doBuild(s, buildAmount, passPhrase)) {
		fprintf(stderr, _("Build failed.\n"));
		res = 1;
	    }
	}
        freeSpec(s);
    } else {
	/* Spec parse failed -- could be Exclude: Exclusive: */
	res = 1;
	if (rpmErrorCode() == RPMERR_BADARCH) {
	    fprintf(stderr, _("%s doesn't build on this architecture\n"), arg);
	} else {
	    fprintf(stderr, _("Build failed.\n"));
	}
    }

    if (fromTarball) unlink(specfile);

    return res;
}

int main(int argc, char ** argv) {
    enum modes bigMode = MODE_UNKNOWN;
    enum querysources querySource = QUERY_PACKAGE;
    enum verifysources verifySource = VERIFY_PACKAGE;
    int arg, len;
    int queryFor = 0, test = 0, version = 0, help = 0, force = 0;
    int quiet = 0, replaceFiles = 0, replacePackages = 0, showPercents = 0;
    int showHash = 0, installFlags = 0, uninstallFlags = 0, interfaceFlags = 0;
    int buildAmount = 0, oldPackage = 0, clean = 0, signIt = 0;
    int shortCircuit = 0, queryTags = 0, excldocs = 0;
    int incldocs = 0, noScripts = 0, noDeps = 0;
    int noPgp = 0, dump = 0, initdb = 0, ignoreArch = 0, showrc = 0;
    int gotDbpath = 0, building = 0, ignoreOs = 0, noFiles = 0, verifyFlags;
    int noMd5 = 0;
    int checksigFlags = 0;
    char *tce;
    int timeCheck = 0;
    int addSign = NEW_SIGNATURE;
    char * rcfile = NULL, * queryFormat = NULL, * prefix = NULL;
    char buildChar = ' ';
    char * rootdir = "/";
    char * pipeOutput = NULL;
    char * specFile;
    char *passPhrase = "";
    char *buildRootOverride = NULL;
    char *arch = NULL;
    char * ftpProxy = NULL, * ftpPort = NULL;
    char *os = NULL;
    char * optArg;
    pid_t pipeChild = 0;
    char * pkg;
    char ** currarg;
    char * errString;
    poptContext optCon;
    char * infoCommand[] = { "--info", NULL };
    char * installCommand[] = { "--install", NULL };
    int ec = 0;
    int status;
    int p[2];
    struct poptOption optionsTable[] = {
	    { "addsign", '\0', 0, 0, GETOPT_ADDSIGN },
	    { "all", 'a', 0, 0, 'a' },
	    { "build", 'b', POPT_ARG_STRING, 0, 'b' },
	    { "buildarch", '\0', POPT_ARG_STRING, 0, 0 },
	    { "buildos", '\0', POPT_ARG_STRING, 0, 0 },
	    { "buildroot", '\0', POPT_ARG_STRING, 0, GETOPT_BUILDROOT },
	    { "checksig", 'K', 0, 0, 'K' },
	    { "clean", '\0', 0, &clean, 0 },
	    { "configfiles", 'c', 0, 0, 'c' },
	    { "dbpath", '\0', POPT_ARG_STRING, 0, GETOPT_DBPATH },
	    { "docfiles", 'd', 0, 0, 'd' },
	    { "dump", '\0', 0, &dump, 0 },
	    { "erase", 'e', 0, 0, 'e' },
            { "excludedocs", '\0', 0, &excldocs, 0},
	    { "file", 'f', 0, 0, 'f' },
	    { "force", '\0', 0, &force, 0 },
	    { "ftpport", '\0', POPT_ARG_STRING, &ftpPort, 0},
	    { "ftpproxy", '\0', POPT_ARG_STRING, &ftpProxy, 0},
	    { "group", 'g', 0, 0, 'g' },
	    { "hash", 'h', 0, &showHash, 0 },
	    { "help", '\0', 0, &help, 0 },
	    {  NULL, 'i', 0, 0, 'i' },
	    { "ignorearch", '\0', 0, &ignoreArch, 0 },
	    { "ignoreos", '\0', 0, &ignoreOs, 0 },
            { "includedocs", '\0', 0, &incldocs, 0},
	    { "initdb", '\0', 0, &initdb, 0 },
	/* info and install both using 'i' is dumb */
	    { "install", '\0', 0, 0, GETOPT_INSTALL },
	    { "list", 'l', 0, 0, 'l' },
	    { "nodeps", '\0', 0, &noDeps, 0 },
	    { "nofiles", '\0', 0, &noFiles, 0 },
	    { "nomd5", '\0', 0, &noMd5, 0 },
	    { "nopgp", '\0', 0, &noPgp, 0 },
	    { "noscripts", '\0', 0, &noScripts, 0 },
	    { "oldpackage", '\0', 0, &oldPackage, 0 },
	    { "package", 'p', 0, 0, 'p' },
	    { "percent", '\0', 0, &showPercents, 0 },
	    { "pipe", '\0', POPT_ARG_STRING, &pipeOutput, 0 },
	    { "prefix", '\0', POPT_ARG_STRING, &prefix, 0 },
	    { "qf", '\0', POPT_ARG_STRING, 0, GETOPT_QUERYFORMAT },
	    { "query", 'q', 0, 0, 'q' },
	    { "querybynumber", '\0', 0, 0, GETOPT_QUERYBYNUMBER },
	    { "queryformat", '\0', POPT_ARG_STRING, 0, GETOPT_QUERYFORMAT },
	    { "querytags", '\0', 0, &queryTags, 0 },
	    { "quiet", '\0', 0, &quiet, 0 },
	    { "rcfile", '\0', POPT_ARG_STRING, 0, 0 },
	    { "recompile", '\0', 0, 0, GETOPT_RECOMPILE },
	    { "rebuild", '\0', 0, 0, GETOPT_REBUILD },
	    { "rebuilddb", '\0', 0, 0, GETOPT_REBUILDDB },
	    { "replacefiles", '\0', 0, &replaceFiles, 0 },
	    { "replacepkgs", '\0', 0, &replacePackages, 0 },
	    { "resign", '\0', 0, 0, GETOPT_RESIGN },
	    { "root", 'r', POPT_ARG_STRING, &rootdir, 0 },
	    { "short-circuit", '\0', 0, &shortCircuit, 0 },
	    { "showrc", '\0', 0, 0, 0 },
	    { "sign", '\0', 0, &signIt, 0 },
	    { "state", 's', 0, 0, 's' },
	    { "tarball", 't', POPT_ARG_STRING, 0, 't' },
	    { "test", '\0', 0, &test, 0 },
	    { "timecheck", '\0', POPT_ARG_STRING, 0, GETOPT_TIMECHECK },
	    { "upgrade", 'U', 0, 0, 'U' },
	    { "uninstall", 'u', 0, 0, 'u' },
	    { "verbose", 'v', 0, 0, 'v' },
	    { "verify", 'V', 0, 0, 'V' },
	    {  NULL, 'y', 0, 0, 'V' },
	    { "version", '\0', 0, &version, 0 },
	    { "whatrequires", '\0', 0, 0, GETOPT_WHATREQUIRES },
	    { "whatprovides", '\0', 0, 0, GETOPT_WHATPROVIDES },
	    { 0, 0, 0, 0, 0 } 
	} ;

    /* set up the correct locale */
    setlocale(LC_ALL, "" );

    bindtextdomain(RPMNLSPACKAGE, RPMNLSDIR);
    textdomain(RPMNLSPACKAGE);

    /* Make a first pass through the arguments, looking for --rcfile */
    /* as well as --arch and --os.  We need to handle that before    */
    /* dealing with the rest of the arguments.                       */
    currarg = argv;
    while (*currarg) {
	if (!strcmp(*currarg, "--rcfile")) {
	    rcfile = *(++currarg);
	} else if (!strcmp(*currarg, "--buildarch")) {
	    arch = *(++currarg);
	} else if (!strcmp(*currarg, "--buildos")) {
	    os = *(++currarg);
	} else if (!strcmp(*currarg, "--showrc")) {
	    showrc = 1;
	    building = 1;
	} else if (!strncmp(*currarg, "-b", 2) ||
		   !strncmp(*currarg, "-t", 2) ||
		   !strcmp(*currarg, "--tarbuild") ||
		   !strcmp(*currarg, "--build") ||
		   !strcmp(*currarg, "--rebuild") ||
		   !strcmp(*currarg, "--recompile")) {
	    building = 1;
	}

	if (*currarg) currarg++;
    } 

    /* reading this early makes it easy to override */
    if (rpmReadConfigFiles(rcfile, arch, os, building))  
	exit(1);
    if (showrc) {
	rpmShowRC(stdout);
	exit(0);
    }

    optCon = poptGetContext("rpm", argc, argv, optionsTable, 0);
    poptReadConfigFile(optCon, LIBRPMALIAS_FILENAME);
    poptReadDefaultConfig(optCon, 1);

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
	    rpmMessage(RPMMESS_ERROR, _("-u and --uninstall are depricated and no"
		    " longer work.\n"));
	    rpmMessage(RPMMESS_ERROR, _("Use -e or --erase instead.\n"));
	    exit(1);
	
	  case 'e':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_UNINSTALL)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_UNINSTALL;
	    break;
	
	  case 'b':
	  case 't':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_BUILD)
		argerror(_("only one major mode may be specified"));

	    if (arg == 'b') {
		bigMode = MODE_BUILD;
		errString = _("--build (-b) requires one of a,b,i,c,p,l as "
				"its sole argument");
	    } else {
		bigMode = MODE_TARBUILD;
		errString = _("--tarbuild (-t) requires one of a,b,i,c,p,l as "
			      "its sole argument");
	    }

	    if (strlen(optArg) > 1) 
		argerror(errString);

	    buildChar = optArg[0];
	    switch (buildChar) {
	      case 'a':
	      case 'b':
	      case 'i':
	      case 'c':
	      case 'p':
	      case 'l':
		break;
	      default:
		argerror(errString);
	    }

	    break;
	
	  case 'v':
	    rpmIncreaseVerbosity();
	    break;

	  case 'i':
	    if (bigMode == MODE_QUERY)
		poptStuffArgs(optCon, infoCommand);
	    else if (bigMode == MODE_INSTALL)
		/* ignore it */ ;
	    else if (bigMode == MODE_UNKNOWN)
		poptStuffArgs(optCon, installCommand);
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
	    installFlags |= RPMINSTALL_UPGRADE;
	    break;

	  case 's':
	    queryFor |= QUERY_FOR_LIST | QUERY_FOR_STATE;
	    break;

	  case 'l':
	    queryFor |= QUERY_FOR_LIST;
	    break;

	  case 'd':
	    queryFor |= QUERY_FOR_DOCS | QUERY_FOR_LIST;
	    break;

	  case 'c':
	    queryFor |= QUERY_FOR_CONFIG | QUERY_FOR_LIST;
	    break;

	  case 'p':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_RPM)
		argerror(_("one type of query/verify may be performed at a " "time"));
	    querySource = QUERY_RPM;
	    verifySource = VERIFY_RPM;
	    break;

	  case 'g':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_GROUP)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_GROUP;
	    verifySource = VERIFY_GRP;
	    break;

	  case 'f':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_PATH)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_PATH;
	    verifySource = VERIFY_PATH;
	    break;

	  case 'a':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_ALL)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_ALL;
	    verifySource = VERIFY_EVERY;
	    break;

	  case GETOPT_QUERYFORMAT:
	    if (queryFormat) {
		len = strlen(queryFormat) + strlen(optArg) + 1;
		queryFormat = realloc(queryFormat, len);
		strcat(queryFormat, optArg);
	    } else {
		queryFormat = malloc(strlen(optArg) + 1);
		strcpy(queryFormat, optArg);
	    }
	    break;

	  case GETOPT_WHATREQUIRES:
	    if (querySource != QUERY_PACKAGE && 
		querySource != QUERY_WHATREQUIRES)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_WHATREQUIRES;
	    break;

	  case GETOPT_WHATPROVIDES:
	    if (querySource != QUERY_PACKAGE && 
		querySource != QUERY_WHATPROVIDES)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_WHATPROVIDES;
	    break;

	  case GETOPT_REBUILD:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_REBUILD)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_REBUILD;
	    break;

	  case GETOPT_RECOMPILE:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_RECOMPILE)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_RECOMPILE;
	    break;

	  case GETOPT_BUILDROOT:
	    if (bigMode != MODE_UNKNOWN &&
		bigMode != MODE_BUILD && bigMode != MODE_REBUILD)
		argerror(_("only one major mode may be specified"));
	    buildRootOverride = optArg;
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

	  case GETOPT_DBPATH:
            if (optArg[0] != '/')
                argerror(_("arguments to --dbpath must begin with a /"));
	    rpmSetVar(RPMVAR_DBPATH, optArg);
	    gotDbpath = 1;
	    break;

	  case GETOPT_QUERYBYNUMBER:
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_RPM)
		argerror(_("one type of query may be performed at a " 
			    "time"));
	    querySource = QUERY_DBOFFSET;
	    verifySource = VERIFY_RPM;
	    break;

	  case GETOPT_TIMECHECK:
	    tce = NULL;
	    timeCheck = strtoul(optArg, &tce, 10);
	    if ((*tce) || (tce == optArg) || (timeCheck == ULONG_MAX)) {
		argerror("Argument to --timecheck must be integer");
	    }
	    rpmSetVar(RPMVAR_TIMECHECK, optArg);
	    break;

	  case GETOPT_REBUILDDB:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_REBUILDDB)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_REBUILDDB;
	    break;

	  default:
	    fprintf(stderr, "Internal error in argument processing :-(\n");
	    exit(1);
	}
    }

    if (quiet)
	rpmSetVerbosity(RPMMESS_QUIET);

    if (version) printVersion();
    if (help) printHelp();

    if (arg < -1) {
	fprintf(stderr, "%s: %s\n", 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(arg));
	exit(1);
    }

    if (initdb)
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_INITDB;

    if (queryTags)
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_QUERYTAGS;

    if (buildRootOverride && bigMode != MODE_BUILD &&
	bigMode != MODE_REBUILD) {
	argerror("--buildroot may only be used during package builds");
    }

    if (bigMode != MODE_QUERY && bigMode != MODE_INSTALL && 
	bigMode != MODE_UNINSTALL && bigMode != MODE_VERIFY &&
	bigMode != MODE_INITDB && gotDbpath)
	argerror(_("--dbpath given for operation that does not use a "
			"database"));

    if (timeCheck && bigMode != MODE_BUILD && bigMode != MODE_REBUILD &&
	bigMode != MODE_RECOMPILE) 
	argerror(_("--timecheck may only be used during package builds"));
    
    if (bigMode != MODE_QUERY && queryFor) 
	argerror(_("unexpected query specifiers"));

    if (bigMode != MODE_QUERY && queryFormat) 
	argerror(_("unexpected query specifiers"));

    if (bigMode != MODE_QUERY && bigMode != MODE_VERIFY &&
	querySource != QUERY_PACKAGE) 
	argerror(_("unexpected query source"));

    if (bigMode != MODE_INSTALL && force)
	argerror(_("only installation and upgrading may be forced"));

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

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_VERIFY && noScripts)
	argerror(_("--noscripts may only be specified during package "
		   "installation, erasure, and verification"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_VERIFY && noDeps)
	argerror(_("--nodeps may only be specified during package "
		   "installation, erasure, and verification"));

    if (bigMode != MODE_VERIFY && noFiles)
	argerror(_("--nofiles may only be specified during package "
		   "verification"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL &&
	bigMode != MODE_BUILD && test)
	argerror(_("--test may only be specified during package installation, "
		 "erasure, and building"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_QUERY   && bigMode != MODE_VERIFY    && 			bigMode != MODE_REBUILDDB && bigMode != MODE_INITDB && rootdir[1])
	argerror(_("--root (-r) may only be specified during "
		 "installation, erasure, querying, and "
		 "database rebuilds"));

    if (rootdir && rootdir[0] != '/')
	argerror(_("arguments to --root (-r) must begin with a /"));

    if (bigMode != MODE_BUILD && bigMode != MODE_TARBUILD && clean) 
	argerror(_("--clean may only be used during package building"));

    if (bigMode != MODE_BUILD && bigMode != MODE_TARBUILD && shortCircuit) 
	argerror(_("--short-circuit may only be used during package building"));

    if (shortCircuit && (buildChar != 'c') && (buildChar != 'i')) {
	argerror(_("--short-circuit may only be used with -bc, -bi, -tc "
			"or -ti"));
    }

    if (oldPackage && !(installFlags & RPMINSTALL_UPGRADE))
	argerror(_("--oldpackage may only be used during upgrades"));

    if (bigMode != MODE_QUERY && dump) 
	argerror(_("--dump may only be used during queryies"));

    if (bigMode == MODE_QUERY && dump && !(queryFor & QUERY_FOR_LIST))
	argerror(_("--dump of queries must be used with -l, -c, or -d"));

    if ((ftpProxy || ftpPort) && !(bigMode == MODE_INSTALL ||
	((bigMode == MODE_QUERY && querySource == QUERY_RPM)) ||
	((bigMode == MODE_VERIFY && querySource == VERIFY_RPM))))
	argerror(_("ftp options can only be used during package queries, "
		 "installs, and upgrades"));

    if (oldPackage || (force && (installFlags & RPMINSTALL_UPGRADE)))
	installFlags |= RPMINSTALL_UPGRADETOOLD;

    if (noPgp && bigMode != MODE_CHECKSIG)
	argerror(_("--nopgp may only be used during signature checking"));

    if (noMd5 && bigMode != MODE_CHECKSIG && bigMode != MODE_VERIFY)
	argerror(_("--nopgp may only be used during signature checking and "
		   "package verification"));

    if (ftpProxy) rpmSetVar(RPMVAR_FTPPROXY, ftpProxy);
    if (ftpPort) rpmSetVar(RPMVAR_FTPPORT, ftpPort);

    if (signIt) {
        if (bigMode == MODE_REBUILD || bigMode == MODE_BUILD ||
	    bigMode == MODE_RESIGN) {
            if (poptPeekArg(optCon)) {
		switch (rpmLookupSignatureType()) {
		  case RPMSIGTAG_PGP:
		    if (!(passPhrase = rpmGetPassPhrase("Enter pass phrase: "))) {
			fprintf(stderr, _("Pass phrase check failed\n"));
			exit(1);
		    } else {
			fprintf(stderr, _("Pass phrase is good.\n"));
			passPhrase = strdup(passPhrase);
		    }
		    break;
		  case 0:
		    break;
		  default:
		    fprintf(stderr, "Invalid signature spec in rc file\n");
		    exit(1);
		}
	    }
	} else {
	    argerror(_("--sign may only be used during package building"));
	}
    } else {
        /* Override any rpmrc setting */
        rpmSetVar(RPMVAR_SIGTYPE, "none");
    }

    if (pipeOutput) {
	pipe(p);

	if (!(pipeChild = fork())) {
	    close(p[1]);
	    dup2(p[0], 0);
	    close(p[0]);
	    execl("/bin/sh", "/bin/sh", "-c", pipeOutput, NULL);
	    fprintf(stderr, "exec failed\n");
	}

	close(p[0]);
	dup2(p[1], 1);
	close(p[1]);
    }
	
    switch (bigMode) {
      case MODE_UNKNOWN:
	if (!version && !help) printUsage();
	break;

      case MODE_REBUILDDB:
	ec = rpmdbRebuild(rootdir);
	break;

      case MODE_QUERYTAGS:
	if (argc != 2)
	    argerror(_("unexpected arguments to --querytags "));

	queryPrintTags();
	break;

      case MODE_INITDB:
	rpmdbInit(rootdir, 0644);
	break;

      case MODE_CHECKSIG:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for signature check"));
	if (!noPgp) checksigFlags |= CHECKSIG_PGP;
	if (!noMd5) checksigFlags |= CHECKSIG_MD5;
	exit(doCheckSig(checksigFlags, poptGetArgs(optCon)));

      case MODE_RESIGN:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for signing"));
	exit(doReSign(addSign, passPhrase, poptGetArgs(optCon)));
	
      case MODE_REBUILD:
      case MODE_RECOMPILE:
        if (rpmGetVerbosity() == RPMMESS_NORMAL)
	    rpmSetVerbosity(RPMMESS_VERBOSE);

	if (!poptPeekArg(optCon))
	    argerror(_("no packages files given for rebuild"));

	buildAmount = RPMBUILD_PREP | RPMBUILD_BUILD | RPMBUILD_INSTALL |
	    RPMBUILD_SWEEP | RPMBUILD_RMSOURCE;
	if (bigMode == MODE_REBUILD) {
	    buildAmount |= RPMBUILD_BINARY;
	}

	while ((pkg = poptGetArg(optCon))) {
	    if (doSourceInstall("/", pkg, &specFile))
		exit(1);

	    if (build(specFile, buildAmount, passPhrase, buildRootOverride,
			0)) {
		exit(1);
	    }
	}
	break;

      case MODE_BUILD:
      case MODE_TARBUILD:
        if (rpmGetVerbosity() == RPMMESS_NORMAL)
	    rpmSetVerbosity(RPMMESS_VERBOSE);
       
	switch (buildChar) {
	  /* these fallthroughs are intentional */
	  case 'a':
	    buildAmount |= RPMBUILD_SOURCE;
	  case 'b':
	    buildAmount |= RPMBUILD_BINARY;
	  case 'i':
	    buildAmount |= RPMBUILD_INSTALL;
	    if ((buildChar == 'i') && shortCircuit)
		break;
	  case 'c':
	    buildAmount |= RPMBUILD_BUILD;
	    if ((buildChar == 'c') && shortCircuit)
		break;
	  case 'p':
	    buildAmount |= RPMBUILD_PREP;
	    break;
	    
	  case 'l':
	    buildAmount |= RPMBUILD_LIST;
	    break;
	}

	if (clean)
	    buildAmount |= RPMBUILD_SWEEP;

	if (test)
	    buildAmount |= RPMBUILD_TEST;

	if (!poptPeekArg(optCon))
	    if (bigMode == MODE_BUILD)
		argerror(_("no spec files given for build"));
	    else
		argerror(_("no tar files given for build"));

	while ((pkg = poptGetArg(optCon)))
	    if (build(pkg, buildAmount, passPhrase, buildRootOverride,
			bigMode == MODE_TARBUILD)) {
		exit(1);
	    }
	break;

      case MODE_UNINSTALL:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for uninstall"));

	if (noScripts) uninstallFlags |= RPMUNINSTALL_NOSCRIPTS;
	if (test) uninstallFlags |= RPMUNINSTALL_TEST;
	if (noDeps) interfaceFlags |= UNINSTALL_NODEPS;

	ec = doUninstall(rootdir, poptGetArgs(optCon), uninstallFlags, 
		interfaceFlags);
	break;

      case MODE_INSTALL:
	if (force)
	    installFlags |= (RPMINSTALL_REPLACEPKG | RPMINSTALL_REPLACEFILES);
	if (replaceFiles) installFlags |= RPMINSTALL_REPLACEFILES;
	if (replacePackages) installFlags |= RPMINSTALL_REPLACEPKG;
	if (test) installFlags |= RPMINSTALL_TEST;
	if (noScripts) installFlags |= RPMINSTALL_NOSCRIPTS;
	if (ignoreArch) installFlags |= RPMINSTALL_NOARCH;
	if (ignoreOs) installFlags |= RPMINSTALL_NOOS;

	if (showPercents) interfaceFlags |= INSTALL_PERCENT;
	if (showHash) interfaceFlags |= INSTALL_HASH;
	if (noDeps) interfaceFlags |= INSTALL_NODEPS;

	if (!incldocs) {
	    if (excldocs)
		installFlags |= RPMINSTALL_NODOCS;
	    else if (rpmGetBooleanVar(RPMVAR_EXCLUDEDOCS))
		installFlags |= RPMINSTALL_NODOCS;
	}

	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for install"));

	ec += doInstall(rootdir, poptGetArgs(optCon), prefix, installFlags, 
			interfaceFlags);
	break;

      case MODE_QUERY:
	if (dump) queryFor |= QUERY_FOR_DUMPFILES;

	if (querySource == QUERY_ALL) {
	    if (poptPeekArg(optCon))
		argerror(_("extra arguments given for query of all packages"));

	    ec = doQuery(rootdir, QUERY_ALL, queryFor, NULL, queryFormat);
	} else {
	    if (!poptPeekArg(optCon))
		argerror(_("no arguments given for query"));
	    while ((pkg = poptGetArg(optCon)))
		ec = doQuery(rootdir, querySource, queryFor, pkg, queryFormat);
	}
	break;

      case MODE_VERIFY:
	verifyFlags = 0;
	if (!noFiles) verifyFlags |= VERIFY_FILES;
	if (!noDeps) verifyFlags |= VERIFY_DEPS;
	if (!noScripts) verifyFlags |= VERIFY_SCRIPT;
	if (!noMd5) verifyFlags |= VERIFY_MD5;

	if (verifySource == VERIFY_EVERY) {
	    doVerify(rootdir, VERIFY_EVERY, NULL, verifyFlags);
	} else {
	    if (!poptPeekArg(optCon))
		argerror(_("no arguments given for verify"));
	    doVerify(rootdir, verifySource, poptGetArgs(optCon), verifyFlags);
	}
	break;
    }

    poptFreeContext(optCon);

    if (pipeChild) {
	fclose(stdout);
	waitpid(pipeChild, &status, 0);
    }

    /* keeps memory leak checkers quiet */
    if (queryFormat) free(queryFormat);

    return ec;
}
