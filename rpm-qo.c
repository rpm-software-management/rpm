#include "system.h"

#include "build/rpmbuild.h"

#include "install.h"
#include "lib/signature.h"
#include "query.h"
#include "verify.h"
#include "checksig.h"

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
    fprintf(stderr, _("rpm: %s\n"), desc);
    exit(EXIT_FAILURE);
}

void printHelp(void);
void printVersion(void);
void printBanner(void);
void printUsage(void);
int build(char *arg, int buildAmount, char *passPhrase,
	  char *buildRootOverride);

void printVersion(void) {
    fprintf(stdout, _("RPM version %s\n"), version);
}

void printBanner(void) {
    puts(_("Copyright (C) 1995 - Red Hat Software"));
    puts(_("This may be freely redistributed under the terms of the GNU "
	   "Public License"), stdout);
}

void printUsage(void) {
    printVersion();
    printBanner();
    puts("");

    puts(_("usage: rpm {--help}"));
    puts(_("       rpm {--version}"));
    puts(_("       rpm {--query -q} [-afFpP] [-i] [-l] [-s] [-d] [-c] [-v] [-R]"));
    puts(_("                        [--scripts] [--root <dir>] [--rcfile <file>]"));
    puts(_("                        [--whatprovides] [--whatrequires] [--requires]"));
    puts(_("                        [--ftpuseport] [--ftpproxy <host>] [--ftpport <port>]"));
    puts(_("                        [--provides] [--dump] [--dbpath <dir>] [targets]"));
    puts(_("       rpm {--querytags}"));
}

void printHelp(void) {
    printVersion();
    printBanner();
    puts("");

    puts(_("usage:"));
    puts(_("   --help		- print this message"));
    puts(_("   --version	- print the version of rpm being used"));
    puts(_("   all modes support the following arguments:"));
    puts(_("       -v		      - be a little more verbose"));
    puts(_("       -vv	              - be incredibly verbose (for debugging)"));
    puts(_("   -q                   - query mode"));
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
    puts("");
    puts(_("    --querytags         - list the tags that can be used in a query format"));
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
    char * ftpProxy = NULL, * ftpPort = NULL;
    char * smallArgv[2] = { NULL, NULL };
    char ** currarg;
    int ec = 0;
    struct option optionsTable[] = {
	    { "all", 0, 0, 'a' },
	    { "configfiles", 0, 0, 'c' },
	    { "docfiles", 0, 0, 'd' },
	    { "dump", 0, &dump, 0 },
	    { "group", 0, 0, 'g' },
	    { "help", 0, &help, 0 },
	    { "info", 0, 0, 'i' },
	    { "package", 0, 0, 'p' },
	    { "provides", 0, 0, GETOPT_PROVIDES },
	    { "qf", 1, 0, GETOPT_QUERYFORMAT },
	    { "query", 0, 0, 'q' },
	    { "querybynumber", 0, 0, GETOPT_QUERYBYNUMBER },
	    { "queryformat", 1, 0, GETOPT_QUERYFORMAT },
	    { "querytags", 0, &queryTags, 0 },
	    { "quiet", 0, &quiet, 0 },
	    { "requires", 0, 0, 'R' },
	    { "scripts", 0, &queryScripts, 0 },
	    { "state", 0, 0, 's' },
	    { "stdin-files", 0, 0, 'F' },
	    { "stdin-group", 0, 0, 'G' },
	    { "stdin-packages", 0, 0, 'P' },
	    { "stdin-query", 0, 0, 'Q' },
	    { "verbose", 0, 0, 'v' },
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

    /* reading this early makes it easy to override */
    if (rpmReadConfigFiles(rcfile, NULL))  
	exit(EXIT_FAILURE);
    if (showrc) {
	rpmShowRC(stdout);
	exit(EXIT_SUCCESS);
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

	  case GETOPT_PROVIDES:
	    queryFor |= QUERY_FOR_PROVIDES;
	    break;

	  default:
	    if (options[long_index].flag) {
		*options[long_index].flag = 1;
	    } else {
		badOption = 1;
	    }
	    break;
	}
    }

    if (quiet)
	rpmSetVerbosity(RPMMESS_QUIET);

    if (version) printVersion();
    if (help) printHelp();

    if (badOption)
	exit(EXIT_FAILURE);

    if (queryScripts) {
	queryFor |= QUERY_FOR_SCRIPTS;
    }

    if (queryTags)
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_QUERYTAGS;

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

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && noScripts)
	argerror(_("--noscripts may only be specified during package "
		   "installation and uninstallation"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_VERIFY && noDeps)
	argerror(_("--nodeps may only be specified during package "
		   "installation, uninstallation, and verification"));

    if (bigMode != MODE_VERIFY && noFiles)
	argerror(_("--nofiles may only be specified during package "
		   "verification"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL &&
	bigMode != MODE_BUILD && test)
	argerror(_("--test may only be specified during package installation, "
		 "uninstallation, and building"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_UNINSTALL && 
	bigMode != MODE_QUERY   && bigMode != MODE_VERIFY    && 			bigMode != MODE_REBUILDDB && rootdir[1])
	argerror(_("--root (-r) may only be specified during "
		 "installation, uninstallation, querying, and "
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

    switch (bigMode) {
      case MODE_UNKNOWN:
	if (!version && !help) printUsage();
	break;

      case MODE_QUERYTAGS:
	if (argc != 2)
	    argerror(_("unexpected arguments to --querytags "));

	queryPrintTags();
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
    }

    return ec;
}
