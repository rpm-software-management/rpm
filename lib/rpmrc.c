#include "system.h"

#include <stdarg.h>

#if HAVE_SYS_SYSTEMCFG_H
#include <sys/systemcfg.h>
#else
#define __power_pc() 0
#endif

#include <rpmlib.h>
#include <rpmmacro.h>

#include "misc.h"

static const char *defrcfiles = LIBRPMRC_FILENAME ":/etc/rpmrc:~/.rpmrc";

struct machCacheEntry {
    char * name;
    int count;
    char ** equivs;
    int visited;
};

struct machCache {
    struct machCacheEntry * cache;
    int size;
};

struct machEquivInfo {
    char * name;
    int score;
};

struct machEquivTable {
    int count;
    struct machEquivInfo * list;
};

struct rpmvarValue {
    char * value;
    /* eventually, this arch will be replaced with a generic condition */
    char * arch;
    struct rpmvarValue * next;
};

struct rpmOption {
    char * name;
    int var;
    int archSpecific, required, macroize, localize;
    struct rpmOptionValue * value;
};

struct defaultEntry {
    char *name;
    char *defName;
};

struct canonEntry {
    char *name;
    char *short_name;
    short num;
};

/* tags are 'key'canon, 'key'translate, 'key'compat

   for giggles, 'key'_canon, 'key'_compat, and 'key'_canon will also work */
struct tableType {
    char * key;
    int hasCanon, hasTranslate;
    struct machEquivTable equiv;
    struct machCache cache;
    struct defaultEntry * defaults;
    struct canonEntry * canons;
    int defaultsLength;
    int canonsLength;
};

/*@-fullinitblock@*/
static struct tableType tables[RPM_MACHTABLE_COUNT] = {
    { "arch", 1, 0 },
    { "os", 1, 0 },
    { "buildarch", 0, 1 },
    { "buildos", 0, 1 }
};

/* this *must* be kept in alphabetical order */
/* The order of the flags is archSpecific, required, macroize, localize */

static struct rpmOption optionTable[] = {
    { "include",		RPMVAR_INCLUDE,			0, 1,	0, 2 },
    { "macrofiles",		RPMVAR_MACROFILES,		0, 0,	0, 1 },
    { "optflags",		RPMVAR_OPTFLAGS,		1, 0,	1, 0 },
    { "provides",               RPMVAR_PROVIDES,                0, 0,	0, 0 },
};
/*@=fullinitblock@*/
static int optionTableSize = sizeof(optionTable) / sizeof(*optionTable);

#define OS	0
#define ARCH	1

static char * current[2];
static int currTables[2] = { RPM_MACHTABLE_INSTOS, RPM_MACHTABLE_INSTARCH };
static struct rpmvarValue values[RPMVAR_NUM];

/* prototypes */
static void defaultMachine(/*@out@*/char ** arch, /*@out@*/char ** os);
static int doReadRC(FD_t fd, const char * filename);
static int optionCompare(const void * a, const void * b);
static int addCanon(struct canonEntry **table, int *tableLen, char *line,
			const char *fn, int lineNum);
static int addDefault(struct defaultEntry **table, int *tableLen, char *line,
			const char *fn, int lineNum);
static void freeRpmVar(struct rpmvarValue * orig);
static void rpmSetVarArch(int var, const char * val, const char * arch);
static struct canonEntry *lookupInCanonTable(char *name,
					   struct canonEntry *table,
					   int tableLen);
static const char *lookupInDefaultTable(const char *name,
				  struct defaultEntry *table,
				  int tableLen);

static void setVarDefault(int var, const char *macroname, const char *val, const char *body);
static void setPathDefault(int var, const char *macroname, const char *subdir);
static void setDefaults(void);

static void rebuildCompatTables(int type, char *name);

/* compatiblity tables */
static int machCompatCacheAdd(char * name, const char * fn, int linenum,
				struct machCache * cache);
static struct machCacheEntry * machCacheFindEntry(struct machCache * cache,
						  char * key);
static struct machEquivInfo * machEquivSearch(
		struct machEquivTable * table, char * name);
static void machAddEquiv(struct machEquivTable * table, char * name,
			   int distance);
static void machCacheEntryVisit(struct machCache * cache,
				  struct machEquivTable * table,
				  char * name,
	  			  int distance);
static void machFindEquivs(struct machCache * cache,
			     struct machEquivTable * table,
			     char * key);

static int optionCompare(const void * a, const void * b) {
    return strcasecmp(((struct rpmOption *) a)->name,
		      ((struct rpmOption *) b)->name);
}

static void rpmRebuildTargetVars(const char **target, const char ** canontarget);

static struct machCacheEntry * machCacheFindEntry(struct machCache * cache,
						  char * key)
{
    int i;

    for (i = 0; i < cache->size; i++)
	if (!strcmp(cache->cache[i].name, key)) return cache->cache + i;

    return NULL;
}

static int machCompatCacheAdd(char * name, const char * fn, int linenum,
				struct machCache * cache)
{
    char * chptr, * equivs;
    int delEntry = 0;
    int i;
    struct machCacheEntry * entry = NULL;

    while (*name && isspace(*name)) name++;

    chptr = name;
    while (*chptr && *chptr != ':') chptr++;
    if (!*chptr) {
	rpmError(RPMERR_RPMRC, _("missing second ':' at %s:%d"), fn, linenum);
	return 1;
    } else if (chptr == name) {
	rpmError(RPMERR_RPMRC, _("missing architecture name at %s:%d"), fn,
			     linenum);
	return 1;
    }

    while (*chptr == ':' || isspace(*chptr)) chptr--;
    *(++chptr) = '\0';
    equivs = chptr + 1;
    while (*equivs && isspace(*equivs)) equivs++;
    if (!*equivs) {
	delEntry = 1;
    }

    if (cache->size) {
	entry = machCacheFindEntry(cache, name);
	if (entry) {
	    for (i = 0; i < entry->count; i++)
		free(entry->equivs[i]);
	    if (entry->count) free(entry->equivs);
	    entry->count = 0;
	}
    }

    if (!entry) {
	cache->cache = xrealloc(cache->cache,
			       (cache->size + 1) * sizeof(*cache->cache));
	entry = cache->cache + cache->size++;
	entry->name = xstrdup(name);
	entry->count = 0;
	entry->visited = 0;
    }

    if (delEntry) return 0;

    while ((chptr = strtok(equivs, " ")) != NULL) {
	equivs = NULL;
	if (chptr[0] == '\0')	/* does strtok() return "" ever?? */
	    continue;
	if (entry->count)
	    entry->equivs = xrealloc(entry->equivs, sizeof(*entry->equivs)
					* (entry->count + 1));
	else
	    entry->equivs = xmalloc(sizeof(*entry->equivs));

	entry->equivs[entry->count] = xstrdup(chptr);
	entry->count++;
    }

    return 0;
}

static struct machEquivInfo * machEquivSearch(
		struct machEquivTable * table, char * name)
{
    int i;

/*
 * XXX The strcasecmp below is necessary so the old (rpm < 2.90) style
 * XXX os-from-uname (e.g. "Linux") is compatible with the new
 * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
 * XXX A copy of this string is embedded in headers and is
 * XXX used by rpmInstallPackage->{os,arch}Okay->rpmMachineScore->.
 * XXX to verify correct arch/os from headers.
 */
    for (i = 0; i < table->count; i++)
	if (!strcasecmp(table->list[i].name, name))
	    return table->list + i;

    return NULL;
}

static void machAddEquiv(struct machEquivTable * table, char * name,
			   int distance)
{
    struct machEquivInfo * equiv;

    equiv = machEquivSearch(table, name);
    if (!equiv) {
	if (table->count)
	    table->list = xrealloc(table->list, (table->count + 1)
				    * sizeof(*table->list));
	else
	    table->list = xmalloc(sizeof(*table->list)); /* XXX memory leak */

	table->list[table->count].name = xstrdup(name); /* XXX memory leak */
	table->list[table->count++].score = distance;
    }
}

static void machCacheEntryVisit(struct machCache * cache,
				  struct machEquivTable * table,
				  char * name,
	  			  int distance)
{
    struct machCacheEntry * entry;
    int i;

    entry = machCacheFindEntry(cache, name);
    if (!entry || entry->visited) return;

    entry->visited = 1;

    for (i = 0; i < entry->count; i++) {
	machAddEquiv(table, entry->equivs[i], distance);
    }

    for (i = 0; i < entry->count; i++) {
	machCacheEntryVisit(cache, table, entry->equivs[i], distance + 1);
    }
}

static void machFindEquivs(struct machCache * cache,
			     struct machEquivTable * table,
			     char * key)
{
    int i;

    for (i = 0; i < cache->size; i++)
	cache->cache[i].visited = 0;

    table->count = 0;

    /* We have a general graph built using strings instead of pointers.
       Yuck. We have to start at a point at traverse it, remembering how
       far away everything is. */
    machAddEquiv(table, key, 1);
    machCacheEntryVisit(cache, table, key, 2);
}

static int addCanon(struct canonEntry **table, int *tableLen, char *line,
		    const char *fn, int lineNum)
{
    struct canonEntry *t;
    char *s, *s1;

    if (! *tableLen) {
	*tableLen = 2;
	*table = xmalloc(2 * sizeof(struct canonEntry));
    } else {
	(*tableLen) += 2;
	*table = xrealloc(*table, sizeof(struct canonEntry) * (*tableLen));
    }
    t = & ((*table)[*tableLen - 2]);

    t->name = strtok(line, ": \t");
    t->short_name = strtok(NULL, " \t");
    s = strtok(NULL, " \t");
    if (! (t->name && t->short_name && s)) {
	rpmError(RPMERR_RPMRC, _("Incomplete data line at %s:%d"), fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	rpmError(RPMERR_RPMRC, _("Too many args in data line at %s:%d"),
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->num = strtoul(s, &s1, 10);
    if ((*s1) || (s1 == s) || (t->num == ULONG_MAX)) {
	rpmError(RPMERR_RPMRC, _("Bad arch/os number: %s (%s:%d)"), s,
	      fn, lineNum);
	return(RPMERR_RPMRC);
    }

    t->name = xstrdup(t->name);
    t->short_name = xstrdup(t->short_name);

    /* From A B C entry */
    /* Add  B B C entry */
    t[1].name = xstrdup(t->short_name);
    t[1].short_name = xstrdup(t->short_name);
    t[1].num = t->num;

    return 0;
}

static int addDefault(struct defaultEntry **table, int *tableLen, char *line,
			const char *fn, int lineNum)
{
    struct defaultEntry *t;

    if (! *tableLen) {
	*tableLen = 1;
	*table = xmalloc(sizeof(struct defaultEntry));
    } else {
	(*tableLen)++;
	*table = xrealloc(*table, sizeof(struct defaultEntry) * (*tableLen));
    }
    t = & ((*table)[*tableLen - 1]);

    t->name = strtok(line, ": \t");
    t->defName = strtok(NULL, " \t");
    if (! (t->name && t->defName)) {
	rpmError(RPMERR_RPMRC, _("Incomplete default line at %s:%d"),
		 fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	rpmError(RPMERR_RPMRC, _("Too many args in default line at %s:%d"),
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->name = xstrdup(t->name);
    t->defName = xstrdup(t->defName);

    return 0;
}

static struct canonEntry *lookupInCanonTable(char *name,
					     struct canonEntry *table,
					     int tableLen) {
    while (tableLen) {
	tableLen--;
	if (!strcmp(name, table[tableLen].name)) {
	    return &(table[tableLen]);
	}
    }

    return NULL;
}

static const char *lookupInDefaultTable(const char *name, struct defaultEntry *table,
				  int tableLen) {
    while (tableLen) {
	tableLen--;
	if (!strcmp(name, table[tableLen].name)) {
	    return table[tableLen].defName;
	}
    }

    return name;
}

int rpmReadConfigFiles(const char * file, const char * target)
{

    /* Preset target macros */
    rpmRebuildTargetVars(&target, NULL);

    /* Read the files */
    if (rpmReadRC(file)) return -1;

    /* Reset target macros */
    rpmRebuildTargetVars(&target, NULL);

    /* Finally set target platform */
    {	const char *cpu = rpmExpand("%{_target_cpu}", NULL);
	const char *os = rpmExpand("%{_target_os}", NULL);
	rpmSetMachine(cpu, os);
	xfree(cpu);
	xfree(os);
    }

    return 0;
}

static void setVarDefault(int var, const char *macroname, const char *val, const char *body)
{
    if (var >= 0) {	/* XXX Dying ... */
	if (rpmGetVar(var)) return;
	rpmSetVar(var, val);
    }
    if (body == NULL)
	body = val;
    addMacro(NULL, macroname, NULL, body, RMIL_DEFAULT);
}

static void setPathDefault(int var, const char *macroname, const char *subdir)
{

    if (var >= 0) {	/* XXX Dying ... */
	const char * topdir;
	char * fn;

	if (rpmGetVar(var)) return;

	topdir = rpmGetPath("%{_topdir}", NULL);

	fn = alloca(strlen(topdir) + strlen(subdir) + 2);
	strcpy(fn, topdir);
	if (fn[strlen(topdir) - 1] != '/')
	    strcat(fn, "/");
	strcat(fn, subdir);

	rpmSetVar(var, fn);
	if (topdir)	xfree(topdir);
    }

    if (macroname != NULL) {
#define	_TOPDIRMACRO	"%{_topdir}/"
	char *body = alloca(sizeof(_TOPDIRMACRO) + strlen(subdir));
	strcpy(body, _TOPDIRMACRO);
	strcat(body, subdir);
	addMacro(NULL, macroname, NULL, body, RMIL_DEFAULT);
#undef _TOPDIRMACRO
    }
}

static const char *prescriptenviron = "\n\
RPM_SOURCE_DIR=\"%{_sourcedir}\"\n\
RPM_BUILD_DIR=\"%{_builddir}\"\n\
RPM_OPT_FLAGS=\"%{optflags}\"\n\
RPM_ARCH=\"%{_arch}\"\n\
RPM_OS=\"%{_os}\"\n\
export RPM_SOURCE_DIR RPM_BUILD_DIR RPM_OPT_FLAGS RPM_ARCH RPM_OS\n\
RPM_DOC_DIR=\"%{_docdir}\"\n\
export RPM_DOC_DIR\n\
RPM_PACKAGE_NAME=\"%{name}\"\n\
RPM_PACKAGE_VERSION=\"%{version}\"\n\
RPM_PACKAGE_RELEASE=\"%{release}\"\n\
export RPM_PACKAGE_NAME RPM_PACKAGE_VERSION RPM_PACKAGE_RELEASE\n\
%{?buildroot:RPM_BUILD_ROOT=\"%{buildroot}\"\n\
export RPM_BUILD_ROOT\n}\
";

static void setDefaults(void) {

    addMacro(NULL, "_usr", NULL, "/usr", RMIL_DEFAULT);
    addMacro(NULL, "_var", NULL, "/var", RMIL_DEFAULT);

    addMacro(NULL, "_preScriptEnvironment",NULL, prescriptenviron,RMIL_DEFAULT);

    setVarDefault(-1,			"_topdir",
		"/usr/src/redhat",	"%{_usr}/src/redhat");
    setVarDefault(-1,			"_tmppath",
		"/var/tmp",		"%{_var}/tmp");
    setVarDefault(-1,			"_dbpath",
		"/var/lib/rpm",		"%{_var}/lib/rpm");
    setVarDefault(-1,			"_defaultdocdir",
		"/usr/doc",		"%{_usr}/doc");

    setVarDefault(-1,			"_rpmfilename",
	"%%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm",NULL);

    setVarDefault(RPMVAR_OPTFLAGS,	"optflags",
		"-O2",			NULL);
    setVarDefault(-1,			"sigtype",
		"none",			NULL);
    setVarDefault(-1,			"_buildshell",
		"/bin/sh",		NULL);

    setPathDefault(-1,			"_builddir",	"BUILD");
    setPathDefault(-1,			"_rpmdir",	"RPMS");
    setPathDefault(-1,			"_srcrpmdir",	"SRPMS");
    setPathDefault(-1,			"_sourcedir",	"SOURCES");
    setPathDefault(-1,			"_specdir",	"SPECS");

}

int rpmReadRC(const char * rcfiles)
{
    char *myrcfiles, *r, *re;
    int rc;
    static int first = 1;

    if (first) {
	setDefaults();
	first = 0;
    }

    if (rcfiles == NULL)
	rcfiles = defrcfiles;

    /* Read each file in rcfiles. */
    rc = 0;
    for (r = myrcfiles = xstrdup(rcfiles); *r != '\0'; r = re) {
	char fn[4096];
	FD_t fd;

	/* Get pointer to rest of files */
	if ((re = strchr(r, ':')) != NULL)
	    *re++ = '\0';
	else
	    re = r + strlen(r);

	/* Expand ~/ to $HOME/ */
	fn[0] = '\0';
	if (r[0] == '~' && r[1] == '/') {
	    char *home = getenv("HOME");
	    if (home == NULL) {
		rpmError(RPMERR_RPMRC, _("Cannot expand %s"), r);
		rc = 1;
		break;
	    }
	    strcpy(fn, home);
	    r++;
	}
	strcat(fn, r);

	/* Read another rcfile */
	fd = fdOpen(fn, O_RDONLY, 0);
	if (fdFileno(fd) < 0) {
	    /* XXX Only /usr/lib/rpm/rpmrc must exist in default rcfiles list */
	    if (rcfiles == defrcfiles && myrcfiles != r)
		continue;
	    rpmError(RPMERR_RPMRC, _("Unable to open %s for reading: %s."),
		 fn, strerror(errno));
	    rc = 1;
	    break;
	}
	rc = doReadRC(fd, fn);
	fdClose(fd);
 	if (rc) break;
    }
    if (myrcfiles)	free(myrcfiles);
    if (rc)
	return rc;

    rpmSetMachine(NULL, NULL);	/* XXX WTFO? Why bother? */

    if ((r = rpmGetVar(RPMVAR_MACROFILES)) != NULL)
	initMacros(NULL, r);

    return rc;
}

static int doReadRC(FD_t fd, const char * filename)
{
    char *s, *se, *next;
    int linenum = 0;
    struct rpmOption searchOption, * option;
    int rc;

  { struct stat sb;
    fstat(fdFileno(fd), &sb);
    next = alloca(sb.st_size + 2);
    if (fdRead(fd, next, sb.st_size) != sb.st_size) {
	rpmError(RPMERR_RPMRC, _("Failed to read %s: %s."), filename,
		 strerror(errno));
	return 1;
    }
    next[sb.st_size] = '\n';
    next[sb.st_size + 1] = '\0';
  }

    while (*next) {
	linenum++;

	se = s = next;
	while (*se && *se != '\n') se++;
	if (*se) *se++ = '\0';
	next = se;

	while (*s && isspace(*s)) s++;

	/* we used to allow comments to begin anywhere, but not anymore */
	if (*s == '#' || *s == '\0') continue;

	se = s;
	while (*se && !isspace(*se) && *se != ':') se++;

	if (isspace(*se)) {
	    *se++ = '\0';
	    while (*se && isspace(*se) && *se != ':') se++;
	}

	if (*se != ':') {
	    rpmError(RPMERR_RPMRC, _("missing ':' at %s:%d"),
		     filename, linenum);
	    return 1;
	}
	*se++ = '\0';
	while (*se && isspace(*se)) se++;

	/* Find keyword in table */
	searchOption.name = s;
	option = bsearch(&searchOption, optionTable, optionTableSize,
			 sizeof(struct rpmOption), optionCompare);

	if (option) {	/* For configuration variables  ... */
	    const char *arch, *val, *fn;

	    arch = val = fn = NULL;
	    if (*se == '\0') {
		rpmError(RPMERR_RPMRC, _("missing argument for %s at %s:%d"),
		      option->name, filename, linenum);
		return 1;
	    }

	    switch (option->var) {
	    case RPMVAR_INCLUDE:
	      {	FD_t fdinc;

		s = se;
		while (*se && !isspace(*se)) se++;
		if (*se) *se++ = '\0';

		rpmRebuildTargetVars(NULL, NULL);

		fn = rpmGetPath(s, NULL);
		if (fn == NULL || *fn == '\0') {
		    rpmError(RPMERR_RPMRC, _("%s expansion failed at %s:%d \"%s\""),
			option->name, filename, linenum, s);
		    if (fn) xfree(fn);
		    return 1;
		}

		fdinc = fdOpen(fn, O_RDONLY, 0);
		if (fdFileno(fdinc) < 0) {
		    rpmError(RPMERR_RPMRC, _("cannot open %s at %s:%d"),
			fn, filename, linenum);
		    rc = 1;
		} else {
		    rc = doReadRC(fdinc, fn);
		    fdClose(fdinc);
		}
		if (fn) xfree(fn);
		if (rc) return rc;
		continue;	/* XXX don't save include value as var/macro */
	      }	break;
	    case RPMVAR_MACROFILES:
		fn = rpmGetPath(se, NULL);
		if (fn == NULL || *fn == '\0') {
		    rpmError(RPMERR_RPMRC, _("%s expansion failed at %s:%d \"%s\""),
			option->name, filename, linenum, fn);
		    if (fn) xfree(fn);
		    return 1;
		}
		se = (char *)fn;
		break;
	    case RPMVAR_PROVIDES:
	      {	char *t;
		s = rpmGetVar(RPMVAR_PROVIDES);
		if (s == NULL) s = "";
		fn = t = xmalloc(strlen(s) + strlen(se) + 2);
		while (*s) *t++ = *s++;
		*t++ = ' ';
		while (*se) *t++ = *se++;
		*t++ = '\0';
		se = (char *)fn;
	      }	break;
	    default:
		break;
	    }

	    if (option->archSpecific) {
		arch = se;
		while (*se && !isspace(*se)) se++;
		if (*se == '\0') {
		    rpmError(RPMERR_RPMRC,
				_("missing architecture for %s at %s:%d"),
			  	option->name, filename, linenum);
		    return 1;
		}
		*se++ = '\0';
		while (*se && isspace(*se)) se++;
		if (*se == '\0') {
		    rpmError(RPMERR_RPMRC,
				_("missing argument for %s at %s:%d"),
			  	option->name, filename, linenum);
		    return 1;
		}
	    }
	
	    val = se;

	    /* Only add macros if appropriate for this arch */
	    if (option->macroize &&
	      (arch == NULL || !strcmp(arch, current[ARCH]))) {
		char *n, *name;
		n = name = xmalloc(strlen(option->name)+2);
		if (option->localize)
		    *n++ = '_';
		strcpy(n, option->name);
		addMacro(NULL, name, NULL, val, RMIL_RPMRC);
		free(name);
	    }
	    rpmSetVarArch(option->var, val, arch);
	    if (fn) xfree(fn);

	} else {	/* For arch/os compatibilty tables ... */
	    int gotit;
	    int i;

	    gotit = 0;

	    for (i = 0; i < RPM_MACHTABLE_COUNT; i++) {
		if (!strncmp(tables[i].key, s, strlen(tables[i].key)))
		    break;
	    }

	    if (i < RPM_MACHTABLE_COUNT) {
		char *rest = s + strlen(tables[i].key);
		if (*rest == '_') rest++;

		if (!strcmp(rest, "compat")) {
		    if (machCompatCacheAdd(se, filename, linenum,
						&tables[i].cache))
			return 1;
		    gotit = 1;
		} else if (tables[i].hasTranslate &&
			   !strcmp(rest, "translate")) {
		    if (addDefault(&tables[i].defaults,
				   &tables[i].defaultsLength,
				   se, filename, linenum))
			return 1;
		    gotit = 1;
		} else if (tables[i].hasCanon &&
			   !strcmp(rest, "canon")) {
		    if (addCanon(&tables[i].canons, &tables[i].canonsLength,
				 se, filename, linenum))
			return 1;
		    gotit = 1;
		}
	    }

	    if (!gotit) {
		rpmError(RPMERR_RPMRC, _("bad option '%s' at %s:%d"),
			    s, filename, linenum);
	    }
	}
    }

    return 0;
}

static void defaultMachine(char ** arch, char ** os) {
    static struct utsname un;
    static int gotDefaults = 0;
    char * chptr;
    struct canonEntry * canon;

    if (!gotDefaults) {
	uname(&un);

#if !defined(__linux__)
#ifdef SNI
        /* USUALLY un.sysname on sinix does start with the word "SINIX"
         * let's be absolutely sure
         */
        sprintf(un.sysname,"SINIX");
#endif
	if (!strcmp(un.sysname, "AIX")) {
	    strcpy(un.machine, __power_pc() ? "ppc" : "rs6000");
            sprintf(un.sysname,"aix%s.%s",un.version,un.release);
	}
        else if (!strcmp(un.sysname, "SunOS")) {
           if (!strncmp(un.release,"4", 1)) /* SunOS 4.x */ {
	      int fd;
              for (fd=0;(un.release[fd] != 0 && (fd < sizeof(un.release)));fd++)
                 if (!isdigit(un.release[fd]) && (un.release[fd] != '.')) {
                    un.release[fd] = 0;
                    break;
                 }
              sprintf(un.sysname,"sunos%s",un.release);
           }

           else /* Solaris 2.x: n.x.x becomes n-3.x.x */
              sprintf(un.sysname,"solaris%1d%s",atoi(un.release)-3,un.release+1+(atoi(un.release)/10));
        }
        else if (!strcmp(un.sysname, "HP-UX"))
           /*make un.sysname look like hpux9.05 for example*/
           sprintf(un.sysname,"hpux%s",strpbrk(un.release,"123456789"));
        else if (!strcmp(un.sysname, "OSF1"))
           /*make un.sysname look like osf3.2 for example*/
           sprintf(un.sysname,"osf%s",strpbrk(un.release,"123456789"));
        else if (!strncmp(un.sysname, "IP", 2))
           un.sysname[2] = '\0';
        else if (!strncmp(un.sysname, "SINIX", 5)) {
           sprintf(un.sysname, "sinix%s",un.release);
           if (!strncmp(un.machine, "RM", 2))
              sprintf(un.machine, "mips");
        }
        else if ((!strncmp(un.machine, "34", 2) || \
                 !strncmp(un.machine, "33", 2)) && \
                 !strncmp(un.release, "4.0", 3)) {
           /* we are on ncr-sysv4 */
	   char *prelid = NULL;
           FD_t fd = fdOpen("/etc/.relid", O_RDONLY, 0700);
           if (fdFileno(fd) > 0) {
              chptr = (char *) xcalloc(1, 256);
              if (chptr != NULL) {
                 int irelid = fdRead(fd, (void *)chptr, 256);
                 fdClose(fd);
                 /* example: "112393 RELEASE 020200 Version 01 OS" */
                 if (irelid > 0) {
                    if ((prelid=strstr(chptr, "RELEASE "))){
                       prelid += strlen("RELEASE ")+1;
                       sprintf(un.sysname,"ncr-sysv4.%.*s",1,prelid);
                    }
		 }
                 free (chptr);
              }
           }
           if (!prelid)
              /* parsing /etc/.relid file failed */
              strcpy(un.sysname,"ncr-sysv4");
           /* wrong, just for now, find out how to look for i586 later*/
           strcpy(un.machine,"i486");
        }
#endif	/* __linux__ */

	/* get rid of the hyphens in the sysname */
	for (chptr = un.machine; *chptr; chptr++)
	    if (*chptr == '/') *chptr = '-';

#	if defined(__MIPSEL__) || defined(__MIPSEL) || defined(_MIPSEL)
	    /* little endian */
	    strcpy(un.machine, "mipsel");
#	elif defined(__MIPSEB__) || defined(__MIPSEB) || defined(_MIPSEB)
	   /* big endian */
		strcpy(un.machine, "mipseb");
#	endif

#	if defined(__hpux) && defined(_SC_CPU_VERSION)
	{
#	    if !defined(CPU_PA_RISC1_2)
#                define CPU_PA_RISC1_2  0x211 /* HP PA-RISC1.2 */
#           endif
#           if !defined(CPU_PA_RISC2_0)
#               define CPU_PA_RISC2_0  0x214 /* HP PA-RISC2.0 */
#           endif
	    int cpu_version = sysconf(_SC_CPU_VERSION);

#	    if defined(CPU_HP_MC68020)
		if (cpu_version == CPU_HP_MC68020)
		    strcpy(un.machine, "m68k");
#	    endif
#	    if defined(CPU_HP_MC68030)
		if (cpu_version == CPU_HP_MC68030)
		    strcpy(un.machine, "m68k");
#	    endif
#	    if defined(CPU_HP_MC68040)
		if (cpu_version == CPU_HP_MC68040)
		    strcpy(un.machine, "m68k");
#	    endif

#	    if defined(CPU_PA_RISC1_0)
		if (cpu_version == CPU_PA_RISC1_0)
		    strcpy(un.machine, "hppa1.0");
#	    endif
#	    if defined(CPU_PA_RISC1_1)
		if (cpu_version == CPU_PA_RISC1_1)
		    strcpy(un.machine, "hppa1.1");
#	    endif
#	    if defined(CPU_PA_RISC1_2)
		if (cpu_version == CPU_PA_RISC1_2)
		    strcpy(un.machine, "hppa1.2");
#	    endif
#	    if defined(CPU_PA_RISC2_0)
		if (cpu_version == CPU_PA_RISC2_0)
		    strcpy(un.machine, "hppa2.0");
#	    endif
	}
#	endif	/* hpux */

#	if defined(__linux__) && defined(__sparc__)
	if (!strcmp(un.machine, "sparc")) {
	    #define PERS_LINUX		0x00000000
	    #define PERS_LINUX_32BIT	0x00800000
	    #define PERS_LINUX32	0x00000008

	    extern int personality(unsigned long);
	    int oldpers;
	    
	    oldpers = personality(PERS_LINUX_32BIT);
	    if (oldpers != -1) {
		if (personality(PERS_LINUX) != -1) {
		    uname(&un);
		    if (! strcmp(un.machine, "sparc64")) {
			strcpy(un.machine, "sparcv9");
			oldpers = PERS_LINUX32;
		    }
		}
		personality(oldpers);
	    }
	}
#	endif	/* sparc*-linux */

	/* the uname() result goes through the arch_canon table */
	canon = lookupInCanonTable(un.machine,
				   tables[RPM_MACHTABLE_INSTARCH].canons,
				   tables[RPM_MACHTABLE_INSTARCH].canonsLength);
	if (canon)
	    strcpy(un.machine, canon->short_name);

	canon = lookupInCanonTable(un.sysname,
				   tables[RPM_MACHTABLE_INSTOS].canons,
				   tables[RPM_MACHTABLE_INSTOS].canonsLength);
	if (canon)
	    strcpy(un.sysname, canon->short_name);
	gotDefaults = 1;
    }

    if (arch) *arch = un.machine;
    if (os) *os = un.sysname;
}

static char * rpmGetVarArch(int var, char * arch) {
    struct rpmvarValue * next;

    if (!arch) arch = current[ARCH];

    if (arch) {
	next = &values[var];
	while (next) {
	    if (next->arch && !strcmp(next->arch, arch)) return next->value;
	    next = next->next;
	}
    }

    next = values + var;
    while (next && next->arch) next = next->next;

    return next ? next->value : NULL;
}

char *rpmGetVar(int var)
{
    return rpmGetVarArch(var, NULL);
}

/* this doesn't free the passed pointer! */
static void freeRpmVar(struct rpmvarValue * orig) {
    struct rpmvarValue * next, * var = orig;

    while (var) {
	next = var->next;
	if (var->arch) {
	    free(var->arch);
	    var->arch = NULL;
	}
	if (var->value) {
	    free(var->value);
	    var->value = NULL;
	}

	if (var != orig) free(var);
	var = next;
    }
}

void rpmSetVar(int var, const char *val) {
    freeRpmVar(&values[var]);
    values[var].value = (val ? xstrdup(val) : NULL);
}

static void rpmSetVarArch(int var, const char * val, const char * arch) {
    struct rpmvarValue * next = values + var;

    if (next->value) {
	if (arch) {
	    while (next->next) {
		if (next->arch && !strcmp(next->arch, arch)) break;
		next = next->next;
	    }
	} else {
	    while (next->next) {
		if (!next->arch) break;
		next = next->next;
	    }
	}

	if (next->arch && arch && !strcmp(next->arch, arch)) {
	    if (next->value) free(next->value);
	    if (next->arch) free(next->arch);
	} else if (next->arch || arch) {
	    next->next = xmalloc(sizeof(*next->next));
	    next = next->next;
	    next->next = NULL;
	}
    }

    next->value = xstrdup(val);
    next->arch = (arch ? xstrdup(arch) : NULL);
}

void rpmSetTables(int archTable, int osTable) {
    char * arch, * os;

    defaultMachine(&arch, &os);

    if (currTables[ARCH] != archTable) {
	currTables[ARCH] = archTable;
	rebuildCompatTables(ARCH, arch);
    }

    if (currTables[OS] != osTable) {
	currTables[OS] = osTable;
	rebuildCompatTables(OS, os);
    }
}

int rpmMachineScore(int type, char * name) {
    struct machEquivInfo * info = machEquivSearch(&tables[type].equiv, name);
    return (info != NULL ? info->score : 0);
}

void rpmGetMachine(char **arch, char **os)
{
    if (arch)
	*arch = current[ARCH];

    if (os)
	*os = current[OS];
}

void rpmSetMachine(const char * arch, const char * os) {
    char * host_cpu, * host_os;

    defaultMachine(&host_cpu, &host_os);

    if (arch == NULL) {
	arch = host_cpu;
	if (tables[currTables[ARCH]].hasTranslate)
	    arch = lookupInDefaultTable(arch,
			    tables[currTables[ARCH]].defaults,
			    tables[currTables[ARCH]].defaultsLength);
    }

    if (os == NULL) {
	os = host_os;
	if (tables[currTables[OS]].hasTranslate)
	    os = lookupInDefaultTable(os,
			    tables[currTables[OS]].defaults,
			    tables[currTables[OS]].defaultsLength);
    }

    if (!current[ARCH] || strcmp(arch, current[ARCH])) {
	if (current[ARCH]) free(current[ARCH]);
	current[ARCH] = xstrdup(arch);
	rebuildCompatTables(ARCH, host_cpu);
    }

    if (!current[OS] || strcmp(os, current[OS])) {
	if (current[OS]) free(current[OS]);
	current[OS] = xstrdup(os);
	/*
	 * XXX Capitalizing the 'L' is needed to insure that old
	 * XXX os-from-uname (e.g. "Linux") is compatible with the new
	 * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
	 * XXX A copy of this string is embedded in headers and is
	 * XXX used by rpmInstallPackage->{os,arch}Okay->rpmMachineScore->
	 * XXX to verify correct arch/os from headers.
	 */
	if (!strcmp(current[OS], "linux"))
	    *current[OS]= 'L';
	rebuildCompatTables(OS, host_os);
    }
}

static void rebuildCompatTables(int type, char * name) {
    machFindEquivs(&tables[currTables[type]].cache,
		   &tables[currTables[type]].equiv,
		   name);
}

static void getMachineInfo(int type, /*@out@*/char ** name, /*@out@*/int * num) {
    struct canonEntry * canon;
    int which = currTables[type];

    /* use the normal canon tables, even if we're looking up build stuff */
    if (which >= 2) which -= 2;

    canon = lookupInCanonTable(current[type],
			       tables[which].canons,
			       tables[which].canonsLength);

    if (canon) {
	if (num) *num = canon->num;
	if (name) *name = canon->short_name;
    } else {
	if (num) *num = 255;
	if (name) *name = current[type];

	if (tables[currTables[type]].hasCanon) {
	    rpmMessage(RPMMESS_WARNING, _("Unknown system: %s\n"), current[type]);
	    rpmMessage(RPMMESS_WARNING, _("Please contact rpm-list@redhat.com\n"));
	}
    }
}

void rpmGetArchInfo(char ** name, int * num) {
    getMachineInfo(ARCH, name, num);
}

void rpmGetOsInfo(char ** name, int * num) {
    getMachineInfo(OS, name, num);
}

void rpmRebuildTargetVars(const char **buildtarget, const char ** canontarget)
{

    char *ca = NULL, *co = NULL, *ct = NULL;
    int x;

    /* Rebuild the compat table to recalculate the current target arch.  */

    rpmSetMachine(NULL, NULL);
    rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
    rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);

    if (buildtarget && *buildtarget) {
	char *c;
	/* Set arch and os from specified build target */
	ca = xstrdup(*buildtarget);
	if ((c = strchr(ca, '-')) != NULL) {
	    *c++ = '\0';
	    
	    if ((co = strrchr(c, '-')) == NULL) {
		co = c;
	    } else {
		if (!strcasecmp(co, "-gnu"))
		    *co = '\0';
		if ((co = strrchr(c, '-')) == NULL)
		    co = c;
		else
		    co++;
	    }
	    if (co != NULL) co = xstrdup(co);
	}
    } else {
	/* Set build target from rpm arch and os */
	rpmGetArchInfo(&ca,NULL);
	if (ca)	ca = xstrdup(ca);
	rpmGetOsInfo(&co,NULL);
	if (co)	co = xstrdup(co);
    }

    /* If still not set, Set target arch/os from default uname(2) values */
    if (ca == NULL) {
	defaultMachine(&ca, NULL);
	ca = xstrdup(ca);
     }
    for (x = 0; ca[x]; x++)
	ca[x] = tolower(ca[x]);

    if (co == NULL) {
	defaultMachine(NULL, &co);
	co = xstrdup(co);
    }
    for (x = 0; co[x]; x++)
	co[x] = tolower(co[x]);

    /* XXX For now, set canonical target to arch-os */
    if (ct == NULL) {
	ct = xmalloc(strlen(ca) + sizeof("-") + strlen(co));
	sprintf(ct, "%s-%s", ca, co);
    }

/*
 * XXX All this macro pokery/jiggery could be achieved by doing a delayed
 *	initMacros(NULL, PER-PLATFORM-MACRO-FILE-NAMES);
 */
    delMacro(NULL, "_target");
    addMacro(NULL, "_target", NULL, ct, RMIL_RPMRC);
    delMacro(NULL, "_target_cpu");
    addMacro(NULL, "_target_cpu", NULL, ca, RMIL_RPMRC);
    delMacro(NULL, "_target_os");
    addMacro(NULL, "_target_os", NULL, co, RMIL_RPMRC);
/*
 * XXX Make sure that per-arch optflags is initialized correctly.
 */
  { char *optflags = rpmGetVarArch(RPMVAR_OPTFLAGS, ca);
    if (optflags != NULL) {
	delMacro(NULL, "optflags");
	addMacro(NULL, "optflags", NULL, optflags, RMIL_RPMRC);
    }
  }

    if (canontarget)
	*canontarget = ct;
    else
	free(ct);
    free(ca);
    free(co);
}

void rpmFreeRpmrc(void)
{
    int i, j, k;

    for (i = 0; i < RPM_MACHTABLE_COUNT; i++) {
	struct tableType *t;
	t = tables + i;
	if (t->equiv.list) {
	    for (j = 0; j < t->equiv.count; j++) {
		if (t->equiv.list[j].name)	xfree(t->equiv.list[j].name);
	    }
	    xfree(t->equiv.list);
	}
	if (t->cache.cache) {
	    for (j = 0; j < t->cache.size; j++) {
		struct machCacheEntry *e;
		e = t->cache.cache + j;
		if (e == NULL)	continue;
		if (e->name)		xfree(e->name);
		if (e->equivs) {
		    for (k = 0; k < e->count; k++) {
		    if (e->equivs[k])	xfree(e->equivs[k]);
		    }
		    xfree(e->equivs);
		}
	    }
	    xfree(t->cache.cache);
	}
	if (t->defaults) {
	    for (j = 0; j < t->defaultsLength; j++) {
		if (t->defaults[j].name)	xfree(t->defaults[j].name);
		if (t->defaults[j].defName)	xfree(t->defaults[j].defName);
	    }
	    xfree(t->defaults);
	}
	if (t->canons) {
	    for (j = 0; j < t->canonsLength; j++) {
		if (t->canons[j].name)		xfree(t->canons[j].name);
		if (t->canons[j].short_name)	xfree(t->canons[j].short_name);
	    }
	    xfree(t->canons);
	}
    }

    for (i = 0; i < RPMVAR_NUM; i++) {
	struct rpmvarValue *this;
	while ((this = values[i].next) != NULL) {
	    values[i].next = this->next;
	    if (this->value)	xfree(this->value);
	    if (this->arch)	xfree(this->arch);
	    xfree(this);
	}
	if (values[i].value)	xfree(values[i].value);
	if (values[i].arch)	xfree(values[i].arch);
    }
    if (current[OS])	xfree(current[OS]);
    if (current[ARCH])	xfree(current[ARCH]);
    return;
}

int rpmShowRC(FILE *f)
{
    struct rpmOption *opt;
    int i;
    struct machEquivTable * equivTable;

    /* the caller may set the build arch which should be printed here */
    fprintf(f, "ARCHITECTURE AND OS:\n");
    fprintf(f, "build arch            : %s\n", current[ARCH]);

    fprintf(f, "compatible build archs:");
    equivTable = &tables[RPM_MACHTABLE_BUILDARCH].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(f," %s", equivTable->list[i].name);
    fprintf(f, "\n");

    fprintf(f, "build os              : %s\n", current[OS]);

    fprintf(f, "compatible build os's :");
    equivTable = &tables[RPM_MACHTABLE_BUILDOS].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(f," %s", equivTable->list[i].name);
    fprintf(f, "\n");

    rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
    rpmSetMachine(NULL, NULL);	/* XXX WTFO? Why bother? */

    fprintf(f, "install arch          : %s\n", current[ARCH]);
    fprintf(f, "install os            : %s\n", current[OS]);

    fprintf(f, "compatible archs      :");
    equivTable = &tables[RPM_MACHTABLE_INSTARCH].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(f," %s", equivTable->list[i].name);
    fprintf(f, "\n");

    fprintf(f, "compatible os's       :");
    equivTable = &tables[RPM_MACHTABLE_INSTOS].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(f," %s", equivTable->list[i].name);
    fprintf(f, "\n");

    fprintf(f, "\nRPMRC VALUES:\n");
    for (i = 0, opt = optionTable; i < optionTableSize; i++, opt++) {
	char *s = rpmGetVar(opt->var);
	if (s != NULL || rpmGetVerbosity() < RPMMESS_NORMAL)
	    fprintf(f, "%-21s : %s\n", opt->name, s ? s : "(not set)");
    }

    dumpMacroTable(NULL, f);

    return 0;
}
