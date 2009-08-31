/** \ingroup rpmbuild
 * \file build/parseFiles.c
 *  Parse %files section from spec file.
 */

#include "system.h"

#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include "debug.h"

int parseFiles(rpmSpec spec)
{
    int nextPart, res = PART_ERROR;
    Package pkg;
    int rc, argc;
    int arg;
    const char ** argv = NULL;
    const char *name = NULL;
    int flag = PART_SUBNAME;
    poptContext optCon = NULL;
    struct poptOption optionsTable[] = {
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n', NULL, NULL},
	{ NULL, 'f', POPT_ARG_STRING, NULL, 'f', NULL, NULL},
	{ 0, 0, 0, 0, 0, NULL, NULL}
    };

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("line %d: Error parsing %%files: %s\n"),
		 spec->lineNum, poptStrerror(rc));
	goto exit;
    }

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	if (arg == 'n') {
	    flag = PART_NAME;
	}
    }

    if (arg < -1) {
	rpmlog(RPMLOG_ERR, _("line %d: Bad option %s: %s\n"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 spec->line);
	goto exit;
    }

    if (poptPeekArg(optCon)) {
	if (name == NULL)
	    name = poptGetArg(optCon);
	if (poptPeekArg(optCon)) {
	    rpmlog(RPMLOG_ERR, _("line %d: Too many names: %s\n"),
		     spec->lineNum,
		     spec->line);
	    goto exit;
	}
    }

    if (lookupPackage(spec, name, flag, &pkg)) {
	rpmlog(RPMLOG_ERR, _("line %d: Package does not exist: %s\n"),
		 spec->lineNum, spec->line);
	goto exit;
    }

    for (arg=1; arg<argc; arg++) {
	if (rstreq(argv[arg], "-f") && argv[arg+1]) {
	    char *file = rpmGetPath(argv[arg+1], NULL);
	    if (!pkg->fileFile) pkg->fileFile = newStringBuf();
	    appendLineStringBuf(pkg->fileFile, file);
	    free(file);
	}
    }

    pkg->fileList = newStringBuf();
    
    if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else if (rc < 0) {
	goto exit;
    } else {
	while (! (nextPart = isPart(spec->line))) {
	    appendStringBuf(pkg->fileList, spec->line);
	    if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    } else if (rc < 0) {
		goto exit;
	    }
	}
    }
    res = nextPart;

exit:
    argv = _free(argv);
    optCon = poptFreeContext(optCon);
	
    return res;
}
