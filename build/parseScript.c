#include <malloc.h>
#include <string.h>

#include "header.h"
#include "read.h"
#include "part.h"
#include "misc.h"
#include "rpmlib.h"
#include "popt/popt.h"
#include "reqprov.h"
#include "package.h"

/* Define this to be 1 to turn on -p "<prog> <args>..." ability */
#define USE_PROG_STRING_ARRAY 0

/* these have to be globab because of stupid compilers */
    static char *name;
    static char *prog;
    static char *file;
    static struct poptOption optionsTable[] = {
	{ NULL, 'p', POPT_ARG_STRING, &prog, 'p' },
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n' },
	{ NULL, 'f', POPT_ARG_STRING, &file, 'f' },
	{ 0, 0, 0, 0, 0 }
    };

int parseScript(Spec spec, int parsePart)
{
    /* There are a few options to scripts: */
    /*  <pkg>                              */
    /*  -n <pkg>                           */
    /*  -p <sh>                            */
    /*  -p "<sh> <args>..."                */
    /*  -f <file>                          */

    char *p;
    char **progArgv = NULL;
    int progArgc;
    char *partname = NULL;
    int tag = 0;
    int progtag = 0;
    int flag = PART_SUBNAME;
    Package pkg;
    StringBuf sb;
    int nextPart;

    char *name = NULL;
    char *prog = "/bin/sh";
    char *file = NULL;
    
    int rc, argc;
    char arg, **argv = NULL;
    poptContext optCon = NULL;
    
    switch (parsePart) {
      case PART_PRE:
	tag = RPMTAG_PREIN;
	progtag = RPMTAG_PREINPROG;
	partname = "%pre";
	break;
      case PART_POST:
	tag = RPMTAG_POSTIN;
	progtag = RPMTAG_POSTINPROG;
	partname = "%post";
	break;
      case PART_PREUN:
	tag = RPMTAG_PREUN;
	progtag = RPMTAG_PREUNPROG;
	partname = "%preun";
	break;
      case PART_POSTUN:
	tag = RPMTAG_POSTUN;
	progtag = RPMTAG_POSTUNPROG;
	partname = "%postun";
	break;
      case PART_VERIFYSCRIPT:
	tag = PART_VERIFYSCRIPT;
	progtag = RPMTAG_VERIFYSCRIPTPROG;
	partname = "%verifyscript";
    }

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, "line %d: Error parsing %s: %s",
		 spec->lineNum, partname, poptStrerror(rc));
	return RPMERR_BADSPEC;
    }
    
    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	if (arg == 'p') {
	    if (prog[0] != '/') {
		rpmError(RPMERR_BADSPEC,
			 "line %d: script program must begin "
			 "with \'/\': %s", prog);
		FREE(argv);
		poptFreeContext(optCon);
		return RPMERR_BADSPEC;
	    }
	} else if (arg == 'n') {
	    flag = PART_NAME;
	}
    }
    
    if (arg < -1) {
	rpmError(RPMERR_BADSPEC, "line %d: Bad option %s: %s",
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 spec->line);
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }
    
    if (poptPeekArg(optCon)) {
	if (! name) {
	    name = poptGetArg(optCon);
	}
	if (poptPeekArg(optCon)) {
	    rpmError(RPMERR_BADSPEC, "line %d: Too many names: %s",
		     spec->lineNum,
		     spec->line);
	    FREE(argv);
	    poptFreeContext(optCon);
	    return RPMERR_BADSPEC;
	}
    }
    
    if (lookupPackage(spec, name, flag, &pkg)) {
	rpmError(RPMERR_BADSPEC, "line %d: Package does not exist: %s",
		 spec->lineNum, spec->line);
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }

    if (headerIsEntry(pkg->header, progtag)) {
	rpmError(RPMERR_BADSPEC, "line %d: Second %s",
		 spec->lineNum, partname);
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }

    if ((rc = poptParseArgvString(prog, &progArgc, &progArgv))) {
	rpmError(RPMERR_BADSPEC, "line %d: Error parsing %s: %s",
		 spec->lineNum, partname, poptStrerror(rc));
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }
    
    sb = newStringBuf();
    if (readLine(spec, STRIP_NOTHING) > 0) {
	nextPart = PART_NONE;
    } else {
	while (! (nextPart = isPart(spec->line))) {
	    appendStringBuf(sb, spec->line);
	    if (readLine(spec, STRIP_NOTHING) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	}
    }
    stripTrailingBlanksStringBuf(sb);
    p = getStringBuf(sb);

#if USE_PROG_STRING_ARRAY
    addReqProv(spec, pkg, RPMSENSE_PREREQ, progArgv[0], NULL);
    headerAddEntry(pkg->header, progtag, RPM_STRING_ARRAY_TYPE,
		   progArgv, progArgc);
#else
    addReqProv(spec, pkg, RPMSENSE_PREREQ, prog, NULL);
    headerAddEntry(pkg->header, progtag, RPM_STRING_TYPE, prog, 1);
#endif
    if (*p) {
	headerAddEntry(pkg->header, tag, RPM_STRING_TYPE, p, 1);
    }

    if (file) {
	switch (parsePart) {
	  case PART_PRE:
	    pkg->preInFile = strdup(file);
	    break;
	  case PART_POST:
	    pkg->postInFile = strdup(file);
	    break;
	  case PART_PREUN:
	    pkg->preUnFile = strdup(file);
	    break;
	  case PART_POSTUN:
	    pkg->postUnFile = strdup(file);
	    break;
	  case PART_VERIFYSCRIPT:
	    pkg->verifyFile = strdup(file);
	    break;
	}
    }
    
    freeStringBuf(sb);
    FREE(progArgv);
    FREE(argv);
    poptFreeContext(optCon);
    
    return nextPart;
}
