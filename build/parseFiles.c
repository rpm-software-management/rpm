#include <string.h>
#include <malloc.h>

#include "header.h"
#include "read.h"
#include "part.h"
#include "misc.h"
#include "rpmlib.h"
#include "package.h"
#include "stringbuf.h"
#include "popt/popt.h"

int parseFiles(Spec spec)
{
    int nextPart;
    Package pkg;
    char *name = NULL;
    char *file = NULL;
    int rc, argc;
    char arg, **argv = NULL;
    int flag = PART_SUBNAME;
    poptContext optCon = NULL;
    struct poptOption optionsTable[] = {
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n' },
	{ NULL, 'f', POPT_ARG_STRING, &file, 'f' },
	{ 0, 0, 0, 0, 0 }
    };

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, "line %d: Error parsing %%files: %s",
		 spec->lineNum, poptStrerror(rc));
	return RPMERR_BADSPEC;
    }

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	if (arg == 'n') {
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

    if (pkg->fileList) {
	rpmError(RPMERR_BADSPEC, "line %d: Second %%files list",
		 spec->lineNum);
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }

    if (file) {
	pkg->fileFile = strdup(file);
    }
    pkg->fileList = newStringBuf();
    
    if (readLine(spec, STRIP_NOTHING) > 0) {
	nextPart = PART_NONE;
    } else {
	while (! (nextPart = isPart(spec->line))) {
	    appendStringBuf(pkg->fileList, spec->line);
	    if (readLine(spec, STRIP_NOTHING) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	}
    }

    FREE(argv);
    poptFreeContext(optCon);
	
    return nextPart;
}
