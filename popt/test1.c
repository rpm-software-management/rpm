#include <stdio.h>
#include <stdlib.h>

#include "popt.h"

int main(int argc, char ** argv) {
    int rc;
    int arg1 = 0;
    char * arg2 = "(none)";
    poptContext optCon;
    char ** rest;
    int arg3 = 0;
    struct poptOption options[] = {
	{ "arg1", '\0', 0, &arg1, 0 },
	{ "arg2", '2', POPT_ARG_STRING, &arg2, 0 },
	{ "arg3", '3', POPT_ARG_INT, &arg3, 0 },
	{ NULL, '\0', 0, NULL, 0 } 
    };

    optCon = poptGetContext("test1", argc, argv, options, 0);
    poptReadConfigFile(optCon, "./test-poptrc");

    if ((rc = poptGetNextOpt(optCon)) < -1) {
	fprintf(stderr, "test1: bad argument %s: %s\n", 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(rc));
	return 2;
    }

    printf("arg1: %d arg2: %s", arg1, arg2);

    if (arg3)
	printf(" arg3: %d", arg3);

    rest = poptGetArgs(optCon);
    if (rest) {
	printf(" rest:");
	while (*rest) {
	    printf(" %s", *rest);
	    rest++;
	}
    }

    printf("\n");

    return 0;
}
