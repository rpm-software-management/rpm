/** \ingroup rpmbuild
 * \file build/build.c
 *  Top-level build dispatcher.
 */

#include "system.h"

#include <errno.h>
#include <sys/wait.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include "rpmio/rpmstring.h"
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"
#include "lib/rpmug.h"

#include "debug.h"

/**
 */
static rpmRC doRmSource(rpmSpec spec)
{
    struct Source *p;
    Package pkg;
    int rc = 0;
    
    for (p = spec->sources; p != NULL; p = p->next) {
	if (! (p->flags & RPMBUILD_ISNO)) {
	    char *fn = rpmGetPath("%{_sourcedir}/", p->source, NULL);
	    rc = unlink(fn);
	    free(fn);
	    if (rc) goto exit;
	}
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	for (p = pkg->icon; p != NULL; p = p->next) {
	    if (! (p->flags & RPMBUILD_ISNO)) {
		char *fn = rpmGetPath("%{_sourcedir}/", p->source, NULL);
		rc = unlink(fn);
		free(fn);
	        if (rc) goto exit;
	    }
	}
    }
exit:
    return !rc ? RPMRC_OK : RPMRC_FAIL;
}

/*
 */
static char *
rpmEscapePercent(const char *arg)
{
    size_t blen;
    char *ret = NULL, *b;

    if (arg == NULL) {
        ret = xstrdup("");
        goto exit;
    }

    blen = strlen(arg);

    /* calculate number of '%' characters in 'arg' */
    for (b = (char *)arg; *b; b++)
        blen += ('%' == *b);

    ret = xmalloc(blen + 1);
    ret[blen] = '\0';

    /* Loop over 'arg' by character and copy */
    for (b = ret; *arg; *b++ = *arg++)
        if ('%' == *arg)
            *b++ = *arg; /* duplicate '%' */

exit:
    return ret;
}

/*
 * @todo Single use by %%doc in files.c prevents static.
 */
rpmRC doScript(rpmSpec spec, rpmBuildFlags what, const char *name,
		const char *sb, int test)
{
    char *scriptName = NULL;
    const char * sbEscaped = rpmEscapePercent(sb);
    char * sbLocal = NULL;
    char * buildDir = rpmGenPath(spec->rootDir, "%{_builddir}", "");
    char * buildCmd = NULL;
    char * buildTemplate = NULL;
    const char * mTemplate = NULL;
    const char * mCmd = NULL;
    int argc = 0;
    const char **argv = NULL;
    FILE * fp = NULL;

    FD_t fd = NULL;
    pid_t pid;
    pid_t child;
    int status;
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    
    switch (what) {
    case RPMBUILD_PREP:
	mTemplate = "%{__spec_prep_template}";
	mCmd = "%{__spec_prep_cmd}";
	break;
    case RPMBUILD_BUILD:
	mTemplate = "%{__spec_build_template}";
	mCmd = "%{__spec_build_cmd}";
	break;
    case RPMBUILD_INSTALL:
	mTemplate = "%{__spec_install_template}";
	mCmd = "%{__spec_install_cmd}";
	break;
    case RPMBUILD_CHECK:
	mTemplate = "%{__spec_check_template}";
	mCmd = "%{__spec_check_cmd}";
	break;
    case RPMBUILD_CLEAN:
	mTemplate = "%{__spec_clean_template}";
	mCmd = "%{__spec_clean_cmd}";
	break;
    case RPMBUILD_RMBUILD:
	mTemplate = "%{__spec_clean_template}";
	mCmd = "%{__spec_clean_cmd}";
	break;
    case RPMBUILD_STRINGBUF:
    default:
	mTemplate = "%{___build_template}";
	mCmd = "%{___build_cmd}";
	break;
    }

    // Prepare a local string for the '%___build_body' macro, prepended with
    // the appropriate "cd spec->buildSubdir" line if necessary.
    if (what == RPMBUILD_RMBUILD) {
	if (spec->buildSubdir)
	    rasprintf(&sbLocal, "rm -rf '%s'\n", spec->buildSubdir);
    } else if (sb != NULL) {
	if (what != RPMBUILD_PREP && spec->buildSubdir)
	    rasprintf(&sbLocal, "cd '%s'\n%s", spec->buildSubdir, sbEscaped);
	else
	    rasprintf(&sbLocal, "%s", sbEscaped);
    } else {
	rc = RPMRC_OK;
	goto exit;
    }

    // Transfer the contents of 'sbLocal' to the '%___build_body' macro ...
    rpmPushMacro(spec->macros, "___build_body", NULL, sbLocal, RMIL_SPEC);
    // ... which will then dynamically be expanded and used as integral part
    // of the '%___build_template' macro (or one of the many variants) that
    // is held by 'mTemplate' here.
    buildTemplate = rpmExpand(mTemplate, NULL);
    // Flush temporary macro content.
    rpmPopMacro(spec->macros, "___build_body");

    fd = rpmMkTempFile(spec->rootDir, &scriptName);
    if (Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("Unable to open temp file: %s\n"), Fstrerror(fd));
	goto exit;
    }

    if ((fp = fdopen(Fileno(fd), "w")) == NULL) {
	rpmlog(RPMLOG_ERR, _("Unable to open stream: %s\n"), strerror(errno));
	goto exit;
    }

    (void) fputs(buildTemplate, fp);
    (void) fclose(fp);

    if (test) {
	rc = RPMRC_OK;
	goto exit;
    }
    
    if (buildDir && buildDir[0] != '/') {
	goto exit;
    }

    buildCmd = rpmExpand(mCmd, " ", scriptName, NULL);
    (void) poptParseArgvString(buildCmd, &argc, &argv);

    rpmlog(RPMLOG_NOTICE, _("Executing(%s): %s\n"), name, buildCmd);
    if (!(child = fork())) {
	errno = 0;
	(void) execvp(argv[0], (char *const *)argv);

	rpmlog(RPMLOG_ERR, _("Exec of %s failed (%s): %s\n"),
		scriptName, name, strerror(errno));

	_exit(127); /* exit 127 for compatibility with bash(1) */
    }

    pid = waitpid(child, &status, 0);

    if (pid == -1) {
	rpmlog(RPMLOG_ERR, _("Error executing scriptlet %s (%s)\n"),
		 scriptName, name);
	goto exit;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmlog(RPMLOG_ERR, _("Bad exit status from %s (%s)\n"),
		 scriptName, name);
    } else
	rc = RPMRC_OK;
    
exit:
    Fclose(fd);
    if (scriptName) {
	if (rc == RPMRC_OK && !rpmIsDebug())
	    (void) unlink(scriptName);
	free(scriptName);
    }
    free(argv);
    free(buildCmd);
    free(buildTemplate);
    free(buildDir);
    free(sbLocal);
    free(sbEscaped);

    return rc;
}

static rpmRC buildSpec(BTA_t buildArgs, rpmSpec spec, int what)
{
    rpmRC rc = RPMRC_OK;
    int test = (what & RPMBUILD_NOBUILD);
    char *cookie = buildArgs->cookie ? xstrdup(buildArgs->cookie) : NULL;

    if (rpmExpandNumeric("%{?source_date_epoch_from_changelog}") &&
	getenv("SOURCE_DATE_EPOCH") == NULL) {
	/* Use date of first (== latest) changelog entry */
	Header h = spec->packages->header;
	struct rpmtd_s td;
	if (headerGet(h, RPMTAG_CHANGELOGTIME, &td, (HEADERGET_MINMEM|HEADERGET_RAW))) {
	    char sdestr[22];
	    long long sdeint = rpmtdGetNumber(&td);
	    if (sdeint % 86400 == 43200) /* date was rounded to 12:00 */
		/* make sure it is in the past, so that clamping times works */
		sdeint -= 43200;
	    snprintf(sdestr, sizeof(sdestr), "%lli", sdeint);
	    rpmlog(RPMLOG_NOTICE, _("setting %s=%s\n"), "SOURCE_DATE_EPOCH", sdestr);
	    setenv("SOURCE_DATE_EPOCH", sdestr, 0);
	    rpmtdFreeData(&td);
	}
    }

    /* XXX TODO: rootDir is only relevant during build, eliminate from spec */
    spec->rootDir = buildArgs->rootdir;
    if (!spec->recursing && spec->BACount) {
	int x;
	/* When iterating over BANames, do the source    */
	/* packaging on the first run, and skip RMSOURCE altogether */
	if (spec->BASpecs != NULL)
	for (x = 0; x < spec->BACount; x++) {
	    if ((rc = buildSpec(buildArgs, spec->BASpecs[x],
				(what & ~RPMBUILD_RMSOURCE) |
				(x ? 0 : (what & RPMBUILD_PACKAGESOURCE))))) {
		goto exit;
	    }
	}
    } else {
	int didBuild = (what & (RPMBUILD_PREP|RPMBUILD_BUILD|RPMBUILD_INSTALL));

	if ((what & RPMBUILD_PREP) &&
	    (rc = doScript(spec, RPMBUILD_PREP, "%prep",
			   getStringBuf(spec->prep), test)))
		goto exit;

	if ((what & RPMBUILD_BUILD) &&
	    (rc = doScript(spec, RPMBUILD_BUILD, "%build",
			   getStringBuf(spec->build), test)))
		goto exit;

	if ((what & RPMBUILD_INSTALL) &&
	    (rc = doScript(spec, RPMBUILD_INSTALL, "%install",
			   getStringBuf(spec->install), test)))
		goto exit;

	if ((what & RPMBUILD_CHECK) &&
	    (rc = doScript(spec, RPMBUILD_CHECK, "%check",
			   getStringBuf(spec->check), test)))
		goto exit;

	if ((what & RPMBUILD_PACKAGESOURCE) &&
	    (rc = processSourceFiles(spec, buildArgs->pkgFlags)))
		goto exit;

	if (((what & RPMBUILD_INSTALL) || (what & RPMBUILD_PACKAGEBINARY) ||
	    (what & RPMBUILD_FILECHECK)) &&
	    (rc = processBinaryFiles(spec, buildArgs->pkgFlags,
				     what & RPMBUILD_INSTALL, test)))
		goto exit;

	if (((what & RPMBUILD_INSTALL) || (what & RPMBUILD_PACKAGEBINARY)) &&
	    (rc = processBinaryPolicies(spec, test)))
		goto exit;

	if (((what & RPMBUILD_PACKAGESOURCE) && !test) &&
	    (rc = packageSources(spec, &cookie)))
		goto exit;

	if (((what & RPMBUILD_PACKAGEBINARY) && !test) &&
	    (rc = packageBinaries(spec, cookie, (didBuild == 0))))
		goto exit;
	
	if ((what & RPMBUILD_CLEAN) &&
	    (rc = doScript(spec, RPMBUILD_CLEAN, "%clean",
			   getStringBuf(spec->clean), test)))
		goto exit;

	if ((what & RPMBUILD_RMBUILD) &&
	    (rc = doScript(spec, RPMBUILD_RMBUILD, "--clean", NULL, test)))
		goto exit;
    }

    if (what & RPMBUILD_RMSOURCE)
	doRmSource(spec);

    if (what & RPMBUILD_RMSPEC)
	(void) unlink(spec->specFile);

exit:
    free(cookie);
    spec->rootDir = NULL;
    if (rc != RPMRC_OK && rpmlogGetNrecs() > 0) {
	rpmlog(RPMLOG_NOTICE, _("\n\nRPM build errors:\n"));
	rpmlogPrint(NULL);
    }
    rpmugFree();

    return rc;
}

rpmRC rpmSpecBuild(rpmSpec spec, BTA_t buildArgs)
{
    /* buildSpec() can recurse with different buildAmount, pass it separately */
    return buildSpec(buildArgs, spec, buildArgs->buildAmount);
}
