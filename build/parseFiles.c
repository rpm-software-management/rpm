/** \ingroup rpmbuild
 * \file build/parseFiles.c
 *  Parse %files section from spec file.
 */

#include "system.h"

#include "rpmbuild.h"

/* These have to be global scope to make up for *stupid* compilers */
    /*@observer@*/ /*@null@*/ static const char *name = NULL;
    /*@observer@*/ /*@null@*/ static const char *file = NULL;
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
    const char **argv = NULL;
    int flag = PART_SUBNAME;
    poptContext optCon = NULL;

    name = file = NULL;

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, _("line %d: Error parsing %%files: %s"),
		 spec->lineNum, poptStrerror(rc));
	rc = RPMERR_BADSPEC;
	goto exit;
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
	rc = RPMERR_BADSPEC;
	goto exit;
    }

    if (poptPeekArg(optCon)) {
	if (name == NULL)
	    name = poptGetArg(optCon);
	if (poptPeekArg(optCon)) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Too many names: %s"),
		     spec->lineNum,
		     spec->line);
	    rc = RPMERR_BADSPEC;
	    goto exit;
	}
    }

    if (lookupPackage(spec, name, flag, &pkg)) {
	rpmError(RPMERR_BADSPEC, _("line %d: Package does not exist: %s"),
		 spec->lineNum, spec->line);
	rc = RPMERR_BADSPEC;
	goto exit;
    }

    if (pkg->fileList != NULL) {
	rpmError(RPMERR_BADSPEC, _("line %d: Second %%files list"),
		 spec->lineNum);
	rc = RPMERR_BADSPEC;
	goto exit;
    }

    if (file)  {
    /* XXX not necessary as readline has expanded already, but won't hurt.  */
	pkg->fileFile = rpmGetPath(file, NULL);
    }

    pkg->fileList = newStringBuf();
    
    if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc)
	    goto exit;
	while (! (nextPart = isPart(spec->line))) {
	    appendStringBuf(pkg->fileList, spec->line);
	    if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc)
		goto exit;
	}
    }
    rc = nextPart;

exit:
    if (argv)
	FREE(argv);
    if (optCon)
	poptFreeContext(optCon);
	
    return rc;
}
