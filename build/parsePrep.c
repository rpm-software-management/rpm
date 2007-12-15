/** \ingroup rpmbuild
 * \file build/parsePrep.c
 *  Parse %prep section from spec file.
 */

#include "system.h"

#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include "debug.h"

/* These have to be global to make up for stupid compilers */
    static int leaveDirs, skipDefaultAction;
    static int createDir, quietly;
static const char * dirName = NULL;
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

/**
 * Check that file owner and group are known.
 * @param urlfn		file url
 * @return		RPMRC_OK on success
 */
static rpmRC checkOwners(const char * urlfn)
{
    struct stat sb;

    if (lstat(urlfn, &sb)) {
	rpmlog(RPMLOG_ERR, _("Bad source: %s: %s\n"),
		urlfn, strerror(errno));
	return RPMRC_FAIL;
    }
    if (!getUname(sb.st_uid) || !getGname(sb.st_gid)) {
	rpmlog(RPMLOG_ERR, _("Bad owner/group: %s\n"), urlfn);
	return RPMRC_FAIL;
    }

    return RPMRC_OK;
}

/**
 * Expand %patchN macro into %prep scriptlet.
 * @param spec		build info
 * @param c		patch index
 * @param strip		patch level (i.e. patch -p argument)
 * @param db		saved file suffix (i.e. patch --suffix argument)
 * @param reverse	include -R?
 * @param fuzz		include -F?
 * @param removeEmpties	include -E?
 * @return		expanded %patch macro (NULL on error)
 */

static char *doPatch(rpmSpec spec, int c, int strip, const char *db,
		     int reverse, int removeEmpties, int fuzz)
{
    const char *fn, *urlfn;
    static char buf[BUFSIZ];
    char args[BUFSIZ], *t = args;
    struct Source *sp;
    rpmCompressedMagic compressed = COMPRESSED_NOT;
    int urltype;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISPATCH) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmlog(RPMLOG_ERR, _("No patch number %d\n"), c);
	return NULL;
    }

    urlfn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

    *t = '\0';
    if (db) {
#if HAVE_OLDPATCH_21 == 0
	t = stpcpy(t, "-b ");
#endif
	t = stpcpy( stpcpy(t, "--suffix "), db);
    }
    if (fuzz) {
	t = stpcpy(t, " -F");
	sprintf(t, "%d", fuzz);
	t += strlen(t);
    }
    if (reverse)
	t = stpcpy(t, " -R");
    if (removeEmpties)
	t = stpcpy(t, " -E");

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (!spec->force && (isCompressed(urlfn, &compressed) || checkOwners(urlfn))) {
	urlfn = _free(urlfn);
	return NULL;
    }

    fn = NULL;
    urltype = urlPath(urlfn, &fn);
    switch (urltype) {
    case URL_IS_HTTPS:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HKP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
	urlfn = _free(urlfn);
	return NULL;
	break;
    }

    if (compressed) {
	char *zipper = rpmGetPath(
	    (compressed == COMPRESSED_BZIP2 ? "%{_bzip2bin}" : "%{_gzipbin}"),
	    NULL);

	sprintf(buf,
		"echo \"Patch #%d (%s):\"\n"
		"%s -d < '%s' | patch -p%d %s -s\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi",
		c, (const char *) basename(fn),
		zipper,
		fn, strip, args);
	zipper = _free(zipper);
    } else {
	sprintf(buf,
		"echo \"Patch #%d (%s):\"\n"
		"patch -p%d %s -s < '%s'", c, (const char *) basename(fn),
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
static const char *doUntar(rpmSpec spec, int c, int quietly)
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
	rpmlog(RPMLOG_ERR, _("No source number %d\n"), c);
	return NULL;
    }

    urlfn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

    /* FIX: shrug */
    taropts = ((rpmIsVerbose() && !quietly) ? "-xvvf" : "-xf");

#ifdef AUTOFETCH_NOT	/* XXX don't expect this code to be enabled */
    /* XXX
     * XXX If nosource file doesn't exist, try to fetch from url.
     * XXX TODO: add a "--fetch" enabler.
     */
    if (sp->flags & RPMTAG_NOSOURCE && autofetchnosource) {
	struct stat st;
	int rc;
	if (lstat(urlfn, &st) != 0 && errno == ENOENT &&
	    urlIsUrl(sp->fullSource) != URL_IS_UNKNOWN) {
	    if ((rc = urlGetFile(sp->fullSource, urlfn)) != 0) {
		rpmlog(RPMLOG_ERR,
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
    case URL_IS_HTTPS:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HKP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
	urlfn = _free(urlfn);
	return NULL;
	break;
    }

    if (compressed != COMPRESSED_NOT) {
	char *zipper;
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
	    if (rpmIsVerbose() && !quietly)
		t = "%{_unzipbin}";
	    else
		t = "%{_unzipbin} -qq";
	    needtar = 0;
	    break;
	case COMPRESSED_LZMA:
	    t = "%{__lzma} -dc";
	    break;
	}
	zipper = rpmGetPath(t, NULL);
	buf[0] = '\0';
	t = stpcpy(buf, zipper);
	zipper = _free(zipper);
	t = stpcpy(t, " '");
	t = stpcpy(t, fn);
	t = stpcpy(t, "'");
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
 * @return		RPMRC_OK on success
 */
static int doSetupMacro(rpmSpec spec, char *line)
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
	rpmlog(RPMLOG_ERR, _("Error parsing %%setup: %s\n"),
			poptStrerror(rc));
	return RPMRC_FAIL;
    }

    before = newStringBuf();
    after = newStringBuf();

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	/* We only parse -a and -b here */

	if (parseNum(optArg, &num)) {
	    rpmlog(RPMLOG_ERR, _("line %d: Bad arg to %%setup: %s\n"),
		     spec->lineNum, (optArg ? optArg : "???"));
	    before = freeStringBuf(before);
	    after = freeStringBuf(after);
	    optCon = poptFreeContext(optCon);
	    argv = _free(argv);
	    return RPMRC_FAIL;
	}

	{   const char *chptr = doUntar(spec, num, quietly);
	    if (chptr == NULL)
		return RPMRC_FAIL;

	    appendLineStringBuf((arg == 'a' ? after : before), chptr);
	}
    }

    if (arg < -1) {
	rpmlog(RPMLOG_ERR, _("line %d: Bad %%setup option %s: %s\n"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 poptStrerror(arg));
	before = freeStringBuf(before);
	after = freeStringBuf(after);
	optCon = poptFreeContext(optCon);
	argv = _free(argv);
	return RPMRC_FAIL;
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
	sprintf(buf, "cd '%s'", buildDir);
	appendLineStringBuf(spec->prep, buf);
	buildDirURL = _free(buildDirURL);
    }
    
    /* delete any old sources */
    if (!leaveDirs) {
	sprintf(buf, "rm -rf '%s'", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    /* if necessary, create and cd into the proper dir */
    if (createDir) {
	sprintf(buf, RPM_MKDIR_P " %s\ncd '%s'",
		spec->buildSubdir, spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    /* do the default action */
   if (!createDir && !skipDefaultAction) {
	const char *chptr = doUntar(spec, 0, quietly);
	if (!chptr)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, chptr);
    }

    appendStringBuf(spec->prep, getStringBuf(before));
    before = freeStringBuf(before);

    if (!createDir) {
	sprintf(buf, "cd '%s'", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    if (createDir && !skipDefaultAction) {
	const char * chptr = doUntar(spec, 0, quietly);
	if (chptr == NULL)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, chptr);
    }
    
    appendStringBuf(spec->prep, getStringBuf(after));
    after = freeStringBuf(after);

    /* XXX FIXME: owner & group fixes were conditioned on !geteuid() */
    /* Fix the owner, group, and permissions of the setup build tree */
    {	static const char *fixmacs[] =
		{ "%{_fixowner}", "%{_fixgroup}", "%{_fixperms}", NULL };
	const char ** fm;

	for (fm = fixmacs; *fm; fm++) {
	    char * fix = rpmExpand(*fm, " .", NULL);
	    if (fix && *fix != '%')
		appendLineStringBuf(spec->prep, fix);
	    fix = _free(fix);
	}
    }
    
    return RPMRC_OK;
}

/**
 * Parse %patch line.
 * @param spec		build info
 * @param line		current line from spec file
 * @return		RPMRC_OK on success
 */
static rpmRC doPatchMacro(rpmSpec spec, char *line)
{
    char *opt_b;
    int opt_P, opt_p, opt_R, opt_E, opt_F;
    char *s;
    char buf[BUFSIZ], *bp;
    int patch_nums[1024];  /* XXX - we can only handle 1024 patches! */
    int patch_index, x;

    memset(patch_nums, 0, sizeof(patch_nums));
    opt_P = opt_p = opt_R = opt_E = opt_F = 0;
    opt_b = NULL;
    patch_index = 0;

    if (! strchr(" \t\n", line[6])) {
	/* %patchN */
	sprintf(buf, "%%patch -P %s", line + 6);
    } else {
	strcpy(buf, line);
    }
    
   	/* FIX: strtok has state */
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
		rpmlog(RPMLOG_ERR,
			_("line %d: Need arg to %%patch -b: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else if (!strcmp(s, "-z")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmlog(RPMLOG_ERR,
			_("line %d: Need arg to %%patch -z: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else if (!strncmp(s, "-F", strlen("-F"))) {
	    /* fuzz factor */
	    const char * fnum = NULL;
	    char * end = NULL;

	    if (! strchr(" \t\n", s[2])) {
		fnum = s + 2;
	    } else {
		fnum = strtok(NULL, " \t\n");
	    }
	    opt_F = (fnum ? strtol(fnum, &end, 10) : 0);
	    if (! opt_F || *end) {
		rpmlog(RPMLOG_ERR,
			_("line %d: Bad arg to %%patch -F: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else if (!strncmp(s, "-p", sizeof("-p")-1)) {
	    /* unfortunately, we must support -pX */
	    if (! strchr(" \t\n", s[2])) {
		s = s + 2;
	    } else {
		s = strtok(NULL, " \t\n");
		if (s == NULL) {
		    rpmlog(RPMLOG_ERR,
			     _("line %d: Need arg to %%patch -p: %s\n"),
			     spec->lineNum, spec->line);
		    return RPMRC_FAIL;
		}
	    }
	    if (parseNum(s, &opt_p)) {
		rpmlog(RPMLOG_ERR,
			_("line %d: Bad arg to %%patch -p: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else {
	    /* Must be a patch num */
	    if (patch_index == 1024) {
		rpmlog(RPMLOG_ERR, _("Too many patches!\n"));
		return RPMRC_FAIL;
	    }
	    if (parseNum(s, &(patch_nums[patch_index]))) {
		rpmlog(RPMLOG_ERR, _("line %d: Bad arg to %%patch: %s\n"),
			 spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	    patch_index++;
	}
    }

    /* All args processed */

    if (! opt_P) {
	s = doPatch(spec, 0, opt_p, opt_b, opt_R, opt_E, opt_F);
	if (s == NULL)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, s);
    }

    for (x = 0; x < patch_index; x++) {
	s = doPatch(spec, patch_nums[x], opt_p, opt_b, opt_R, opt_E, opt_F);
	if (s == NULL)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, s);
    }
    
    return RPMRC_OK;
}

int parsePrep(rpmSpec spec)
{
    int nextPart, res, rc;
    StringBuf sb;
    char **lines, **saveLines;

    if (spec->prep != NULL) {
	rpmlog(RPMLOG_ERR, _("line %d: second %%prep\n"), spec->lineNum);
	return RPMRC_FAIL;
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

    freeSplitString(saveLines);
    sb = freeStringBuf(sb);

    return nextPart;
}
