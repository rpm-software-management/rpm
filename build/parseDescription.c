/** \ingroup rpmbuild
 * \file build/parseDescription.c
 *  Parse %description section from spec file.
 */

#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

extern int noLang;		/* XXX FIXME: pass as arg */

/* These have to be global scope to make up for *stupid* compilers */
    /*@observer@*/ /*@null@*/ static const char *name = NULL;
    /*@observer@*/ /*@null@*/ static const char *lang = NULL;

    static struct poptOption optionsTable[] = {
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n',	NULL, NULL},
	{ NULL, 'l', POPT_ARG_STRING, &lang, 'l',	NULL, NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL}
    };

/** */
int parseDescription(Spec spec)
{
    int nextPart;
    StringBuf sb;
    int flag = PART_SUBNAME;
    Package pkg;
    int rc, argc;
    int arg;
    const char **argv = NULL;
    poptContext optCon = NULL;
    struct spectag *t = NULL;

    name = NULL;
    lang = RPMBUILD_DEFAULT_LANG;

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, _("line %d: Error parsing %%description: %s"),
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
	if (name == NULL)
	    name = poptGetArg(optCon);
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


    /******************/

#if 0    
    if (headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	rpmError(RPMERR_BADSPEC, _("line %d: Second description"), spec->lineNum);
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }
#endif

    t = stashSt(spec, pkg->header, RPMTAG_DESCRIPTION, lang);
    
    sb = newStringBuf();

    if ((rc = readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc) {
	    return rc;
	}
	while (! (nextPart = isPart(spec->line))) {
	    appendLineStringBuf(sb, spec->line);
	    if (t) t->t_nlines++;
	    if ((rc =
		 readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc) {
		return rc;
	    }
	}
    }
    
    stripTrailingBlanksStringBuf(sb);
    if (!(noLang && strcmp(lang, RPMBUILD_DEFAULT_LANG))) {
	headerAddI18NString(pkg->header, RPMTAG_DESCRIPTION,
			getStringBuf(sb), lang);
    }
    
    freeStringBuf(sb);
     
    FREE(argv);
    poptFreeContext(optCon);
    
    return nextPart;
}
