#include <stdio.h>
#include <stdlib.h>

#include "popt.h"

void option_callback(poptContext con, const struct poptOption * opt, 
		     char * arg, void * data) {
    fprintf(stdout, "callback: %c %s %s ", opt->val, (char *) data, arg);    
}

int main(int argc, char ** argv) {
    int rc;
    int arg1 = 0;
    char * arg2 = "(none)";
    poptContext optCon;
    char ** rest;
    int arg3 = 0;
    int inc = 0;
    int help = 0;
    int usage = 0;
    struct poptOption callbackArgs[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK, option_callback, 0, "sampledata" },
	{ "cb", 'c', POPT_ARG_STRING, NULL, 'c', "Test argument callbacks" },
	{ "long", '\0', 0, NULL, 'l', "Unused option for help testing" },
	{ NULL, '\0', 0, NULL, 0 } 
    };
    struct poptOption moreArgs[] = {
	{ "inc", 'i', 0, &inc, 0, "An included argument" },
	{ NULL, '\0', 0, NULL, 0 } 
    };
    struct poptOption options[] = {
	{ "arg1", '\0', 0, &arg1, 0, "First argument with a really long" 
	    " description. After all, we have to test argument help"
	    " wrapping somehow, right?", NULL },
	{ "arg2", '2', POPT_ARG_STRING, &arg2, 0, "Another argument", "ARG" },
	{ "arg3", '3', POPT_ARG_INT, &arg3, 0, "A third argument", "ANARG" },
	{ "unused", '\0', POPT_ARG_STRING, NULL, 0, 
	    "Unused option for help testing", "UNUSED" },
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &moreArgs, 0, NULL },
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &callbackArgs, 0, "Callback arguments" },
	POPT_AUTOHELP
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

    if (help) {
	poptPrintHelp(optCon, stdout, 0);
	return 0;
    } if (usage) {
	poptPrintUsage(optCon, stdout, 0);
	return 0;
    }

    fprintf(stdout, "arg1: %d arg2: %s", arg1, arg2);

    if (arg3)
	fprintf(stdout, " arg3: %d", arg3);
    if (inc)
	fprintf(stdout, " inc: %d", inc);

    rest = poptGetArgs(optCon);
    if (rest) {
	fprintf(stdout, " rest:");
	while (*rest) {
	    fprintf(stdout, " %s", *rest);
	    rest++;
	}
    }

    fprintf(stdout, "\n");

    return 0;
}
