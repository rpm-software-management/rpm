/** \ingroup rpmbuild
 * \file build/parsePrep.c
 *  Parse %prep section from spec file.
 */

#include "system.h"

#include <rpm/header.h>
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
 * @param fuzz		fuzz factor, fuzz<0 means no fuzz set
 * @param removeEmpties	include -E?
 * @return		expanded %patch macro (NULL on error)
 */

static char *doPatch(rpmSpec spec, int c, int strip, const char *db,
		     int reverse, int removeEmpties, int fuzz)
{
    char *fn;
    char *buf = NULL;
    char *arg_backup = NULL;
    char *arg_fuzz = NULL;
    char *args = NULL;
    struct Source *sp;
    char *patchcmd;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISPATCH) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmlog(RPMLOG_ERR, _("No patch number %d\n"), c);
	return NULL;
    }

    fn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

    /*
     * FIXME: On non-build parse's, file cannot be stat'd or read but
     * %{uncompress} doesn't know that so we get errors on non-existent files.
     */
    if (!spec->force && checkOwners(fn)) {
	fn = _free(fn);
	return NULL;
    }

    if (db) {
	rasprintf(&arg_backup,
#if HAVE_OLDPATCH_21 == 0
		  "-b "
#endif
		  "--suffix %s", db);
    } else arg_backup = xstrdup("");

    if (fuzz >= 0) {
	rasprintf(&arg_fuzz, " --fuzz=%d", fuzz);
    } else arg_fuzz = xstrdup("");

    rasprintf(&args, "-s -p%d %s%s%s%s", strip, arg_backup, arg_fuzz, 
		reverse ? " -R" : "", 
		removeEmpties ? " -E" : "");

    patchcmd = rpmExpand("%{uncompress: ", fn, "} | %{__patch} ", args, NULL);

    free(arg_fuzz);
    free(arg_backup);
    free(args);
    
    rasprintf(&buf, "echo \"Patch #%d (%s):\"\n"
		    "%s\n", 
		    strip, basename(fn), patchcmd);
		
    free(fn);
    free(patchcmd);
    
    return buf;
}

/**
 * Expand %setup macro into %prep scriptlet.
 * @param spec		build info
 * @param c		source index
 * @param quietly	should -vv be omitted from tar?
 * @return		expanded %setup macro (NULL on error)
 */
static char *doUntar(rpmSpec spec, int c, int quietly)
{
    char *fn;
    char *buf = NULL;
    char *tar, *taropts;
    struct Source *sp;
    rpmCompressedMagic compressed = COMPRESSED_NOT;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISSOURCE) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmlog(RPMLOG_ERR, _("No source number %d\n"), c);
	return NULL;
    }

    fn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

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
	if (lstat(fn, &st) != 0 && errno == ENOENT &&
	    urlIsUrl(sp->fullSource) != URL_IS_UNKNOWN) {
	    if ((rc = urlGetFile(sp->fullSource, fn)) != 0) {
		rpmlog(RPMLOG_ERR,
			_("Couldn't download nosource %s: %s\n"),
			sp->fullSource, ftpStrerror(rc));
		return NULL;
	    }
	}
    }
#endif

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (!spec->force && (rpmFileIsCompressed(fn, &compressed) || checkOwners(fn))) {
	fn = _free(fn);
	return NULL;
    }

    tar = rpmGetPath("%{__tar}", NULL);
    if (compressed != COMPRESSED_NOT) {
	char *zipper, *t = NULL;
	int needtar = 1;

	switch (compressed) {
	case COMPRESSED_NOT:	/* XXX can't happen */
	case COMPRESSED_OTHER:
	    t = "%{__gzip} -dc";
	    break;
	case COMPRESSED_BZIP2:
	    t = "%{__bzip2} -dc";
	    break;
	case COMPRESSED_ZIP:
	    if (rpmIsVerbose() && !quietly)
		t = "%{__unzip}";
	    else
		t = "%{__unzip} -qq";
	    needtar = 0;
	    break;
	case COMPRESSED_LZMA:
	    t = "%{__lzma} -dc";
	    break;
	}
	zipper = rpmGetPath(t, NULL);
	if (needtar) {
	    rasprintf(&buf, "%s '%s' | %s %s - \n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi", zipper, fn, tar, taropts);
	} else {
	    rasprintf(&buf, "%s '%s'\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi", zipper, fn);
	}
	zipper = _free(zipper);
    } else {
	rasprintf(&buf, "%s %s %s", tar, taropts, fn);
    }

    fn = _free(fn);
    tar = _free(tar);
    return buf;
}

/**
 * Parse %setup macro.
 * @todo FIXME: Option -q broken when not immediately after %setup.
 * @param spec		build info
 * @param line		current line from spec file
 * @return		RPMRC_OK on success
 */
static int doSetupMacro(rpmSpec spec, const char *line)
{
    char *buf = NULL;
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

	{   char *chptr = doUntar(spec, num, quietly);
	    if (chptr == NULL)
		return RPMRC_FAIL;

	    appendLineStringBuf((arg == 'a' ? after : before), chptr);
	    free(chptr);
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
	rasprintf(&spec->buildSubdir, "%s-%s", name, version);
    }
    addMacro(spec->macros, "buildsubdir", NULL, spec->buildSubdir, RMIL_SPEC);
    
    optCon = poptFreeContext(optCon);
    argv = _free(argv);

    /* cd to the build dir */
    {	char * buildDir = rpmGenPath(spec->rootDir, "%{_builddir}", "");

	rasprintf(&buf, "cd '%s'", buildDir);
	appendLineStringBuf(spec->prep, buf);
	free(buf);
	free(buildDir);
    }
    
    /* delete any old sources */
    if (!leaveDirs) {
	rasprintf(&buf, "rm -rf '%s'", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
	free(buf);
    }

    /* if necessary, create and cd into the proper dir */
    if (createDir) {
	rasprintf(&buf, RPM_MKDIR_P " %s\ncd '%s'",
		spec->buildSubdir, spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
	free(buf);
    }

    /* do the default action */
   if (!createDir && !skipDefaultAction) {
	char *chptr = doUntar(spec, 0, quietly);
	if (!chptr)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, chptr);
	free(chptr);
    }

    appendStringBuf(spec->prep, getStringBuf(before));
    before = freeStringBuf(before);

    if (!createDir) {
	rasprintf(&buf, "cd '%s'", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
	free(buf);
    }

    if (createDir && !skipDefaultAction) {
	char *chptr = doUntar(spec, 0, quietly);
	if (chptr == NULL)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, chptr);
	free(chptr);
    }
    
    appendStringBuf(spec->prep, getStringBuf(after));
    after = freeStringBuf(after);

    /* Fix the permissions of the setup build tree */
    {	char *fix = rpmExpand("%{_fixperms} .", NULL);
	if (fix && *fix != '%') {
	    appendLineStringBuf(spec->prep, fix);
	}
	free(fix);
    }
	
    return RPMRC_OK;
}

/**
 * Parse %patch line.
 * This supports too many crazy syntaxes:
 * - %patch is equal to %patch0
 * - %patchN is equal to %patch -P<N>
 * - -P<N> -P<N+1>... can be used to apply several patch on a single line
 * - Any trailing arguments are treated as patch numbers
 * - Any combination of the above, except unless at least one -P is specified,
 *   %patch is treated as %patch -P0 so that "%patch 1" is actually
 *   equal to "%patch -P0 -P1".
 *
 * @param spec		build info
 * @param line		current line from spec file
 * @return		RPMRC_OK on success
 */
static rpmRC doPatchMacro(rpmSpec spec, const char *line)
{
    char *opt_b;
    char *buf = NULL;
    int opt_P, opt_p, opt_R, opt_E, opt_F;
    int argc, c;
    const char **argv = NULL;
    ARGV_t patch, patchnums = NULL;
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    
    struct poptOption const patchOpts[] = {
	{ NULL, 'P', POPT_ARG_INT, &opt_P, 'P', NULL, NULL },
	{ NULL, 'p', POPT_ARG_INT, &opt_p, 'p', NULL, NULL },
	{ NULL, 'R', POPT_ARG_NONE, &opt_R, 'R', NULL, NULL },
	{ NULL, 'E', POPT_ARG_NONE, &opt_E, 'E', NULL, NULL },
	{ NULL, 'b', POPT_ARG_STRING, &opt_b, 'b', NULL, NULL },
	{ NULL, 'z', POPT_ARG_STRING, &opt_b, 'z', NULL, NULL },
	{ NULL, 'F', POPT_ARG_INT, &opt_F, 'F', NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL }
    };
    poptContext optCon;

    opt_p = opt_R = opt_E = 0;
    opt_P = -1;		/* no explicit -P <N> was found */
    opt_F = -1;		/* fuzz<0 indicates no explicit -F x was set */
    opt_b = NULL;

    /* Convert %patchN to %patch -PN to simplify further processing */
    if (! strchr(" \t\n", line[6])) {
	rasprintf(&buf, "%%patch -P %s", line + 6);
    } else {
	buf = xstrdup(line);
    }
    poptParseArgvString(buf, &argc, &argv);
    free(buf);

    /* 
     * Grab all -P<N> numbers for later processing. Stored as strings
     * at this point so we only have to worry about conversion in one place.
     */
    optCon = poptGetContext(NULL, argc, argv, patchOpts, 0);
    while ((c = poptGetNextOpt(optCon)) > 0) {
	switch (c) {
	case 'P': {
	    char *arg = poptGetOptArg(optCon);
	    if (arg) {
	    	argvAdd(&patchnums, arg);
	    	free(arg);
	    }
	    break;
	}
	default:
	    break;
	}
    }

    if (c < -1) {
	rpmlog(RPMLOG_ERR, _("%s: %s: %s\n"), poptStrerror(c), 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), line);
	goto exit;
    }

    /* %patch without -P<N> is treated as %patch0, urgh */
    if (opt_P < 0) {
	argvAdd(&patchnums, "0");
    }
    /* Any trailing arguments are treated as patch numbers */
    argvAppend(&patchnums, (ARGV_const_t) poptGetArgs(optCon));

    /* Convert to number, generate patch command and append to %prep script */
    for (patch = patchnums; *patch; patch++) {
	int pnum;
	char *s;
	if (parseNum(*patch, &pnum)) {
	    rpmlog(RPMLOG_ERR, _("Invalid patch number %s: %s\n"),
		     *patch, line);
	    goto exit;
	}
	s = doPatch(spec, pnum, opt_p, opt_b, opt_R, opt_E, opt_F);
	if (s == NULL) {
	    goto exit;
	}
	appendLineStringBuf(spec->prep, s);
	free(s);
    }
	
    rc = RPMRC_OK;

exit:
    argvFree(patchnums);
    free(argv);
    poptFreeContext(optCon);
    return rc;
}

int parsePrep(rpmSpec spec)
{
    int nextPart, res, rc;
    StringBuf sb;
    char **lines;
    ARGV_t saveLines = NULL;

    if (spec->prep != NULL) {
	rpmlog(RPMLOG_ERR, _("line %d: second %%prep\n"), spec->lineNum);
	return PART_ERROR;
    }

    spec->prep = newStringBuf();

    /* There are no options to %prep */
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	return PART_NONE;
    } else if (rc < 0) {
	return PART_ERROR;
    }
    
    sb = newStringBuf();
    
    while (! (nextPart = isPart(spec->line))) {
	/* Need to expand the macros inline.  That way we  */
	/* can give good line number information on error. */
	appendStringBuf(sb, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	    nextPart = PART_NONE;
	    break;
	} else if (rc < 0) {
	    goto exit;
	}
    }

    argvSplit(&saveLines, getStringBuf(sb), "\n");
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
	    /* fixup from RPMRC_FAIL do*Macro() codes for now */
	    res = PART_ERROR; 
	    goto exit;
	}
    }
    res = nextPart;

exit:
    argvFree(saveLines);
    sb = freeStringBuf(sb);

    return nextPart;
}
