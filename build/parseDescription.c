#include <malloc.h>

#include "spec.h"
#include "header.h"
#include "read.h"
#include "part.h"
#include "misc.h"
#include "rpmlib.h"
#include "package.h"
#include "popt/popt.h"

int parseDescription(Spec spec)
{
    int nextPart;
    StringBuf sb;
    char *name = NULL;
    char *lang = RPMBUILD_DEFAULT_LANG;
    int flag = PART_SUBNAME;
    Package pkg;
    int rc, argc;
    char arg, **argv = NULL;
    poptContext optCon = NULL;
    struct poptOption optionsTable[] = {
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n' },
	{ NULL, 'l', POPT_ARG_STRING, &lang, 'l' },
	{ 0, 0, 0, 0, 0 }
    };

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, "line %d: Error parsing %%description: %s",
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


    /******************/

#if 0    
    if (headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	rpmError(RPMERR_BADSPEC, "line %d: Second description", spec->lineNum);
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }
#endif
    
    sb = newStringBuf();

    if (readLine(spec, STRIP_TRAILINGSPACE) > 0) {
	nextPart = PART_NONE;
    } else {
	while (! (nextPart = isPart(spec->line))) {
	    appendLineStringBuf(sb, spec->line);
	    if (readLine(spec, STRIP_TRAILINGSPACE) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	}
    }
    
    stripTrailingBlanksStringBuf(sb);
    headerAddI18NString(pkg->header, RPMTAG_DESCRIPTION,
			getStringBuf(sb), lang);
    
    freeStringBuf(sb);
     
    FREE(argv);
    poptFreeContext(optCon);
    
    return nextPart;
}
