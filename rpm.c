#include <getopt.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "install.h"
#include "lib/messages.h"
#include "lib/signature.h"
#include "query.h"
#include "verify.h"
#include "checksig.h"
#include "rpmlib.h"
#include "build/build.h"

#define GETOPT_QUERYFORMAT	1000
#define GETOPT_WHATREQUIRES	1001
#define GETOPT_WHATPROVIDES	1002
#define GETOPT_REBUILD		1003
#define GETOPT_RECOMPILE	1004
#define GETOPT_ADDSIGN		1005
#define GETOPT_RESIGN		1006
#define GETOPT_BUILDROOT 	1007
#define GETOPT_PROVIDES		1008
#define GETOPT_QUERYBYNUMBER	1009
#define GETOPT_DBPATH		1010
#define GETOPT_PREFIX		1011
#define GETOPT_TIMECHECK        1012
#define GETOPT_REBUILDDB        1013
#define GETOPT_FTPPORT          1014
#define GETOPT_FTPPROXY         1015

char * version = VERSION;

enum modes { MODE_QUERY, MODE_INSTALL, MODE_UNINSTALL, MODE_VERIFY,
	     MODE_BUILD, MODE_REBUILD, MODE_CHECKSIG, MODE_RESIGN,
	     MODE_RECOMPILE, MODE_QUERYTAGS, MODE_INITDB, 
	     MODE_REBUILDDB, MODE_UNKNOWN };

static void argerror(char * desc);

static void argerror(char * desc) {
    fprintf(stderr, "rpm: %s\n", desc);
    exit(1);
}

void printHelp(void);
void printVersion(void);
void printBanner(void);
void printUsage(void);
int build(char *arg, int buildAmount, char *passPhrase,
	  char *buildRootOverride);

void printVersion(void) {
    printf(_("RPM version %s\n"), version);
}

void printBanner(void) {
    puts(_("Copyright (C) 1995 - Red Hat Software"));
    puts(_("This may be freely redistributed under the terms of the GNU "
	   "Public License"));
}

void printUsage(void) {
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
    puts(_("       rpm {--query -q} [-afFpP] [-i] [-l] [-s] [-d] [-c] [-v] [-R]"));
    puts(_("                        [--scripts] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--whatprovides] [--whatrequires] [--requires]"));
    puts(_("                        [--ftpuseport] [--ftpproxy <host>] [--ftpport <port>]"));
    puts(_("                        [--provides] [--dump] [--dbpath <dir>] [targets]"));
    puts(_("       rpm {--verify -V -y} [-afFpP] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--dbpath <dir>] [--nodeps] [--nofiles] [--noscripts]"));
    puts(_("                        [targets]"));
    puts(_("       rpm {--erase -e] [--root <dir>] [--noscripts] [--rcfile <file>]"));
    puts(_("                        [--dbpath <dir>] [--nodeps] package1 ... packageN"));
    puts(_("       rpm {-b}[plciba] [-v] [--short-circuit] [--clean] [--rcfile  <file>]"));
    puts(_("                        [--sign] [--test] [--timecheck <s>] specfile"));
    puts(_("       rpm {--rebuild} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm"));
    puts(_("       rpm {--recompile} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm"));
    puts(_("       rpm {--resign} [--rcfile <file>] package1 package2 ... packageN"));
    puts(_("       rpm {--addsign} [--rcfile <file>] package1 package2 ... packageN"));
    puts(_("       rpm {--checksig -K} [--nopgp] [--rcfile <file>] package1 ... packageN"));
    puts(_("       rpm {--rebuilddb} [--rcfile <file>] [--dbpath <dir>]"));
    puts(_("       rpm {--querytags}"));
}

void printHelp(void) {
    printVersion();
    printBanner();
    puts(_(""));

    puts(_("usage:"));
    puts(_("   --help		- print this message"));
    puts(_("   --version	- print the version of rpm being used"));
    puts(_("   all modes support the following arguments:"));
    puts(_("      --rcfile <file>     - use <file> instead of /etc/rpmrc and $HOME/.rpmrc"));
    puts(_("       -v		      - be a little more verbose"));
    puts(_("       -vv	              - be incredibly verbose (for debugging)"));
    puts(_("   -q                   - query mode"));
    puts(_("      --root <dir>        - use <dir> as the top level directory"));
    puts(_("      --dbpath <dir>      - use <dir> as the directory for the database"));
    puts(_("      --queryformat <s>   - use s as the header format (implies -i)"));
    puts(_("   install, upgrade and query (with -p) allow ftp URL's to be used in place\n"));
    puts(_("   file names as well as the following options:\n"));
    puts(_("      --ftpproxy <host>   - hostname or IP of ftp proxy\n"));
    puts(_("      --ftpport <port>    - port number of ftp server (or proxy)\n"));
    puts(_("      Package specification options:"));
    puts(_("        -a                - query all packages"));
    puts(_("        -f <file>+        - query package owning <file>"));
    puts(_("        -F                - like -f, but read file names from stdin"));
    puts(_("        -p <packagefile>+ - query (uninstalled) package <packagefile>"));
    puts(_("        -P                - like -p, but read package names from stdin"));
    puts(_("	   --whatprovides <i> - query packages which provide <i> capability"));
    puts(_("	   --whatrequires <i> - query packages which require <i> capability"));
    puts(_("      Information selection options:"));
    puts(_("        -i                - display package information"));
    puts(_("        -l                - display package file list"));
    puts(_("        -s                - show file states (implies -l)"));
    puts(_("        -d                - list only documentation files (implies -l)"));
    puts(_("        -c                - list only configuration files (implies -l)"));
    puts(_("        --dump            - show all verifiable information for each file"));
    puts(_("                            (must be used with -l, -c, or -d)"));
    puts(_("        --provides        - list capabilbities package provides"));
    puts(_("        --requires"));
    puts(_("        -R                - list package dependencies"));
    puts(_("        --scripts         - print the various [un]install scripts"));
    puts(_(""));
    puts(_("    -V"));
    puts(_("    -y"));
    puts(_("    --verify            - verify a package installation"));
    puts(_("			  same package specification options as -q"));
    puts(_("      --dbpath <dir>    - use <dir> as the directory for the database"));
    puts(_("      --root <dir>      - use <dir> as the top level directory"));
    puts(_("      --nodeps          - do not verify package dependencies"));
    puts(_("      --nofiles         - do not verify file attributes"));
    puts(_(""));
    puts(_("    --install <packagefile>"));
    puts(_("    -i <packagefile>	- install package"));
    puts(_("       -h"));
    puts(_("	  --prefix <dir>    - relocate the package to <dir>, if relocatable"));
    puts(_("      --dbpath <dir>    - use <dir> as the directory for the database"));
    puts(_("      --excludedocs     - do not install documentation"));
    puts(_("      --force           - short hand for --replacepkgs --replacefiles"));
    puts(_("      --hash            - print hash marks as package installs (good with -v)"));
    puts(_("      --ignorearch      - don't verify package architecure"));
    puts(_("      --ignoreos        - don't verify package operating system"));
    puts(_("      --includedocs     - install documentation"));
    puts(_("      --nodeps          - do not verify package dependencies"));
    puts(_("      --noscripts       - don't execute any installation scripts"));
    puts(_("      --percent         - print percentages as package installs"));
    puts(_("      --replacefiles    - install even if the package replaces installed files"));
    puts(_("      --replacepkgs     - reinstall if the package is already present"));
    puts(_("      --root <dir>	- use <dir> as the top level directory"));
    puts(_("      --test            - don't install, but tell if it would work or not"));
    puts(_(""));
    puts(_("    --upgrade <packagefile>"));
    puts(_("    -U <packagefile>	- upgrade package (same options as --install, plus)"));
    puts(_("      --oldpackage      - upgrade to an old version of the package (--force"));
    puts(_("                          on upgrades does this automatically)"));
    puts(_(""));
    puts(_("    --erase <package>"));
    puts(_("    -e <package>        - erase (uninstall) package"));
    puts(_("      --dbpath <dir>      - use <dir> as the directory for the database"));
    puts(_("      --nodeps          - do not verify package dependencies"));
    puts(_("      --noscripts       - do not execute any package specific scripts"));
    puts(_("      --root <dir>	- use <dir> as the top level directory"));
    puts(_(""));
    puts(_("    -b<stage> <spec>    - build package, where <stage> is one of:"));
    puts(_("			  p - prep (unpack sources and apply patches)"));
    puts(_("			  l - list check (do some cursory checks on %files)"));
    puts(_("			  c - compile (prep and compile)"));
    puts(_("			  i - install (prep, compile, install)"));
    puts(_("			  b - binary package (prep, compile, install, package)"));
    puts(_("			  a - bin/src package (prep, compile, install, package)"));
    puts(_("      --short-circuit   - skip straight to specified stage (only for c,i)"));
    puts(_("      --clean           - remove build tree when done"));
    puts(_("      --sign            - generate PGP signature"));
    puts(_("      --buildroot <s>   - use s as the build root"));
    puts(_("      --test            - do not execute any stages"));
    puts(_("      --timecheck <s>   - set the time check to S seconds (0 disables it)"));
    puts(_(""));
    puts(_("    --rebuild <source_package>"));
    puts(_("                        - install source package, build binary package,"));
    puts(_("                          and remove spec file, sources, patches, and icons."));
    puts(_("    --recompile <source_package>"));
    puts(_("                        - like --rebuild, but don't package"));
    puts(_("    --resign <pkg>+     - sign a package (discard current signature)"));
    puts(_("    --addsign <pkg>+    - add a signature to a package"));
    puts(_("    -K"));
    puts(_("    --checksig <pkg>+   - verify package signature"));
    puts(_("      --nopgp             - skip any PGP signatures (MD5 only)"));
    puts(_("    --querytags         - list the tags that can be used in a query format"));
    puts(_("    --initdb            - make sure a valid database exists"));
    puts(_("    --rebuilddb         - rebuild database from existing database"));
    puts(_("      --dbpath <dir>    - use <dir> as the directory for the database"));
    puts(_("      --root <dir>	    - use <dir> as the top level directory"));
}

int build(char *arg, int buildAmount, char *passPhrase,
	  char *buildRootOverride) {
    FILE *f;
    Spec s;
    char * specfile;
    int res = 0;
    struct stat statbuf;

    if (arg[0] == '/') {
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
	    /* For now, warn and try to continue */
	    res = 1;
	    fprintf(stderr, "\n%cSpec file check failed!!\n", 7);
	    fprintf(stderr,
		    "Tell rpm-list@redhat.com if this is incorrect.\n\n");
	    sleep(5);
	}
	if (doBuild(s, buildAmount, passPhrase)) {
	    fprintf(stderr, _("Build failed.\n"));
	    res = 1;
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

    return res;
}

int main(int argc, char ** argv) {
    int long_index;
    enum modes bigMode = MODE_UNKNOWN;
    enum querysources querySource = QUERY_PACKAGE;
    enum verifysources verifySource = VERIFY_PACKAGE;
    int arg;
    int queryFor = 0, test = 0, version = 0, help = 0, force = 0;
    int quiet = 0, replaceFiles = 0, replacePackages = 0, showPercents = 0;
    int showHash = 0, installFlags = 0, uninstallFlags = 0, interfaceFlags = 0;
    int buildAmount = 0, oldPackage = 0, clean = 0, signIt = 0;
    int shortCircuit = 0, badOption = 0, queryTags = 0, excldocs = 0;
    int incldocs = 0, queryScripts = 0, noScripts = 0, noDeps = 0;
    int noPgp = 0, dump = 0, initdb = 0, ignoreArch = 0, showrc = 0;
    int gotDbpath = 0, building = 0, ignoreOs = 0, noFiles = 0, verifyFlags;
    char *tce;
    int timeCheck;
    int addSign = NEW_SIGNATURE;
    char * rcfile = NULL, * queryFormat = NULL, * prefix = NULL;
    char buildChar = ' ';
    char * rootdir = "/";
    char * specFile;
    char *passPhrase = "";
    char *buildRootOverride = NULL;
    char *arch = NULL;
    char * ftpProxy = NULL, * ftpPort = NULL;
    char *os = NULL;
    char * smallArgv[2] = { NULL, NULL };
    char ** currarg;
    int ec = 0;
    struct option optionsTable[] = {
	    { "addsign", 0, 0, GETOPT_ADDSIGN },
	    { "all", 0, 0, 'a' },
	    { "build", 1, 0, 'b' },
	    { "buildarch", 1, 0, 0 },
	    { "buildos", 1, 0, 0 },
	    { "buildroot", 1, 0, GETOPT_BUILDROOT },
	    { "checksig", 0, 0, 'K' },
	    { "clean", 0, &clean, 0 },
	    { "configfiles", 0, 0, 'c' },
	    { "dbpath", 1, 0, GETOPT_DBPATH },
	    { "docfiles", 0, 0, 'd' },
	    { "dump", 0, &dump, 0 },
	    { "erase", 0, 0, 'e' },
            { "excludedocs", 0, &excldocs, 0},
	    { "file", 0, 0, 'f' },
	    { "force", 0, &force, 0 },
	    { "ftpport", 1, 0, GETOPT_FTPPORT },
	    { "ftpproxy", 1, 0, GETOPT_FTPPROXY },
	    { "group", 0, 0, 'g' },
	    { "hash", 0, &showHash, 'h' },
	    { "help", 0, &help, 0 },
	    { "ignorearch", 0, &ignoreArch, 0 },
	    { "ignoreos", 0, &ignoreOs, 0 },
	    { "info", 0, 0, 'i' },
            { "includedocs", 0, &incldocs, 0},
	    { "initdb", 0, &initdb, 0 },
	    { "install", 0, 0, 'i' },
	    { "list", 0, 0, 'l' },
	    { "nodeps", 0, &noDeps, 0 },
	    { "nofiles", 0, &noFiles, 0 },
	    { "nopgp", 0, &noPgp, 0 },
	    { "noscripts", 0, &noScripts, 0 },
	    { "oldpackage", 0, &oldPackage, 0 },
	    { "package", 0, 0, 'p' },
	    { "percent", 0, &showPercents, 0 },
	    { "prefix", 1, 0, GETOPT_PREFIX },
	    { "provides", 0, 0, GETOPT_PROVIDES },
	    { "qf", 1, 0, GETOPT_QUERYFORMAT },
	    { "query", 0, 0, 'q' },
	    { "querybynumber", 0, 0, GETOPT_QUERYBYNUMBER },
	    { "queryformat", 1, 0, GETOPT_QUERYFORMAT },
	    { "querytags", 0, &queryTags, 0 },
	    { "quiet", 0, &quiet, 0 },
	    { "rcfile", 1, 0, 0 },
	    { "recompile", 0, 0, GETOPT_RECOMPILE },
	    { "rebuild", 0, 0, GETOPT_REBUILD },
	    { "rebuilddb", 0, 0, GETOPT_REBUILDDB },
	    { "replacefiles", 0, &replaceFiles, 0 },
	    { "replacepkgs", 0, &replacePackages, 0 },
	    { "resign", 0, 0, GETOPT_RESIGN },
	    { "requires", 0, 0, 'R' },
	    { "root", 1, 0, 'r' },
	    { "scripts", 0, &queryScripts, 0 },
	    { "short-circuit", 0, &shortCircuit, 0 },
	    { "showrc", 0, 0, 0 },
	    { "sign", 0, &signIt, 0 },
	    { "state", 0, 0, 's' },
	    { "stdin-files", 0, 0, 'F' },
	    { "stdin-group", 0, 0, 'G' },
	    { "stdin-packages", 0, 0, 'P' },
	    { "stdin-query", 0, 0, 'Q' },
	    { "test", 0, &test, 0 },
	    { "timecheck", 1, 0, GETOPT_TIMECHECK },
	    { "upgrade", 0, 0, 'U' },
	    { "uninstall", 0, 0, 'u' },
	    { "verbose", 0, 0, 'v' },
	    { "verify", 0, 0, 'V' },
	    { "version", 0, &version, 0 },
	    { "whatrequires", 0, 0, GETOPT_WHATREQUIRES },
	    { "whatprovides", 0, 0, GETOPT_WHATPROVIDES },
	    { 0, 0, 0, 0 } 
	} ;
    struct option * options = optionsTable;

    /* set up the correct locale via the LANG environment variable */
    setlocale(LC_ALL, "" );

    bindtextdomain(NLSPACKAGE, "/usr/lib/locale");
    textdomain(NLSPACKAGE);

    /* Make a first pass through the arguments, looking for --rcfile */
    /* as well as --arch and --os.  We need to handle that before    */
    /* dealing with the rest of the arguments.                       */
    currarg = argv;
    while (*currarg) {
	if (!strcmp(*currarg, "--rcfile")) {
	    rcfile = *(currarg + 1);
	} else if (!strcmp(*currarg, "--buildarch")) {
	    arch = *(currarg + 1);
	} else if (!strcmp(*currarg, "--buildos")) {
	    os = *(currarg + 1);
	} else if (!strcmp(*currarg, "--showrc")) {
	    showrc = 1;
	    building = 1;
	} else if (!strncmp(*currarg, "-b", 2) ||
		   !strcmp(*currarg, "--build") ||
		   !strcmp(*currarg, "--rebuild") ||
		   !strcmp(*currarg, "--recompile")) {
	    building = 1;
	}
	currarg++;
    } 

    /* reading this early makes it easy to override */
    if (rpmReadConfigFiles(rcfile, arch, os, building))  
	exit(1);
    if (showrc) {
	rpmShowRC(stdout);
	exit(0);
    }

    while (1) {
	long_index = 0;

	arg = getopt_long(argc, argv, "RQqVyUYhpvKPfFilseagGDducr:b:", options, 
			  &long_index);
	if (arg == -1) break;

	switch (arg) {
	  case '?':
	    badOption = 1;
	    break;

	  case 'K':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_CHECKSIG)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_CHECKSIG;
	    break;
	    
	  case 'Q':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_SPACKAGE)
		argerror(_("only one type of query may be performed at a "
			   "time"));
	    querySource = QUERY_SPACKAGE;
	    /* fallthrough */
	  case 'q':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_QUERY)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_QUERY;
	    break;

	  case 'Y':
	    if (verifySource != VERIFY_PACKAGE && 
		verifySource != VERIFY_SPACKAGE)
		argerror(_("only one type of verify may be performed at a "
				"time"));
	    verifySource = VERIFY_SPACKAGE;
	    /* fallthrough */
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
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_BUILD)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_BUILD;

	    if (strlen(optarg) > 1)
		argerror(_("--build (-b) requires one of a,b,i,c,p,l as "
			 "its sole argument"));

	    buildChar = optarg[0];
	    switch (buildChar) {
	      case 'a':
	      case 'b':
	      case 'i':
	      case 'c':
	      case 'p':
	      case 'l':
		break;
	      default:
		argerror(_("--build (-b) requires one of a,b,i,c,p,l as "
			 "its sole argument"));
	    }

	    break;
	
	  case 'v':
	    rpmIncreaseVerbosity();
	    break;

	  case 'i':
	    if (!long_index) {
		if (bigMode == MODE_QUERY)
		    queryFor |= QUERY_FOR_INFO;
		else if (bigMode == MODE_INSTALL)
		    /* ignore it */ ;
		else if (bigMode == MODE_UNKNOWN)
		    bigMode = MODE_INSTALL;
	    }
	    else if (!strcmp(options[long_index].name, "info"))
		queryFor |= QUERY_FOR_INFO;
	    else 
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

	  case 'R':
	    queryFor |= QUERY_FOR_REQUIRES;
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

	  case 'P':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_SRPM)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_SRPM;
	    verifySource = VERIFY_SRPM;
	    break;

	  case 'p':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_RPM)
		argerror(_("one type of query/verify may be performed at a " "time"));
	    querySource = QUERY_RPM;
	    verifySource = VERIFY_RPM;
	    break;

	  case 'G':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_SGROUP)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_SGROUP;
	    verifySource = VERIFY_SGROUP;
	    break;

	  case 'g':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_GROUP)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_GROUP;
	    verifySource = VERIFY_GRP;
	    break;

	  case 'F':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_SPATH)
		argerror(_("one type of query/verify may be performed at a "
				"time"));
	    querySource = QUERY_SPATH;
	    verifySource = VERIFY_SPATH;
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

	  case 'h':
	    showHash = 1;
	    break;

	  case 'r':
	    if (optarg[0] != '/') 
		argerror(_("arguments to --root (-r) must begin with a /"));
	    rootdir = optarg;
	    break;

	  case GETOPT_PREFIX:
	    if (optarg[0] != '/') 
		argerror(_("arguments to --prefix must begin with a /"));
	    prefix = optarg;
	    break;

	  case GETOPT_QUERYFORMAT:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_QUERY)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_QUERY;
	    queryFormat = optarg;
	    queryFor |= QUERY_FOR_INFO;
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
	    buildRootOverride = optarg;
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

	  case GETOPT_PROVIDES:
	    queryFor |= QUERY_FOR_PROVIDES;
	    break;

	  case GETOPT_DBPATH:
            if (optarg[0] != '/')
                argerror(_("arguments to --dbpath must begin with a /"));
	    rpmSetVar(RPMVAR_DBPATH, optarg);
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
	    timeCheck = strtoul(optarg, &tce, 10);
	    if ((*tce) || (tce == optarg) || (timeCheck == ULONG_MAX)) {
		argerror("Argument to --timecheck must be integer");
	    }
	    rpmSetVar(RPMVAR_TIMECHECK, optarg);
	    break;

	  case GETOPT_REBUILDDB:
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_REBUILDDB)
		argerror(_("only one major mode may be specified"));
	    bigMode = MODE_REBUILDDB;
	    break;

	  case GETOPT_FTPPORT:
	    ftpPort = optarg;
	    break;

	  case GETOPT_FTPPROXY:
	    ftpProxy = optarg;
	    break;
	    
	  default:
	    if (options[long_index].flag) {
		*options[long_index].flag = 1;
	    }
	}
    }

    if (quiet)
	rpmSetVerbosity(RPMMESS_QUIET);

    if (version) printVersion();
    if (help) printHelp();

    if (badOption)
	exit(1);

    if (queryScripts) {
	queryFor |= QUERY_FOR_SCRIPTS;
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
    
    if (bigMode != MODE_QUERY && queryFor) 
	argerror(_("unexpected query specifiers"));

    if (bigMode != MODE_QUERY && bigMode != MODE_VERIFY &&
	querySource != QUERY_PACKAGE) 
	argerror(_("unexpected query source"));

    if (bigMode != MODE_INSTALL && force)
	argerror(_("only installation and upgrading may be forced"));

    if (bigMode != MODE_INSTALL && prefix)
	argerror(_("--prefix may only be used when installing new packages"));

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

    if (bigMode != MODE_BUILD && clean) 
	argerror(_("--clean may only be used during package building"));

    if (bigMode != MODE_BUILD && shortCircuit) 
	argerror(_("--short-circuit may only be used during package building"));

    if (shortCircuit && (buildChar != 'c') && (buildChar != 'i')) {
	argerror(_("--short-circuit may only be used with -bc or -bi"));
    }

    if (oldPackage && !(installFlags & RPMINSTALL_UPGRADE))
	argerror(_("--oldpackage may only be used during upgrades"));

    if (bigMode != MODE_QUERY && dump) 
	argerror(_("--dump may only be used during queryies"));

    if (bigMode == MODE_QUERY && dump && !(queryFor & QUERY_FOR_LIST))
	argerror(_("--dump of queries must be used with -l, -c, or -d"));

    if ((ftpProxy || ftpPort) && !(bigMode == MODE_INSTALL ||
	(bigMode == MODE_QUERY && (querySource == QUERY_RPM ||
	    querySource == QUERY_SRPM)))) 
	argerror(_("ftp options can only be used during package queries, "
		 "installs, and upgrades"));

    if (oldPackage || (force && (installFlags & RPMINSTALL_UPGRADE)))
	installFlags |= RPMINSTALL_UPGRADETOOLD;

    if (ftpProxy) rpmSetVar(RPMVAR_FTPPROXY, ftpProxy);
    if (ftpPort) rpmSetVar(RPMVAR_FTPPORT, ftpPort);

    if (signIt) {
        if (bigMode == MODE_REBUILD || bigMode == MODE_BUILD ||
	    bigMode == MODE_RESIGN) {
            if (optind != argc) {
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
	if (optind == argc) 
	    argerror(_("no packages given for signature check"));
	exit(doCheckSig(1-noPgp, argv + optind));

      case MODE_RESIGN:
	if (optind == argc) 
	    argerror(_("no packages given for signing"));
	exit(doReSign(addSign, passPhrase, argv + optind));
	
      case MODE_REBUILD:
      case MODE_RECOMPILE:
        if (rpmGetVerbosity() == RPMMESS_NORMAL)
	    rpmSetVerbosity(RPMMESS_VERBOSE);

        if (optind == argc) 
	    argerror(_("no packages files given for rebuild"));

	buildAmount = RPMBUILD_PREP | RPMBUILD_BUILD | RPMBUILD_INSTALL |
	    RPMBUILD_SWEEP | RPMBUILD_RMSOURCE;
	if (bigMode == MODE_REBUILD) {
	    buildAmount |= RPMBUILD_BINARY;
	}

	while (optind < argc) {
	    if (doSourceInstall("/", argv[optind++], &specFile))
		exit(1);

	    if (build(specFile, buildAmount, passPhrase, buildRootOverride)) {
		exit(1);
	    }
	}
	break;

      case MODE_BUILD:
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

	if (optind == argc) 
	    argerror(_("no spec files given for build"));

	while (optind < argc) 
	    if (build(argv[optind++], buildAmount,
		      passPhrase, buildRootOverride)) {
		exit(1);
	    }
	break;

      case MODE_UNINSTALL:
	if (optind == argc) 
	    argerror(_("no packages given for uninstall"));

	if (noScripts) uninstallFlags |= RPMUNINSTALL_NOSCRIPTS;
	if (test) uninstallFlags |= RPMUNINSTALL_TEST;
	if (noDeps) interfaceFlags |= UNINSTALL_NODEPS;

	ec = doUninstall(rootdir, argv + optind, uninstallFlags, 
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

	if (optind == argc) 
	    argerror(_("no packages given for install"));

	ec += doInstall(rootdir, argv + optind, prefix, installFlags, 
			interfaceFlags);
	break;

      case MODE_QUERY:
	if (dump) queryFor |= QUERY_FOR_DUMPFILES;

	if (querySource == QUERY_ALL) {
	    if (optind != argc) 
		argerror(_("extra arguments given for query of all packages"));

	    ec = doQuery(rootdir, QUERY_ALL, queryFor, NULL, queryFormat);
	} else if (querySource == QUERY_SPATH || 
                   querySource == QUERY_SPACKAGE ||
		   querySource == QUERY_SRPM) {
	    char buffer[255];
	    int i;

	    while (fgets(buffer, 255, stdin)) {
		i = strlen(buffer) - 1;
		if (buffer[i] == '\n') buffer[i] = 0;
		if (strlen(buffer)) 
		    ec += doQuery(rootdir, querySource, queryFor, buffer, 
				  queryFormat);
	    }
	} else {
	    if (optind == argc) 
		argerror(_("no arguments given for query"));
	    while (optind < argc) 
		ec = doQuery(rootdir, querySource, queryFor, argv[optind++], 
			     queryFormat);
	}
	break;

      case MODE_VERIFY:
	verifyFlags = 0;
	if (!noFiles) verifyFlags |= VERIFY_FILES;
	if (!noDeps) verifyFlags |= VERIFY_DEPS;
	if (!noScripts) verifyFlags |= VERIFY_SCRIPT;

	if (verifySource == VERIFY_EVERY) {
	    doVerify(rootdir, VERIFY_EVERY, NULL, verifyFlags);
	} else if (verifySource == VERIFY_SPATH || 
                   verifySource == VERIFY_SPACKAGE ||
		   verifySource == VERIFY_SRPM) {
	    char buffer[255];
	    int i;

	    smallArgv[0] = buffer;
	    while (fgets(buffer, 255, stdin)) {
		i = strlen(buffer) - 1;
		if (buffer[i] == '\n') buffer[i] = 0;
		if (strlen(buffer))
		    doVerify(rootdir, verifySource, smallArgv, verifyFlags);
	    }
	} else {
	    if (optind == argc) 
		argerror(_("no arguments given for verify"));
	    doVerify(rootdir, verifySource, argv + optind, verifyFlags);
	}
	break;
    }

    return ec;
}
