/** \file build/build.c
 *  Top-level build dispatcher.
 */

#include "system.h"

#include <rpmio_internal.h>
#include <rpmbuild.h>

static int _build_debug = 0;

static void doRmSource(Spec spec)
{
    struct Source *p;
    Package pkg;
    
#if 0
    unlink(spec->specFile);
#endif

    for (p = spec->sources; p != NULL; p = p->next) {
	if (! (p->flags & RPMBUILD_ISNO)) {
	    const char *fn = rpmGetPath("%{_sourcedir}/", p->source, NULL);
	    unlink(fn);
	    xfree(fn);
	}
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	for (p = pkg->icon; p != NULL; p = p->next) {
	    if (! (p->flags & RPMBUILD_ISNO)) {
		const char *fn = rpmGetPath("%{_sourcedir}/", p->source, NULL);
		unlink(fn);
		xfree(fn);
	    }
	}
    }
}

/*
 * The _preScript string is expanded to export values to a script environment.
 */
/** */
int doScript(Spec spec, int what, const char *name, StringBuf sb, int test)
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
    const char * mPost = NULL;
    int argc = 0;
    const char **argv = NULL;
    FILE * fp = NULL;
    urlinfo u = NULL;

    FD_t fd;
    FD_t xfd;
    int child;
    int status;
    int rc;
    
    switch (what) {
      case RPMBUILD_PREP:
	name = "%prep";
	sb = spec->prep;
	mTemplate = "%{__spec_prep_template}";
	mPost = "%{__spec_prep_post}";
	break;
      case RPMBUILD_BUILD:
	name = "%build";
	sb = spec->build;
	mTemplate = "%{__spec_build_template}";
	mPost = "%{__spec_build_post}";
	break;
      case RPMBUILD_INSTALL:
	name = "%install";
	sb = spec->install;
	mTemplate = "%{__spec_install_template}";
	mPost = "%{__spec_install_post}";
	break;
      case RPMBUILD_CLEAN:
	name = "%clean";
	sb = spec->clean;
	mTemplate = "%{__spec_clean_template}";
	mPost = "%{__spec_clean_post}";
	break;
      case RPMBUILD_RMBUILD:
	name = "--clean";
	mTemplate = "%{__spec_clean_template}";
	mPost = "%{__spec_clean_post}";
	break;
      case RPMBUILD_STRINGBUF:
      default:
	mTemplate = "%{___build_template}";
	mPost = "%{___build_post}";
	break;
    }

    if ((what != RPMBUILD_RMBUILD) && sb == NULL) {
	rc = 0;
	goto exit;
    }
    
    if (makeTempFile(rootURL, &scriptName, &fd) || fd == NULL || Ferror(fd)) {
	rpmError(RPMERR_SCRIPT, _("Unable to open temp file."));
	rc = RPMERR_SCRIPT;
	goto exit;
    }

#ifdef HAVE_FCHMOD
    switch (rootut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	(void)fchmod(Fileno(fd), 0600);
	break;
    default:
	break;
    }
#endif

    if (fdGetFp(fd) == NULL)
	xfd = Fdopen(fd, "w.fpio");
    else
	xfd = fd;
    if ((fp = fdGetFp(xfd)) == NULL) {
	rc = RPMERR_SCRIPT;
	goto exit;
    }
    
    (void) urlPath(rootURL, &rootDir);
    if (*rootDir == '\0') rootDir = "/";

    (void) urlPath(scriptName, &buildScript);

    buildTemplate = rpmExpand(mTemplate, NULL);
    buildPost = rpmExpand(mPost, NULL);

    fputs(buildTemplate, fp);

    if (what != RPMBUILD_PREP && what != RPMBUILD_RMBUILD && spec->buildSubdir)
	fprintf(fp, "cd %s\n", spec->buildSubdir);

    if (what == RPMBUILD_RMBUILD) {
	if (spec->buildSubdir)
	    fprintf(fp, "rm -rf %s\n", spec->buildSubdir);
    } else
	fprintf(fp, "%s", getStringBuf(sb));

    fputs(buildPost, fp);
    
    Fclose(xfd);

    if (test) {
	rc = 0;
	goto exit;
    }
    
if (_build_debug)
fprintf(stderr, "*** rootURL %s buildDirURL %s\n", rootURL, buildDirURL);
    if (buildDirURL && buildDirURL[0] != '/' &&
	(urlSplit(buildDirURL, &u) != 0)) {
	rc = RPMERR_SCRIPT;
	goto exit;
    }
    if (u) {
	switch (u->urltype) {
	case URL_IS_FTP:
if (_build_debug)
fprintf(stderr, "*** addMacros\n");
	    addMacro(spec->macros, "_remsh", NULL, "%{__remsh}", RMIL_SPEC);
	    addMacro(spec->macros, "_remhost", NULL, u->host, RMIL_SPEC);
	    if (strcmp(rootDir, "/"))
		addMacro(spec->macros, "_remroot", NULL, rootDir, RMIL_SPEC);
	    break;
	case URL_IS_HTTP:
	default:
	    break;
	}
    }

    buildCmd = rpmExpand("%{___build_cmd}", " ", buildScript, NULL);
    poptParseArgvString(buildCmd, &argc, &argv);

    rpmMessage(RPMMESS_NORMAL, _("Executing(%s): %s\n"), name, buildCmd);
    if (!(child = fork())) {

	errno = 0;
	execvp(argv[0], (char *const *)argv);

	rpmError(RPMERR_SCRIPT, _("Exec of %s failed (%s): %s"), scriptName, name, strerror(errno));

	_exit(-1);
    }

    rc = waitpid(child, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SCRIPT, _("Bad exit status from %s (%s)"),
		 scriptName, name);
	rc = RPMERR_SCRIPT;
    } else
	rc = 0;
    
exit:
    if (scriptName) {
	if (!rc)
	    Unlink(scriptName);
	xfree(scriptName);
    }
    if (u) {
	switch (u->urltype) {
	case URL_IS_FTP:
	case URL_IS_HTTP:
if (_build_debug)
fprintf(stderr, "*** delMacros\n");
	    delMacro(spec->macros, "_remsh");
	    delMacro(spec->macros, "_remhost");
	    if (strcmp(rootDir, "/"))
		delMacro(spec->macros, "_remroot");
	    break;
	default:
	    break;
	}
    }
    FREE(argv);
    FREE(buildCmd);
    FREE(buildTemplate);
    FREE(buildDirURL);

    return rc;
}

/** */
int buildSpec(Spec spec, int what, int test)
{
    int x, rc;

    if (!spec->inBuildArchitectures && spec->buildArchitectureCount) {
	/* When iterating over buildArchitectures, do the source    */
	/* packaging on the first run, and skip RMSOURCE altogether */
	for (x = 0; x < spec->buildArchitectureCount; x++) {
	    if ((rc = buildSpec(spec->buildArchitectureSpecs[x],
				(what & ~RPMBUILD_RMSOURCE) |
				(x ? 0 : (what & RPMBUILD_PACKAGESOURCE)),
				test))) {
		return rc;
	    }
	}
    } else {
	if ((what & RPMBUILD_PREP) &&
	    (rc = doScript(spec, RPMBUILD_PREP, NULL, NULL, test)))
		return rc;

	if ((what & RPMBUILD_BUILD) &&
	    (rc = doScript(spec, RPMBUILD_BUILD, NULL, NULL, test)))
		return rc;

	if ((what & RPMBUILD_INSTALL) &&
	    (rc = doScript(spec, RPMBUILD_INSTALL, NULL, NULL, test)))
		return rc;

	if ((what & RPMBUILD_PACKAGESOURCE) &&
	    (rc = processSourceFiles(spec)))
		return rc;

	if (((what & RPMBUILD_INSTALL) || (what & RPMBUILD_PACKAGEBINARY) ||
	    (what & RPMBUILD_FILECHECK)) &&
	    (rc = processBinaryFiles(spec, what & RPMBUILD_INSTALL, test)))
		return rc;

	if (((what & RPMBUILD_PACKAGESOURCE) && !test) &&
	    (rc = packageSources(spec)))
		return rc;

	if (((what & RPMBUILD_PACKAGEBINARY) && !test) &&
	    (rc = packageBinaries(spec)))
		return rc;
	
	if ((what & RPMBUILD_CLEAN) &&
	    (rc = doScript(spec, RPMBUILD_CLEAN, NULL, NULL, test)))
		return rc;

	if ((what & RPMBUILD_RMBUILD) &&
	    (rc = doScript(spec, RPMBUILD_RMBUILD, NULL, NULL, test)))
		return rc;
    }

    if (what & RPMBUILD_RMSOURCE)
	doRmSource(spec);

    if (what & RPMBUILD_RMSPEC)
	unlink(spec->specFile);

    return 0;
}
