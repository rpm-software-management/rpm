#include "system.h"

#include "rpmbuild.h"

/* These have to be global scope to make up for *stupid* compilers */
    static char *name;
    static char *file;
    static struct poptOption optionsTable[] = {
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n',	NULL, NULL},
	{ NULL, 'f', POPT_ARG_STRING, &file, 'f',	NULL, NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL}
    };

int parseFiles(Spec spec)
{
    int nextPart;
    Package pkg;
    int rc, argc;
    int arg;
    char **argv = NULL;
    int flag = PART_SUBNAME;
    poptContext optCon = NULL;

    name = file = NULL;

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, _("line %d: Error parsing %%files: %s"),
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
	rpmError(RPMERR_BADSPEC, _("line %d: Bad option %s: %s"),
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
	    rpmError(RPMERR_BADSPEC, _("line %d: Too many names: %s"),
		     spec->lineNum,
		     spec->line);
	    FREE(argv);
	    poptFreeContext(optCon);
	    return RPMERR_BADSPEC;
	}
    }

    if (lookupPackage(spec, name, flag, &pkg)) {
	rpmError(RPMERR_BADSPEC, _("line %d: Package does not exist: %s"),
		 spec->lineNum, spec->line);
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }

    if (pkg->fileList != NULL) {
	rpmError(RPMERR_BADSPEC, _("line %d: Second %%files list"),
		 spec->lineNum);
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }

    if (file) {
	pkg->fileFile = xstrdup(file);
    }
    pkg->fileList = newStringBuf();
    
    if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc) {
	    return rc;
	}
	while (! (nextPart = isPart(spec->line))) {
	    appendStringBuf(pkg->fileList, spec->line);
	    if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc) {
		return rc;
	    }
	}
    }

    FREE(argv);
    poptFreeContext(optCon);
	
    return nextPart;
}
