#include "system.h"

#include "rpmbuild.h"

#ifdef	DYING
static void doRmSource(Spec spec);
static int writeVars(Spec spec, FILE *f);
#endif

static void doRmSource(Spec spec)
{
    struct Source *p;
    Package pkg;
    char buf[BUFSIZ];
    
    unlink(spec->specFile);

    p = spec->sources;
    while (p) {
	if (! (p->flags & RPMBUILD_ISNO)) {
	    strcpy(buf, "%{_sourcedir}/");
	    expandMacros(spec, spec->macros, buf, sizeof(buf));
	    strcat(buf, p->source);
	    unlink(buf);
	}
	p = p->next;
    }

    pkg = spec->packages;
    while (pkg) {
	p = pkg->icon;
	while (p) {
	    if (! (p->flags & RPMBUILD_ISNO)) {
		strcpy(buf, "%{_sourcedir}/");
		expandMacros(spec, spec->macros, buf, sizeof(buf));
		strcat(buf, p->source);
		unlink(buf);
	    }
	    p = p->next;
	}
	pkg = pkg->next;
    }
}

/*
 * The _preScript string is expanded to export values to a script environment.
 */

static char *_preScriptEnvironment = 
	"RPM_SOURCE_DIR=\"%{_sourcedir}\"\n"
	"RPM_BUILD_DIR=\"%{_builddir}\"\n"
	"RPM_OPT_FLAGS=\"%{optflags}\"\n"
	"export  RPM_SOURCE_DIR RPM_BUILD_DIR RPM_OPT_FLAGS\n"
;

static int writeVars(Spec spec, FILE *f)
{
    char *arch, *os, *s;
    char buf[BUFSIZ];

    strcpy(buf, _preScriptEnvironment);
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    fprintf(f, "%s\n", buf);
    
    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);

    fprintf(f, "RPM_DOC_DIR=\"%s\"\n", spec->docDir);
    fprintf(f, "RPM_ARCH=\"%s\"\n", arch);
    fprintf(f, "RPM_OS=\"%s\"\n", os);

    fprintf(f, "export RPM_DOC_DIR RPM_ARCH RPM_OS\n");

    if (spec->buildRoot) {
	fprintf(f, "RPM_BUILD_ROOT=\"%s\"\n", spec->buildRoot);
	fprintf(f, "export RPM_BUILD_ROOT\n");
	/* This could really be checked internally */
	fprintf(f, "if [ -z \"$RPM_BUILD_ROOT\" -o -z \"`echo $RPM_BUILD_ROOT | sed -e 's#/##g'`\" ]; then\n");
	fprintf(f, "  echo 'Warning: Spec contains BuildRoot: tag that is either empty or is set to \"/\"'\n");
	fprintf(f, "  exit 1\n");
	fprintf(f, "fi\n");
    }

    headerGetEntry(spec->packages->header, RPMTAG_NAME,
		   NULL, (void **)&s, NULL);
    fprintf(f, "RPM_PACKAGE_NAME=\"%s\"\n", s);
    headerGetEntry(spec->packages->header, RPMTAG_VERSION,
		   NULL, (void **)&s, NULL);
    fprintf(f, "RPM_PACKAGE_VERSION=\"%s\"\n", s);
    headerGetEntry(spec->packages->header, RPMTAG_RELEASE,
		   NULL, (void **)&s, NULL);
    fprintf(f, "RPM_PACKAGE_RELEASE=\"%s\"\n", s);
    fprintf(f, "export RPM_PACKAGE_NAME RPM_PACKAGE_VERSION "
	    "RPM_PACKAGE_RELEASE\n");
    
    return 0;
}

static char *_preScriptChdir = 
	"umask 022\n"
	"cd %{_builddir}\n"
;

int doScript(Spec spec, int what, char *name, StringBuf sb, int test)
{
    int fd;
    FILE *f;
    char *scriptName;
    int pid;
    int status;
    char *buildShell;
    
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
    if ((what != RPMBUILD_RMBUILD) && !sb) {
	return 0;
    }
    
    if (makeTempFile(NULL, &scriptName, &fd)) {
	rpmError(RPMERR_SCRIPT, "Unable to open temp file");
	return RPMERR_SCRIPT;
    }
    f = fdopen(fd, "w");
    
    if (writeVars(spec, f)) {
	fclose(f);
	unlink(scriptName);
	FREE(scriptName);
	return RPMERR_SCRIPT;
    }
    
    fprintf(f, rpmIsVerbose() ? "set -x\n\n" : "exec > /dev/null\n\n");

/* XXX umask 022; cd %{_builddir} */
  { char buf[BUFSIZ];
    strcpy(buf, _preScriptChdir);
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    fputs(buf, f);
  }

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
    chmod(scriptName, 0600);

    if (test) {
	FREE(scriptName);
	return 0;
    }
    
    rpmMessage(RPMMESS_NORMAL, "Executing: %s\n", name);
    if (!(pid = fork())) {
	buildShell = rpmGetVar(RPMVAR_BUILDSHELL);
	execl(buildShell, buildShell, "-e", scriptName, scriptName, NULL);
	rpmError(RPMERR_SCRIPT, "Exec of %s failed (%s)",
		 scriptName, name);
	unlink(scriptName);
	FREE(scriptName);
	return RPMERR_SCRIPT;
    }
    wait(&status);
    if (! WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SCRIPT, "Bad exit status from %s (%s)",
		 scriptName, name);
	unlink(scriptName);
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
