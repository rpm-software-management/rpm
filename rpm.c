#include <getopt.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "install.h"
#include "lib/rpmerr.h"
#include "lib/messages.h"
#include "lib/signature.h"
#include "query.h"
#include "verify.h"
#include "checksig.h"
#include "rpmlib.h"
#include "build/build.h"

#define _(String) gettext(String)

char * version = VERSION;

enum modes { MODE_QUERY, MODE_INSTALL, MODE_UNINSTALL, MODE_VERIFY,
	     MODE_BUILD, MODE_REBUILD, MODE_CHECKSIG, MODE_RESIGN,
	     MODE_RECOMPILE, MODE_QUERYTAGS, MODE_INITDB, MODE_UNKNOWN };

static void argerror(char * desc);

static void argerror(char * desc) {
    fprintf(stderr, "rpm: %s\n", desc);
    exit(1);
}

void printHelp(void);
void printVersion(void);
void printBanner(void);
void printUsage(void);
int build(char * arg, int buildAmount, char *passPhrase);

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
    puts(_("       rpm {--initdb}"));
    puts(_("       rpm {--install -i} [-v] [--hash -h] [--percent] [--force] [--test]"));
    puts(_("                          [--replacepkgs] [--replacefiles] [--root <dir>]"));
    puts(_("                          [--excludedocs] [--includedocs] [--noscripts]"));
    puts(_("                          [--rcfile <file>] file1.rpm ... fileN.rpm"));
    puts(_("       rpm {--upgrade -U} [-v] [--hash -h] [--percent] [--force] [--test]"));
    puts(_("                          [--oldpackage] [--root <dir>] [--noscripts]"));
    puts(_("                          [--excludedocs] [--includedocs] [--rcfile <file>]"));
    puts(_("                          file1.rpm ... fileN.rpm"));
    puts(_("       rpm {--query -q} [-afFpP] [-i] [-l] [-s] [-d] [-c] [-v] [-R]"));
    puts(_("                        [--scripts] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--whatprovides] [--whatrequires] [--requires]"));
    puts(_("                        [--provides] [--dump] [targets]"));
    puts(_("       rpm {--verify -V -y] [-afFpP] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [targets]"));
    puts(_("       rpm {--erase -e] [--root <dir>] [--noscripts] [--rcfile <file>]"));
    puts(_("                        package1 package2 ... packageN"));
    puts(_("       rpm {-b}[plciba] [-v] [--short-circuit] [--clean] [--rcfile  <file>]"));
    puts(_("                        [--sign] [--test] [--time-check <s>] specfile"));
    puts(_("       rpm {--rebuild} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm"));
    puts(_("       rpm {--recompile} [--rcfile <file>] [-v] source1.rpm ... sourceN.rpm"));
    puts(_("       rpm {--resign} [--rcfile <file>] package1 package2 ... packageN"));
    puts(_("       rpm {--checksig} [--rcfile <file>] package1 package2 ... packageN"));
    puts(_("       rpm {--querytags}"));
}

void printHelp(void) {
    printVersion();
    printBanner();
    puts(_(""));

    puts(_("usage:"));
    puts(_("   --help		- print this message"));
    puts(_("   --version	- print the version of rpm being used"));
    puts(_("   all modes support the following arguments"));
    puts(_("      --rcfile <file>     - use <file> instead of /etc/rpmrc and $HOME/.rpmrc"));
    puts(_("       -v		      - be a little more verbose"));
    puts(_("       -vv		      - be incredible verbose (for debugging)"));
    puts(_("   -q                   - query mode"));
    puts(_("      --root <dir>        - use <dir> as the top level directory"));
    puts(_("      --queryformat <s>   - use s as the header format (implies -i)"));
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
    puts(_("      --root <dir>	- use <dir> as the top level directory"));
    puts(_(""));
    puts(_("    --install <packagefile>"));
    puts(_("    -i <packagefile>	- install package"));
    puts(_("       -h"));
    puts(_("      --hash            - print hash marks as package installs (good with -v)"));
    puts(_("      --percent         - print percentages as package installs"));
    puts(_("      --replacepkgs     - reinstall if the package is already present"));
    puts(_("      --replacefiles    - install even if the package replaces installed files"));
    puts(_("      --force           - short hand for --replacepkgs --replacefiles"));
    puts(_("      --test            - don't install, but tell if it would work or not"));
    puts(_("      --noscripts       - don't execute any installation scripts"));
    puts(_("      --excludedocs     - do not install documentation"));
    puts(_("      --includedocs     - install documentation"));
    puts(_("      --root <dir>	- use <dir> as the top level directory"));
    puts(_(""));
    puts(_("    --upgrade <packagefile>"));
    puts(_("    -U <packagefile>	- upgrade package (same options as --install, plus)"));
    puts(_("      --oldpackage      - upgrade to an old version of the package (--force"));
    puts(_("                          on upgrades does this automatically)"));
    puts(_(""));
    puts(_("    --erase <package>"));
    puts(_("    -e <package>        - uninstall (erase) package"));
    puts(_("      --noscripts       - don't execute any installation scripts"));
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
    puts(_("      --test            - do not execute any stages"));
    puts(_("      --time-check <s>  - set the time check to S seconds (0 disables it)"));
    puts(_(""));
    puts(_("    --rebuild <source_package>"));
    puts(_("                        - install source package, build binary package,"));
    puts(_("                          and remove spec file, sources, patches, and icons."));
    puts(_("    --recompile <source_package>"));
    puts(_("                        - like --rebuild, but don't package"));
    puts(_("    --resign <pkg>+     - sign a package"));
    puts(_("    -K"));
    puts(_("    --checksig <pkg>+   - verify package signature"));
    puts(_("      --nopgp             - skip any PGP signatures (MD5 only)"));
    puts(_("    --querytags         - list the tags that can be used in a query format"));
    puts(_("    --initdb            - make sure a valid database exists"));
}

int build(char * arg, int buildAmount, char *passPhrase) {
    FILE *f;
    Spec s;
    char * specfile;
    int res = 0;

    if (arg[0] == '/') {
	specfile = arg;
    } else {
	/* XXX this is broken if PWD is near 1024 */
	specfile = alloca(1024);
	getcwd(specfile, 1024);
	strcat(specfile, "/");
	strcat(specfile, arg);
    }

    if (!(f = fopen(specfile, "r"))) {
	fprintf(stderr, _("unable to open: %s\n"), specfile);
	return 1;
    }
    s = parseSpec(f, specfile);
    fclose(f);
    if (s) {
	if (verifySpec(s)) {
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
	if (errCode() == RPMERR_BADARCH) {
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
    int noPgp = 0, dump = 0, initdb = 0;
    char * rcfile = NULL;
    char * queryFormat = NULL;
    char buildChar = ' ';
    char * prefix = "/";
    char * specFile;
    char *passPhrase = "";
    char *arch = NULL;
    char *os = NULL;
    char * smallArgv[2] = { NULL, NULL };
    char ** currarg;
    int ec = 0;
    struct option optionsTable[] = {
	    { "all", 0, 0, 'a' },
	    { "arch", 1, 0, 0 },
	    { "build", 1, 0, 'b' },
	    { "checksig", 0, 0, 'K' },
	    { "clean", 0, &clean, 0 },
	    { "configfiles", 0, 0, 'c' },
	    { "docfiles", 0, 0, 'd' },
	    { "dump", 0, &dump, 0 },
	    { "erase", 0, 0, 'e' },
            { "excludedocs", 0, &excldocs, 0},
	    { "file", 0, 0, 'f' },
	    { "force", 0, &force, 0 },
	    { "group", 0, 0, 'g' },
	    { "hash", 0, &showHash, 'h' },
	    { "help", 0, &help, 0 },
	    { "info", 0, 0, 'i' },
            { "includedocs", 0, &incldocs, 0},
	    { "initdb", 0, &initdb, 0 },
	    { "install", 0, 0, 'i' },
	    { "list", 0, 0, 'l' },
	    { "nodeps", 0, &noDeps, 0 },
	    { "nopgp", 0, &noPgp, 0 },
	    { "noscripts", 0, &noScripts, 0 },
	    { "oldpackage", 0, &oldPackage, 0 },
	    { "os", 1, 0, 0 },
	    { "package", 0, 0, 'p' },
	    { "percent", 0, &showPercents, 0 },
	    { "provides", 0, 0, 0 },
	    { "query", 0, 0, 'q' },
	    { "querybynumber", 0, 0, 0 },
	    { "queryformat", 1, 0, 0 },
	    { "querytags", 0, &queryTags, 0 },
	    { "quiet", 0, &quiet, 0 },
	    { "rcfile", 1, 0, 0 },
	    { "recompile", 0, 0, 0 },
	    { "rebuild", 0, 0, 0 },
	    { "replacefiles", 0, &replaceFiles, 0 },
	    { "replacepkgs", 0, &replacePackages, 0 },
	    { "resign", 0, 0, 0 },
	    { "requires", 0, 0, 'R' },
	    { "root", 1, 0, 'r' },
	    { "scripts", 0, &queryScripts, 0 },
	    { "short-circuit", 0, &shortCircuit, 0 },
	    { "sign", 0, &signIt, 0 },
	    { "state", 0, 0, 's' },
	    { "stdin-files", 0, 0, 'F' },
	    { "stdin-group", 0, 0, 'G' },
	    { "stdin-packages", 0, 0, 'P' },
	    { "stdin-query", 0, 0, 'Q' },
	    { "test", 0, &test, 0 },
	    { "upgrade", 0, 0, 'U' },
	    { "uninstall", 0, 0, 'u' },
	    { "verbose", 0, 0, 'v' },
	    { "verify", 0, 0, 'V' },
	    { "version", 0, &version, 0 },
	    { "whatrequires", 0, 0, 0 },
	    { "whatprovides", 0, 0, 0 },
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
	} else if (!strcmp(*currarg, "--arch")) {
	    arch = *(currarg + 1);
	} else if (!strcmp(*currarg, "--os")) {
	    os = *(currarg + 1);
	}
	currarg++;
    } 

    /* reading this early makes it easy to override */
    if (readConfigFiles(rcfile, arch, os))  
	exit(1);

    while (1) {
	long_index = 0;

	arg = getopt_long(argc, argv, "QqVyUYhpvKPfFilseagGDducr:b:", options, 
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
	    message(MESS_WARNING, _("-u and --uninstall are depricated and will"
		    " be removed soon.\n"));
	    message(MESS_WARNING, _("Use -e or --erase instead.\n"));
	    break;
	
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
	    increaseVerbosity();
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
	    installFlags |= INSTALL_UPGRADE;
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
		argerror("one type of query/verify may be performed at a time");
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
	    prefix = optarg;
	    break;

	  default:
	    if (options[long_index].flag)
		*options[long_index].flag = 1;
	    else if (!strcmp(options[long_index].name, "whatrequires")) {
		if (querySource != QUERY_PACKAGE && 
		    querySource != QUERY_WHATREQUIRES)
		    argerror(_("one type of query/verify may be performed at a "
				    "time"));
		querySource = QUERY_WHATREQUIRES;
	    } else if (!strcmp(options[long_index].name, "whatprovides")) {
		if (querySource != QUERY_PACKAGE && 
		    querySource != QUERY_WHATPROVIDES)
		    argerror(_("one type of query/verify may be performed at a "
				    "time"));
		querySource = QUERY_WHATPROVIDES;
	    } else if (!strcmp(options[long_index].name, "provides")) {
		queryFor |= QUERY_FOR_PROVIDES;
	    } else if (!strcmp(options[long_index].name, "rebuild")) {
		if (bigMode != MODE_UNKNOWN && bigMode != MODE_REBUILD)
		    argerror(_("only one major mode may be specified"));
		bigMode = MODE_REBUILD;
	    } else if (!strcmp(options[long_index].name, "recompile")) {
		if (bigMode != MODE_UNKNOWN && bigMode != MODE_RECOMPILE)
		    argerror(_("only one major mode may be specified"));
		bigMode = MODE_RECOMPILE;
	    } else if (!strcmp(options[long_index].name, "resign")) {
		if (bigMode != MODE_UNKNOWN && bigMode != MODE_RESIGN)
		    argerror(_("only one major mode may be specified"));
		bigMode = MODE_RESIGN;
		signIt = 1;
	    } else if (!strcmp(options[long_index].name, "querybynumber")) {
		if (querySource != QUERY_PACKAGE && querySource != QUERY_RPM)
		    argerror(_("one type of query may be performed at a " "time"));
		querySource = QUERY_DBOFFSET;
		verifySource = VERIFY_RPM;
	    } else if (!strcmp(options[long_index].name, "queryformat")) {
		if (bigMode != MODE_UNKNOWN && bigMode != MODE_QUERY)
		    argerror(_("only one major mode may be specified"));
		bigMode = MODE_QUERY;
		queryFormat = optarg;
		queryFor |= QUERY_FOR_INFO;
	    }
	}
    }

    if (quiet)
	setVerbosity(MESS_QUIET);

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

    if (bigMode != MODE_QUERY && queryFor) 
	argerror(_("unexpected query specifiers"));

    if (bigMode != MODE_QUERY && bigMode != MODE_VERIFY &&
	querySource != QUERY_PACKAGE) 
	argerror(_("unexpected query source"));

    if (bigMode != MODE_INSTALL && force)
	argerror(_("only installation and upgrading may be forced"));

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
	argerror(_("--exclude-docs may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && incldocs)
	argerror(_("--include-docs may only be specified during package "
		   "installation"));

    if (excldocs && incldocs)
	argerror(_("only one of --exclude-docs and --include-docs may be "
		 "specified"));
  
    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && noScripts)
	argerror(_("--noscripts may only be specified during package "
		   "installation and uninstallation"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && noDeps)
	argerror(_("--nodeps may only be specified during package "
		   "installation, uninstallation, and verification"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && test)
	argerror(_("--test may only be specified during package installation "
		 "and uninstallation"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_QUERY   && bigMode != MODE_VERIFY    && prefix[1])
	argerror(_("--root (-r) may only be specified during "
		 "installation, uninstallation, and querying"));

    if (bigMode != MODE_BUILD && clean) 
	argerror(_("--clean may only be used during package building"));

    if (bigMode != MODE_BUILD && shortCircuit) 
	argerror(_("--short-circuit may only be used during package building"));

    if (shortCircuit && (buildChar != 'c') && (buildChar != 'i')) {
	argerror(_("--short-circuit may only be used with -bc or -bi"));
    }

    if (oldPackage && !(installFlags & INSTALL_UPGRADE))
	argerror(_("--oldpackage may only be used during upgrades"));

    if (bigMode != MODE_QUERY && dump) 
	argerror(_("--dump may only be used during queryies"));

    if (bigMode == MODE_QUERY && dump && !(queryFor & QUERY_FOR_LIST))
	argerror(_("--dump of queries must be used with -l, -c, or -d"));

    if (oldPackage || (force && (installFlags & INSTALL_UPGRADE)))
	installFlags |= INSTALL_UPGRADETOOLD;

    if (signIt) {
        if (bigMode == MODE_REBUILD || bigMode == MODE_BUILD ||
	    bigMode == MODE_RESIGN) {
            if ((optind != argc) && (sigLookupType() == RPMSIG_MD5_PGP)) {
	        if (!(passPhrase = getPassPhrase("Enter pass phrase: "))) {
		    fprintf(stderr, _("Pass phrase check failed\n"));
		    exit(1);
		} else {
		    fprintf(stderr, _("Pass phrase is good.\n"));
		    passPhrase = strdup(passPhrase);
		}
	    }
	} else {
	    argerror(_("--sign may only be used during package building"));
	}
    } else {
        /* Override any rpmrc setting */
        setVar(RPMVAR_SIGTYPE, "none");
    }
	
    switch (bigMode) {
      case MODE_UNKNOWN:
	if (!version && !help) printUsage();
	break;

      case MODE_QUERYTAGS:
	if (argc != 2)
	    argerror(_("unexpected arguments to --querytags "));

	queryPrintTags();
	break;

      case MODE_INITDB:
	if (argc != 2)
	    argerror(_("unexpected arguments to --initdb "));

	rpmdbInit(prefix, 0644);
	break;

      case MODE_CHECKSIG:
	if (optind == argc) 
	    argerror(_("no packages given for signature check"));
	exit(doCheckSig(1-noPgp, argv + optind));

      case MODE_RESIGN:
	if (optind == argc) 
	    argerror(_("no packages given for signing"));
	exit(doReSign(passPhrase, argv + optind));
	
      case MODE_REBUILD:
      case MODE_RECOMPILE:
        if (getVerbosity() == MESS_NORMAL)
	    setVerbosity(MESS_VERBOSE);

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

	    if (build(specFile, buildAmount, passPhrase)) {
		exit(1);
	    }
	}
	break;

      case MODE_BUILD:
        if (getVerbosity() == MESS_NORMAL)
	    setVerbosity(MESS_VERBOSE);
       
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

	if (optind == argc) 
	    argerror(_("no spec files given for build"));

	while (optind < argc) 
	    if (build(argv[optind++], buildAmount, passPhrase)) {
		exit(1);
	    }
	break;

      case MODE_UNINSTALL:
	if (optind == argc) 
	    argerror(_("no packages given for uninstall"));

	if (noScripts) uninstallFlags |= UNINSTALL_NOSCRIPTS;
	if (test) uninstallFlags |= UNINSTALL_TEST;
	if (noDeps) interfaceFlags |= RPMUNINSTALL_NODEPS;

	doUninstall(prefix, argv + optind, uninstallFlags, interfaceFlags);
	break;

      case MODE_INSTALL:
	if (force) installFlags |= (INSTALL_REPLACEPKG | INSTALL_REPLACEFILES);
	if (replaceFiles) installFlags |= INSTALL_REPLACEFILES;
	if (replacePackages) installFlags |= INSTALL_REPLACEPKG;
	if (test) installFlags |= INSTALL_TEST;
	if (noScripts) installFlags |= INSTALL_NOSCRIPTS;

	if (showPercents) interfaceFlags |= RPMINSTALL_PERCENT;
	if (showHash) interfaceFlags |= RPMINSTALL_HASH;
	if (noDeps) interfaceFlags |= RPMINSTALL_NODEPS;

	if (!incldocs) {
	    if (excldocs)
		installFlags |= INSTALL_NODOCS;
	    else if (getBooleanVar(RPMVAR_EXCLUDEDOCS))
		installFlags |= INSTALL_NODOCS;
	}

	if (optind == argc) 
	    argerror(_("no packages given for install"));

	ec += doInstall(prefix, argv + optind, installFlags, interfaceFlags);
	break;

      case MODE_QUERY:
	if (dump) queryFor |= QUERY_FOR_DUMPFILES;

	if (querySource == QUERY_ALL) {
	    if (optind != argc) 
		argerror(_("extra arguments given for query of all packages"));

	    ec = doQuery(prefix, QUERY_ALL, queryFor, NULL, queryFormat);
	} else if (querySource == QUERY_SPATH || 
                   querySource == QUERY_SPACKAGE ||
		   querySource == QUERY_SRPM) {
	    char buffer[255];
	    int i;

	    while (fgets(buffer, 255, stdin)) {
		i = strlen(buffer) - 1;
		if (buffer[i] == '\n') buffer[i] = 0;
		if (strlen(buffer)) 
		    ec += doQuery(prefix, querySource, queryFor, buffer, 
				  queryFormat);
	    }
	} else {
	    if (optind == argc) 
		argerror(_("no arguments given for query"));
	    while (optind < argc) 
		ec = doQuery(prefix, querySource, queryFor, argv[optind++], 
			     queryFormat);
	}
	break;

      case MODE_VERIFY:
	if (verifySource == VERIFY_EVERY) {
	    doVerify(prefix, VERIFY_EVERY, NULL);
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
		    doVerify(prefix, verifySource, smallArgv);
	    }
	} else {
	    if (optind == argc) 
		argerror(_("no arguments given for verify"));
	    doVerify(prefix, verifySource, argv + optind);
	}
	break;
    }

    return ec;
}
