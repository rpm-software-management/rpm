/* (C) 1998-2000 Red Hat, Inc. -- Licensing details are in the COPYING
   file accompanying popt source distributions, available from
   ftp://ftp.rpm.org/pub/rpm/dist. */

#include "system.h"

/*@unchecked@*/
static int pass2 = 0;
static void option_callback(/*@unused@*/ poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt,
		char * arg, void * data)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    if (pass2)
	fprintf(stdout, "callback: %c %s %s ", opt->val, (char *) data, arg);
}

/*@unchecked@*/
static int arg1 = 0;
/*@unchecked@*/ /*@observer@*/
static char * arg2 = "(none)";
/*@unchecked@*/
static int arg3 = 0;
/*@unchecked@*/
static int inc = 0;
/*@unchecked@*/
static int shortopt = 0;

/*@unchecked@*/
static int aVal = 141421;
/*@unchecked@*/
static int bVal = 141421;
/*@unchecked@*/
static int aFlag = 0;
/*@unchecked@*/
static int bFlag = 0;

/*@unchecked@*/
static int aInt = 271828;
/*@unchecked@*/
static int bInt = 271828;
/*@unchecked@*/
static long aLong = 738905609L;
/*@unchecked@*/
static long bLong = 738905609L;
/*@unchecked@*/
static float aFloat = 3.1415926535;
/*@unchecked@*/
static float bFloat = 3.1415926535;
/*@unchecked@*/
static double aDouble = 9.86960440108935861883;
/*@unchecked@*/
static double bDouble = 9.86960440108935861883;
/*@unchecked@*/ /*@null@*/
static char * oStr = (char *) -1;
/*@unchecked@*/
static int singleDash = 0;

/*@unchecked@*/ /*@observer@*/
static char * lStr =
"This tests default strings and exceeds the ... limit. "
"123456789+123456789+123456789+123456789+123456789+ "
"123456789+123456789+123456789+123456789+123456789+ "
"123456789+123456789+123456789+123456789+123456789+ "
"123456789+123456789+123456789+123456789+123456789+ ";
/*@unchecked@*/ /*@null@*/
static char * nStr = NULL; 

/*@unchecked@*/
static struct poptOption moreCallbackArgs[] = {
  { NULL, '\0', POPT_ARG_CALLBACK|POPT_CBFLAG_INC_DATA,
	(void *)option_callback, 0,
	NULL, NULL },
  { "cb2", 'c', POPT_ARG_STRING, NULL, 'c',
	"Test argument callbacks", NULL },
  POPT_TABLEEND
};

/*@unchecked@*/
static struct poptOption callbackArgs[] = {
  { NULL, '\0', POPT_ARG_CALLBACK, (void *)option_callback, 0,
	"sampledata", NULL },
  { "cb", 'c', POPT_ARG_STRING, NULL, 'c',
	"Test argument callbacks", NULL },
  { "longopt", '\0', 0, NULL, 'l',
	"Unused option for help testing", NULL },
  POPT_TABLEEND
};

/*@unchecked@*/
static struct poptOption moreArgs[] = {
  { "inc", 'I', 0, &inc, 0, "An included argument", NULL },
  POPT_TABLEEND
};

/*@unchecked@*/
static struct poptOption options[] = {
  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, &moreCallbackArgs, 0,
	"arg for cb2", NULL },
  { "arg1", '\0', 0, &arg1, 0, "First argument with a really long"
	    " description. After all, we have to test argument help"
	    " wrapping somehow, right?", NULL },
  { "arg2", '2', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &arg2, 0,
	"Another argument", "ARG" },
  { "arg3", '3', POPT_ARG_INT, &arg3, 0,
	"A third argument", "ANARG" },
  { "onedash", '\0', POPT_ARGFLAG_ONEDASH, &shortopt, 0,
	"POPT_ARGFLAG_ONEDASH: Option takes a single -", NULL },
  { "hidden", '\0', POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, NULL, 0,
	"POPT_ARGFLAG_HIDDEN: A hidden option (--help shouldn't display)",
	NULL },
  { "optional", '\0', POPT_ARG_STRING | POPT_ARGFLAG_OPTIONAL, &oStr, 0,
	"POPT_ARGFLAG_OPTIONAL: Takes an optional string argument", NULL },

  { "val", '\0', POPT_ARG_VAL | POPT_ARGFLAG_SHOW_DEFAULT, &aVal, 125992,
	"POPT_ARG_VAL: 125992 141421", 0},

  { "int", 'i', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &aInt, 0,
	"POPT_ARG_INT: 271828", NULL },
  { "long", 'l', POPT_ARG_LONG | POPT_ARGFLAG_SHOW_DEFAULT, &aLong, 0,
	"POPT_ARG_LONG: 738905609", NULL },
  { "float", 'f', POPT_ARG_FLOAT | POPT_ARGFLAG_SHOW_DEFAULT, &aFloat, 0,
	"POPT_ARG_FLOAT: 3.14159", NULL },
  { "double", 'd', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &aDouble, 0,
	"POPT_ARG_DOUBLE: 9.8696", NULL },

  { "bitset", '\0', POPT_BIT_SET | POPT_ARGFLAG_SHOW_DEFAULT, &aFlag, 0x4321,
	"POPT_BIT_SET: |= 0x4321", 0},
  { "bitclr", '\0', POPT_BIT_CLR | POPT_ARGFLAG_SHOW_DEFAULT, &aFlag, 0x1234,
	"POPT_BIT_CLR: &= ~0x1234", 0},

  { "nstr", '\0', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &nStr, 0,
	"POPT_ARG_STRING: (null)", NULL},
  { "lstr", '\0', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &lStr, 0,
	"POPT_ARG_STRING: \"123456789...\"", NULL},

  { NULL, '-', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, &singleDash, 0,
	NULL, NULL },
  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, &moreArgs, 0,
	NULL, NULL },
  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, &callbackArgs, 0,
	"Callback arguments", NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

static void resetVars(void)
	/*@globals arg1, arg2, arg3, inc, shortopt,
		aVal, aFlag, aInt, aLong, aFloat, aDouble,
		oStr, singleDash, pass2 @*/
	/*@modifies arg1, arg2, arg3, inc, shortopt,
		aVal, aFlag, aInt, aLong, aFloat, aDouble,
		oStr, singleDash, pass2 @*/
{
    arg1 = 0;
    arg2 = "(none)";
    arg3 = 0;
    inc = 0;
    shortopt = 0;

    aVal = bVal;
    aFlag = bFlag;

    aInt = bInt;
    aLong = bLong;
    aFloat = bFloat;
    aDouble = bDouble;

    oStr = (char *) -1;

    singleDash = 0;
    pass2 = 0;
}

int main(int argc, const char ** argv)
	/*@globals pass2, fileSystem @*/
	/*@modifies pass2, fileSystem @*/
{
    int rc;
    int ec = 0;
    poptContext optCon;
    const char ** rest;
    int help = 0;
    int usage = 0;

#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-moduncon -noeffectuncon@*/
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=moduncon =noeffectuncon@*/
#endif

/*@-modobserver@*/
    resetVars();
/*@=modobserver@*/
/*@-temptrans@*/
    optCon = poptGetContext("test1", argc, argv, options, 0);
/*@=temptrans@*/
    (void) poptReadConfigFile(optCon, "./test-poptrc");

#if 1
    while ((rc = poptGetNextOpt(optCon)) > 0)	/* Read all the options ... */
	{};

    poptResetContext(optCon);			/* ... and then start over. */
/*@-modobserver@*/
    resetVars();
/*@=modobserver@*/
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
    if (aVal != bVal)
	fprintf(stdout, " aVal: %d", aVal);
    if (aFlag != bFlag)
	fprintf(stdout, " aFlag: %d", aFlag);
    if (aInt != bInt)
	fprintf(stdout, " aInt: %d", aInt);
    if (aLong != bLong)
	fprintf(stdout, " aLong: %ld", aLong);
/*@-realcompare@*/
    if (aFloat != bFloat)
	fprintf(stdout, " aFloat: %g", aFloat);
    if (aDouble != bDouble)
	fprintf(stdout, " aDouble: %g", aDouble);
/*@=realcompare@*/
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
    /*@-moduncon -noeffectuncon@*/
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=moduncon =noeffectuncon@*/
#endif
    return ec;
}
