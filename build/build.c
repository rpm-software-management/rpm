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

static char *_preScriptEnvironment = "%{_preScriptEnvironment}";

static char *_preScriptChdir = 
	"umask 022\n"
	"cd %{_builddir}\n"
;

int doScript(Spec spec, int what, const char *name, StringBuf sb, int test)
{
    FD_t fd;
    FD_t xfd;
    const char *scriptName;
    int pid;
    int status;
    char buf[BUFSIZ];
    
    switch (what) {
      case RPMBUILD_PREP:
	name = "%prep";
	sb = spec->prep;
	break;
      case RPMBUILD_BUILD:
	name = "%build";
	sb = spec->build;
	break;
      case RPMBUILD_INSTALL:
	name = "%install";
	sb = spec->install;
	break;
      case RPMBUILD_CLEAN:
	name = "%clean";
	sb = spec->clean;
	break;
      case RPMBUILD_RMBUILD:
	name = "--clean";
	break;
      case RPMBUILD_STRINGBUF:
	break;
    }

    if ((what != RPMBUILD_RMBUILD) && sb == NULL)
	return 0;
    
    if (makeTempFile(spec->rootdir, &scriptName, &fd)) {
	    Fclose(fd);
	    FREE(scriptName);
	    rpmError(RPMERR_SCRIPT, _("Unable to open temp file"));
	    return RPMERR_SCRIPT;
    }
#ifdef HAVE_FCHMOD
    (void)fchmod(Fileno(fd), 0600);	/* XXX fubar on ufdio */
#endif
    xfd = Fdopen(fd, "w.fdio");
    
    strcpy(buf, _preScriptEnvironment);
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    strcat(buf, "\n");
    fputs(buf, fpio->ffileno(xfd));

    fprintf(fpio->ffileno(xfd), rpmIsVerbose() ? "set -x\n\n" : "exec > /dev/null\n\n");

/* XXX umask 022; cd %{_builddir} */
    strcpy(buf, _preScriptChdir);
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    fputs(buf, fpio->ffileno(xfd));

    if (what != RPMBUILD_PREP && what != RPMBUILD_RMBUILD) {
	if (spec->buildSubdir)
	    fprintf(fpio->ffileno(xfd), "cd %s\n", spec->buildSubdir);
    }
    if (what == RPMBUILD_RMBUILD) {
	if (spec->buildSubdir)
	    fprintf(fpio->ffileno(xfd), "rm -rf %s\n", spec->buildSubdir);
    } else
	fprintf(fpio->ffileno(xfd), "%s", getStringBuf(sb));
    fprintf(fpio->ffileno(xfd), "\nexit 0\n");
    
    Fclose(xfd);

    if (test) {
	FREE(scriptName);
	return 0;
    }
    
    rpmMessage(RPMMESS_NORMAL, _("Executing: %s\n"), name);
    if (!(pid = fork())) {
	const char *buildShell = rpmGetPath("%{_buildshell}", NULL);

	if (spec->rootdir)
	    Chroot(spec->rootdir);
	chdir("/");

	switch (urlIsURL(scriptName)) {
	case URL_IS_PATH:
	    scriptName += sizeof("file://") - 1;
	    scriptName = strchr(scriptName, '/');
	    /*@fallthrough@*/
	case URL_IS_UNKNOWN:
	    execl(buildShell, buildShell, "-e", scriptName, scriptName, NULL);
	    break;
	default:
	    break;
	}

	rpmError(RPMERR_SCRIPT, _("Exec of %s failed (%s)"), scriptName, name);
	_exit(-1);
    }

    (void)wait(&status);
    if (! WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SCRIPT, _("Bad exit status from %s (%s)"),
		 scriptName, name);
#if HACK
	Unlink(scriptName);
#endif
	FREE(scriptName);
	return RPMERR_SCRIPT;
    }
    
    Unlink(scriptName);
    FREE(scriptName);

    return 0;
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
