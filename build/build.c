#include "system.h"

#include "rpmbuild.h"

#ifdef	DYING
static void doRmSource(Spec spec);
#endif

static void doRmSource(Spec spec)
{
    struct Source *p;
    Package pkg;
    
    unlink(spec->specFile);

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
    FILE *f;
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
    if ((what != RPMBUILD_RMBUILD) && sb == NULL) {
	return 0;
    }
    
    if (makeTempFile(NULL, &scriptName, &fd)) {
	    fdClose(fd);
	    FREE(scriptName);
	    rpmError(RPMERR_SCRIPT, _("Unable to open temp file"));
	    return RPMERR_SCRIPT;
    }
    (void)fchmod(fdFileno(fd), 0600);
    f = fdFdopen(fd, "w");
    
    strcpy(buf, _preScriptEnvironment);
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    strcat(buf, "\n");
    fputs(buf, f);

    fprintf(f, rpmIsVerbose() ? "set -x\n\n" : "exec > /dev/null\n\n");

/* XXX umask 022; cd %{_builddir} */
    strcpy(buf, _preScriptChdir);
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    fputs(buf, f);

    if (what != RPMBUILD_PREP && what != RPMBUILD_RMBUILD) {
	if (spec->buildSubdir) {
	    fprintf(f, "cd %s\n", spec->buildSubdir);
	}
    }
    if (what == RPMBUILD_RMBUILD) {
	if (spec->buildSubdir) {
	    fprintf(f, "rm -rf %s\n", spec->buildSubdir);
	}
    } else {
	fprintf(f, "%s", getStringBuf(sb));
    }
    fprintf(f, "\nexit 0\n");
    
    fclose(f);

    if (test) {
	FREE(scriptName);
	return 0;
    }
    
    rpmMessage(RPMMESS_NORMAL, _("Executing: %s\n"), name);
    if (!(pid = fork())) {
	const char *buildShell = rpmGetPath("%{_buildshell}", NULL);
	execl(buildShell, buildShell, "-e", scriptName, scriptName, NULL);
	rpmError(RPMERR_SCRIPT, _("Exec of %s failed (%s)"),
		 scriptName, name);
#if 0   /* XXX don't erase the failing script */
	unlink(scriptName);
#endif
	FREE(scriptName);
	return RPMERR_SCRIPT;
    }
    (void)wait(&status);
    if (! WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SCRIPT, _("Bad exit status from %s (%s)"),
		 scriptName, name);
#if HACK
	unlink(scriptName);
#endif
	FREE(scriptName);
	return RPMERR_SCRIPT;
    }
    
    unlink(scriptName);
    FREE(scriptName);

    return 0;
}

int buildSpec(Spec spec, int what, int test)
{
    int x, rc;

    if (!spec->inBuildArchitectures && spec->buildArchitectureCount) {
	/* When iterating over buildArchitectures, do the source    */
	/* packaging on the first run, and skip RMSOURCE altogether */
	x = 0;
	while (x < spec->buildArchitectureCount) {
	    if ((rc = buildSpec(spec->buildArchitectureSpecs[x],
				(what & ~RPMBUILD_RMSOURCE) |
				(x ? 0 : (what & RPMBUILD_PACKAGESOURCE)),
				test))) {
		return rc;
	    }
	    x++;
	}
    } else {
	if (what & RPMBUILD_PREP) {
	    if ((rc = doScript(spec, RPMBUILD_PREP, NULL, NULL, test))) {
		return rc;
	    }
	}
	if (what & RPMBUILD_BUILD) {
	    if ((rc = doScript(spec, RPMBUILD_BUILD, NULL, NULL, test))) {
		return rc;
	    }
	}
	if (what & RPMBUILD_INSTALL) {
	    if ((rc = doScript(spec, RPMBUILD_INSTALL, NULL, NULL, test))) {
		return rc;
	    }
	}

	if (what & RPMBUILD_PACKAGESOURCE) {
	    if ((rc = processSourceFiles(spec))) {
		return rc;
	    }
	}

	if ((what & RPMBUILD_INSTALL) || (what & RPMBUILD_PACKAGEBINARY) ||
	    (what & RPMBUILD_FILECHECK)) {
	    if ((rc = processBinaryFiles(spec, what & RPMBUILD_INSTALL,
					 test))) {
		return rc;
	    }
	}

	if (what & RPMBUILD_PACKAGESOURCE && !test) {
	    if ((rc = packageSources(spec))) {
		return rc;
	    }
	}
	if (what & RPMBUILD_PACKAGEBINARY && !test) {
	    if ((rc = packageBinaries(spec))) {
		return rc;
	    }
	}
	
	if (what & RPMBUILD_CLEAN) {
	    if ((rc = doScript(spec, RPMBUILD_CLEAN, NULL, NULL, test))) {
		return rc;
	    }
	}
	if (what & RPMBUILD_RMBUILD) {
	    if ((rc = doScript(spec, RPMBUILD_RMBUILD, NULL, NULL, test))) {
		return rc;
	    }
	}
    }

    if (what & RPMBUILD_RMSOURCE) {
	doRmSource(spec);
    }

    return 0;
}
