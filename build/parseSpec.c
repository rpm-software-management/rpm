#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "header.h"
#include "rpmlib.h"
#include "part.h"
#include "spec.h"
#include "parse.h"
#include "read.h"
#include "misc.h"

static void setStandardMacros(Spec spec, char *arch, char *os);

int parseSpec(Spec *specp, char *specFile, char *buildRoot,
	      int inBuildArch, char *passPhrase, char *cookie)
{
    int parsePart = PART_PREAMBLE;
    int initialPackage = 1;
    char *name, *arch, *os;
    char *saveArch;
    Package pkg;
    int x, index;
    Spec spec;
    
    /* Set up a new Spec structure with no packages. */
    spec = newSpec();

    spec->specFile = strdup(specFile);
    if (buildRoot) {
	spec->gotBuildRoot = 1;
	spec->buildRoot = strdup(buildRoot);
    }
    spec->docDir = strdup(rpmGetVar(RPMVAR_DEFAULTDOCDIR));
    spec->inBuildArchitectures = inBuildArch;
    if (passPhrase) {
	spec->passPhrase = strdup(passPhrase);
    }
    if (cookie) {
	spec->cookie = strdup(cookie);
    }

    if (rpmGetVar(RPMVAR_TIMECHECK)) {
	if (parseNum(rpmGetVar(RPMVAR_TIMECHECK), &(spec->timeCheck))) {
	    rpmError(RPMERR_BADSPEC, "Timecheck value must be an integer: %s",
		     rpmGetVar(RPMVAR_TIMECHECK));
	    freeSpec(spec);
	    return RPMERR_BADSPEC;
	}
    } else {
	spec->timeCheck = 0;
    }

    /* Add some standard macros */
    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);
    setStandardMacros(spec, arch, os);

    /* All the parse*() functions expect to have a line pre-read */
    /* in the spec's line buffer.  Except for parsePreamble(),   */
    /* which handles the initial entry into a spec file.         */
    
    while (parsePart != PART_NONE) {
	switch (parsePart) {
	  case PART_PREAMBLE:
	    parsePart = parsePreamble(spec, initialPackage);
	    initialPackage = 0;
	    break;
	  case PART_PREP:
	    parsePart = parsePrep(spec);
	    break;
	  case PART_BUILD:
	  case PART_INSTALL:
	  case PART_CLEAN:
	    parsePart = parseBuildInstallClean(spec, parsePart);
	    break;
	  case PART_CHANGELOG:
	    parsePart = parseChangelog(spec);
	    break;
	  case PART_DESCRIPTION:
	    parsePart = parseDescription(spec);
	    break;

	  case PART_PRE:
	  case PART_POST:
	  case PART_PREUN:
	  case PART_POSTUN:
	  case PART_VERIFYSCRIPT:
	  case PART_TRIGGERIN:
	  case PART_TRIGGERUN:
	  case PART_TRIGGERPOSTUN:
	    parsePart = parseScript(spec, parsePart);
	    break;

	  case PART_FILES:
	    parsePart = parseFiles(spec);
	    break;

	}

	if (parsePart < 0) {
	    freeSpec(spec);
	    return parsePart;
	}

	if (parsePart == PART_BUILDARCHITECTURES) {
	    spec->buildArchitectureSpecs =
		malloc(sizeof(Spec) * spec->buildArchitectureCount);
	    x = 0;
	    index = 0;
	    while (x < spec->buildArchitectureCount) {
		if (rpmMachineScore(RPM_MACHTABLE_BUILDARCH,
				    spec->buildArchitectures[x])) {
		    rpmGetMachine(&saveArch, NULL);
		    saveArch = strdup(saveArch);
		    rpmSetMachine(spec->buildArchitectures[x], NULL);
		    if (parseSpec(&(spec->buildArchitectureSpecs[index]),
				  specFile, buildRoot, 1,
				  passPhrase, cookie)) {
			spec->buildArchitectureCount = index;
			freeSpec(spec);
			return RPMERR_BADSPEC;
		    }
		    rpmSetMachine(saveArch, NULL);
		    free(saveArch);
		    index++;
		}
		x++;
	    }
	    spec->buildArchitectureCount = index;
	    if (! index) {
		freeSpec(spec);
		rpmError(RPMERR_BADSPEC, "No buildable architectures");
		return RPMERR_BADSPEC;
	    }
	    closeSpec(spec);
	    *specp = spec;
	    return 0;
	}
    }

    /* Check for description in each package and add arch and os */
    pkg = spec->packages;
    while (pkg) {
	headerGetEntry(pkg->header, RPMTAG_NAME, NULL, (void **) &name, NULL);
	if (!headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	    rpmError(RPMERR_BADSPEC, "Package has no %%description: %s", name);
	    freeSpec(spec);
	    return RPMERR_BADSPEC;
	}

	headerAddEntry(pkg->header, RPMTAG_OS, RPM_STRING_TYPE, os, 1);
	headerAddEntry(pkg->header, RPMTAG_ARCH, RPM_STRING_TYPE, arch, 1);

	pkg = pkg->next;
    }

    closeSpec(spec);
    *specp = spec;
    return 0;
}

static void setStandardMacros(Spec spec, char *arch, char *os)
{
    char buf[BUFSIZ];
    int x;

    addMacro(&spec->macros, "sourcedir", rpmGetVar(RPMVAR_SOURCEDIR));
    addMacro(&spec->macros, "builddir", rpmGetVar(RPMVAR_BUILDDIR));
    addMacro(&spec->macros, "optflags", rpmGetVar(RPMVAR_OPTFLAGS));
    addMacro(&spec->macros, "buildarch", arch);
    addMacro(&spec->macros, "buildos", os);
    
    x = 0;
    while (arch[x]) {
	buf[x] = tolower(arch[x]);
	x++;
    }
    buf[x] = '\0';
    addMacro(&spec->macros, "buildarch_lc", buf);
    x = 0;
    while (os[x]) {
	buf[x] = tolower(os[x]);
	x++;
    }
    buf[x] = '\0';
    addMacro(&spec->macros, "buildos_lc", buf);
}
