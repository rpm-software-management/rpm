#include "system.h"

#include "rpmbuild.h"

/* These have to be global to make up for stupid compilers */
    static int leaveDirs, skipDefaultAction;
    static int createDir, quietly;
    static char * dirName;
    static struct poptOption optionsTable[] = {
	    { NULL, 'a', POPT_ARG_STRING, NULL, 'a',	NULL, NULL},
	    { NULL, 'b', POPT_ARG_STRING, NULL, 'b',	NULL, NULL},
	    { NULL, 'c', 0, &createDir, 0,		NULL, NULL},
	    { NULL, 'D', 0, &leaveDirs, 0,		NULL, NULL},
	    { NULL, 'n', POPT_ARG_STRING, &dirName, 0,	NULL, NULL},
	    { NULL, 'T', 0, &skipDefaultAction, 0,	NULL, NULL},
	    { NULL, 'q', 0, &quietly, 0,		NULL, NULL},
	    { 0, 0, 0, 0, 0,	NULL, NULL}
    };

static int checkOwners(const char *file)
{
    struct stat sb;

    if (lstat(file, &sb)) {
	rpmError(RPMERR_BADSPEC, _("Bad source: %s: %s"), file, strerror(errno));
	return RPMERR_BADSPEC;
    }
    if (!getUname(sb.st_uid) || !getGname(sb.st_gid)) {
	rpmError(RPMERR_BADSPEC, _("Bad owner/group: %s"), file);
	return RPMERR_BADSPEC;
    }

    return 0;
}

static char *doPatch(Spec spec, int c, int strip, char *db,
		     int reverse, int removeEmpties)
{
    const char *fn = NULL;
    static char buf[BUFSIZ];
    char args[BUFSIZ];
    struct Source *sp;
    int compressed = 0;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISPATCH) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmError(RPMERR_BADSPEC, _("No patch number %d"), c);
	return NULL;
    }

    fn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

    args[0] = '\0';
    if (db) {
#if HAVE_OLDPATCH_21 == 0
	strcat(args, "-b ");
#endif
	strcat(args, "--suffix ");
	strcat(args, db);
    }
    if (reverse) {
	strcat(args, " -R");
    }
    if (removeEmpties) {
	strcat(args, " -E");
    }

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (!spec->force && (isCompressed(fn, &compressed) || checkOwners(fn))) {
	xfree(fn);
	return NULL;
    }

    if (compressed) {
	const char *zipper = rpmGetPath(
	    (compressed == COMPRESSED_BZIP2 ? "%{_bzip2bin}" : "%{_gzipbin}"),
	    NULL);
	sprintf(buf,
		"echo \"Patch #%d:\"\n"
		"%s -d < %s | patch -p%d %s -s\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi",
		c,
		zipper,
		fn, strip, args);
	xfree(zipper);
    } else {
	sprintf(buf,
		"echo \"Patch #%d:\"\n"
		"patch -p%d %s -s < %s", c, strip, args, fn);
    }

    xfree(fn);
    return buf;
}

static char *doUntar(Spec spec, int c, int quietly)
{
    const char *fn;
    static char buf[BUFSIZ];
    char *taropts;
    struct Source *sp;
    int compressed = 0;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISSOURCE) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmError(RPMERR_BADSPEC, _("No source number %d"), c);
	return NULL;
    }

    fn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

    taropts = ((rpmIsVerbose() && !quietly) ? "-xvvf" : "-xf");

#ifdef AUTOFETCH_NOT	/* XXX don't expect this code to be enabled */
    /* XXX
     * XXX If nosource file doesn't exist, try to fetch from url.
     * XXX TODO: add a "--fetch" enabler.
     */
    if (sp->flags & RPMTAG_NOSOURCE && autofetchnosource) {
	struct stat st;
	int rc;
	if (lstat(fn, &st) != 0 && errno == ENOENT &&
	    urlIsUrl(sp->fullSource) != URL_IS_UNKNOWN) {
	    if ((rc = urlGetFile(sp->fullSource, fn)) != 0) {
		rpmError(RPMERR_BADFILENAME, _("Couldn't download nosource %s: %s"),
		    sp->fullSource, ftpStrerror(rc));
		return NULL;
	    }
	}
    }
#endif

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (!spec->force && (isCompressed(fn, &compressed) || checkOwners(fn))) {
	xfree(fn);
	return NULL;
    }

    if (compressed) {
	const char *zipper = rpmGetPath(
	    (compressed == COMPRESSED_BZIP2 ? "%{_bzip2bin}" : "%{_gzipbin}"),
	    NULL);
	sprintf(buf,
		"%s -dc %s | tar %s -\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi",
		zipper,
		fn, taropts);
	xfree(zipper);
    } else {
	sprintf(buf, "tar %s %s", taropts, fn);
    }

    xfree(fn);
    return buf;
}

static int doSetupMacro(Spec spec, char *line)
{
    char *version, *name;
    char buf[BUFSIZ];
    StringBuf before;
    StringBuf after;
    poptContext optCon;
    int argc;
    char ** argv;
    int arg;
    char * optArg;
    char * chptr;
    int rc;
    int num;

    leaveDirs = skipDefaultAction = 0;
    createDir = quietly = 0;
    dirName = NULL;

    if ((rc = poptParseArgvString(line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, _("Error parsing %%setup: %s"),
			poptStrerror(rc));
	return RPMERR_BADSPEC;
    }

    before = newStringBuf();
    after = newStringBuf();

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	/* We only parse -a and -b here */

	if (parseNum(optArg, &num)) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Bad arg to %%setup %c: %s"),
		     spec->lineNum, num, optArg);
	    free(argv);
	    freeStringBuf(before);
	    freeStringBuf(after);
	    poptFreeContext(optCon);
	    return RPMERR_BADSPEC;
	}

	chptr = doUntar(spec, num, quietly);
	if (!chptr) {
	    return RPMERR_BADSPEC;
	}

	if (arg == 'a')
	    appendLineStringBuf(after, chptr);
	else
	    appendLineStringBuf(before, chptr);
    }

    if (arg < -1) {
	rpmError(RPMERR_BADSPEC, _("line %d: Bad %%setup option %s: %s"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 poptStrerror(arg));
	free(argv);
	freeStringBuf(before);
	freeStringBuf(after);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }

    if (dirName) {
	spec->buildSubdir = strdup(dirName);
    } else {
	headerGetEntry(spec->packages->header, RPMTAG_VERSION, NULL,
		 (void **) &version, NULL);
	headerGetEntry(spec->packages->header, RPMTAG_NAME, NULL,
		 (void **) &name, NULL);
	sprintf(buf, "%s-%s", name, version);
	spec->buildSubdir = strdup(buf);
    }
    
    free(argv);
    poptFreeContext(optCon);

    /* cd to the build dir */
    strcpy(buf, "cd %{_builddir}");
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    appendLineStringBuf(spec->prep, buf);
    
    /* delete any old sources */
    if (!leaveDirs) {
	sprintf(buf, "rm -rf %s", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    /* if necessary, create and cd into the proper dir */
    if (createDir) {
	sprintf(buf, MKDIR_P " %s\ncd %s",
		spec->buildSubdir, spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    /* do the default action */
   if (!createDir && !skipDefaultAction) {
	chptr = doUntar(spec, 0, quietly);
	if (!chptr) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, chptr);
    }

    appendStringBuf(spec->prep, getStringBuf(before));
    freeStringBuf(before);

    if (!createDir) {
	sprintf(buf, "cd %s", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    if (createDir && !skipDefaultAction) {
	chptr = doUntar(spec, 0, quietly);
	if (!chptr) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, chptr);
    }
    
    appendStringBuf(spec->prep, getStringBuf(after));
    freeStringBuf(after);

    /* XXX FIXME: owner & group fixes were conditioned on !geteuid() */
    /* Fix the owner, group, and permissions of the setup build tree */
    {	static const char *fixmacs[] = {
	    "%{_fixowner}", "%{_fixgroup}", "%{_fixperms}", NULL
	};
	const char **fm;

	for (fm = fixmacs; *fm; fm++) {
	    const char *fix = rpmExpand(*fm, " .", NULL);
	    if (fix && *fix != '%')
		appendLineStringBuf(spec->prep, fix);
	    xfree(fix);
	}
    }
    
    return 0;
}

static int doPatchMacro(Spec spec, char *line)
{
    char *opt_b;
    int opt_P, opt_p, opt_R, opt_E;
    char *s;
    char buf[BUFSIZ], *bp;
    int patch_nums[1024];  /* XXX - we can only handle 1024 patches! */
    int patch_index, x;

    opt_P = opt_p = opt_R = opt_E = 0;
    opt_b = NULL;
    patch_index = 0;

    if (! strchr(" \t\n", line[6])) {
	/* %patchN */
	sprintf(buf, "%%patch -P %s", line + 6);
    } else {
	strcpy(buf, line);
    }
    
    for (bp = buf; (s = strtok(bp, " \t\n")) != NULL;) {
	if (bp) {	/* remove 1st token (%patch) */
		bp = NULL;
		continue;
	}
	if (!strcmp(s, "-P")) {
	    opt_P = 1;
	} else if (!strcmp(s, "-R")) {
	    opt_R = 1;
	} else if (!strcmp(s, "-E")) {
	    opt_E = 1;
	} else if (!strcmp(s, "-b")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmError(RPMERR_BADSPEC, _("line %d: Need arg to %%patch -b: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else if (!strcmp(s, "-z")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmError(RPMERR_BADSPEC, _("line %d: Need arg to %%patch -z: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else if (!strncmp(s, "-p", sizeof("-p")-1)) {
	    /* unfortunately, we must support -pX */
	    if (! strchr(" \t\n", s[2])) {
		s = s + 2;
	    } else {
		s = strtok(NULL, " \t\n");
		if (s == NULL) {
		    rpmError(RPMERR_BADSPEC,
			     _("line %d: Need arg to %%patch -p: %s"),
			     spec->lineNum, spec->line);
		    return RPMERR_BADSPEC;
		}
	    }
	    if (parseNum(s, &opt_p)) {
		rpmError(RPMERR_BADSPEC, _("line %d: Bad arg to %%patch -p: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else {
	    /* Must be a patch num */
	    if (patch_index == 1024) {
		rpmError(RPMERR_BADSPEC, _("Too many patches!"));
		return RPMERR_BADSPEC;
	    }
	    if (parseNum(s, &(patch_nums[patch_index]))) {
		rpmError(RPMERR_BADSPEC, _("line %d: Bad arg to %%patch: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	    patch_index++;
	}
    }

    /* All args processed */

    if (! opt_P) {
	s = doPatch(spec, 0, opt_p, opt_b, opt_R, opt_E);
	if (s == NULL) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, s);
    }

    for (x = 0; x < patch_index; x++) {
	s = doPatch(spec, patch_nums[x], opt_p, opt_b, opt_R, opt_E);
	if (s == NULL) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, s);
    }
    
    return 0;
}

int parsePrep(Spec spec)
{
    int nextPart, res, rc;
    StringBuf buf;
    char **lines, **saveLines;

    if (spec->prep != NULL) {
	rpmError(RPMERR_BADSPEC, _("line %d: second %%prep"), spec->lineNum);
	return RPMERR_BADSPEC;
    }

    spec->prep = newStringBuf();

    /* There are no options to %prep */
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	return PART_NONE;
    }
    if (rc) {
	return rc;
    }
    
    buf = newStringBuf();
    
    while (! (nextPart = isPart(spec->line))) {
	/* Need to expand the macros inline.  That way we  */
	/* can give good line number information on error. */
	appendStringBuf(buf, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	    nextPart = PART_NONE;
	    break;
	}
	if (rc) {
	    return rc;
	}
    }

    lines = splitString(getStringBuf(buf), strlen(getStringBuf(buf)), '\n');
    saveLines = lines;
    while (*lines) {
	res = 0;
	if (! strncmp(*lines, "%setup", sizeof("%setup")-1)) {
	    res = doSetupMacro(spec, *lines);
	} else if (! strncmp(*lines, "%patch", sizeof("%patch")-1)) {
	    res = doPatchMacro(spec, *lines);
	} else {
	    appendLineStringBuf(spec->prep, *lines);
	}
	if (res && !spec->force) {
	    freeSplitString(saveLines);
	    freeStringBuf(buf);
	    return res;
	}
	lines++;
    }

    freeSplitString(saveLines);
    freeStringBuf(buf);

    return nextPart;
}
