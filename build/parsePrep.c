/** \ingroup rpmbuild
 * \file build/parsePrep.c
 *  Parse %prep section from spec file.
 */

#include "system.h"

#include <rpmio_internal.h>
#include <rpmbuild.h>
#include "debug.h"

/*@access StringBuf @*/	/* compared with NULL */

/* These have to be global to make up for stupid compilers */
/*@unchecked@*/
    static int leaveDirs, skipDefaultAction;
/*@unchecked@*/
    static int createDir, quietly;
/*@unchecked@*/
/*@observer@*/ /*@null@*/ static const char * dirName = NULL;
/*@unchecked@*/
/*@observer@*/ static struct poptOption optionsTable[] = {
	    { NULL, 'a', POPT_ARG_STRING, NULL, 'a',	NULL, NULL},
	    { NULL, 'b', POPT_ARG_STRING, NULL, 'b',	NULL, NULL},
	    { NULL, 'c', 0, &createDir, 0,		NULL, NULL},
	    { NULL, 'D', 0, &leaveDirs, 0,		NULL, NULL},
	    { NULL, 'n', POPT_ARG_STRING, &dirName, 0,	NULL, NULL},
	    { NULL, 'T', 0, &skipDefaultAction, 0,	NULL, NULL},
	    { NULL, 'q', 0, &quietly, 0,		NULL, NULL},
	    { 0, 0, 0, 0, 0,	NULL, NULL}
    };

/**
 * Check that file owner and group are known.
 * @param urlfn		file url
 * @return		0 on success
 */
static int checkOwners(const char * urlfn)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    struct stat sb;

    if (Lstat(urlfn, &sb)) {
	rpmError(RPMERR_BADSPEC, _("Bad source: %s: %s\n"),
		urlfn, strerror(errno));
	return RPMERR_BADSPEC;
    }
    if (!getUname(sb.st_uid) || !getGname(sb.st_gid)) {
	rpmError(RPMERR_BADSPEC, _("Bad owner/group: %s\n"), urlfn);
	return RPMERR_BADSPEC;
    }

    return 0;
}

/**
 * Expand %patchN macro into %prep scriptlet.
 * @param spec		build info
 * @param c		patch index
 * @param strip		patch level (i.e. patch -p argument)
 * @param db		saved file suffix (i.e. patch --suffix argument)
 * @param reverse	include -R?
 * @param removeEmpties	include -E?
 * @return		expanded %patch macro (NULL on error)
 */
/*@observer@*/ static char *doPatch(Spec spec, int c, int strip, const char *db,
		     int reverse, int removeEmpties)
	/*@globals rpmGlobalMacroContext,
		fileSystem@*/
	/*@modifies fileSystem @*/
{
    const char *fn, *urlfn;
    static char buf[BUFSIZ];
    char args[BUFSIZ];
    struct Source *sp;
    rpmCompressedMagic compressed = COMPRESSED_NOT;
    int urltype;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISPATCH) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmError(RPMERR_BADSPEC, _("No patch number %d\n"), c);
	return NULL;
    }

    urlfn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

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
    if (!spec->force && (isCompressed(urlfn, &compressed) || checkOwners(urlfn))) {
	urlfn = _free(urlfn);
	return NULL;
    }

    fn = NULL;
    urltype = urlPath(urlfn, &fn);
    switch (urltype) {
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
	urlfn = _free(urlfn);
	return NULL;
	/*@notreached@*/ break;
    }

    if (compressed) {
	const char *zipper = rpmGetPath(
	    (compressed == COMPRESSED_BZIP2 ? "%{_bzip2bin}" : "%{_gzipbin}"),
	    NULL);

	sprintf(buf,
		"echo \"Patch #%d (%s):\"\n"
		"%s -d < %s | patch -p%d %s -s\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi",
		c, /*@-unrecog@*/ (const char *) basename(fn), /*@=unrecog@*/
		zipper,
		fn, strip, args);
	zipper = _free(zipper);
    } else {
	sprintf(buf,
		"echo \"Patch #%d (%s):\"\n"
		"patch -p%d %s -s < %s", c, (const char *) basename(fn),
		strip, args, fn);
    }

    urlfn = _free(urlfn);
    return buf;
}

/**
 * Expand %setup macro into %prep scriptlet.
 * @param spec		build info
 * @param c		source index
 * @param quietly	should -vv be omitted from tar?
 * @return		expanded %setup macro (NULL on error)
 */
/*@observer@*/ static const char *doUntar(Spec spec, int c, int quietly)
	/*@globals rpmGlobalMacroContext,
		fileSystem@*/
	/*@modifies fileSystem @*/
{
    const char *fn, *urlfn;
    static char buf[BUFSIZ];
    char *taropts;
    char *t = NULL;
    struct Source *sp;
    rpmCompressedMagic compressed = COMPRESSED_NOT;
    int urltype;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISSOURCE) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmError(RPMERR_BADSPEC, _("No source number %d\n"), c);
	return NULL;
    }

    urlfn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

    /*@-internalglobs@*/ /* FIX: shrug */
    taropts = ((rpmIsVerbose() && !quietly) ? "-xvvf" : "-xf");
    /*@=internalglobs@*/

#ifdef AUTOFETCH_NOT	/* XXX don't expect this code to be enabled */
    /* XXX
     * XXX If nosource file doesn't exist, try to fetch from url.
     * XXX TODO: add a "--fetch" enabler.
     */
    if (sp->flags & RPMTAG_NOSOURCE && autofetchnosource) {
	struct stat st;
	int rc;
	if (Lstat(urlfn, &st) != 0 && errno == ENOENT &&
	    urlIsUrl(sp->fullSource) != URL_IS_UNKNOWN) {
	    if ((rc = urlGetFile(sp->fullSource, urlfn)) != 0) {
		rpmError(RPMERR_BADFILENAME,
			_("Couldn't download nosource %s: %s\n"),
			sp->fullSource, ftpStrerror(rc));
		return NULL;
	    }
	}
    }
#endif

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (!spec->force && (isCompressed(urlfn, &compressed) || checkOwners(urlfn))) {
	urlfn = _free(urlfn);
	return NULL;
    }

    fn = NULL;
    urltype = urlPath(urlfn, &fn);
    switch (urltype) {
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
	urlfn = _free(urlfn);
	return NULL;
	/*@notreached@*/ break;
    }

    if (compressed != COMPRESSED_NOT) {
	const char *zipper;
	int needtar = 1;

	switch (compressed) {
	case COMPRESSED_NOT:	/* XXX can't happen */
	case COMPRESSED_OTHER:
	    t = "%{_gzipbin} -dc";
	    break;
	case COMPRESSED_BZIP2:
	    t = "%{_bzip2bin} -dc";
	    break;
	case COMPRESSED_ZIP:
	    t = "%{_unzipbin}";
	    needtar = 0;
	    break;
	}
	zipper = rpmGetPath(t, NULL);
	buf[0] = '\0';
	t = stpcpy(buf, zipper);
	zipper = _free(zipper);
	*t++ = ' ';
	t = stpcpy(t, fn);
	if (needtar)
	    t = stpcpy( stpcpy( stpcpy(t, " | tar "), taropts), " -");
	t = stpcpy(t,
		"\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi");
    } else {
	buf[0] = '\0';
	t = stpcpy( stpcpy(buf, "tar "), taropts);
	*t++ = ' ';
	t = stpcpy(t, fn);
    }

    urlfn = _free(urlfn);
    return buf;
}

/**
 * Parse %setup macro.
 * @todo FIXME: Option -q broken when not immediately after %setup.
 * @param spec		build info
 * @param line		current line from spec file
 * @return		0 on success
 */
static int doSetupMacro(Spec spec, char *line)
	/*@globals rpmGlobalMacroContext,
		fileSystem@*/
	/*@modifies spec->buildSubdir, spec->macros, spec->prep,
		fileSystem @*/
{
    char buf[BUFSIZ];
    StringBuf before;
    StringBuf after;
    poptContext optCon;
    int argc;
    const char ** argv;
    int arg;
    const char * optArg;
    int rc;
    int num;

    leaveDirs = skipDefaultAction = 0;
    createDir = quietly = 0;
    dirName = NULL;

    if ((rc = poptParseArgvString(line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, _("Error parsing %%setup: %s\n"),
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
	    rpmError(RPMERR_BADSPEC, _("line %d: Bad arg to %%setup: %s\n"),
		     spec->lineNum, (optArg ? optArg : "???"));
	    before = freeStringBuf(before);
	    after = freeStringBuf(after);
	    optCon = poptFreeContext(optCon);
	    argv = _free(argv);
	    return RPMERR_BADSPEC;
	}

	{   const char *chptr = doUntar(spec, num, quietly);
	    if (chptr == NULL)
		return RPMERR_BADSPEC;

	    appendLineStringBuf((arg == 'a' ? after : before), chptr);
	}
    }

    if (arg < -1) {
	rpmError(RPMERR_BADSPEC, _("line %d: Bad %%setup option %s: %s\n"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 poptStrerror(arg));
	before = freeStringBuf(before);
	after = freeStringBuf(after);
	optCon = poptFreeContext(optCon);
	argv = _free(argv);
	return RPMERR_BADSPEC;
    }

    if (dirName) {
	spec->buildSubdir = xstrdup(dirName);
    } else {
	const char *name, *version;
	(void) headerNVR(spec->packages->header, &name, &version, NULL);
	sprintf(buf, "%s-%s", name, version);
	spec->buildSubdir = xstrdup(buf);
    }
    addMacro(spec->macros, "buildsubdir", NULL, spec->buildSubdir, RMIL_SPEC);
    
    optCon = poptFreeContext(optCon);
    argv = _free(argv);

    /* cd to the build dir */
    {	const char * buildDirURL = rpmGenPath(spec->rootURL, "%{_builddir}", "");
	const char *buildDir;

	(void) urlPath(buildDirURL, &buildDir);
	sprintf(buf, "cd %s", buildDir);
	appendLineStringBuf(spec->prep, buf);
	buildDirURL = _free(buildDirURL);
    }
    
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
	const char *chptr = doUntar(spec, 0, quietly);
	if (!chptr)
	    return RPMERR_BADSPEC;
	appendLineStringBuf(spec->prep, chptr);
    }

    appendStringBuf(spec->prep, getStringBuf(before));
    before = freeStringBuf(before);

    if (!createDir) {
	sprintf(buf, "cd %s", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    if (createDir && !skipDefaultAction) {
	const char * chptr = doUntar(spec, 0, quietly);
	if (chptr == NULL)
	    return RPMERR_BADSPEC;
	appendLineStringBuf(spec->prep, chptr);
    }
    
    appendStringBuf(spec->prep, getStringBuf(after));
    after = freeStringBuf(after);

    /* XXX FIXME: owner & group fixes were conditioned on !geteuid() */
    /* Fix the owner, group, and permissions of the setup build tree */
    {	/*@observer@*/ static const char *fixmacs[] =
		{ "%{_fixowner}", "%{_fixgroup}", "%{_fixperms}", NULL };
	const char ** fm;

	for (fm = fixmacs; *fm; fm++) {
	    const char *fix;
	    /*@-nullpass@*/
	    fix = rpmExpand(*fm, " .", NULL);
	    /*@=nullpass@*/
	    if (fix && *fix != '%')
		appendLineStringBuf(spec->prep, fix);
	    fix = _free(fix);
	}
    }
    
    return 0;
}

/**
 * Parse %patch line.
 * @param spec		build info
 * @param line		current line from spec file
 * @return		0 on success
 */
static int doPatchMacro(Spec spec, char *line)
	/*@globals rpmGlobalMacroContext,
		fileSystem@*/
	/*@modifies spec->prep, fileSystem @*/
{
    char *opt_b;
    int opt_P, opt_p, opt_R, opt_E;
    char *s;
    char buf[BUFSIZ], *bp;
    int patch_nums[1024];  /* XXX - we can only handle 1024 patches! */
    int patch_index, x;

    memset(patch_nums, 0, sizeof(patch_nums));
    opt_P = opt_p = opt_R = opt_E = 0;
    opt_b = NULL;
    patch_index = 0;

    if (! strchr(" \t\n", line[6])) {
	/* %patchN */
	sprintf(buf, "%%patch -P %s", line + 6);
    } else {
	strcpy(buf, line);
    }
    
    /*@-internalglobs@*/	/* FIX: strtok has state */
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
		rpmError(RPMERR_BADSPEC,
			_("line %d: Need arg to %%patch -b: %s\n"),
			spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else if (!strcmp(s, "-z")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmError(RPMERR_BADSPEC,
			_("line %d: Need arg to %%patch -z: %s\n"),
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
			     _("line %d: Need arg to %%patch -p: %s\n"),
			     spec->lineNum, spec->line);
		    return RPMERR_BADSPEC;
		}
	    }
	    if (parseNum(s, &opt_p)) {
		rpmError(RPMERR_BADSPEC,
			_("line %d: Bad arg to %%patch -p: %s\n"),
			spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else {
	    /* Must be a patch num */
	    if (patch_index == 1024) {
		rpmError(RPMERR_BADSPEC, _("Too many patches!\n"));
		return RPMERR_BADSPEC;
	    }
	    if (parseNum(s, &(patch_nums[patch_index]))) {
		rpmError(RPMERR_BADSPEC, _("line %d: Bad arg to %%patch: %s\n"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	    patch_index++;
	}
    }
    /*@=internalglobs@*/

    /* All args processed */

    if (! opt_P) {
	s = doPatch(spec, 0, opt_p, opt_b, opt_R, opt_E);
	if (s == NULL)
	    return RPMERR_BADSPEC;
	appendLineStringBuf(spec->prep, s);
    }

    for (x = 0; x < patch_index; x++) {
	s = doPatch(spec, patch_nums[x], opt_p, opt_b, opt_R, opt_E);
	if (s == NULL)
	    return RPMERR_BADSPEC;
	appendLineStringBuf(spec->prep, s);
    }
    
    return 0;
}

int parsePrep(Spec spec)
{
    int nextPart, res, rc;
    StringBuf sb;
    char **lines, **saveLines;

    if (spec->prep != NULL) {
	rpmError(RPMERR_BADSPEC, _("line %d: second %%prep\n"), spec->lineNum);
	return RPMERR_BADSPEC;
    }

    spec->prep = newStringBuf();

    /* There are no options to %prep */
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	return PART_NONE;
    }
    if (rc)
	return rc;
    
    sb = newStringBuf();
    
    while (! (nextPart = isPart(spec->line))) {
	/* Need to expand the macros inline.  That way we  */
	/* can give good line number information on error. */
	appendStringBuf(sb, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	    nextPart = PART_NONE;
	    break;
	}
	if (rc)
	    return rc;
    }

    saveLines = splitString(getStringBuf(sb), strlen(getStringBuf(sb)), '\n');
    /*@-usereleased@*/
    for (lines = saveLines; *lines; lines++) {
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
	    sb = freeStringBuf(sb);
	    return res;
	}
    }
    /*@=usereleased@*/

    freeSplitString(saveLines);
    sb = freeStringBuf(sb);

    return nextPart;
}
