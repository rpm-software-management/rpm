#include "system.h"

#include <rpmbuild.h>
#include <rpmurl.h>

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

int doScript(Spec spec, int what, const char *name, StringBuf sb, int test)
{
    const char * rootURL = spec->rootURL;
    const char * rootDir;
    const char *scriptName = NULL;
    const char * buildURL = rpmGenPath(rootURL, "%{_builddir}", "");
#ifdef	DYING
    const char * buildDir;
    const char * buildSubdir;
    const char * buildScript;
    const char * remsh = rpmGetPath("%{?_remsh:%{_remsh}}", NULL);
    const char * remchroot = rpmGetPath("%{?_remchroot:%{_remchroot}}", NULL);
    const char * buildShell =
	   rpmGetPath("%{?_buildshell:%{_buildshell}}%{!?_buildshell:/bin/sh}", NULL);
    const char * buildEnv = rpmExpand("%{_preScriptEnvironment}", NULL);
#else
    const char * buildScript;
    const char * buildCmd = NULL;
    const char * buildTemplate = NULL;
    const char * buildPost = NULL;
    const char * mTemplate = NULL;
    const char * mPost = NULL;
    int argc = 0;
    const char **argv = NULL;
#endif
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
    
    if (makeTempFile(rootURL, &scriptName, &fd)) {
	Fclose(fd);
	rpmError(RPMERR_SCRIPT, _("Unable to open temp file"));
	rc = RPMERR_SCRIPT;
	goto exit;
    }

#ifdef HAVE_FCHMOD
    switch (rootut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	(void)fchmod(Fileno(fd), 0600);	/* XXX fubar on ufdio */
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
#ifdef	DYING
    (void) urlPath(buildURL, &buildDir);
    (void) urlPath(spec->buildSubdir, &buildSubdir);
#endif

    (void) urlPath(scriptName, &buildScript);

    buildTemplate = rpmExpand(mTemplate, NULL);
    buildPost = rpmExpand(mPost, NULL);
#ifdef	DYING
    fprintf(fp, "#!%s\n", buildShell);
    fputs(buildEnv, fp);
    fputs("\n", fp);

    fprintf(fp, rpmIsVerbose()
		? "set -x\n\n"
		: "exec > /dev/null\n\n");

    fprintf(fp, "umask 022\ncd %s\n", buildDir);
#else
    fputs(buildTemplate, fp);
#endif

    if (what != RPMBUILD_PREP && what != RPMBUILD_RMBUILD && spec->buildSubdir)
	fprintf(fp, "cd %s\n", spec->buildSubdir);

    if (what == RPMBUILD_RMBUILD) {
	if (spec->buildSubdir)
	    fprintf(fp, "rm -rf %s\n", spec->buildSubdir);
    } else
	fprintf(fp, "%s", getStringBuf(sb));

#ifdef	DYING
    fprintf(fp, "\nexit 0\n");
#else
    fputs(buildPost, fp);
#endif
    
    Fclose(xfd);

    if (test) {
	rc = 0;
	goto exit;
    }
    
    if (buildURL && buildURL[0] != '/' && (urlSplit(buildURL, &u) != 0)) {
	rc = RPMERR_SCRIPT;
	goto exit;
    }
    if (u)
	addMacro(spec->macros, "_build_hostname", NULL, u->path, RMIL_SPEC);

    buildCmd = rpmExpand("%{___build_cmd}", " ", buildScript, NULL);
    poptParseArgvString(buildCmd, &argc, &argv);

    rpmMessage(RPMMESS_NORMAL, _("Executing(%s): %s\n"), name, buildCmd);
    if (!(child = fork())) {

#ifdef	DYING
fprintf(stderr, "*** root %s buildDir %s script %s remsh %s \n", rootDir, buildDir, scriptName, remsh);

	if (u == NULL || *remsh == '\0') {
fprintf(stderr, "*** LOCAL %s %s -e %s %s\n", buildShell, buildShell, buildScript, buildScript);
	    if (rootURL) {
		if (!(rootDir[0] == '/' && rootDir[1] == '\0')) {
		    chroot(rootDir);
		    chdir("/");
		}
	    }
	    errno = 0;
	    execl(buildShell, buildShell, "-e", buildScript, buildScript, NULL);
	} else {
	    if (*remchroot == '\0') {
fprintf(stderr, "*** REMSH %s %s %s -e %s %s\n", remsh, u->host, buildShell, buildScript, buildScript);
		errno = 0;
		execl(remsh, remsh, u->host, buildShell, "-e", buildScript, buildScript, NULL);
	    } else {
fprintf(stderr, "*** REMCHROOT %s %s %s %s -e %s %s\n", remsh, u->host, remchroot, buildShell, buildScript, buildScript);
		errno = 0;
		execl(remsh, remsh, u->host, remchroot, buildShell, "-e", buildScript, buildScript, NULL);
	    }
	}
#else
	execvp(argv[0], (char *const *)argv);
#endif

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
#ifdef	DYING
    FREE(buildShell);
    FREE(buildEnv);
    FREE(remsh);
    FREE(remchroot);
#else
    if (u)
	delMacro(spec->macros, "_build_hostname");
    FREE(argv);
    FREE(buildCmd);
    FREE(buildTemplate);
#endif
    FREE(buildURL);

    return rc;
}

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
