#include "system.h"

#include "rpmbuild.h"

#include "popt/popt.h"

static int addTriggerIndex(Package pkg, char *file, char *script, char *prog)
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

    new = malloc(sizeof(*new));

    new->fileName = (file) ? strdup(file) : NULL;
    new->script = (*script) ? strdup(script) : NULL;
    new->prog = strdup(prog);
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
    static char *name;
    static char *prog;
    static char *file;
    static struct poptOption optionsTable[] = {
	{ NULL, 'p', POPT_ARG_STRING, &prog, 'p',	NULL, NULL},
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n',	NULL, NULL},
	{ NULL, 'f', POPT_ARG_STRING, &file, 'f',	NULL, NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL}
    };

/* %trigger is a strange combination of %pre and Requires: behavior */
/* We can handle it by parsing the args before "--" in parseScript. */
/* We then pass the remaining arguments to parseReqProv, along with */
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
    char **progArgv = NULL;
    int progArgc;
    char *partname = NULL;
    int reqtag = 0;
    int tag = 0;
    int progtag = 0;
    int flag = PART_SUBNAME;
    Package pkg;
    StringBuf sb;
    int nextPart;
    int index;
    char reqargs[BUFSIZ];

    int rc, argc;
    int arg;
    char **argv = NULL;
    poptContext optCon = NULL;
    
    name = NULL;
    prog = "/bin/sh";
    file = NULL;
    
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
	break;
      case PART_TRIGGERIN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	reqtag = RPMTAG_TRIGGERIN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	partname = "%triggerin";
	break;
      case PART_TRIGGERUN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	reqtag = RPMTAG_TRIGGERUN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	partname = "%triggerun";
	break;
      case PART_TRIGGERPOSTUN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	reqtag = RPMTAG_TRIGGERPOSTUN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	partname = "%triggerpostun";
	break;
    }

    if (tag == RPMTAG_TRIGGERSCRIPTS) {
	/* break line into two */
	p = strstr(spec->line, "--");
	if (!p) {
	    rpmError(RPMERR_BADSPEC, _("line %d: triggers must have --: %s"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}

	*p = '\0';
	strcpy(reqargs, p + 2);
    }
    
    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, _("line %d: Error parsing %s: %s"),
		 spec->lineNum, partname, poptStrerror(rc));
	return RPMERR_BADSPEC;
    }
    
    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	if (arg == 'p') {
	    if (prog[0] != '/') {
		rpmError(RPMERR_BADSPEC,
			 _("line %d: script program must begin "
			 "with \'/\': %s"), spec->lineNum, prog);
		FREE(argv);
		poptFreeContext(optCon);
		return RPMERR_BADSPEC;
	    }
	} else if (arg == 'n') {
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

    if (tag != RPMTAG_TRIGGERSCRIPTS) {
	if (headerIsEntry(pkg->header, progtag)) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Second %s"),
		     spec->lineNum, partname);
	    FREE(argv);
	    poptFreeContext(optCon);
	    return RPMERR_BADSPEC;
	}
    }

    if ((rc = poptParseArgvString(prog, &progArgc, &progArgv))) {
	rpmError(RPMERR_BADSPEC, _("line %d: Error parsing %s: %s"),
		 spec->lineNum, partname, poptStrerror(rc));
	FREE(argv);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }
    
    sb = newStringBuf();
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc) {
	    return rc;
	}
	while (! (nextPart = isPart(spec->line))) {
	    appendStringBuf(sb, spec->line);
	    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc) {
		return rc;
	    }
	}
    }
    stripTrailingBlanksStringBuf(sb);
    p = getStringBuf(sb);

    addReqProv(spec, pkg->header, RPMSENSE_PREREQ, prog, NULL, 0);

    /* Trigger script insertion is always delayed in order to */
    /* get the index right.                                   */
    if (tag == RPMTAG_TRIGGERSCRIPTS) {
	/* Add file/index/prog triple to the trigger file list */
	index = addTriggerIndex(pkg, file, p, prog);

	/* Generate the trigger tags */
	if ((rc = parseRequiresConflicts(spec, pkg, reqargs, reqtag, index))) {
	    freeStringBuf(sb);
	    FREE(progArgv);
	    FREE(argv);
	    poptFreeContext(optCon);
	    return rc;
	}
    } else {
	headerAddEntry(pkg->header, progtag, RPM_STRING_TYPE, prog, 1);
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
    }
    
    freeStringBuf(sb);
    FREE(progArgv);
    FREE(argv);
    poptFreeContext(optCon);
    
    return nextPart;
}
