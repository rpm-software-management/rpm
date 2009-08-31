/** \ingroup rpmbuild
 * \file build/parseDescription.c
 *  Parse %description section from spec file.
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include "debug.h"

extern int noLang;

int parseDescription(rpmSpec spec)
{
    int nextPart = PART_ERROR;	/* assume error */
    StringBuf sb;
    int flag = PART_SUBNAME;
    Package pkg;
    int rc, argc;
    int arg;
    const char **argv = NULL;
    const char *name = NULL;
    const char *lang = RPMBUILD_DEFAULT_LANG;
    poptContext optCon = NULL;
    spectag t = NULL;
    struct poptOption optionsTable[] = {
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n', NULL, NULL},
	{ NULL, 'l', POPT_ARG_STRING, &lang, 'l', NULL, NULL},
	{ 0, 0, 0, 0, 0, NULL, NULL}
    };

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("line %d: Error parsing %%description: %s\n"),
		 spec->lineNum, poptStrerror(rc));
	return PART_ERROR;
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


    /******************/

#if 0    
    if (headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	rpmlog(RPMLOG_ERR, _("line %d: Second description\n"),
		spec->lineNum);
	goto exit;
    }
#endif

    t = stashSt(spec, pkg->header, RPMTAG_DESCRIPTION, lang);
    
    sb = newStringBuf();

    if ((rc = readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else if (rc < 0) {
	    nextPart = PART_ERROR;
	    goto exit;
    } else {
	while (! (nextPart = isPart(spec->line))) {
	    appendLineStringBuf(sb, spec->line);
	    if (t) t->t_nlines++;
	    if ((rc =
		readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    } else if (rc < 0) {
		nextPart = PART_ERROR;
		goto exit;
	    }
	}
    }
    
    stripTrailingBlanksStringBuf(sb);
    if (!(noLang && !rstreq(lang, RPMBUILD_DEFAULT_LANG))) {
	(void) headerAddI18NString(pkg->header, RPMTAG_DESCRIPTION,
			getStringBuf(sb), lang);
    }
    
    sb = freeStringBuf(sb);
     
exit:
    argv = _free(argv);
    optCon = poptFreeContext(optCon);
    return nextPart;
}
