/** \ingroup rpmbuild
 * \file build/parsePrep.c
 *  Parse %prep section from spec file.
 */

#include "system.h"

#include <errno.h>

#include <rpm/header.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"
#include "lib/rpmug.h"
#include "debug.h"

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

    return RPMRC_OK;
}

/**
 * Expand %patchN macro into %prep scriptlet.
 * @param spec		build info
 * @param c		patch index
 * @param strip		patch level (i.e. patch -p argument)
 * @param db		saved file suffix (i.e. patch --suffix argument)
 * @param reverse	include -R?
 * @param removeEmpties	include -E?
 * @param fuzz		fuzz factor, fuzz<0 means no fuzz set
 * @param dir		dir to change to (i.e. patch -d argument)
 * @return		expanded %patch macro (NULL on error)
 */

static char *doPatch(rpmSpec spec, uint32_t c, int strip, const char *db,
		     int reverse, int removeEmpties, int fuzz, const char *dir)
{
    char *fn = NULL;
    char *buf = NULL;
    char *arg_backup = NULL;
    char *arg_fuzz = NULL;
    char *arg_dir = NULL;
    char *args = NULL;
    char *arg_patch_flags = rpmExpand("%{?_default_patch_flags}", NULL);
    struct Source *sp;
    char *patchcmd;
    rpmCompressedMagic compressed = COMPRESSED_NOT;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISPATCH) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	if (c != INT_MAX) {
	    rpmlog(RPMLOG_ERR, _("No patch number %u\n"), c);
	} else {
	    rpmlog(RPMLOG_ERR, _("%%patch without corresponding \"Patch:\" tag\n"));
	}
	goto exit;
    }

    fn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

    /* On non-build parse's, file cannot be stat'd or read. */
    if ((spec->flags & RPMSPEC_FORCE) || rpmFileIsCompressed(fn, &compressed) || checkOwners(fn)) goto exit;

    if (db) {
	rasprintf(&arg_backup,
#if HAVE_OLDPATCH_21 == 0
		  "-b "
#endif
		  "--suffix %s", db);
    } else arg_backup = xstrdup("");

    if (dir) {
	rasprintf(&arg_dir, " -d %s", dir);
    } else arg_dir = xstrdup("");

    if (fuzz >= 0) {
	rasprintf(&arg_fuzz, " --fuzz=%d", fuzz);
    } else arg_fuzz = xstrdup("");

    rasprintf(&args, "%s -p%d %s%s%s%s%s", arg_patch_flags, strip, arg_backup, arg_fuzz, arg_dir,
		reverse ? " -R" : "", 
		removeEmpties ? " -E" : "");

    /* Avoid the extra cost of fork and pipe for uncompressed patches */
    if (compressed != COMPRESSED_NOT) {
	patchcmd = rpmExpand("{ %{uncompress: ", fn, "} || echo patch_fail ; } | "
                             "%{__patch} ", args, NULL);
    } else {
	patchcmd = rpmExpand("%{__patch} ", args, " < ", fn, NULL);
    }

    free(arg_fuzz);
    free(arg_dir);
    free(arg_backup);
    free(args);

    if (c != INT_MAX) {
	rasprintf(&buf, "echo \"Patch #%u (%s):\"\n"
			"%s\n", 
			c, basename(fn), patchcmd);
    } else {
	rasprintf(&buf, "echo \"Patch (%s):\"\n"
			"%s\n", 
			basename(fn), patchcmd);
    }
    free(patchcmd);

exit:
    free(arg_patch_flags);
    free(fn);
    return buf;
}

/**
 * Expand %setup macro into %prep scriptlet.
 * @param spec		build info
 * @param c		source index
 * @param quietly	should -vv be omitted from tar?
 * @return		expanded %setup macro (NULL on error)
 */
static char *doUntar(rpmSpec spec, uint32_t c, int quietly)
{
    char *fn = NULL;
    char *buf = NULL;
    char *tar = NULL;
    const char *taropts = ((rpmIsVerbose() && !quietly) ? "-xvvof" : "-xof");
    struct Source *sp;
    rpmCompressedMagic compressed = COMPRESSED_NOT;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISSOURCE) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	if (c) {
	    rpmlog(RPMLOG_ERR, _("No source number %u\n"), c);
	} else {
	    rpmlog(RPMLOG_ERR, _("No \"Source:\" tag in the spec file\n"));
	}
	goto exit;
    }

    fn = rpmGetPath("%{_sourcedir}/", sp->source, NULL);

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (!(spec->flags & RPMSPEC_FORCE) && (rpmFileIsCompressed(fn, &compressed) || checkOwners(fn))) {
	goto exit;
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
	case COMPRESSED_XZ:
	    t = "%{__xz} -dc";
	    break;
	case COMPRESSED_LZIP:
	    t = "%{__lzip} -dc";
	    break;
	case COMPRESSED_LRZIP:
	    t = "%{__lrzip} -dqo-";
	    break;
	case COMPRESSED_7ZIP:
	    t = "%{__7zip} x";
	    needtar = 0;
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
	free(zipper);
    } else {
	rasprintf(&buf, "%s %s %s", tar, taropts, fn);
    }

exit:
    free(fn);
    free(tar);
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
    StringBuf before = newStringBuf();
    StringBuf after = newStringBuf();
    poptContext optCon = NULL;
    int argc;
    const char ** argv = NULL;
    int arg;
    const char * optArg;
    int xx;
    rpmRC rc = RPMRC_FAIL;
    uint32_t num;
    int leaveDirs = 0, skipDefaultAction = 0;
    int createDir = 0, quietly = 0;
    int buildInPlace = 0;
    const char * dirName = NULL;
    struct poptOption optionsTable[] = {
	    { NULL, 'a', POPT_ARG_STRING, NULL, 'a',	NULL, NULL},
	    { NULL, 'b', POPT_ARG_STRING, NULL, 'b',	NULL, NULL},
	    { NULL, 'c', 0, &createDir, 0,		NULL, NULL},
	    { NULL, 'D', 0, &leaveDirs, 0,		NULL, NULL},
	    { NULL, 'n', POPT_ARG_STRING, &dirName, 0,	NULL, NULL},
	    { NULL, 'T', 0, &skipDefaultAction, 0,	NULL, NULL},
	    { NULL, 'q', 0, &quietly, 0,		NULL, NULL},
	    { 0, 0, 0, 0, 0,	NULL, NULL}
    };

    if ((xx = poptParseArgvString(line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("Error parsing %%setup: %s\n"), poptStrerror(xx));
	goto exit;
    }

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	/* We only parse -a and -b here */

	if (parseUnsignedNum(optArg, &num)) {
	    rpmlog(RPMLOG_ERR, _("line %d: Bad arg to %%setup: %s\n"),
		     spec->lineNum, (optArg ? optArg : "???"));
	    goto exit;
	}

	{   char *chptr = doUntar(spec, num, quietly);
	    if (chptr == NULL)
		goto exit;

	    appendLineStringBuf((arg == 'a' ? after : before), chptr);
	    free(chptr);
	}
    }

    if (arg < -1) {
	rpmlog(RPMLOG_ERR, _("line %d: Bad %%setup option %s: %s\n"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 poptStrerror(arg));
	goto exit;
    }

    if (dirName) {
	spec->buildSubdir = xstrdup(dirName);
    } else {
	rasprintf(&spec->buildSubdir, "%s-%s", 
		  headerGetString(spec->packages->header, RPMTAG_NAME),
		  headerGetString(spec->packages->header, RPMTAG_VERSION));
    }
    /* Mer addition - support --build-in-place */
    if (rpmExpandNumeric("%{_build_in_place}")) {
	buildInPlace = 1;
	spec->buildSubdir = NULL;
    }
    addMacro(spec->macros, "buildsubdir", NULL, spec->buildSubdir, RMIL_SPEC);
    if (buildInPlace) {
	rc = RPMRC_OK;
	goto exit;
    }
    
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
	buf = rpmExpand("%{__mkdir_p} ", spec->buildSubdir, "\n",
			"cd '", spec->buildSubdir, "'", NULL);
	appendLineStringBuf(spec->prep, buf);
	free(buf);
    }

    /* do the default action */
   if (!createDir && !skipDefaultAction) {
	char *chptr = doUntar(spec, 0, quietly);
	if (!chptr)
	    goto exit;
	appendLineStringBuf(spec->prep, chptr);
	free(chptr);
    }

    appendStringBuf(spec->prep, getStringBuf(before));

    if (!createDir) {
	rasprintf(&buf, "cd '%s'", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
	free(buf);
    }

    if (createDir && !skipDefaultAction) {
	char *chptr = doUntar(spec, 0, quietly);
	if (chptr == NULL)
	    goto exit;
	appendLineStringBuf(spec->prep, chptr);
	free(chptr);
    }
    
    appendStringBuf(spec->prep, getStringBuf(after));

    /* Fix the permissions of the setup build tree */
    {	char *fix = rpmExpand("%{_fixperms} .", NULL);
	if (fix && *fix != '%') {
	    appendLineStringBuf(spec->prep, fix);
	}
	free(fix);
    }
    rc = RPMRC_OK;

exit:
    freeStringBuf(before);
    freeStringBuf(after);
    poptFreeContext(optCon);
    free(argv);

    return rc;
}

/**
 * Parse %patch line.
 * This supports too many crazy syntaxes:
 * - %patchN is equal to %patch -P\<N\>
 * - -P\<N\> -P\<N+1\>... can be used to apply several patch on a single line
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
    char *opt_b, *opt_P, *opt_d;
    char *buf = NULL;
    int opt_p, opt_R, opt_E, opt_F;
    int argc, c;
    const char **argv = NULL;
    ARGV_t patch, patchnums = NULL;
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    
    struct poptOption const patchOpts[] = {
	{ NULL, 'P', POPT_ARG_STRING, &opt_P, 'P', NULL, NULL },
	{ NULL, 'p', POPT_ARG_INT, &opt_p, 'p', NULL, NULL },
	{ NULL, 'R', POPT_ARG_NONE, &opt_R, 'R', NULL, NULL },
	{ NULL, 'E', POPT_ARG_NONE, &opt_E, 'E', NULL, NULL },
	{ NULL, 'b', POPT_ARG_STRING, &opt_b, 'b', NULL, NULL },
	{ NULL, 'z', POPT_ARG_STRING, &opt_b, 'z', NULL, NULL },
	{ NULL, 'F', POPT_ARG_INT, &opt_F, 'F', NULL, NULL },
	{ NULL, 'd', POPT_ARG_STRING, &opt_d, 'd', NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL }
    };
    poptContext optCon = NULL;

    opt_p = opt_R = opt_E = 0;
    opt_F = rpmExpandNumeric("%{_default_patch_fuzz}");		/* get default fuzz factor for %patch */
    opt_b = opt_d = NULL;

    /* Convert %patchN to %patch -PN to simplify further processing */
    if (! strchr(" \t\n", line[6])) {
	rasprintf(&buf, "%%patch -P %s", line + 6);
    } else {
	if (strstr(line+6, " -P") == NULL)
	    rasprintf(&buf, "%%patch -P %d %s", INT_MAX, line + 6); /* INT_MAX denotes not numbered %patch */
	else
	    buf = xstrdup(line); /* it is not numberless patch because -P is present */
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

    /* Any trailing arguments are treated as patch numbers */
    argvAppend(&patchnums, (ARGV_const_t) poptGetArgs(optCon));

    /* Convert to number, generate patch command and append to %prep script */
    for (patch = patchnums; *patch; patch++) {
	uint32_t pnum;
	char *s;
	if (parseUnsignedNum(*patch, &pnum)) {
	    rpmlog(RPMLOG_ERR, _("Invalid patch number %s: %s\n"),
		     *patch, line);
	    goto exit;
	}
	s = doPatch(spec, pnum, opt_p, opt_b, opt_R, opt_E, opt_F, opt_d);
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
    int nextPart, rc, res = PART_ERROR;
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
    
    while (! (nextPart = isPart(spec->line))) {
	/* Need to expand the macros inline.  That way we  */
	/* can give good line number information on error. */
	argvAdd(&saveLines, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	    nextPart = PART_NONE;
	    break;
	} else if (rc < 0) {
	    goto exit;
	}
    }

    for (ARGV_const_t lines = saveLines; lines && *lines; lines++) {
	rc = RPMRC_OK;
	if (rstreqn(*lines, "%setup", sizeof("%setup")-1)) {
	    rc = doSetupMacro(spec, *lines);
	} else if (rstreqn(*lines, "%patch", sizeof("%patch")-1)) {
	    rc = doPatchMacro(spec, *lines);
	} else {
	    appendStringBuf(spec->prep, *lines);
	}
	if (rc != RPMRC_OK && !(spec->flags & RPMSPEC_FORCE)) {
	    goto exit;
	}
    }
    res = nextPart;

exit:
    argvFree(saveLines);

    return res;
}
