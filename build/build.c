/** \ingroup rpmbuild
 * \file build/build.c
 *  Top-level build dispatcher.
 */

#include "system.h"

#include <errno.h>
#include <sys/wait.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
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
	    fn = _free(fn);
	    if (rc) goto exit;
	}
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	for (p = pkg->icon; p != NULL; p = p->next) {
	    if (! (p->flags & RPMBUILD_ISNO)) {
		char *fn = rpmGetPath("%{_sourcedir}/", p->source, NULL);
		rc = unlink(fn);
		fn = _free(fn);
	        if (rc) goto exit;
	    }
	}
    }
exit:
    return !rc ? RPMRC_OK : RPMRC_FAIL;
}

/*
 * @todo Single use by %%doc in files.c prevents static.
 */
rpmRC doScript(rpmSpec spec, rpmBuildFlags what, const char *name,
		const char *sb, int test)
{
    const char * rootDir = spec->rootDir;
    char *scriptName = NULL;
    char * buildDir = rpmGenPath(rootDir, "%{_builddir}", "");
    char * buildCmd = NULL;
    char * buildTemplate = NULL;
    char * buildPost = NULL;
    const char * mTemplate = NULL;
    const char * mCmd = NULL;
    const char * mPost = NULL;
    int argc = 0;
    const char **argv = NULL;
    FILE * fp = NULL;

    FD_t fd;
    FD_t xfd;
    pid_t pid;
    pid_t child;
    int status;
    rpmRC rc;
    
    switch (what) {
    case RPMBUILD_PREP:
	mTemplate = "%{__spec_prep_template}";
	mPost = "%{__spec_prep_post}";
	mCmd = "%{__spec_prep_cmd}";
	break;
    case RPMBUILD_BUILD:
	mTemplate = "%{__spec_build_template}";
	mPost = "%{__spec_build_post}";
	mCmd = "%{__spec_build_cmd}";
	break;
    case RPMBUILD_INSTALL:
	mTemplate = "%{__spec_install_template}";
	mPost = "%{__spec_install_post}";
	mCmd = "%{__spec_install_cmd}";
	break;
    case RPMBUILD_CHECK:
	mTemplate = "%{__spec_check_template}";
	mPost = "%{__spec_check_post}";
	mCmd = "%{__spec_check_cmd}";
	break;
    case RPMBUILD_CLEAN:
	mTemplate = "%{__spec_clean_template}";
	mPost = "%{__spec_clean_post}";
	mCmd = "%{__spec_clean_cmd}";
	break;
    case RPMBUILD_RMBUILD:
	mTemplate = "%{__spec_clean_template}";
	mPost = "%{__spec_clean_post}";
	mCmd = "%{__spec_clean_cmd}";
	break;
    case RPMBUILD_STRINGBUF:
    default:
	mTemplate = "%{___build_template}";
	mPost = "%{___build_post}";
	mCmd = "%{___build_cmd}";
	break;
    }

    if ((what != RPMBUILD_RMBUILD) && sb == NULL) {
	rc = RPMRC_OK;
	goto exit;
    }
    
    fd = rpmMkTempFile(rootDir, &scriptName);
    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("Unable to open temp file.\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }

    if (fdGetFILE(fd) == NULL)
	xfd = Fdopen(fd, "w.fpio");
    else
	xfd = fd;

    if ((fp = fdGetFILE(xfd)) == NULL) {
	rc = RPMRC_FAIL;
	goto exit;
    }
    
    if (*rootDir == '\0') rootDir = "/";

    buildTemplate = rpmExpand(mTemplate, NULL);
    buildPost = rpmExpand(mPost, NULL);

    (void) fputs(buildTemplate, fp);

    if (what != RPMBUILD_PREP && what != RPMBUILD_RMBUILD && spec->buildSubdir)
	fprintf(fp, "cd '%s'\n", spec->buildSubdir);

    if (what == RPMBUILD_RMBUILD) {
	if (spec->buildSubdir)
	    fprintf(fp, "rm -rf '%s'\n", spec->buildSubdir);
    } else if (sb != NULL)
	fprintf(fp, "%s", sb);

    (void) fputs(buildPost, fp);
    
    (void) Fclose(xfd);

    if (test) {
	rc = RPMRC_OK;
	goto exit;
    }
    
    if (buildDir && buildDir[0] != '/') {
	rc = RPMRC_FAIL;
	goto exit;
    }

    buildCmd = rpmExpand(mCmd, " ", scriptName, NULL);
    (void) poptParseArgvString(buildCmd, &argc, &argv);

    rpmlog(RPMLOG_NOTICE, _("Executing(%s): %s\n"), name, buildCmd);
    if (!(child = fork())) {
	/* NSPR messes with SIGPIPE, reset to default for the kids */
	signal(SIGPIPE, SIG_DFL);
	errno = 0;
	(void) execvp(argv[0], (char *const *)argv);

	rpmlog(RPMLOG_ERR, _("Exec of %s failed (%s): %s\n"),
		scriptName, name, strerror(errno));

	_exit(127); /* exit 127 for compatibility with bash(1) */
    }

    pid = waitpid(child, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmlog(RPMLOG_ERR, _("Bad exit status from %s (%s)\n"),
		 scriptName, name);
	rc = RPMRC_FAIL;
    } else
	rc = RPMRC_OK;
    
exit:
    if (scriptName) {
	if (rc == RPMRC_OK)
	    (void) unlink(scriptName);
	scriptName = _free(scriptName);
    }
    argv = _free(argv);
    buildCmd = _free(buildCmd);
    buildTemplate = _free(buildTemplate);
    buildPost = _free(buildPost);
    buildDir = _free(buildDir);

    return rc;
}

static rpmRC buildSpec(BTA_t buildArgs, rpmSpec spec, int what)
{
    rpmRC rc = RPMRC_OK;
    int test = (what & RPMBUILD_NOBUILD);
    char *cookie = buildArgs->cookie ? xstrdup(buildArgs->cookie) : NULL;

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
		return rc;

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
