/** \ingroup rpmbuild
 * \file build/parseScript.c
 *  Parse install-time script section from spec file.
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>

#include "rpmio/rpmlua.h"

#include "debug.h"


/**
 */
static int addTriggerIndex(Package pkg, const char *file,
	const char *script, const char *prog)
{
    struct TriggerFileEntry *tfe;
    struct TriggerFileEntry *list = pkg->triggerFiles;
    struct TriggerFileEntry *last = NULL;
    int index = 0;

    while (list) {
	last = list;
	list = list->next;
    }

    if (last)
	index = last->index + 1;

    tfe = xcalloc(1, sizeof(*tfe));

    tfe->fileName = (file) ? xstrdup(file) : NULL;
    tfe->script = (script && *script != '\0') ? xstrdup(script) : NULL;
    tfe->prog = xstrdup(prog);
    tfe->index = index;
    tfe->next = NULL;

    if (last)
	last->next = tfe;
    else
	pkg->triggerFiles = tfe;

    return index;
}

/* %trigger is a strange combination of %pre and Requires: behavior */
/* We can handle it by parsing the args before "--" in parseScript. */
/* We then pass the remaining arguments to parseRCPOT, along with   */
/* an index we just determined.                                     */

int parseScript(rpmSpec spec, int parsePart)
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
    const char *partname = NULL;
    rpmTag reqtag = 0;
    rpmTag tag = 0;
    rpmsenseFlags tagflags = 0;
    rpmTag progtag = 0;
    int flag = PART_SUBNAME;
    Package pkg;
    StringBuf sb = NULL;
    int nextPart;
    int index;
    char * reqargs = NULL;

    int res = PART_ERROR; /* assume failure */
    int rc, argc;
    int arg;
    const char **argv = NULL;
    poptContext optCon = NULL;
    const char *name = NULL;
    const char *prog = "/bin/sh";
    const char *file = NULL;
    struct poptOption optionsTable[] = {
	{ NULL, 'p', POPT_ARG_STRING, &prog, 'p',	NULL, NULL},
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n',	NULL, NULL},
	{ NULL, 'f', POPT_ARG_STRING, &file, 'f',	NULL, NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL}
    };

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
      case PART_PRETRANS:
	tag = RPMTAG_PRETRANS;
	tagflags = 0;
	progtag = RPMTAG_PRETRANSPROG;
	partname = "%pretrans";
	break;
      case PART_POSTTRANS:
	tag = RPMTAG_POSTTRANS;
	tagflags = 0;
	progtag = RPMTAG_POSTTRANSPROG;
	partname = "%posttrans";
	break;
      case PART_VERIFYSCRIPT:
	tag = RPMTAG_VERIFYSCRIPT;
	tagflags = RPMSENSE_SCRIPT_VERIFY;
	progtag = RPMTAG_VERIFYSCRIPTPROG;
	partname = "%verifyscript";
	break;
      case PART_TRIGGERPREIN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	tagflags = 0;
	reqtag = RPMTAG_TRIGGERPREIN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	partname = "%triggerprein";
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
	    rpmlog(RPMLOG_ERR, _("line %d: triggers must have --: %s\n"),
		     spec->lineNum, spec->line);
	    return PART_ERROR;
	}

	*p = '\0';
	reqargs = xstrdup(p + 2);
    }
    
    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("line %d: Error parsing %s: %s\n"),
		 spec->lineNum, partname, poptStrerror(rc));
	goto exit;
    }
    
    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	switch (arg) {
	case 'p':
	    if (prog[0] == '<') {
		if (prog[strlen(prog)-1] != '>') {
		    rpmlog(RPMLOG_ERR,
			     _("line %d: internal script must end "
			     "with \'>\': %s\n"), spec->lineNum, prog);
		    goto exit;
		}
	    } else if (prog[0] != '/') {
		rpmlog(RPMLOG_ERR,
			 _("line %d: script program must begin "
			 "with \'/\': %s\n"), spec->lineNum, prog);
		goto exit;
	    }
	    break;
	case 'n':
	    flag = PART_NAME;
	    break;
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

    if (tag != RPMTAG_TRIGGERSCRIPTS) {
	if (headerIsEntry(pkg->header, progtag)) {
	    rpmlog(RPMLOG_ERR, _("line %d: Second %s\n"),
		     spec->lineNum, partname);
	    goto exit;
	}
    }

    if ((rc = poptParseArgvString(prog, &progArgc, &progArgv))) {
	rpmlog(RPMLOG_ERR, _("line %d: Error parsing %s: %s\n"),
		 spec->lineNum, partname, poptStrerror(rc));
	goto exit;
    }

    sb = newStringBuf();
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	nextPart = PART_NONE;
    } else if (rc < 0) {
	goto exit;
    } else {
	while (! (nextPart = isPart(spec->line))) {
	    appendStringBuf(sb, spec->line);
	    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
		nextPart = PART_NONE;
		break;
	    } else if (rc < 0) {
		goto exit;
	    }
	}
    }
    stripTrailingBlanksStringBuf(sb);
    p = getStringBuf(sb);

#ifdef WITH_LUA
    if (rstreq(progArgv[0], "<lua>")) {
	rpmlua lua = NULL; /* Global state. */
	if (rpmluaCheckScript(lua, p, partname) != RPMRC_OK) {
	    goto exit;
	}
	(void) rpmlibNeedsFeature(pkg->header,
				  "BuiltinLuaScripts", "4.2.2-1");
    } else
#endif
    if (progArgv[0][0] == '<') {
	rpmlog(RPMLOG_ERR,
		 _("line %d: unsupported internal script: %s\n"),
		 spec->lineNum, progArgv[0]);
	goto exit;
    } else {
        (void) addReqProv(spec, pkg->header, RPMTAG_REQUIRENAME,
		progArgv[0], NULL, (tagflags | RPMSENSE_INTERP), 0);
    }

    /* Trigger script insertion is always delayed in order to */
    /* get the index right.                                   */
    if (tag == RPMTAG_TRIGGERSCRIPTS) {
	/* Add file/index/prog triple to the trigger file list */
	index = addTriggerIndex(pkg, file, p, progArgv[0]);

	/* Generate the trigger tags */
	if ((rc = parseRCPOT(spec, pkg, reqargs, reqtag, index, tagflags)))
	    goto exit;
    } else {
	struct rpmtd_s td;

	/*
 	 * XXX Ancient rpm uses STRING, not STRING_ARRAY type here. Construct
 	 * the td manually and preserve legacy compat for now...
 	 */
	rpmtdReset(&td);
	td.tag = progtag;
	td.count = progArgc;
	if (progArgc == 1) {
	    td.data = (void *) *progArgv;
	    td.type = RPM_STRING_TYPE;
	} else {
	    (void) rpmlibNeedsFeature(pkg->header,
			"ScriptletInterpreterArgs", "4.0.3-1");
	    td.data = progArgv;
	    td.type = RPM_STRING_ARRAY_TYPE;
	}
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);

	if (*p != '\0') {
	    headerPutString(pkg->header, tag, p);
	}

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
	      case PART_PRETRANS:
		pkg->preTransFile = xstrdup(file);
		break;
	      case PART_POSTTRANS:
		pkg->postTransFile = xstrdup(file);
		break;
	      case PART_VERIFYSCRIPT:
		pkg->verifyFile = xstrdup(file);
		break;
	    }
	}
    }
    res = nextPart;
    
exit:
    free(reqargs);
    sb = freeStringBuf(sb);
    progArgv = _free(progArgv);
    argv = _free(argv);
    optCon = poptFreeContext(optCon);
    
    return res;
}
