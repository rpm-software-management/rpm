/* SEXP implementation code sexp-main.c
 * Ron Rivest
 * 6/29/1997
 */

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <popt.h>
#include "sexp.h"

/*@access sexpInputStream @*/
/*@access sexpOutputStream @*/

/*@unchecked@*/ /*@observer@*/
static const char *help =
"The program `sexp' reads, parses, and prints out S-expressions.\n"
" INPUT:\n"
"   Input is normally taken from stdin, but this can be changed:\n"
"      -i filename      -- takes input from file instead.\n"
"      -p               -- prompts user for console input\n"
"   Input is normally parsed, but this can be changed:\n"
"      -s               -- treat input up to EOF as a single string\n"
" CONTROL LOOP:\n"
"   The main routine typically reads one S-expression, prints it out again, \n"
"   and stops.  This may be modified:\n"
"      -x               -- execute main loop repeatedly until EOF\n"
" OUTPUT:\n"
"   Output is normally written to stdout, but this can be changed:\n"
"      -o filename      -- write output to file instead\n"
"   The output format is normally canonical, but this can be changed:\n"
"      -a               -- write output in advanced transport format\n"
"      -b               -- write output in base-64 output format\n"
"      -c               -- write output in canonical format\n"
"      -l               -- suppress linefeeds after output\n"
"   More than one output format can be requested at once.\n"
" There is normally a line-width of 75 on output, but:\n"
"      -w width         -- changes line width to specified width.\n"
"                          (0 implies no line-width constraint)\n"
" The default switches are: -p -a -b -c -x\n"
" Typical usage: cat certificate-file | sexp -a -x \n";

/*@unchecked@*/
static int _debug = 0;
/*@unchecked@*/ /*@null@*/
static const char * ifn = NULL;
/*@unchecked@*/ /*@null@*/
static const char * ofn = NULL;
/*@unchecked@*/
static int swa = TRUE;
/*@unchecked@*/
static int swb = TRUE;
/*@unchecked@*/
static int swc = TRUE;
/*@unchecked@*/
static int swl = FALSE;
/*@unchecked@*/
static int swp = TRUE;
/*@unchecked@*/
static int sws = FALSE;
/*@unchecked@*/
static int swx = TRUE;
/*@unchecked@*/
static int width = -1;

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&_debug, -1,
	NULL, NULL },

 { "advanced", 'a', POPT_ARG_VAL,	&swa, TRUE,
	"advanced transport format", NULL },
 { "base64", 'b', POPT_ARG_VAL,		&swb, TRUE,
	"base64 format", NULL },
 { "canonical", 'c', POPT_ARG_VAL,	&swc, TRUE,
	"canonical format", NULL },
 { "input", 'i', POPT_ARG_STRING,	&ifn, 0,
	"read input from FILE", "FILE" },
 { "nolf", 'l', POPT_ARG_VAL,		&swl, TRUE,
	"suppress linefeeds", NULL },
 { "output", 'o', POPT_ARG_STRING,	&ofn, 0,
	"write output to FILE", "FILE" },
 { "prompt", 'p', POPT_ARG_VAL,		&swp, TRUE,
	"prompt for input", NULL },
 { "single", 's', POPT_ARG_VAL,		&sws, TRUE,
	"single string input", NULL },
 { "execute", 'x', POPT_ARG_VAL,	&swx, TRUE,
	"execute loop until EOF", NULL },
 { "width", 'w', POPT_ARG_INT,		&width, 0,
	"output line WIDTH", "WIDTH" },

  POPT_AUTOHELP
  POPT_TABLEEND
};

/*************************************************************************/
/* main(argc,argv)
 */
/*@=nullstate@*/
int main(int argc, char **argv)
	/*@globals optionsTable, ifn, ofn, swa, swb, swc, swl, swp, sws, swx,
		width, fileSystem, internalState @*/
	/*@modifies swa, swb, swc, swl, swp, sws, swx,
		fileSystem, internalState @*/
{
    poptContext optCon;
    int rc;
    sexpObject object;
    sexpInputStream is;
    sexpOutputStream os;

    initializeCharacterTables();
    initializeMemory();
    os = newSexpOutputStream();
    /* process switches */
    if (argc>1)
	swa = swb = swc = swp = sws = swx = swl = FALSE;

    optCon = poptGetContext(argv[0], argc, (const char **)argv, optionsTable, 0);
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	default:
	    fprintf(stderr, "%s: option table misconfigured (%d)\n",
		argv[0], rc);
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
        }
    }

    if (rc < -1) {
	fprintf(stderr, "%s", help); (void) fflush(stderr);
	goto exit;
    }

    is = newSexpInputStream(ifn, "r");

    if (ofn != NULL) {
	os->outputFile = fopen(ofn, "w");
	if (os->outputFile == NULL)
	    ErrorMessage(ERROR, "Can't open output file %s.", ofn);
    }
    if (width >= 0) {
	os->maxcolumn = width;
    }

    if (swa == FALSE && swb == FALSE && swc == FALSE)
	swc = TRUE;  /* must have some output format! */

    /* main loop */
    if (swp == 0) sexpIFgetc(is);
    else sexpIFpoke(is, -2);  /* this is not EOF */

    while (!sexpIFeof(is)) {
	if (swp) { fprintf(stdout, "Input:\n"); (void) fflush(stdout); }

	changeInputByteSize(is, 8);
	if (sexpIFpeek(is) == -2) sexpIFgetc(is);

	skipWhiteSpace(is);
	if (sexpIFeof(is)) break;

	if (sws == FALSE)
	    object = scanObject(is);
	else
	    object = scanToEOF(is);

	if (swc) {
	    if (swp) {
		fprintf(stdout, "Canonical output:"); (void) fflush(stdout);
		os->newLine(os, ADVANCED);
	    }
	    canonicalPrintObject(os, object);
	    if (!swl) { fprintf(stdout, "\n"); (void) fflush(stdout); }
	}

	if (swb) {
	    if (swp) {
		fprintf(stdout, "Base64 (of canonical) output:"); (void) fflush(stdout);
		os->newLine(os, ADVANCED);
	    }
	    base64PrintWholeObject(os, object);
	    if (!swl) { fprintf(stdout, "\n"); (void) fflush(stdout); }
	}

	if (swa) {
	    if (swp) {
		fprintf(stdout, "Advanced transport output:"); (void) fflush(stdout);
		os->newLine(os, ADVANCED);
	    }
	    advancedPrintObject(os,object);
	    if (!swl) { fprintf(stdout, "\n"); (void) fflush(stdout); }
	}

	if (!swx) break;
	if (!swp) skipWhiteSpace(is);
	else if (!swl) { fprintf(stdout, "\n"); (void) fflush(stdout); }
    }
    rc = 0;

exit:
    optCon = poptFreeContext(optCon);

    return rc;
}
/*@=nullstate@*/
