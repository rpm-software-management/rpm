#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "messages.h"
#include "misc.h"
#include "rpmerr.h"
#include "rpmlib.h"

/* the rpmrc is read from /etc/rpmrc or $HOME/.rpmrc - it is not affected
   by a --root option */

struct option {
    char * name;
    int var;
    int archSpecific;
} ;

struct archEquiv {
    char * arch, * equiv;
};

/* this *must* be kept in alphabetical order */
struct option optionTable[] = {
    { "arch_sensitive",		RPMVAR_ARCHSENSITIVE,		0 },
    { "build_arch",	        RPMVAR_BUILDARCH,	        0 },
    { "builddir",		RPMVAR_BUILDDIR,		0 },
    { "buildprefix",            RPMVAR_BUILDPREFIX,             0 },
    { "dbpath",			RPMVAR_DBPATH,			0 },
    { "distribution",		RPMVAR_DISTRIBUTION,		0 },
    { "docdir",			RPMVAR_DOCDIR,			0 },
    { "excludedocs",	        RPMVAR_EXCLUDEDOCS,             0 },
    { "messagelevel",		RPMVAR_MESSAGELEVEL,		0 },
    { "optflags",		RPMVAR_OPTFLAGS,		1 },
    { "os",		        RPMVAR_OS,             		0 },
    { "pgp_name",               RPMVAR_PGP_NAME,                0 },
    { "pgp_path",               RPMVAR_PGP_PATH,                0 },
    { "require_distribution",	RPMVAR_REQUIREDISTRIBUTION,	0 },
    { "require_group",		RPMVAR_REQUIREGROUP,		0 },
    { "require_icon",		RPMVAR_REQUIREICON,		0 },
    { "require_vendor",		RPMVAR_REQUIREVENDOR,		0 },
    { "root",			RPMVAR_ROOT,			0 },
    { "rpmdir",			RPMVAR_RPMDIR,			0 },
    { "signature",		RPMVAR_SIGTYPE,			0 },
    { "sourcedir",		RPMVAR_SOURCEDIR,		0 },
    { "specdir",		RPMVAR_SPECDIR,			0 },
    { "srcrpmdir",		RPMVAR_SRPMDIR,			0 },
    { "timecheck",		RPMVAR_TIMECHECK,		0 },
    { "topdir",			RPMVAR_TOPDIR,			0 },
    { "vendor",			RPMVAR_VENDOR,			0 },
};

static int optionTableSize = sizeof(optionTable) / sizeof(struct option);

static int readRpmrc(FILE * fd, char * fn, int readArchSpecific);
static void setDefaults(void);
static void setPathDefault(int var, char * s);
static int optionCompare(const void * a, const void * b);
static int numArchCompats = 0;
static struct archEquiv * archCompatTable = NULL;

static void addArchCompatibility(char * arch, char * equiv);

static int optionCompare(const void * a, const void * b) {
    return strcmp(((struct option *) a)->name, ((struct option *) b)->name);
}

/* returns the score for this architecture */
static int findArchScore(char * arch, char * test) {
    int i;
    int score, subscore;

    if (!strcmp(arch, test)) return 1;

    score = 1;
    for (i = 0; i < numArchCompats; i++, score++) {
	if (!strcmp(archCompatTable[i].arch, arch)) {
	    score++;
	    if (!strcmp(archCompatTable[i].equiv, test)) return score;
	}
    }

    for (i = 0; i < numArchCompats; i++, score++) {
	if (!strcmp(archCompatTable[i].arch, arch)) {
	    subscore = findArchScore(archCompatTable[i].equiv, test);
	    if (subscore) return (score + subscore);
	}
    }

    return 0;
}

int rpmArchScore(char * test) {
     return findArchScore(getArchName(), test);
}

static int addArchCompats(char * arg, char * fn, int linenum) {
    char * chptr, * archs;
  
    chptr = arg;
    while (*chptr && *chptr != ':') chptr++;
    if (!*chptr) {
	error(RPMERR_RPMRC, "missing second ':' at %s:%d\n", fn, linenum);
	return 1;
    } else if (chptr == arg) {
	error(RPMERR_RPMRC, "missing architecture name at %s:%d\n", fn, 
			     linenum);
	return 1;
    }

    while (*chptr == ':' || isspace(*chptr)) chptr--;
    *(++chptr) = '\0';
    archs = chptr + 1;
    while (*archs && isspace(*archs)) archs++;
    if (!*archs) {
	error(RPMERR_RPMRC, "missing equivalent architecture name at %s:%d\n", 
				fn, linenum);
	return 1;
    }
    
    chptr = strtok(archs, " ");
    while (chptr) {
	if (!strlen(chptr)) return 0;
	
	addArchCompatibility(arg, chptr);
	chptr = strtok(NULL, " ");
    }

    return 0;
}

static void addArchCompatibility(char * arch, char * equiv) {
    if (!numArchCompats) {
	numArchCompats = 1;
	archCompatTable = malloc(sizeof(*archCompatTable) * numArchCompats);
    } else {
	numArchCompats++;
	archCompatTable = realloc(archCompatTable, 
				   sizeof(*archCompatTable) * numArchCompats);
    }

    archCompatTable[numArchCompats - 1].arch = strdup(arch);
    archCompatTable[numArchCompats - 1].equiv = strdup(equiv);
}

static int readRpmrc(FILE * f, char * fn, int readArchSpecific) {
    char buf[1024];
    char * start;
    char * chptr;
    static char * archName = NULL;
    int linenum = 0;
    struct option * option, searchOption;

    while (fgets(buf, sizeof(buf), f)) {
	linenum++;

	/* strip off leading spaces */
	start = buf;
	while (*start && isspace(*start)) start++;

	/* I guess #.* should be a comment, but I don't know that. I'll
	   just assume that and hope I'm right. For kicks, I'll take
	   \# to escape it */

	chptr = start;
	while (*chptr) {
	    switch (*chptr) {
	      case '#':
		*chptr = '\0';
		break;

	      case '\n':
		*chptr = '\0';
		break;

	      case '\\':
		chptr++;
		/* fallthrough */
	      default:
		chptr++;
	    }
	}

	/* strip trailing spaces - chptr is at the end of the line already*/
	for (chptr--; chptr >= start && isspace(*chptr); chptr--);
	*(chptr + 1) = '\0';

	if (!start[0]) continue;			/* blank line */
	
	/* look for a :, the id part ends there */
	for (chptr = start; *chptr && *chptr != ':'; chptr++);

	if (! *chptr) {
	    error(RPMERR_RPMRC, "missing ':' at %s:%d\n", fn, linenum);
	    return 1;
	}

	*chptr = '\0';
	chptr++;	 /* now points at beginning of argument */
	while (*chptr && isspace(*chptr)) chptr++;

	if (! *chptr) {
	    error(RPMERR_RPMRC, "missing argument for %s at %s:%d\n", 
		  start, fn, linenum);
	    return 1;
	}

	message(MESS_DEBUG, "got var '%s' arg '%s'\n", start, chptr);

	/* these are options that don't just get stuffed in a VAR somewhere */
	if (!strcmp(start, "arch_compat")) {
	    if (!readArchSpecific) 
		if (addArchCompats(chptr, fn, linenum)) return 1;
	    continue;
	}

	searchOption.name = start;
	option = bsearch(&searchOption, optionTable, optionTableSize,
			 sizeof(struct option), optionCompare);
	if (!option) {
	    error(RPMERR_RPMRC, "bad option '%s' at %s:%d\n", 
			start, fn, linenum);
	    return 1;
	}

	if (option->archSpecific && readArchSpecific) {
	    start = chptr;

	    if (!archName) archName = getArchName();

	    for (chptr = start; *chptr && !isspace(*chptr); chptr++);
	    if (! *chptr) {
		error(RPMERR_RPMRC, "missing argument for %s at %s:%d\n", 
			option->name, fn, linenum);
		return 1;
	    }
	    *chptr++ = '\0';

	    while (*chptr && isspace(*chptr)) chptr++;
	    if (! *chptr) {
		error(RPMERR_RPMRC, "missing argument for %s at %s:%d\n", 
			option->name, fn, linenum);
		return 1;
	    }
	
	    if (!strcmp(archName, start)) {
		message(MESS_DEBUG, "%s is arg for %s platform", chptr,
			archName);
		setVar(option->var, chptr);
	    }
	} else {
	    if (!readArchSpecific) {
		setVar(option->var, chptr);
	    }
	}
    }

    return 0;
}

static void setDefaults(void) {
    setVar(RPMVAR_ARCHSENSITIVE, "1");
    setVar(RPMVAR_TOPDIR, "/usr/src");
    setVar(RPMVAR_DOCDIR, "/usr/doc");
    setVar(RPMVAR_OPTFLAGS, "-O2");
    setVar(RPMVAR_SIGTYPE, "none");
    setVar(RPMVAR_PGP_PATH, NULL);
    setVar(RPMVAR_PGP_NAME, NULL);
}

static int readConfigFilesAux(char *file, int readArchSpecific)
{
    FILE * f;
    char * fn;
    char * home;
    int rc = 0;

    if (file) 
	fn = file;
    else
	fn = "/etc/rpmrc";

    f = fopen(fn, "r");
    if (f) {
	rc = readRpmrc(f, "/etc/rpmrc", readArchSpecific);
	fclose(f);
	if (rc) return rc;
    }

    if (!file) {
	home = getenv("HOME");
	if (home) {
	    fn = alloca(strlen(home) + 8);
	    strcpy(fn, home);
	    strcat(fn, "/.rpmrc");
	    f = fopen(fn, "r");
	    if (f) {
		rc |= readRpmrc(f, fn, readArchSpecific);
		fclose(f);
		if (rc) return rc;
	    }
	}
    }

    return 0;
}

int rpmReadConfigFiles(char * file, char * arch, char * os, int forbuild) {
    int rc = 0;

    setDefaults();
    if (forbuild) {
	setVar(RPMVAR_BUILDARCH, arch);
	setVar(RPMVAR_OS, os);
    }

    rc = readConfigFilesAux(file, 0);  /* non-arch specific */
    if (rc) return rc;

    if (forbuild)
	initArchOs(getVar(RPMVAR_BUILDARCH), getVar(RPMVAR_OS));
    else
	initArchOs(arch, os);
    
    rc = readConfigFilesAux(file, 1);  /* arch-sepcific     */
    if (rc) return rc;
	
    /* set default directories here */

    setPathDefault(RPMVAR_BUILDDIR, "BUILD");    
    setPathDefault(RPMVAR_RPMDIR, "RPMS");    
    setPathDefault(RPMVAR_SRPMDIR, "SRPMS");    
    setPathDefault(RPMVAR_SOURCEDIR, "SOURCES");    
    setPathDefault(RPMVAR_SPECDIR, "SPECS");

    return 0;
}

static void setPathDefault(int var, char * s) {
    char * topdir;
    char * fn;

    if (getVar(var)) return;

    topdir = getVar(RPMVAR_TOPDIR);
    fn = alloca(strlen(topdir) + strlen(s) + 2);
    strcpy(fn, topdir);
    if (fn[strlen(topdir) - 1] != '/')
	strcat(fn, "/");
    strcat(fn, s);
   
    setVar(var, fn);
}

