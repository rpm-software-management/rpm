#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

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
};

struct archEquiv {
    char * arch;
    char * equiv;
};

struct defaultEntry {
    char *name;
    char *defName;
};

struct dataEntry {
    char *name;
    char *short_name;
    short num;
};

/* this *must* be kept in alphabetical order */
struct option optionTable[] = {
    { "builddir",		RPMVAR_BUILDDIR,		0 },
    { "buildroot",              RPMVAR_BUILDROOT,               0 },
    { "dbpath",			RPMVAR_DBPATH,			0 },
    { "distribution",		RPMVAR_DISTRIBUTION,		0 },
    { "excludedocs",	        RPMVAR_EXCLUDEDOCS,             0 },
    { "messagelevel",		RPMVAR_MESSAGELEVEL,		0 },
    { "optflags",		RPMVAR_OPTFLAGS,		1 },
    { "packager",               RPMVAR_PACKAGER,                0 },
    { "pgp_name",               RPMVAR_PGP_NAME,                0 },
    { "pgp_path",               RPMVAR_PGP_PATH,                0 },
    { "require_distribution",	RPMVAR_REQUIREDISTRIBUTION,	0 },
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

#define READ_TABLES       1
#define READ_OTHER        2

static int readRpmrc(FILE * fd, char * fn, int readWhat);
static void setDefaults(void);
static void setPathDefault(int var, char * s);
static int optionCompare(const void * a, const void * b);
static int numArchCompats = 0;
static struct archEquiv * archCompatTable = NULL;
static void addArchCompatibility(char * arch, char * equiv);

static void setArchOs(char *arch, char *os, int building);
static int addData(struct dataEntry **table, int *tableLen, char *line,
		   char *fn, int lineNum);
static int addDefault(struct defaultEntry **table, int *tableLen, char *line,
		      char *fn, int lineNum);
static struct dataEntry *lookupInDataTable(char *name,
					   struct dataEntry *table,
					   int tableLen);
static char *lookupInDefaultTable(char *name,
				  struct defaultEntry *table,
				  int tableLen);

static int archDefaultTableLen = 0;
static int osDefaultTableLen = 0;
static int archDataTableLen = 0;
static int osDataTableLen = 0;
static struct defaultEntry * archDefaultTable = NULL;
static struct defaultEntry * osDefaultTable = NULL;
static struct dataEntry * archDataTable = NULL;
static struct dataEntry * osDataTable = NULL;

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

static int addData(struct dataEntry **table, int *tableLen, char *line,
		   char *fn, int lineNum)
{
    struct dataEntry *t;
    char *s, *s1;
    
    if (! *tableLen) {
	*tableLen = 1;
	*table = malloc(sizeof(struct dataEntry));
    } else {
	(*tableLen)++;
	*table = realloc(*table, sizeof(struct dataEntry) * (*tableLen));
    }
    t = & ((*table)[*tableLen - 1]);

    t->name = strtok(line, ": \t");
    t->short_name = strtok(NULL, " \t");
    s = strtok(NULL, " \t");
    if (! (t->name && t->short_name && s)) {
	error(RPMERR_RPMRC, "Incomplete data line at %s:%d\n", fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	error(RPMERR_RPMRC, "Too many args in data line at %s:%d\n",
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->num = strtoul(s, &s1, 10);
    if ((*s1) || (s1 == s) || (t->num == ULONG_MAX)) {
	error(RPMERR_RPMRC, "Bad arch/os number: %s (%s:%d)\n", s,
	      fn, lineNum);
	return(RPMERR_RPMRC);
    }

    t->name = strdup(t->name);
    t->short_name = strdup(t->short_name);

    return 0;
}

static int addDefault(struct defaultEntry **table, int *tableLen, char *line,
		      char *fn, int lineNum)
{
    struct defaultEntry *t;
    
    if (! *tableLen) {
	*tableLen = 1;
	*table = malloc(sizeof(struct defaultEntry));
    } else {
	(*tableLen)++;
	*table = realloc(*table, sizeof(struct defaultEntry) * (*tableLen));
    }
    t = & ((*table)[*tableLen - 1]);

    t->name = strtok(line, ": \t");
    t->defName = strtok(NULL, " \t");
    if (! (t->name && t->defName)) {
	error(RPMERR_RPMRC, "Incomplete default line at %s:%d\n", fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	error(RPMERR_RPMRC, "Too many args in default line at %s:%d\n",
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->name = strdup(t->name);
    t->defName = strdup(t->defName);

    return 0;
}

static struct dataEntry *lookupInDataTable(char *name,
					   struct dataEntry *table,
					   int tableLen)
{
    while (tableLen) {
	tableLen--;
	if (!strcmp(name, table[tableLen].name)) {
	    return &(table[tableLen]);
	}
    }

    return NULL;
}

static char *lookupInDefaultTable(char *name,
				  struct defaultEntry *table,
				  int tableLen)
{
    while (tableLen) {
	tableLen--;
	if (!strcmp(name, table[tableLen].name)) {
	    return table[tableLen].defName;
	}
    }

    return name;
}

static int readRpmrc(FILE * f, char * fn, int readWhat) {
    char buf[1024];
    char * start;
    char * chptr;
    char * archName = NULL;
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
	    if (readWhat != READ_TABLES) continue;
	    if (addArchCompats(chptr, fn, linenum))
		return 1;
	} else if (!strcmp(start, "arch_data")) {
	    if (readWhat != READ_TABLES) continue;
	    if (addData(&archDataTable, &archDataTableLen,
			chptr, fn, linenum))
		return 1;
	} else if (!strcmp(start, "os_data")) {
	    if (readWhat != READ_TABLES) continue;
	    if (addData(&osDataTable, &osDataTableLen,
			chptr, fn, linenum))
		return 1;
	} else if (!strcmp(start, "buildarchtranslate")) {
	    if (readWhat != READ_TABLES) continue;
	    if (addDefault(&archDefaultTable, &archDefaultTableLen,
			   chptr, fn, linenum))
		return 1;
	} else if (!strcmp(start, "buildostranslate")) {
	    if (readWhat != READ_TABLES) continue;
	    if (addDefault(&osDefaultTable, &osDefaultTableLen,
			   chptr, fn, linenum))
		return 1;
	}
	if (readWhat == READ_TABLES) {
	    continue;
	}

	/* Parse the argument a little further */
	searchOption.name = start;
	option = bsearch(&searchOption, optionTable, optionTableSize,
			 sizeof(struct option), optionCompare);
	if (!option) {
	    error(RPMERR_RPMRC, "bad option '%s' at %s:%d\n", 
			start, fn, linenum);
	    return 1;
	}

	if (readWhat == READ_OTHER) {
	    if (option->archSpecific) {
		start = chptr;

		/* getArchName() should be safe by now */
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
		setVar(option->var, chptr);
	    }
	    continue;
	}
	error(RPMERR_INTERNAL, "Bad readWhat: %d", readWhat);
	exit(1);
    }

    return 0;
}

static void setDefaults(void) {
    setVar(RPMVAR_TOPDIR, "/usr/src");
    setVar(RPMVAR_OPTFLAGS, "-O2");
    setVar(RPMVAR_SIGTYPE, "none");
    setVar(RPMVAR_PGP_PATH, NULL);
    setVar(RPMVAR_PGP_NAME, NULL);
}

static int readConfigFilesAux(char *file, int readWhat)
{
    FILE * f;
    char * fn;
    char * home;
    int rc = 0;

    f = fopen("/usr/lib/rpmrc", "r");
    if (f) {
	rc = readRpmrc(f, "/usr/lib/rpmrc", readWhat);
	fclose(f);
	if (rc) return rc;
    } else {
	error(RPMERR_RPMRC, "Unable to read /usr/lib/rpmrc");
	return RPMERR_RPMRC;
    }
    
    if (file) 
	fn = file;
    else
	fn = "/etc/rpmrc";

    f = fopen(fn, "r");
    if (f) {
	rc = readRpmrc(f, fn, readWhat);
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
		rc |= readRpmrc(f, fn, readWhat);
		fclose(f);
		if (rc) return rc;
	    }
	}
    }

    return 0;
}

int rpmReadConfigFiles(char * file, char * arch, char * os, int building)
{
    int rc = 0;
    
    setDefaults();
    
    rc = readConfigFilesAux(file, READ_TABLES);
    if (rc) return rc;

    setArchOs(arch, os, building);

    rc = readConfigFilesAux(file, READ_OTHER);
    if (rc) return rc;

    /* set default directories */

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

static int osnum;
static int archnum;
static char *osname;
static char *archname;
static int archOsIsInit = 0;

static void setArchOs(char *arch, char *os, int build)
{
    struct utsname un;
    struct dataEntry *archData, *osData;

    if (archOsIsInit) {
	error(RPMERR_INTERNAL, "Internal error: Arch/OS already initialized!");
        error(RPMERR_INTERNAL, "Arch: %d\nOS: %d", archnum, osnum);
	exit(1);
    }

    uname(&un);
    if (build) {
	if (! arch) {
	    arch = lookupInDefaultTable(un.machine, archDefaultTable,
					archDefaultTableLen);
	}
	if (! os) {
	    os = lookupInDefaultTable(un.sysname, osDefaultTable,
				      osDefaultTableLen);
	}
    } else {
	arch = un.machine;
	os = un.sysname;
    }

    archData = lookupInDataTable(arch, archDataTable, archDataTableLen);
    osData = lookupInDataTable(os, osDataTable, osDataTableLen);
    if (archData) {
	archnum = archData->num;
	archname = strdup(archData->short_name);
    } else {
	archnum = 255;
	archname = strdup(arch);
	message(MESS_WARNING, "Unknown architecture: %s\n", arch);
	message(MESS_WARNING, "Please contact rpm-list@redhat.com\n");
    }
    if (osData) {
	osnum = osData->num;
	osname = strdup(osData->short_name);
    } else {
	osnum = 255;
	osname = strdup(os);
	message(MESS_WARNING, "Unknown OS: %s\n", os);
	message(MESS_WARNING, "Please contact rpm-list@redhat.com\n");
    }

    archOsIsInit = 1;
}

#define FAIL_IF_NOT_INIT \
{\
    if (! archOsIsInit) {\
	error(RPMERR_INTERNAL, "Internal error: Arch/OS not initialized!");\
        error(RPMERR_INTERNAL, "Arch: %d\nOS: %d", archnum, osnum);\
	exit(1);\
    }\
}

int getOsNum(void)
{
    FAIL_IF_NOT_INIT;
    return osnum;
}

int getArchNum(void)
{
    FAIL_IF_NOT_INIT;
    return archnum;
}

char *getOsName(void)
{
    FAIL_IF_NOT_INIT;
    return osname;
}

char *getArchName(void)
{
    FAIL_IF_NOT_INIT;
    return archname;
}

int showRc(FILE *f)
{
    struct option *opt;
    int count = 0;
    char *s;

    fprintf(f, "ARCHITECTURE AND OS:\n");
    fprintf(f, "build arch           : %s\n", getArchName());
    fprintf(f, "build os             : %s\n", getOsName());

    /* This is a major hack */
    archOsIsInit = 0;
    setArchOs(NULL, NULL, 0);
    fprintf(f, "install arch         : %s\n", getArchName());
    fprintf(f, "install os           : %s\n", getOsName());

    fprintf(f, "RPMRC VALUES:\n");
    opt = optionTable;
    while (count < optionTableSize) {
	s = getVar(opt->var);
	fprintf(f, "%-20s : %s\n", opt->name, s ? s : "(not set)");
	opt++;
	count++;
    }
    
    return 0;
}
