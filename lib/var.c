#include <stdlib.h>
#include "var.h"

static char *topdir    = "/usr/src";
static char *sourcedir = "/usr/src/SOURCES";
static char *builddir  = "/usr/src/BUILD";
static char *specdir   = "/usr/src/SPECS";
static char *docdir    = "/usr/doc";
static char *optflags  = "-O2 -m486";

char *getVar(int var)
{
    switch (var) {
    case RPMVAR_SOURCEDIR:
	return sourcedir;
    case RPMVAR_BUILDDIR:
	return builddir;
    case RPMVAR_DOCDIR:
	return docdir;
    case RPMVAR_TOPDIR:
	return topdir;
    case RPMVAR_SPECDIR:
	return specdir;
    case RPMVAR_OPTFLAGS:
	return optflags;
    }
    return NULL;
}
