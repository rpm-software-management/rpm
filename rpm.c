#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "install.h"
#include "lib/rpmerr.h"
#include "lib/messages.h"
#include "query.h"
#include "rpmlib.h"
#include "build/build.h"

char * version = VERSION;

enum modes { MODE_QUERY, MODE_INSTALL, MODE_UNINSTALL, MODE_VERIFY,
	     MODE_BUILD, MODE_REBUILD, MODE_UNKNOWN };

static void argerror(char * desc);

static void argerror(char * desc) {
    fprintf(stderr, "rpm: %s\n", desc);
    exit(1);
}

void printHelp(void);
void printVersion(void);
void printBanner(void);
void printUsage(void);
void build(char * arg, int buildAmount);

void printVersion(void) {
    printf("RPM version %s\n", version);
}

void printBanner(void) {
    puts("Copyright (C) 1995 - Red Hat Software");
    puts("This may be freely redistributed under the terms of the GNU "
	 "Public License");
}

void printUsage(void) {
    printVersion();
    printBanner();
    puts("");

    puts("usage: rpm {--help}");
    puts("       rpm {--version}");
    puts("       rpm {--install -i} [-v] [--hash -h] [--percent] [--force] [--test]");
    puts("                          [--replacepkgs] [--replacefiles] [--search]");
    puts("                          [--root <dir>] file1.rpm ... filen.rpm");
    puts("       rpm {--upgrage -U} [-v] [--hash -h] [--percent] [--force] [--test]");
    puts("                          [--search] [--root <dir>] file1.rpm ... fileN.rpm");
    puts("       rpm {--query -q} [-afFpP] [-i] [-l] [-s] [-d] [-c] [-v] ");
    puts("                        [--root <dir>] [targets]");
    puts("       rpm {--verify -V -y] [-afFpP] [--root <dir>] [targets]");
    puts("       rpm {--uninstall -u] [--root <dir>] package1 package2 ... packageN");
    puts("       rpm {-b}[plciba] [-v] [--short-circuit] [--clean] [--keep-temps]");
    puts("                        [--test] [--time-check <s>] specfile");
    puts("       rpm {--rebuild} [-v] source1.rpm source2.rpm ... sourceN.rpm");
    puts("       rpm {--where} package1 package2 ... packageN");
}

void printHelp(void) {
    printVersion();
    printBanner();
    puts("");

    puts("usage:");
    puts("   --help		- print this message");
    puts("   --version		- print the version of rpm being used");
    puts("    -q                  - query mode");
    puts("      --root <dir>	- use <dir> as the top level directory");
    puts("      Package specification options:");
    puts("        -a                - query all packages");
    puts("        -f <file>+        - query package owning <file>");
    puts("        -F                - like -f, but read file names from stdin");
    puts("        -p <packagefile>+ - query (uninstalled) package <packagefile>");
    puts("        -P                - like -f, but read package names from stdin");
    puts("      Information selection options:");
    puts("        -i                - display package information");
    puts("        -l                - display package file list");
    puts("        -s                - show file states (implies -l)");
    puts("        -d                - list only documentation files (implies -l)");
    puts("        -c                - list only configuration files (implies -l)");
    puts("");
    puts("    -V");
    puts("    -y");
    puts("    --verify            - verify a package installation");
    puts("			  same package specification options as -q");
    puts("      --root <dir>	- use <dir> as the top level directory");
    puts("");
    puts("    --install <packagefile>");
    puts("    -i <packagefile>	- install package");
    puts("       -v	        - be a little verbose ");
    puts("       -h");
    puts("      --hash            - print hash marks as package installs (good with -v)");
    puts("      --percent         - print percentages as package installs");
    puts("      --replacepkgs      - reinstall if the package is already present");
    puts("      --replacefiles    - install even if the package replaces installed files");
    puts("      --force           - short hand for --replacepkgs --replacefiles");
    puts("      --test            - don't install, but tell if it would work or not");
    puts("      --search          - search the paths listed in /etc/rpmrc for rpms");
    puts("      --root <dir>	- use <dir> as the top level directory");
    puts("");
    puts("    --upgrade <packagefile>");
    puts("    -U <packagefile>	- upgrade package (same options as --install)");
    puts("");
    puts("    --uninstall <package>");
    puts("    -u <package>        - uninstall package");
    puts("      --root <dir>	- use <dir> as the top level directory");
    puts("");
    puts("    -b<stage> <spec>    - build package, where <stage> is one of:");
    puts("			  p - prep (unpack sources and apply patches)");
    puts("			  l - list check (do some cursory checks on %files)");
    puts("			  c - compile (prep and compile)");
    puts("			  i - install (prep, compile, install)");
    puts("			  b - binary package (prep, compile, install, package)");
    puts("			  a - bin/src package (prep, compile, install, package)");
    puts("      --short-circuit   - skip straight to specified stage (only for c,i)");
    puts("      --clean           - remove build tree when done");
    puts("      --keep-temps      - do not delete scripts (or any temp files) in /tmp");
    puts("      --test            - do not execute any stages, implies --keep-temps");
    puts("			  in /tmp - useful for testing");
    puts("      --time-check <s>  - set the time check to S seconds (0 disables it)");
    puts("");
    puts("    --where <pkg>+      - search paths listed in /etc/rpmrc for rpms");
    puts("                          matching <pkg>");
    puts("");
    puts("    --rebuild <source_package>");
    puts("                        - install source package, build binary package,");
    puts("                          and remove spec file, sources, patches, and icons.");
}

void build(char * arg, int buildAmount) {
    FILE *f;
    Spec s;
    char * specfile;
    
    setVerbosity(MESS_VERBOSE);

    if (arg[0] == '/') {
	specfile = arg;
    } else {
	/* XXX this is broken if PWD is near 1024 */

	specfile = alloca(1024);
	getcwd(specfile, 1024);
	strcat(specfile, "/");
	strcat(specfile, arg);
    }

    f = fopen(specfile, "r");
    if ((s = parseSpec(f, specfile))) {
	if (doBuild(s, buildAmount)) {
	    fprintf(stderr, "Build failed.\n");
	}
    } else {
	/* Spec parse failed -- could be Exclude: Exclusive: */
	if (errCode() == RPMERR_BADARCH) {
	    fprintf(stderr, "%s doesn't build on this architecture\n", arg);
	} else {
	    fprintf(stderr, "Build failed.\n");
	}
    }
    
    fclose(f);
}

int main(int argc, char ** argv) {
    int long_index;
    enum modes bigMode = MODE_UNKNOWN;
    enum querysources querySource = QUERY_PACKAGE;
    int arg;
    int queryFor = 0;
    int test = 0;
    int version = 0;
    int help = 0;
    int force = 0;
    int quiet = 0;
    int replaceFiles = 0;
    int replacePackages = 0;
    int showPercents = 0;
    int showHash = 0;
    int installFlags = 0;
    int interfaceFlags = 0;
    int buildAmount = 0;
    int clean = 0;
    char * prefix = "/";
    char * specFile;
    struct option options[] = {
	    { "all", 0, 0, 'a' },
	    { "build", 1, 0, 'b' },
	    { "clean", 0, &clean, 0 },
	    { "configfiles", 0, 0, 'c' },
	    { "docfiles", 0, 0, 'd' },
	    { "file", 0, 0, 'f' },
	    { "force", 0, &force, 0 },
	    { "group", 0, 0, 'g' },
	    { "hash", 0, &showHash, 'h' },
	    { "help", 0, &help, 0 },
	    { "info", 0, 0, 'i' },
	    { "install", 0, 0, 'i' },
	    { "list", 0, 0, 'l' },
	    { "package", 0, 0, 'p' },
	    { "percent", 0, &showPercents, 0 },
	    { "query", 0, 0, 'q' },
	    { "quiet", 0, &quiet, 0 },
	    { "rebuild", 0, 0, 0 },
	    { "replacefiles", 0, &replaceFiles, 0 },
	    { "replacepkgs", 0, &replacePackages, 0 },
	    { "root", 1, 0, 'r' },
	    { "state", 0, 0, 's' },
	    { "stdin-files", 0, 0, 'F' },
	    { "stdin-group", 0, 0, 'G' },
	    { "stdin-packages", 0, 0, 'P' },
	    { "stdin-query", 0, 0, 'Q' },
	    { "test", 0, &test, 0 },
	    { "uninstall", 0, 0, 'u' },
	    { "verbose", 0, 0, 'v' },
	    { "version", 0, &version, 0 },
	    { 0, 0, 0, 0 } 
	} ;

    if (readConfigFiles())  /* reading this early makes it easy to override */
	exit(-1);

    while (1) {
	arg = getopt_long(argc, argv, "QqhpvPfFilsagGducr:b:", options, 
			  &long_index);
	if (arg == -1) break;

	switch (arg) {
	  case 'Q':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_SPACKAGE)
		argerror("only one type of query may be performed at a time");
	    querySource = QUERY_SPACKAGE;
	    /* fallthrough */
	  case 'q':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_QUERY)
		argerror("only one major mode may be specified");
	    bigMode = MODE_QUERY;
	    break;

	  case 'u':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_UNINSTALL)
		argerror("only one major mode may be specified");
	    bigMode = MODE_UNINSTALL;
	    break;
	
	  case 'b':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_BUILD)
		argerror("only one major mode may be specified");
	    bigMode = MODE_BUILD;

	    if (strlen(optarg) > 1)
		argerror("--build (-b) requires one of a,b,i,c,p,l as "
			 "its sole argument");

	    switch (optarg[0]) {
	      /* these fallthroughs are intentional */
	      case 'a':
		buildAmount |= RPMBUILD_SOURCE;
	      case 'b':
		buildAmount |= RPMBUILD_BINARY;
	      case 'i':
		buildAmount |= RPMBUILD_INSTALL;
	      case 'c':
		buildAmount |= RPMBUILD_BUILD;
	      case 'p':
		buildAmount |= RPMBUILD_PREP;
		break;

	      case 'l':
		buildAmount |= RPMBUILD_LIST;
		break;
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

	  case 's':
	    queryFor |= QUERY_FOR_LIST | QUERY_FOR_STATE;
	    break;

	  case 'l':
	    queryFor |= QUERY_FOR_LIST;
	    break;

	  case 'd':
	    queryFor |= QUERY_FOR_DOCS;
	    break;

	  case 'c':
	    queryFor |= QUERY_FOR_CONFIG;
	    break;

	  case 'P':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_SRPM)
		argerror("only one type of query may be performed at a time");
	    querySource = QUERY_SRPM;
	    break;

	  case 'p':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_RPM)
		argerror("only one type of query may be performed at a time");
	    querySource = QUERY_RPM;
	    break;

	  case 'G':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_SGROUP)
		argerror("only one type of query may be performed at a time");
	    querySource = QUERY_SGROUP;
	    break;

	  case 'g':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_GROUP)
		argerror("only one type of query may be performed at a time");
	    querySource = QUERY_GROUP;
	    break;

	  case 'F':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_SPATH)
		argerror("only one type of query may be performed at a time");
	    querySource = QUERY_SPATH;
	    break;

	  case 'f':
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_PATH)
		argerror("only one type of query may be performed at a time");
	    querySource = QUERY_PATH;
	    break;

	  case 'a':
	    if (bigMode != MODE_UNKNOWN && bigMode != MODE_QUERY)
		argerror("-a (--all) may only be used for queries");
	    bigMode = MODE_QUERY;
	    if (querySource != QUERY_PACKAGE && querySource != QUERY_ALL)
		argerror("only one type of query may be performed at a time");
	    querySource = QUERY_ALL;
	    break;

	  case 'h':
	    showHash = 1;
	    break;

	  case 'r':
	    if (optarg[0] != '/') 
		argerror("arguments to --root (-r) must begin with a /");
	    prefix = optarg;
	    break;

	  default:
	    if (options[long_index].flag) 
		*options[long_index].flag = 1;
	    else if (!strcmp(options[long_index].name, "rebuild")) {
		if (bigMode != MODE_UNKNOWN && bigMode != MODE_REBUILD)
		    argerror("only one major mode may be specified");
		bigMode = MODE_REBUILD;
	    }
	}
    }

    if (quiet)
	setVerbosity(MESS_QUIET);

    if (version) printVersion();
    if (help) printHelp();

    if (bigMode != MODE_QUERY && queryFor) 
	argerror("unexpected query specifiers");

    if (bigMode != MODE_QUERY && querySource != QUERY_PACKAGE) 
	argerror("unexpected query source");

    if (bigMode != MODE_INSTALL && force)
	argerror("only installation may be forced");

    if (bigMode != MODE_INSTALL && showHash)
	argerror("--hash (-h) may only be specified during package installation");

    if (bigMode != MODE_INSTALL && showPercents)
	argerror("--percent may only be specified during package installation");

    if (bigMode != MODE_INSTALL && replaceFiles)
	argerror("--replacefiles may only be specified during package installation");

    if (bigMode != MODE_INSTALL && replacePackages)
	argerror("--replacepkgs may only be specified during package installation");
  
    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && test)
	argerror("--test may only be specified during package installation "
		 "and uninstallation");

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_QUERY   && prefix[1])
	argerror("--root (-r) may only be specified during "
		 "installation, uninstallation, and querying");

    if (bigMode != MODE_BUILD && clean) 
	argerror("--clean may only be used during package building");

    switch (bigMode) {
      case MODE_UNKNOWN:
	if (!version && !help) printUsage();
	exit(0);

      case MODE_REBUILD:
	if (optind == argc) 
	    argerror("no packages files given for rebuild");

	while (optind < argc) {
	    buildAmount = RPMBUILD_PREP | RPMBUILD_BUILD | RPMBUILD_INSTALL |
			  RPMBUILD_BINARY | RPMBUILD_SWEEP | RPMBUILD_RMSOURCE;

	    if (doSourceInstall("/", argv[optind++], &specFile))
		exit(-1);

	    build(specFile, buildAmount);
	}
	break;

      case MODE_BUILD:
	if (clean)
	    buildAmount |= RPMBUILD_SWEEP;

	if (optind == argc) 
	    argerror("no spec files given for build");

	while (optind < argc) 
	    build(argv[optind++], buildAmount);
	break;

      case MODE_UNINSTALL:
	if (optind == argc) 
	    argerror("no packages given for uninstall");

	while (optind < argc) 
	    doUninstall(prefix, argv[optind++], test, 0);
	break;

      case MODE_INSTALL:
	if (force) installFlags |= (INSTALL_REPLACEPKG | INSTALL_REPLACEFILES);
	if (replaceFiles) installFlags |= INSTALL_REPLACEFILES;
	if (replacePackages) installFlags |= INSTALL_REPLACEPKG;
	if (test) installFlags |= INSTALL_TEST;

	if (showPercents) interfaceFlags |= RPMINSTALL_PERCENT;
	if (showHash) interfaceFlags |= RPMINSTALL_HASH;

	if (optind == argc) 
	    argerror("no packages given for install");

	while (optind < argc) 
	    doInstall(prefix, argv[optind++], installFlags, interfaceFlags);
	break;

      case MODE_QUERY:
	if (querySource == QUERY_ALL) {
	    doQuery(prefix, QUERY_ALL, queryFor, NULL);
	} else if (querySource == QUERY_SPATH || 
                   querySource == QUERY_SPACKAGE ||
		   querySource == QUERY_SRPM) {
	    char buffer[255];
	    int i;

	    while (fgets(buffer, 255, stdin)) {
		i = strlen(buffer) - 1;
		if (buffer[i] == '\n') buffer[i] = 0;
		if (strlen(buffer)) 
		    doQuery(prefix, querySource, queryFor, buffer);
	    }
	} else {
	    if (optind == argc) 
		argerror("no arguments given for query");
	    while (optind < argc) 
		doQuery(prefix, querySource, queryFor, argv[optind++]);
	}
	break;
    }

    return 0;
}
