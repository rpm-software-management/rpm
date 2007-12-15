/** \ingroup rpmbuild
 * \file build/build.c
 *  Top-level build dispatcher.
 */

#include "system.h"

#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>

#include "debug.h"

static int _build_debug = 0;

/**
 */
static void doRmSource(rpmSpec spec)
{
    struct Source *p;
    Package pkg;
    int rc;
    
#if 0
    rc = unlink(spec->specFile);
#endif

    for (p = spec->sources; p != NULL; p = p->next) {
	if (! (p->flags & RPMBUILD_ISNO)) {
	    char *fn = rpmGetPath("%{_sourcedir}/", p->source, NULL);
	    rc = unlink(fn);
	    fn = _free(fn);
	}
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	for (p = pkg->icon; p != NULL; p = p->next) {
	    if (! (p->flags & RPMBUILD_ISNO)) {
		char *fn = rpmGetPath("%{_sourcedir}/", p->source, NULL);
		rc = unlink(fn);
		fn = _free(fn);
	    }
	}
    }
}

/*
 * @todo Single use by %%doc in files.c prevents static.
 */
rpmRC doScript(rpmSpec spec, rpmBuildFlags what, const char *name, StringBuf sb, int test)
{
    const char * rootURL = spec->rootURL;
    const char * rootDir;
    const char *scriptName = NULL;
    const char * buildDirURL = rpmGenPath(rootURL, "%{_builddir}", "");
    const char * buildScript;
    const char * buildCmd = NULL;
    const char * buildTemplate = NULL;
    const char * buildPost = NULL;
    const char * mTemplate = NULL;
    const char * mCmd = NULL;
    const char * mPost = NULL;
    int argc = 0;
    const char **argv = NULL;
    FILE * fp = NULL;
    urlinfo u = NULL;

    FD_t fd;
    FD_t xfd;
    pid_t pid;
    pid_t child;
    int status;
    rpmRC rc;
    
    switch (what) {
    case RPMBUILD_PREP:
	name = "%prep";
	sb = spec->prep;
	mTemplate = "%{__spec_prep_template}";
	mPost = "%{__spec_prep_post}";
	mCmd = "%{__spec_prep_cmd}";
	break;
    case RPMBUILD_BUILD:
	name = "%build";
	sb = spec->build;
	mTemplate = "%{__spec_build_template}";
	mPost = "%{__spec_build_post}";
	mCmd = "%{__spec_build_cmd}";
	break;
    case RPMBUILD_INSTALL:
	name = "%install";
	sb = spec->install;
	mTemplate = "%{__spec_install_template}";
	mPost = "%{__spec_install_post}";
	mCmd = "%{__spec_install_cmd}";
	break;
    case RPMBUILD_CHECK:
	name = "%check";
	sb = spec->check;
	mTemplate = "%{__spec_check_template}";
	mPost = "%{__spec_check_post}";
	mCmd = "%{__spec_check_cmd}";
	break;
    case RPMBUILD_CLEAN:
	name = "%clean";
	sb = spec->clean;
	mTemplate = "%{__spec_clean_template}";
	mPost = "%{__spec_clean_post}";
	mCmd = "%{__spec_clean_cmd}";
	break;
    case RPMBUILD_RMBUILD:
	name = "--clean";
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
    if (name == NULL)	/* XXX shouldn't happen */
	name = "???";

    if ((what != RPMBUILD_RMBUILD) && sb == NULL) {
	rc = RPMRC_OK;
	goto exit;
    }
    
    if (rpmMkTempFile(rootURL, &scriptName, &fd) || fd == NULL || Ferror(fd)) {
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
    
    (void) urlPath(rootURL, &rootDir);
    if (*rootDir == '\0') rootDir = "/";

    (void) urlPath(scriptName, &buildScript);

    buildTemplate = rpmExpand(mTemplate, NULL);
    buildPost = rpmExpand(mPost, NULL);

    (void) fputs(buildTemplate, fp);

    if (what != RPMBUILD_PREP && what != RPMBUILD_RMBUILD && spec->buildSubdir)
	fprintf(fp, "cd '%s'\n", spec->buildSubdir);

    if (what == RPMBUILD_RMBUILD) {
	if (spec->buildSubdir)
	    fprintf(fp, "rm -rf '%s'\n", spec->buildSubdir);
    } else if (sb != NULL)
	fprintf(fp, "%s", getStringBuf(sb));

    (void) fputs(buildPost, fp);
    
    (void) Fclose(xfd);

    if (test) {
	rc = RPMRC_OK;
	goto exit;
    }
    
if (_build_debug)
fprintf(stderr, "*** rootURL %s buildDirURL %s\n", rootURL, buildDirURL);
    if (buildDirURL && buildDirURL[0] != '/' &&
	(urlSplit(buildDirURL, &u) != 0)) {
	rc = RPMRC_FAIL;
	goto exit;
    }
    if (u != NULL) {
	switch (u->urltype) {
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
if (_build_debug)
fprintf(stderr, "*** addMacros\n");
	    addMacro(spec->macros, "_remsh", NULL, "%{__remsh}", RMIL_SPEC);
	    addMacro(spec->macros, "_remhost", NULL, u->host, RMIL_SPEC);
	    if (strcmp(rootDir, "/"))
		addMacro(spec->macros, "_remroot", NULL, rootDir, RMIL_SPEC);
	    break;
	case URL_IS_UNKNOWN:
	case URL_IS_DASH:
	case URL_IS_PATH:
	case URL_IS_HKP:
	default:
	    break;
	}
    }

    buildCmd = rpmExpand(mCmd, " ", buildScript, NULL);
    (void) poptParseArgvString(buildCmd, &argc, &argv);

    rpmlog(RPMLOG_NOTICE, _("Executing(%s): %s\n"), name, buildCmd);
    if (!(child = fork())) {

	errno = 0;
	(void) execvp(argv[0], (char *const *)argv);

	rpmlog(RPMLOG_ERR, _("Exec of %s failed (%s): %s\n"),
		scriptName, name, strerror(errno));

	_exit(-1);
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
    if (u != NULL) {
	switch (u->urltype) {
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
if (_build_debug)
fprintf(stderr, "*** delMacros\n");
	    delMacro(spec->macros, "_remsh");
	    delMacro(spec->macros, "_remhost");
	    if (strcmp(rootDir, "/"))
		delMacro(spec->macros, "_remroot");
	    break;
	case URL_IS_UNKNOWN:
	case URL_IS_DASH:
	case URL_IS_PATH:
	case URL_IS_HKP:
	default:
	    break;
	}
	u = urlFree(u);
    }
    argv = _free(argv);
    buildCmd = _free(buildCmd);
    buildTemplate = _free(buildTemplate);
    buildPost = _free(buildPost);
    buildDirURL = _free(buildDirURL);

    return rc;
}

rpmRC buildSpec(rpmts ts, rpmSpec spec, int what, int test)
{
    rpmRC rc = RPMRC_OK;

    if (!spec->recursing && spec->BACount) {
	int x;
	/* When iterating over BANames, do the source    */
	/* packaging on the first run, and skip RMSOURCE altogether */
	if (spec->BASpecs != NULL)
	for (x = 0; x < spec->BACount; x++) {
	    if ((rc = buildSpec(ts, spec->BASpecs[x],
				(what & ~RPMBUILD_RMSOURCE) |
				(x ? 0 : (what & RPMBUILD_PACKAGESOURCE)),
				test))) {
		goto exit;
	    }
	}
    } else {
	if ((what & RPMBUILD_PREP) &&
	    (rc = doScript(spec, RPMBUILD_PREP, NULL, NULL, test)))
		goto exit;

	if ((what & RPMBUILD_BUILD) &&
	    (rc = doScript(spec, RPMBUILD_BUILD, NULL, NULL, test)))
		goto exit;

	if ((what & RPMBUILD_INSTALL) &&
	    (rc = doScript(spec, RPMBUILD_INSTALL, NULL, NULL, test)))
		goto exit;

	if ((what & RPMBUILD_CHECK) &&
	    (rc = doScript(spec, RPMBUILD_CHECK, NULL, NULL, test)))
		goto exit;

	if ((what & RPMBUILD_PACKAGESOURCE) &&
	    (rc = processSourceFiles(spec)))
		goto exit;

	if (((what & RPMBUILD_INSTALL) || (what & RPMBUILD_PACKAGEBINARY) ||
	    (what & RPMBUILD_FILECHECK)) &&
	    (rc = processBinaryFiles(spec, what & RPMBUILD_INSTALL, test)))
		goto exit;

	if (((what & RPMBUILD_PACKAGESOURCE) && !test) &&
	    (rc = packageSources(spec)))
		return rc;

	if (((what & RPMBUILD_PACKAGEBINARY) && !test) &&
	    (rc = packageBinaries(spec)))
		goto exit;
	
	if ((what & RPMBUILD_CLEAN) &&
	    (rc = doScript(spec, RPMBUILD_CLEAN, NULL, NULL, test)))
		goto exit;

	if ((what & RPMBUILD_RMBUILD) &&
	    (rc = doScript(spec, RPMBUILD_RMBUILD, NULL, NULL, test)))
		goto exit;
    }

    if (what & RPMBUILD_RMSOURCE)
	doRmSource(spec);

    if (what & RPMBUILD_RMSPEC)
	(void) unlink(spec->specFile);

exit:
    if (rc != RPMRC_OK && rpmlogGetNrecs() > 0) {
	rpmlog(RPMLOG_NOTICE, _("\n\nRPM build errors:\n"));
	rpmlogPrint(NULL);
    }

    return rc;
}
