/* SEXP implementation code sexp-main.c
 * Ron Rivest
 * 6/29/1997
 */

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include "sexp.h"

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

/*************************************************************************/
/* main(argc,argv)
 */
/*@-mods@*/
/*@-nullderef@*/
/*@-nullpass@*/
int main(int argc, char **argv)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{ char *c;
  int swa = TRUE;
  int swb = TRUE;
  int swc = TRUE;
  int swp = TRUE;
  int sws = FALSE;
  int swx = TRUE;
  int swl = FALSE;
  int i;
  sexpObject *object;
  sexpInputStream *is;
  sexpOutputStream *os;
  initializeCharacterTables();
  initializeMemory();
  is = newSexpInputStream();
  os = newSexpOutputStream();
  /* process switches */
  if (argc>1) swa = swb = swc = swp = sws = swx = swl = FALSE;
  for (i=1;i<argc;i++)
    { c = argv[i];
      if (*c != '-')
	{ printf("Unrecognized switch %s\n",c); exit(0); }
      c++;
      if (*c == 'a') /* advanced output */
	swa = TRUE;
      else if (*c == 'b') /* base-64 output */
	swb = TRUE;
      else if (*c == 'c') /* canonical output */
	swc = TRUE;
      else if (*c == 'h') /* help */
	{
	  printf("%s",help);
	  fflush(stdout);
	  exit(0);
	}
      else if (*c == 'i') /* input file */
	{ if (i+1<argc) i++;
	  is->inputFile = fopen(argv[i],"r");
	  if (is->inputFile==NULL)
	    ErrorMessage(ERROR,"Can't open input file.",0,0);
	}
      else if (*c == 'l') /* suppress linefeeds after output */
	swl = TRUE;
      else if (*c == 'o') /* output file */
	{ if (i+1<argc) i++;
	  os->outputFile = fopen(argv[i],"w");
	  if (os->outputFile==NULL)
	    ErrorMessage(ERROR,"Can't open output file.",0,0);
	}	
      else if (*c == 'p') /* prompt for input */
	swp = TRUE;
      else if (*c == 's') /* treat input as one big string */
	sws = TRUE;
      else if (*c == 'w') /* set output width */
	{ if (i+1<argc) i++;
	  os->maxcolumn = atoi(argv[i]);
	}
      else if (*c == 'x') /* execute repeatedly */
	swx = TRUE;
      else
	{ printf("Unrecognized switch: %s\n",argv[i]); exit(0); }
    }
  if (swa == FALSE && swb == FALSE && swc == FALSE)
    swc = TRUE;  /* must have some output format! */

  /* main loop */
  if (swp == 0) is->getChar(is);
  else is->nextChar = -2;  /* this is not EOF */
  while (is->nextChar != EOF)
    {
      if (swp)
	{ printf("Input:\n"); fflush(stdout); }

      changeInputByteSize(is,8);
      if (is->nextChar == -2) is->getChar(is);

      skipWhiteSpace(is);
      if (is->nextChar == EOF) break;

      if (sws == FALSE)
	object = scanObject(is);
      else
	object = scanToEOF(is);

      if (swc)
	{ if (swp)
	    { printf("Canonical output:"); fflush(stdout);
	      os->newLine(os,ADVANCED);
	    }
	  canonicalPrintObject(os,object);
	  if (!swl) { printf("\n"); fflush(stdout); }
	}

      if (swb)
	{ if (swp)
	    { printf("Base64 (of canonical) output:"); fflush(stdout);
	      os->newLine(os,ADVANCED);
	    }
	  base64PrintWholeObject(os,object);
	  if (!swl) { printf("\n"); fflush(stdout); }
	}

      if (swa)
	{ if (swp)
	    { printf("Advanced transport output:"); fflush(stdout);
	      os->newLine(os,ADVANCED);
	    }
	  advancedPrintObject(os,object);
	  if (!swl) { printf("\n"); fflush(stdout); }
	}

      if (!swx) break;
      if (!swp) skipWhiteSpace(is);
      else if (!swl) { printf("\n"); fflush(stdout); }
    }
  return 0;
}
/*@=nullpass@*/
/*@=nullderef@*/
/*@=mods@*/
