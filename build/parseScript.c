/** \ingroup rpmbuild
 * \file build/parseScript.c
 *  Parse install-time script section from spec file.
 */

#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

/**
 */
static int addTriggerIndex(Package pkg, const char *file, const char *script, const char *prog)
{
    struct TriggerFileEntry *new;
    struct TriggerFileEntry *list = pkg->triggerFiles;
    struct TriggerFileEntry *last = NULL;
    int index = 0;

    while (list) {
	last = list;
	list = list->next;
    }

    if (last) {
	index = last->index + 1;
    }

    new = xmalloc(sizeof(*new));

    new->fileName = (file) ? xstrdup(file) : NULL;
    new->script = (*script) ? xstrdup(script) : NULL;
    new->prog = xstrdup(prog);
    new->index = index;
    new->next = NULL;

    if (last) {
	last->next = new;
    } else {
	pkg->triggerFiles = new;
    }

    return index;
}

/* these have to be global because of stupid compilers */
    /*@observer@*/ /*@null@*/ static const char *name = NULL;
    /*@observer@*/ /*@null@*/ static const char *prog = NULL;
    /*@observer@*/ /*@null@*/ static const char *file = NULL;
    static struct poptOption optionsTable[] = {
	{ NULL, 'p', POPT_ARG_STRING, &prog, 'p',	NULL, NULL},
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n',	NULL, NULL},
	{ NULL, 'f', POPT_ARG_STRING, &file, 'f',	NULL, NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL}
    };

/* %trigger is a strange combination of %pre and Requires: behavior */
/* We can handle it by parsing the args before "--" in parseScript. */
/* We then pass the remaining arguments to parseRCPOT, along with   */
/* an index we just determined.                                     */

int parseScript(Spec spec, int parsePart)
{
    /* There are a few options to scripts: */
    /*  <pkg>                              */
    /*  -n <pkg>                           */
    /*  -p <sh>                            */
    /*  -p "<sh> <args>..."                */
    /*  -f <file>                          */

    char *p;
    const char **progArgv = NULL;
    int progArgc;
    char *partname = NULL;
    int reqtag = 0;
    int tag = 0;
    int tagflags = 0;
    int progtag = 0;
    int flag = PART_SUBNAME;
    Package pkg;
    StringBuf sb = NULL;
    int nextPart;
    int index;
    char reqargs[BUFSIZ];

    int rc, argc;
    int arg;
    const char **argv = NULL;
    poptContext optCon = NULL;
    
    name = NULL;
    prog = "/bin/sh";
    file = NULL;
    
    switch (parsePart) {
      case PART_PRE:
	tag = RPMTAG_PREIN;
	tagflags = RPMSENSE_SCRIPT_PRE;
	progtag = RPMTAG_PREINPROG;
	partname = "%pre";
	break;
      case PART_POST:
	tag = RPMTAG_POSTIN;
	tagflags = RPMSENSE_SCRIPT_POST;
	progtag = RPMTAG_POSTINPROG;
	partname = "%post";
	break;
      case PART_PREUN:
	tag = RPMTAG_PREUN;
	tagflags = RPMSENSE_SCRIPT_PREUN;
	progtag = RPMTAG_PREUNPROG;
	partname = "%preun";
	break;
      case PART_POSTUN:
	tag = RPMTAG_POSTUN;
	tagflags = RPMSENSE_SCRIPT_POSTUN;
	progtag = RPMTAG_POSTUNPROG;
	partname = "%postun";
	break;
      case PART_VERIFYSCRIPT:
	tag = RPMTAG_VERIFYSCRIPT;
	tagflags = RPMSENSE_SCRIPT_VERIFY;
	progtag = RPMTAG_VERIFYSCRIPTPROG;
	partname = "%verifyscript";
	break;
      case PART_TRIGGERIN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	tagflags = 0;
	reqtag = RPMTAG_TRIGGERIN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	partname = "%triggerin";
	break;
      case PART_TRIGGERUN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	tagflags = 0;
	reqtag = RPMTAG_TRIGGERUN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	partname = "%triggerun";
	break;
      case PART_TRIGGERPOSTUN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	tagflags = 0;
	reqtag = RPMTAG_TRIGGERPOSTUN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	partname = "%triggerpostun";
	break;
    }

    if (tag == RPMTAG_TRIGGERSCRIPTS) {
	/* break line into two */
	p = strstr(spec->line, "--");
	if (!p) {
	    rpmError(RPMERR_BADSPEC, _("line %d: triggers must have --: %s\n"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}

	*p = '\0';
	strcpy(reqargs, p + 2);
    }
    
    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, _("line %d: Error parsing %s: %s\n"),
		 spec->lineNum, partname, poptStrerror(rc));
	return RPMERR_BADSPEC;
    }
    
    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	switch (arg) {
	case 'p':
	    if (prog[0] != '/') {
		rpmError(RPMERR_BADSPEC,
			 _("line %d: script program must begin "
			 "with \'/\': %s\n"), spec->lineNum, prog);
		rc = RPMERR_BADSPEC;
		goto exit;
	    }
	    break;
	case 'n':
	    flag = PART_NAME;
	    break;
	}
    }
    
    if (arg < -1) {
	rpmError(RPMERR_BADSPEC, _("line %d: Bad option %s: %s\n"),
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
	    rpmError(RPMERR_BADSPEC, _("line %d: Too many names: %s\n"),
		     spec->lineNum,
		     spec->line);
	    rc = RPMERR_BADSPEC;
	    goto exit;
	}
    }
    
    if (lookupPackage(spec, name, flag, &pkg)) {
	rpmError(RPMERR_BADSPEC, _("line %d: Package does not exist: %s\n"),
		 spec->lineNum, spec->line);
	rc = RPMERR_BADSPEC;
	goto exit;
    }

    if (tag != RPMTAG_TRIGGERSCRIPTS) {
	if (headerIsEntry(pkg->header, progtag)) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Second %s\n"),
		     spec->lineNum, partname);
	    rc = RPMERR_BADSPEC;
	    goto exit;
	}
    }

    if ((rc = poptParseArgvString(prog, &progArgc, &progArgv))) {
	rpmError(RPMERR_BADSPEC, _("line %d: Error parsing %s: %s\n"),
		 spec->lineNum, partname, poptStrerror(rc));
	rc = RPMERR_BADSPEC;
	goto exit;
    }

    sb = newStringBuf();
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc)
	    goto exit;
	while (! (nextPart = isPart(spec->line))) {
	    appendStringBuf(sb, spec->line);
	    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc)
		goto exit;
	}
    }
    stripTrailingBlanksStringBuf(sb);
    p = getStringBuf(sb);

    addReqProv(spec, pkg->header, (tagflags | RPMSENSE_INTERP), progArgv[0], NULL, 0);

    /* Trigger script insertion is always delayed in order to */
    /* get the index right.                                   */
    if (tag == RPMTAG_TRIGGERSCRIPTS) {
	/* Add file/index/prog triple to the trigger file list */
	index = addTriggerIndex(pkg, file, p, progArgv[0]);

	/* Generate the trigger tags */
	if ((rc = parseRCPOT(spec, pkg, reqargs, reqtag, index, tagflags)))
	    goto exit;
    } else {
	if (progArgc == 1)
	    headerAddEntry(pkg->header, progtag, RPM_STRING_TYPE,
			*progArgv, progArgc);
	else
	    headerAddEntry(pkg->header, progtag, RPM_STRING_ARRAY_TYPE,
			progArgv, progArgc);

	if (*p)
	    headerAddEntry(pkg->header, tag, RPM_STRING_TYPE, p, 1);

	if (file) {
	    switch (parsePart) {
	      case PART_PRE:
		pkg->preInFile = xstrdup(file);
		break;
	      case PART_POST:
		pkg->postInFile = xstrdup(file);
		break;
	      case PART_PREUN:
		pkg->preUnFile = xstrdup(file);
		break;
	      case PART_POSTUN:
		pkg->postUnFile = xstrdup(file);
		break;
	      case PART_VERIFYSCRIPT:
		pkg->verifyFile = xstrdup(file);
		break;
	    }
	}
    }
    rc = nextPart;
    
exit:
    if (sb)
	freeStringBuf(sb);
    progArgv = _free(progArgv);
    argv = _free(argv);
    if (optCon)
	poptFreeContext(optCon);
    
    return rc;
}
