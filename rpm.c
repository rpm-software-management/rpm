#include <gdbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "query.h"

enum modes { MODE_QUERY, MODE_INSTALL, MODE_UNINSTALL, MODE_VERIY,
	     MODE_UNKNOWN };

static void argerror(char * desc);

static void argerror(char * desc) {
    fprintf(stderr, "rpm: %s\n", desc);
    exit(1);
}

void doQuery(enum querysources source, int queryFlags, char * arg) {
    printf("querying not yet implemented\n");
}

int main(int argc, char ** argv) {
    int long_index;
    enum modes bigMode = MODE_UNKNOWN;
    enum querysources querySource = QUERY_PACKAGE;
    int arg;
    int force = 0;
    int queryFor = 0;
    int test = 0;
    struct option options[] = {
	    { "query", 0, 0, 'q' },
	    { "stdin-query", 0, 0, 'Q' },
	    { "file", 0, 0, 'f' },
	    { "stdin-files", 0, 0, 'F' },
	    { "package", 0, 0, 'p' },
	    { "stdin-packages", 0, 0, 'P' },
	    { "root", 1, 0, 'R' },
	    { "all", 0, 0, 'a' },
	    { "info", 0, 0, 'i' },
	    { "list", 0, 0, 'l' },
	    { "state", 0, 0, 's' },
	    { "install", 0, 0, 'i' },
	    { "verbose", 0, 0, 'v' },
	    { "force", 0, &force, 0 },
	    { "quiet", 0, 0, 0 },
	    { "test", 0, &test, 0 },
	    { 0, 0, 0, 0 } 
	} ;


    while (1) {
	arg = getopt_long(argc, argv, "QqpvPfFilsar:", options, &long_index);
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
	
	  case 'v':
	    /*increaseVerbosity();*/
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

	  case 'r':
	    argerror("root argument not yet handled!");
	    break;

	  default:
	    if (options[long_index].flag) 
		*options[long_index].flag = 1;
/*
	    else if (!strcmp(options[long_index].name, "quiet"))
		setVerbosity(MESS_QUIET);
*/
	}
    }

    if (bigMode == MODE_UNKNOWN)
	argerror("no operation specified");

    if (bigMode != MODE_QUERY && queryFor) 
	argerror("unexpected query specifiers");

    if (bigMode != MODE_QUERY && querySource != QUERY_PACKAGE) 
	argerror("unexpected query source");

    if (bigMode != MODE_INSTALL && force)
	argerror("only installation may be forced");

    switch (bigMode) {
      case MODE_UNKNOWN:
	argerror("no operation specified");

      case MODE_INSTALL:
	while (optind < argc) 
	    printf("can't install %s\n", argv[optind++]);
	    /*doInstall(argv[optind++], force, test);*/
	break;

      case MODE_QUERY:
	if (querySource == QUERY_ALL) {
	    doQuery(QUERY_ALL, queryFor, NULL);
	} else if (querySource == QUERY_SPATH || 
                   querySource == QUERY_SPACKAGE ||
		   querySource == QUERY_SRPM) {
	    char buffer[255];
	    int i;

	    while (fgets(buffer, 255, stdin)) {
		i = strlen(buffer) - 1;
		if (buffer[i] == '\n') buffer[i] = 0;
		if (strlen(buffer)) 
		    doQuery(querySource, queryFor, buffer);
	    }
	} else {
	    if (optind == argc) 
		argerror("no arguments given for query");
	    while (optind < argc) 
		doQuery(querySource, queryFor, argv[optind++]);
	}
	break;
    }

    return 0;
}
