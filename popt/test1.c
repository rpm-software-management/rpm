/* (C) 1998-2000 Red Hat, Inc. -- Licensing details are in the COPYING
   file accompanying popt source distributions, available from
   ftp://ftp.rpm.org/pub/rpm/dist. */

#include "system.h"

static int pass2 = 0;
static void option_callback(poptContext con, enum poptCallbackReason reason,
		     const struct poptOption * opt,
		     char * arg, void * data) {
    if (pass2)
	fprintf(stdout, "callback: %c %s %s ", opt->val, (char *) data, arg);
}

int arg1 = 0;
char * arg2 = "(none)";
int arg3 = 0;
int inc = 0;
int shortopt = 0;
float aFloat = 0.0;
double aDouble = 0.0;
char * oStr = (char *)-1;
int singleDash = 0;

static struct poptOption moreCallbackArgs[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA,
		(void *)option_callback, 0, NULL },
	{ "cb2", 'c', POPT_ARG_STRING, NULL, 'c', "Test argument callbacks" },
	{ NULL, '\0', 0, NULL, 0 }
};
static struct poptOption callbackArgs[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK, (void *)option_callback, 0, "sampledata" },
	{ "cb", 'c', POPT_ARG_STRING, NULL, 'c', "Test argument callbacks" },
	{ "long", '\0', 0, NULL, 'l', "Unused option for help testing" },
	{ NULL, '\0', 0, NULL, 0 }
};
static struct poptOption moreArgs[] = {
	{ "inc", 'i', 0, &inc, 0, "An included argument" },
	{ NULL, '\0', 0, NULL, 0 }
};
static struct poptOption options[] = {
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &moreCallbackArgs, 0, "arg for cb2" },
	{ "arg1", '\0', 0, &arg1, 0, "First argument with a really long"
	    " description. After all, we have to test argument help"
	    " wrapping somehow, right?", NULL },
	{ "arg2", '2', POPT_ARG_STRING, &arg2, 0, "Another argument", "ARG" },
	{ "arg3", '3', POPT_ARG_INT, &arg3, 0, "A third argument", "ANARG" },
	{ "shortoption", '\0', POPT_ARGFLAG_ONEDASH, &shortopt, 0,
		"Needs a single -", NULL },
	{ "hidden", '\0', POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, NULL, 0,
		"This shouldn't show up", NULL },
	{ "unused", '\0', POPT_ARG_STRING, NULL, 0,
	    "Unused option for help testing", "UNUSED" },
	{ "float", 'f', POPT_ARG_FLOAT, &aFloat, 0,
	    "A float argument", "FLOAT" },
	{ "double", 'd', POPT_ARG_DOUBLE, &aDouble, 0,
	    "A double argument", "DOUBLE" },
	{ "ostr", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &oStr, 0,
	    "An optional str", "ARG" },

	{ NULL, '-', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, &singleDash, 0 },
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &moreArgs, 0, NULL },
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &callbackArgs, 0, "Callback arguments" },
	POPT_AUTOHELP
	{ NULL, '\0', 0, NULL, 0 }
};

static void resetVars(void)
{
    arg1 = 0;
    arg2 = "(none)";
    arg3 = 0;
    inc = 0;
    shortopt = 0;
    aFloat = 0.0;
    aDouble = 0.0;
    singleDash = 0;
    pass2 = 0;
}

int main(int argc, const char ** argv) {
    int rc;
    int ec = 0;
    poptContext optCon;
    const char ** rest;
    int help = 0;
    int usage = 0;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    resetVars();
    optCon = poptGetContext("test1", argc, argv, options, 0);
    poptReadConfigFile(optCon, "./test-poptrc");

#if 1
    while ((rc = poptGetNextOpt(optCon)) > 0)	/* Read all the options ... */
	;

    poptResetContext(optCon);			/* ... and then start over. */
    resetVars();
#endif

    pass2 = 1;
    if ((rc = poptGetNextOpt(optCon)) < -1) {
	fprintf(stderr, "test1: bad argument %s: %s\n",
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(rc));
	ec = 2;
	goto exit;
    }

    if (help) {
	poptPrintHelp(optCon, stdout, 0);
	goto exit;
    }
    if (usage) {
	poptPrintUsage(optCon, stdout, 0);
	goto exit;
    }

    fprintf(stdout, "arg1: %d arg2: %s", arg1, arg2);

    if (arg3)
	fprintf(stdout, " arg3: %d", arg3);
    if (inc)
	fprintf(stdout, " inc: %d", inc);
    if (shortopt)
	fprintf(stdout, " short: %d", shortopt);
    if (aFloat != 0.0)
	fprintf(stdout, " aFloat: %g", aFloat);
    if (aDouble != 0.0)
	fprintf(stdout, " aDouble: %g", aDouble);
    if (oStr != (char *)-1)
	fprintf(stdout, " oStr: %s", (oStr ? oStr : "(none)"));
    if (singleDash)
	fprintf(stdout, " -");

    rest = poptGetArgs(optCon);
    if (rest) {
	fprintf(stdout, " rest:");
	while (*rest) {
	    fprintf(stdout, " %s", *rest);
	    rest++;
	}
    }

    fprintf(stdout, "\n");

exit:
    optCon = poptFreeContext(optCon);
#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    return ec;
}
