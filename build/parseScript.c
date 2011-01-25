/** \ingroup rpmbuild
 * \file build/parseScript.c
 *  Parse install-time script section from spec file.
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmlog.h>

#include "rpmio/rpmlua.h"
#include "lib/rpmscript.h"	/* script flags */
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"

#include "debug.h"


/**
 */
static int addTriggerIndex(Package pkg, const char *file,
	const char *script, const char *prog, rpmscriptFlags flags)
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
    tfe->flags = flags;
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

    const char *p;
    const char **progArgv = NULL;
    int progArgc;
    const char *partname = NULL;
    rpmTagVal reqtag = 0;
    rpmTagVal tag = 0;
    rpmsenseFlags tagflags = 0;
    rpmTagVal progtag = 0;
    rpmTagVal flagtag = 0;
    rpmscriptFlags scriptFlags = 0;
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
    int expand = 0;
    int qformat = 0;
    struct poptOption optionsTable[] = {
	{ NULL, 'p', POPT_ARG_STRING, &prog, 'p',	NULL, NULL},
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n',	NULL, NULL},
	{ NULL, 'f', POPT_ARG_STRING, &file, 'f',	NULL, NULL},
	{ NULL, 'e', POPT_ARG_NONE, &expand, 'e',	NULL, NULL},
	{ NULL, 'q', POPT_ARG_NONE, &qformat, 'q',	NULL, NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL}
    };

    switch (parsePart) {
      case PART_PRE:
	tag = RPMTAG_PREIN;
	tagflags = RPMSENSE_SCRIPT_PRE;
	progtag = RPMTAG_PREINPROG;
	flagtag = RPMTAG_PREINFLAGS;
	partname = "%pre";
	break;
      case PART_POST:
	tag = RPMTAG_POSTIN;
	tagflags = RPMSENSE_SCRIPT_POST;
	progtag = RPMTAG_POSTINPROG;
	flagtag = RPMTAG_POSTINFLAGS;
	partname = "%post";
	break;
      case PART_PREUN:
	tag = RPMTAG_PREUN;
	tagflags = RPMSENSE_SCRIPT_PREUN;
	progtag = RPMTAG_PREUNPROG;
	flagtag = RPMTAG_PREUNFLAGS;
	partname = "%preun";
	break;
      case PART_POSTUN:
	tag = RPMTAG_POSTUN;
	tagflags = RPMSENSE_SCRIPT_POSTUN;
	progtag = RPMTAG_POSTUNPROG;
	flagtag = RPMTAG_POSTUNFLAGS;
	partname = "%postun";
	break;
      case PART_PRETRANS:
	tag = RPMTAG_PRETRANS;
	tagflags = RPMSENSE_PRETRANS;
	progtag = RPMTAG_PRETRANSPROG;
	flagtag = RPMTAG_PRETRANSFLAGS;
	partname = "%pretrans";
	break;
      case PART_POSTTRANS:
	tag = RPMTAG_POSTTRANS;
	tagflags = RPMSENSE_POSTTRANS;
	progtag = RPMTAG_POSTTRANSPROG;
	flagtag = RPMTAG_POSTTRANSFLAGS;
	partname = "%posttrans";
	break;
      case PART_VERIFYSCRIPT:
	tag = RPMTAG_VERIFYSCRIPT;
	tagflags = RPMSENSE_SCRIPT_VERIFY;
	progtag = RPMTAG_VERIFYSCRIPTPROG;
	flagtag = RPMTAG_VERIFYSCRIPTFLAGS;
	partname = "%verifyscript";
	break;
      case PART_TRIGGERPREIN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	tagflags = 0;
	reqtag = RPMTAG_TRIGGERPREIN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	flagtag = RPMTAG_TRIGGERSCRIPTFLAGS;
	partname = "%triggerprein";
	break;
      case PART_TRIGGERIN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	tagflags = 0;
	reqtag = RPMTAG_TRIGGERIN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	flagtag = RPMTAG_TRIGGERSCRIPTFLAGS;
	partname = "%triggerin";
	break;
      case PART_TRIGGERUN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	tagflags = 0;
	reqtag = RPMTAG_TRIGGERUN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	flagtag = RPMTAG_TRIGGERSCRIPTFLAGS;
	partname = "%triggerun";
	break;
      case PART_TRIGGERPOSTUN:
	tag = RPMTAG_TRIGGERSCRIPTS;
	tagflags = 0;
	reqtag = RPMTAG_TRIGGERPOSTUN;
	progtag = RPMTAG_TRIGGERSCRIPTPROG;
	flagtag = RPMTAG_TRIGGERSCRIPTFLAGS;
	partname = "%triggerpostun";
	break;
    }

    if (tag == RPMTAG_TRIGGERSCRIPTS) {
	/* break line into two */
	char *s = strstr(spec->line, "--");
	if (!s) {
	    rpmlog(RPMLOG_ERR, _("line %d: triggers must have --: %s\n"),
		     spec->lineNum, spec->line);
	    return PART_ERROR;
	}

	*s = '\0';
	reqargs = xstrdup(s + 2);
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

    scriptFlags |= expand ? RPMSCRIPT_EXPAND : 0;
    scriptFlags |= qformat ? RPMSCRIPT_QFORMAT : 0;

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
        (void) addReqProv(pkg->header, RPMTAG_REQUIRENAME,
		progArgv[0], NULL, (tagflags | RPMSENSE_INTERP), 0);
    }

    if (scriptFlags) {
	rpmlibNeedsFeature(pkg->header, "ScriptletExpansion", "4.9.0-1");
    }

    /* Trigger script insertion is always delayed in order to */
    /* get the index right.                                   */
    if (tag == RPMTAG_TRIGGERSCRIPTS) {
	/* Add file/index/prog triple to the trigger file list */
	index = addTriggerIndex(pkg, file, p, progArgv[0], scriptFlags);

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
	if (scriptFlags) {
	    headerPutUint32(pkg->header, flagtag, &scriptFlags, 1);
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
