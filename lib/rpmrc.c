/*@-mods@*/
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
#include "debug.h"

/*@access FD_t@*/		/* compared with NULL */

/*@observer@*/ /*@unchecked@*/
static const char *defrcfiles = LIBRPMRC_FILENAME ":/etc/rpmrc:~/.rpmrc";

/*@observer@*/ /*@checked@*/
const char * macrofiles = MACROFILES;

typedef /*@owned@*/ const char * cptr_t;

typedef struct machCacheEntry_s {
    const char * name;
    int count;
    cptr_t * equivs;
    int visited;
} * machCacheEntry;

typedef struct machCache_s {
    machCacheEntry cache;
    int size;
} * machCache;

typedef struct machEquivInfo_s {
    const char * name;
    int score;
} * machEquivInfo;

typedef struct machEquivTable_s {
    int count;
    machEquivInfo list;
} * machEquivTable;

struct rpmvarValue {
    const char * value;
    /* eventually, this arch will be replaced with a generic condition */
    const char * arch;
/*@only@*/ /*@null@*/ struct rpmvarValue * next;
};

struct rpmOption {
    const char * name;
    int var;
    int archSpecific;
/*@unused@*/ int required;
    int macroize;
    int localize;
/*@unused@*/ struct rpmOptionValue * value;
};

typedef struct defaultEntry_s {
/*@owned@*/ /*@null@*/ const char * name;
/*@owned@*/ /*@null@*/ const char * defName;
} * defaultEntry;

typedef struct canonEntry_s {
/*@owned@*/ const char * name;
/*@owned@*/ const char * short_name;
    short num;
} * canonEntry;

/* tags are 'key'canon, 'key'translate, 'key'compat
 *
 * for giggles, 'key'_canon, 'key'_compat, and 'key'_canon will also work
 */
typedef struct tableType_s {
/*@observer@*/ const char * const key;
    const int hasCanon;
    const int hasTranslate;
    struct machEquivTable_s equiv;
    struct machCache_s cache;
    defaultEntry defaults;
    canonEntry canons;
    int defaultsLength;
    int canonsLength;
} * tableType;

/*@-fullinitblock@*/
/*@unchecked@*/
static struct tableType_s tables[RPM_MACHTABLE_COUNT] = {
    { "arch", 1, 0 },
    { "os", 1, 0 },
    { "buildarch", 0, 1 },
    { "buildos", 0, 1 }
};

/* this *must* be kept in alphabetical order */
/* The order of the flags is archSpecific, required, macroize, localize */

/*@unchecked@*/
static struct rpmOption optionTable[] = {
    { "include",		RPMVAR_INCLUDE,			0, 1,	0, 2 },
    { "macrofiles",		RPMVAR_MACROFILES,		0, 0,	0, 1 },
    { "optflags",		RPMVAR_OPTFLAGS,		1, 0,	1, 0 },
    { "provides",               RPMVAR_PROVIDES,                0, 0,	0, 0 },
};
/*@=fullinitblock@*/

/*@unchecked@*/
static int optionTableSize = sizeof(optionTable) / sizeof(*optionTable);

#define OS	0
#define ARCH	1

/*@unchecked@*/
static cptr_t current[2];

/*@unchecked@*/
static int currTables[2] = { RPM_MACHTABLE_INSTOS, RPM_MACHTABLE_INSTARCH };

/*@unchecked@*/
static struct rpmvarValue values[RPMVAR_NUM];

/*@unchecked@*/
static int defaultsInitialized = 0;

/* prototypes */
static int doReadRC( /*@killref@*/ FD_t fd, const char * urlfn)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies fd, fileSystem, internalState @*/;

static void rpmSetVarArch(int var, const char * val,
		/*@null@*/ const char * arch)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

static void rebuildCompatTables(int type, const char * name)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

static void rpmRebuildTargetVars(/*@null@*/ const char **target, /*@null@*/ const char ** canontarget)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *canontarget, fileSystem, internalState @*/;

static int optionCompare(const void * a, const void * b)
	/*@*/
{
    return xstrcasecmp(((struct rpmOption *) a)->name,
		      ((struct rpmOption *) b)->name);
}

static /*@observer@*/ /*@null@*/ machCacheEntry
machCacheFindEntry(const machCache cache, const char * key)
	/*@*/
{
    int i;

    for (i = 0; i < cache->size; i++)
	if (!strcmp(cache->cache[i].name, key)) return cache->cache + i;

    return NULL;
}

static int machCompatCacheAdd(char * name, const char * fn, int linenum,
				machCache cache)
	/*@globals internalState @*/
	/*@modifies *name, cache->cache, cache->size, internalState @*/
{
    machCacheEntry entry = NULL;
    char * chptr;
    char * equivs;
    int delEntry = 0;
    int i;

    while (*name && xisspace(*name)) name++;

    chptr = name;
    while (*chptr && *chptr != ':') chptr++;
    if (!*chptr) {
	rpmError(RPMERR_RPMRC, _("missing second ':' at %s:%d\n"), fn, linenum);
	return 1;
    } else if (chptr == name) {
	rpmError(RPMERR_RPMRC, _("missing architecture name at %s:%d\n"), fn,
			     linenum);
	return 1;
    }

    while (*chptr == ':' || xisspace(*chptr)) chptr--;
    *(++chptr) = '\0';
    equivs = chptr + 1;
    while (*equivs && xisspace(*equivs)) equivs++;
    if (!*equivs) {
	delEntry = 1;
    }

    if (cache->size) {
	entry = machCacheFindEntry(cache, name);
	if (entry) {
	    for (i = 0; i < entry->count; i++)
		entry->equivs[i] = _free(entry->equivs[i]);
	    entry->equivs = _free(entry->equivs);
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

static /*@observer@*/ /*@null@*/ machEquivInfo
machEquivSearch(const machEquivTable table, const char * name)
	/*@*/
{
    int i;

    for (i = 0; i < table->count; i++)
	if (!xstrcasecmp(table->list[i].name, name))
	    return table->list + i;

    return NULL;
}

static void machAddEquiv(machEquivTable table, const char * name,
			   int distance)
	/*@modifies table->list, table->count @*/
{
    machEquivInfo equiv;

    equiv = machEquivSearch(table, name);
    if (!equiv) {
	if (table->count)
	    table->list = xrealloc(table->list, (table->count + 1)
				    * sizeof(*table->list));
	else
	    table->list = xmalloc(sizeof(*table->list));

	table->list[table->count].name = xstrdup(name);
	table->list[table->count++].score = distance;
    }
}

static void machCacheEntryVisit(machCache cache,
		machEquivTable table, const char * name, int distance)
	/*@modifies table->list, table->count @*/
{
    machCacheEntry entry;
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

static void machFindEquivs(machCache cache, machEquivTable table,
		const char * key)
	/*@modifies cache->cache, table->list, table->count @*/
{
    int i;

    for (i = 0; i < cache->size; i++)
	cache->cache[i].visited = 0;

    while (table->count > 0) {
	--table->count;
	table->list[table->count].name = _free(table->list[table->count].name);
    }
    table->count = 0;
    table->list = _free(table->list);

    /*
     *	We have a general graph built using strings instead of pointers.
     *	Yuck. We have to start at a point at traverse it, remembering how
     *	far away everything is.
     */
    /*@-nullstate@*/	/* FIX: table->list may be NULL. */
    machAddEquiv(table, key, 1);
    machCacheEntryVisit(cache, table, key, 2);
    return;
    /*@=nullstate@*/
}

static int addCanon(canonEntry * table, int * tableLen, char * line,
		    const char * fn, int lineNum)
	/*@globals internalState @*/
	/*@modifies *table, *tableLen, *line, internalState @*/
{
    canonEntry t;
    char *s, *s1;
    const char * tname;
    const char * tshort_name;
    int tnum;

    (*tableLen) += 2;
    /*@-unqualifiedtrans@*/
    *table = xrealloc(*table, sizeof(**table) * (*tableLen));
    /*@=unqualifiedtrans@*/

    t = & ((*table)[*tableLen - 2]);

    tname = strtok(line, ": \t");
    tshort_name = strtok(NULL, " \t");
    s = strtok(NULL, " \t");
    if (! (tname && tshort_name && s)) {
	rpmError(RPMERR_RPMRC, _("Incomplete data line at %s:%d\n"),
		fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	rpmError(RPMERR_RPMRC, _("Too many args in data line at %s:%d\n"),
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    /*@-nullpass@*/	/* LCL: s != NULL here. */
    tnum = strtoul(s, &s1, 10);
    if ((*s1) || (s1 == s) || (tnum == ULONG_MAX)) {
	rpmError(RPMERR_RPMRC, _("Bad arch/os number: %s (%s:%d)\n"), s,
	      fn, lineNum);
	return(RPMERR_RPMRC);
    }
    /*@=nullpass@*/

    t[0].name = xstrdup(tname);
    t[0].short_name = (tshort_name ? xstrdup(tshort_name) : xstrdup(""));
    t[0].num = tnum;

    /* From A B C entry */
    /* Add  B B C entry */
    t[1].name = (tshort_name ? xstrdup(tshort_name) : xstrdup(""));
    t[1].short_name = (tshort_name ? xstrdup(tshort_name) : xstrdup(""));
    t[1].num = tnum;

    return 0;
}

static int addDefault(defaultEntry * table, int * tableLen, char * line,
			const char * fn, int lineNum)
	/*@globals internalState @*/
	/*@modifies *table, *tableLen, *line, internalState @*/
{
    defaultEntry t;

    (*tableLen)++;
    /*@-unqualifiedtrans@*/
    *table = xrealloc(*table, sizeof(**table) * (*tableLen));
    /*@=unqualifiedtrans@*/

    t = & ((*table)[*tableLen - 1]);

    /*@-temptrans@*/
    t->name = strtok(line, ": \t");
    t->defName = strtok(NULL, " \t");
    if (! (t->name && t->defName)) {
	rpmError(RPMERR_RPMRC, _("Incomplete default line at %s:%d\n"),
		 fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	rpmError(RPMERR_RPMRC, _("Too many args in default line at %s:%d\n"),
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->name = xstrdup(t->name);
    t->defName = (t->defName ? xstrdup(t->defName) : NULL);
    /*@=temptrans@*/

    return 0;
}

static /*@null@*/ const canonEntry lookupInCanonTable(const char * name,
		const canonEntry table, int tableLen)
	/*@*/
{
    while (tableLen) {
	tableLen--;
	if (strcmp(name, table[tableLen].name))
	    continue;
	/*@-immediatetrans -retalias@*/
	return &(table[tableLen]);
	/*@=immediatetrans =retalias@*/
    }

    return NULL;
}

static /*@observer@*/ /*@null@*/
const char * lookupInDefaultTable(const char * name,
		const defaultEntry table, int tableLen)
	/*@*/
{
    while (tableLen) {
	tableLen--;
	if (table[tableLen].name && !strcmp(name, table[tableLen].name))
	    return table[tableLen].defName;
    }

    return name;
}

static void setVarDefault(int var, const char * macroname, const char * val,
		/*@null@*/ const char * body)
	/*@globals rpmGlobalMacroContext,
		internalState @*/
	/*@modifies internalState @*/
{
    if (var >= 0) {	/* XXX Dying ... */
	if (rpmGetVar(var)) return;
	rpmSetVar(var, val);
    }
    if (body == NULL)
	body = val;
    addMacro(NULL, macroname, NULL, body, RMIL_DEFAULT);
}

static void setPathDefault(int var, const char * macroname, const char * subdir)
	/*@globals rpmGlobalMacroContext,
		internalState @*/
	/*@modifies internalState @*/
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
	topdir = _free(topdir);
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

/*@observer@*/ /*@unchecked@*/
static const char * prescriptenviron = "\n\
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

static void setDefaults(void)
	/*@globals rpmGlobalMacroContext,
		internalState @*/
	/*@modifies internalState @*/
{

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

/*@-usedef@*/	/*@ FIX: se usage inconsistent, W2DO? */
static int doReadRC( /*@killref@*/ FD_t fd, const char * urlfn)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies fd, fileSystem, internalState @*/
{
    const char *s;
    char *se, *next;
    int linenum = 0;
    struct rpmOption searchOption, * option;
    int rc;

    /* XXX really need rc = Slurp(fd, const char * filename, char ** buf) */
  { off_t size = fdSize(fd);
    size_t nb = (size >= 0 ? size : (8*BUFSIZ - 2));
    if (nb == 0) {
	(void) Fclose(fd);
	return 0;
    }
    next = alloca(nb + 2);
    next[0] = '\0';
    rc = Fread(next, sizeof(*next), nb, fd);
    if (Ferror(fd) || (size > 0 && rc != nb)) {	/* XXX Feof(fd) */
	rpmError(RPMERR_RPMRC, _("Failed to read %s: %s.\n"), urlfn,
		 Fstrerror(fd));
	rc = 1;
    } else
	rc = 0;
    (void) Fclose(fd);
    if (rc) return rc;
    next[nb] = '\n';
    next[nb + 1] = '\0';
  }

    /*@-branchstate@*/
    while (*next != '\0') {
	linenum++;

	s = se = next;

	/* Find end-of-line. */
	while (*se && *se != '\n') se++;
	if (*se != '\0') *se++ = '\0';
	next = se;

	/* Trim leading spaces */
	while (*s && xisspace(*s)) s++;

	/* We used to allow comments to begin anywhere, but not anymore. */
	if (*s == '#' || *s == '\0') continue;

	/* Find end-of-keyword. */
	se = (char *)s;
	while (*se && !xisspace(*se) && *se != ':') se++;

	if (xisspace(*se)) {
	    *se++ = '\0';
	    while (*se && xisspace(*se) && *se != ':') se++;
	}

	if (*se != ':') {
	    rpmError(RPMERR_RPMRC, _("missing ':' (found 0x%02x) at %s:%d\n"),
		     (unsigned)(0xff & *se), urlfn, linenum);
	    return 1;
	}
	*se++ = '\0';	/* terminate keyword or option, point to value */
	while (*se && xisspace(*se)) se++;

	/* Find keyword in table */
	searchOption.name = s;
	option = bsearch(&searchOption, optionTable, optionTableSize,
			 sizeof(optionTable[0]), optionCompare);

	if (option) {	/* For configuration variables  ... */
	    const char *arch, *val, *fn;

	    arch = val = fn = NULL;
	    if (*se == '\0') {
		rpmError(RPMERR_RPMRC, _("missing argument for %s at %s:%d\n"),
		      option->name, urlfn, linenum);
		return 1;
	    }

	    switch (option->var) {
	    case RPMVAR_INCLUDE:
	      {	FD_t fdinc;

		s = se;
		while (*se && !xisspace(*se)) se++;
		if (*se != '\0') *se++ = '\0';

		rpmRebuildTargetVars(NULL, NULL);

		fn = rpmGetPath(s, NULL);
		if (fn == NULL || *fn == '\0') {
		    rpmError(RPMERR_RPMRC, _("%s expansion failed at %s:%d \"%s\"\n"),
			option->name, urlfn, linenum, s);
		    fn = _free(fn);
		    return 1;
		    /*@notreached@*/
		}

		fdinc = Fopen(fn, "r.fpio");
		if (fdinc == NULL || Ferror(fdinc)) {
		    rpmError(RPMERR_RPMRC, _("cannot open %s at %s:%d: %s\n"),
			fn, urlfn, linenum, Fstrerror(fdinc));
		    rc = 1;
		} else {
		    rc = doReadRC(fdinc, fn);
		}
		fn = _free(fn);
		if (rc) return rc;
		continue;	/* XXX don't save include value as var/macro */
	      }	/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMVAR_MACROFILES:
		fn = rpmGetPath(se, NULL);
		if (fn == NULL || *fn == '\0') {
		    rpmError(RPMERR_RPMRC, _("%s expansion failed at %s:%d \"%s\"\n"),
			option->name, urlfn, linenum, fn);
		    fn = _free(fn);
		    return 1;
		}
		se = (char *)fn;
		/*@switchbreak@*/ break;
	    case RPMVAR_PROVIDES:
	      {	char *t;
		s = rpmGetVar(RPMVAR_PROVIDES);
		if (s == NULL) s = "";
		fn = t = xmalloc(strlen(s) + strlen(se) + 2);
		while (*s != '\0') *t++ = *s++;
		*t++ = ' ';
		while (*se != '\0') *t++ = *se++;
		*t++ = '\0';
		se = (char *)fn;
	      }	/*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }

	    if (option->archSpecific) {
		arch = se;
		while (*se && !xisspace(*se)) se++;
		if (*se == '\0') {
		    rpmError(RPMERR_RPMRC,
				_("missing architecture for %s at %s:%d\n"),
			  	option->name, urlfn, linenum);
		    return 1;
		}
		*se++ = '\0';
		while (*se && xisspace(*se)) se++;
		if (*se == '\0') {
		    rpmError(RPMERR_RPMRC,
				_("missing argument for %s at %s:%d\n"),
			  	option->name, urlfn, linenum);
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
	    fn = _free(fn);

	} else {	/* For arch/os compatibilty tables ... */
	    int gotit;
	    int i;

	    gotit = 0;

	    for (i = 0; i < RPM_MACHTABLE_COUNT; i++) {
		if (!strncmp(tables[i].key, s, strlen(tables[i].key)))
		    /*@innerbreak@*/ break;
	    }

	    if (i < RPM_MACHTABLE_COUNT) {
		const char *rest = s + strlen(tables[i].key);
		if (*rest == '_') rest++;

		if (!strcmp(rest, "compat")) {
		    if (machCompatCacheAdd(se, urlfn, linenum,
						&tables[i].cache))
			return 1;
		    gotit = 1;
		} else if (tables[i].hasTranslate &&
			   !strcmp(rest, "translate")) {
		    if (addDefault(&tables[i].defaults,
				   &tables[i].defaultsLength,
				   se, urlfn, linenum))
			return 1;
		    gotit = 1;
		} else if (tables[i].hasCanon &&
			   !strcmp(rest, "canon")) {
		    if (addCanon(&tables[i].canons, &tables[i].canonsLength,
				 se, urlfn, linenum))
			return 1;
		    gotit = 1;
		}
	    }

	    if (!gotit) {
		rpmError(RPMERR_RPMRC, _("bad option '%s' at %s:%d\n"),
			    s, urlfn, linenum);
	    }
	}
    }
    /*@=branchstate@*/

    return 0;
}
/*@=usedef@*/

#	if defined(__linux__) && defined(__i386__)
#include <setjmp.h>
#include <signal.h>

/*
 * Generic CPUID function
 */
static inline void cpuid(int op, int *eax, int *ebx, int *ecx, int *edx)
	/*@modifies *eax, *ebx, *ecx, *edx @*/
{
#ifdef	__LCLINT__
    *eax = *ebx = *ecx = *edx = 0;
#endif
#ifdef PIC
	__asm__("pushl %%ebx; cpuid; movl %%ebx,%1; popl %%ebx"
		: "=a"(*eax), "=g"(*ebx), "=&c"(*ecx), "=&d"(*edx)
		: "a" (op));
#else
	__asm__("cpuid"
		: "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
		: "a" (op));
#endif

}

/*
 * CPUID functions returning a single datum
 */
static inline unsigned int cpuid_eax(unsigned int op)
	/*@*/
{
	unsigned int val;

#ifdef PIC
	__asm__("pushl %%ebx; cpuid; popl %%ebx"
		: "=a" (val) : "a" (op) : "ecx", "edx");
#else
	__asm__("cpuid"
		: "=a" (val) : "a" (op) : "ebx", "ecx", "edx");
#endif
	return val;
}

static inline unsigned int cpuid_ebx(unsigned int op)
	/*@*/
{
	unsigned int tmp, val;

#ifdef PIC
	__asm__("pushl %%ebx; cpuid; movl %%ebx,%1; popl %%ebx"
		: "=a" (tmp), "=g" (val) : "a" (op) : "ecx", "edx");
#else
	__asm__("cpuid"
		: "=a" (tmp), "=b" (val) : "a" (op) : "ecx", "edx");
#endif
	return val;
}

static inline unsigned int cpuid_ecx(unsigned int op)
	/*@*/
{
	unsigned int tmp, val;
#ifdef PIC
	__asm__("pushl %%ebx; cpuid; popl %%ebx"
		: "=a" (tmp), "=c" (val) : "a" (op) : "edx");
#else
	__asm__("cpuid"
		: "=a" (tmp), "=c" (val) : "a" (op) : "ebx", "edx");
#endif
	return val;

}

static inline unsigned int cpuid_edx(unsigned int op)
	/*@*/
{
	unsigned int tmp, val;
#ifdef PIC
	__asm__("pushl %%ebx; cpuid; popl %%ebx"
		: "=a" (tmp), "=d" (val) : "a" (op) : "ecx");
#else
	__asm__("cpuid"
		: "=a" (tmp), "=d" (val) : "a" (op) : "ebx", "ecx");
#endif
	return val;

}

/*@unchecked@*/
static sigjmp_buf jenv;

static inline void model3(int _unused)
	/*@globals internalState @*/
	/*@modifies internalState @*/
{
	siglongjmp(jenv, 1);
}

static inline int RPMClass(void)
	/*@globals internalState @*/
	/*@modifies internalState @*/
{
	int cpu;
	unsigned int tfms, junk, cap;
	
	signal(SIGILL, model3);
	
	if(sigsetjmp(jenv, 1))
		return 3;
		
	if(cpuid_eax(0x000000000)==0)
		return 4;
	cpuid(0x000000001, &tfms, &junk, &junk, &cap);
	
	cpu = (tfms>>8)&15;
	
	if(cpu < 6)
		return cpu;
		
	if(cap & (1<<15))
		return 6;
		
	return 5;
}

/* should only be called for model 6 CPU's */
static int is_athlon(void)
	/*@*/
{
	unsigned int eax, ebx, ecx, edx;
	char vendor[16];
	int i;
	
	cpuid (0, &eax, &ebx, &ecx, &edx);

 	/* If you care about space, you can just check ebx, ecx and edx directly
 	   instead of forming a string first and then doing a strcmp */
 	memset(vendor, 0, sizeof(vendor));
 	
 	for (i=0; i<4; i++)
 		vendor[i] = (unsigned char) (ebx >>(8*i));
 	for (i=0; i<4; i++)
 		vendor[4+i] = (unsigned char) (edx >>(8*i));
 	for (i=0; i<4; i++)
 		vendor[8+i] = (unsigned char) (ecx >>(8*i));
 		
 	if (strcmp(vendor, "AuthenticAMD") != 0)  
 		return 0;

	return 1;
}

#endif

static void defaultMachine(/*@out@*/ const char ** arch,
		/*@out@*/ const char ** os)
	/*@globals fileSystem@*/
	/*@modifies *arch, *os, fileSystem @*/
{
    static struct utsname un;
    static int gotDefaults = 0;
    char * chptr;
    canonEntry canon;
    int rc;

    if (!gotDefaults) {
	rc = uname(&un);
	if (rc < 0) return;

#if !defined(__linux__)
#ifdef SNI
	/* USUALLY un.sysname on sinix does start with the word "SINIX"
	 * let's be absolutely sure
	 */
	strncpy(un.sysname, "SINIX", sizeof(un.sysname));
#endif
	/*@-nullpass@*/
	if (!strcmp(un.sysname, "AIX")) {
	    strcpy(un.machine, __power_pc() ? "ppc" : "rs6000");
	    sprintf(un.sysname,"aix%s.%s", un.version, un.release);
	}
	else if (!strcmp(un.sysname, "SunOS")) {
	    if (!strncmp(un.release,"4", 1)) /* SunOS 4.x */ {
		int fd;
		for (fd = 0;
		    (un.release[fd] != 0 && (fd < sizeof(un.release)));
		    fd++) {
		      if (!xisdigit(un.release[fd]) && (un.release[fd] != '.')) {
			un.release[fd] = 0;
			break;
		      }
		    }
		    sprintf(un.sysname,"sunos%s",un.release);
	    }

	    else /* Solaris 2.x: n.x.x becomes n-3.x.x */
		sprintf(un.sysname, "solaris%1d%s", atoi(un.release)-3,
			un.release+1+(atoi(un.release)/10));

	    /* Solaris on Intel hardware reports i86pc instead of i386
	     * (at least on 2.6 and 2.8)
	     */
	    if (!strcmp(un.machine, "i86pc"))
		sprintf(un.machine, "i386");
	}
	else if (!strcmp(un.sysname, "HP-UX"))
	    /*make un.sysname look like hpux9.05 for example*/
	    sprintf(un.sysname, "hpux%s", strpbrk(un.release, "123456789"));
	else if (!strcmp(un.sysname, "OSF1"))
	    /*make un.sysname look like osf3.2 for example*/
	    sprintf(un.sysname, "osf%s", strpbrk(un.release, "123456789"));
	else if (!strncmp(un.sysname, "IP", 2))
	    un.sysname[2] = '\0';
	else if (!strncmp(un.sysname, "SINIX", 5)) {
	    sprintf(un.sysname, "sinix%s",un.release);
	    if (!strncmp(un.machine, "RM", 2))
		sprintf(un.machine, "mips");
	}
	else if ((!strncmp(un.machine, "34", 2) ||
		!strncmp(un.machine, "33", 2)) && \
		!strncmp(un.release, "4.0", 3))
	{
	    /* we are on ncr-sysv4 */
	    char * prelid = NULL;
	    FD_t fd = Fopen("/etc/.relid", "r.fdio");
	    int gotit = 0;
	    /*@-branchstate@*/
	    if (fd != NULL && !Ferror(fd)) {
		chptr = xcalloc(1, 256);
		{   int irelid = Fread(chptr, sizeof(*chptr), 256, fd);
		    (void) Fclose(fd);
		    /* example: "112393 RELEASE 020200 Version 01 OS" */
		    if (irelid > 0) {
			if ((prelid = strstr(chptr, "RELEASE "))){
			    prelid += strlen("RELEASE ")+1;
			    sprintf(un.sysname,"ncr-sysv4.%.*s",1,prelid);
			    gotit = 1;
			}
		    }
		}
		chptr = _free (chptr);
	    }
	    /*@=branchstate@*/
	    if (!gotit)	/* parsing /etc/.relid file failed? */
		strcpy(un.sysname,"ncr-sysv4");
	    /* wrong, just for now, find out how to look for i586 later*/
	    strcpy(un.machine,"i486");
	}
	/*@=nullpass@*/
#endif	/* __linux__ */

	/* get rid of the hyphens in the sysname */
	for (chptr = un.machine; *chptr != '\0'; chptr++)
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

#	if HAVE_PERSONALITY && defined(__linux__) && defined(__sparc__)
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

#	if defined(__GNUC__) && defined(__alpha__)
	{
	    unsigned long amask, implver;
	    register long v0 __asm__("$0") = -1;
	    __asm__ (".long 0x47e00c20" : "=r"(v0) : "0"(v0));
	    amask = ~v0;
	    __asm__ (".long 0x47e03d80" : "=r"(v0));
	    implver = v0;
	    switch (implver) {
	    case 1:
	    	switch (amask) {
	    	case 0: strcpy(un.machine, "alphaev5"); break;
	    	case 1: strcpy(un.machine, "alphaev56"); break;
	    	case 0x101: strcpy(un.machine, "alphapca56"); break;
	    	}
	    	break;
	    case 2:
	    	switch (amask) {
	    	case 0x303: strcpy(un.machine, "alphaev6"); break;
	    	case 0x307: strcpy(un.machine, "alphaev67"); break;
	    	}
	    	break;
	    }
	}
#	endif

#	if defined(__linux__) && defined(__i386__)
	{
	    char class = (char) (RPMClass() | '0');

	    if (class == '6' && is_athlon())
	    	strcpy(un.machine, "athlon");
	    else if (strchr("3456", un.machine[1]) && un.machine[1] != class)
		un.machine[1] = class;
	}
#	endif

#	if defined(__linux__) && defined(__powerpc__)
	{
	    unsigned pvr;
	    __asm__ __volatile__ ("mfspr %0, 287" : "=r" (pvr));

	    pvr >>= 16;
	    if ( pvr >= 0x40)
		strcpy(un.machine, "ppcpseries");
	    else if ( (pvr == 0x36) || (pvr == 0x37) )
		strcpy(un.machine, "ppciseries");
	    else
		strcpy(un.machine, "ppc");
	}
#	endif

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

static /*@observer@*/ /*@null@*/
const char * rpmGetVarArch(int var, /*@null@*/ const char * arch)
	/*@*/
{
    const struct rpmvarValue * next;

    if (arch == NULL) arch = current[ARCH];

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

const char *rpmGetVar(int var)
{
    return rpmGetVarArch(var, NULL);
}

/* this doesn't free the passed pointer! */
static void freeRpmVar(/*@only@*/ struct rpmvarValue * orig)
	/*@modifies *orig @*/
{
    struct rpmvarValue * next, * var = orig;

    while (var) {
	next = var->next;
	var->arch = _free(var->arch);
	var->value = _free(var->value);

	/*@-branchstate@*/
	if (var != orig) var = _free(var);
	/*@=branchstate@*/
	var = next;
    }
}

void rpmSetVar(int var, const char * val)
{
    /*@-immediatetrans@*/
    freeRpmVar(&values[var]);
    /*@=immediatetrans@*/
    values[var].value = (val ? xstrdup(val) : NULL);
}

static void rpmSetVarArch(int var, const char * val, const char * arch)
	/*@*/
{
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

	/*@-nullpass@*/	/* LCL: arch != NULL here. */
	if (next->arch && arch && !strcmp(next->arch, arch)) {
	/*@=nullpass@*/
	    next->value = _free(next->value);
	    next->arch = _free(next->arch);
	} else if (next->arch || arch) {
	    next->next = xmalloc(sizeof(*next->next));
	    next = next->next;
	    next->value = NULL;
	    next->arch = NULL;
	    next->next = NULL;
	}
    }

    next->value = xstrdup(val);		/* XXX memory leak, hard to plug */
    next->arch = (arch ? xstrdup(arch) : NULL);
}

void rpmSetTables(int archTable, int osTable)
{
    const char * arch, * os;

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

int rpmMachineScore(int type, const char * name)
{
    machEquivInfo info = machEquivSearch(&tables[type].equiv, name);
    return (info != NULL ? info->score : 0);
}

void rpmGetMachine(const char ** arch, const char ** os)
{
    if (arch)
	*arch = current[ARCH];

    if (os)
	*os = current[OS];
}

void rpmSetMachine(const char * arch, const char * os)
{
    const char * host_cpu, * host_os;

    defaultMachine(&host_cpu, &host_os);

    if (arch == NULL) {
	arch = host_cpu;
	if (tables[currTables[ARCH]].hasTranslate)
	    arch = lookupInDefaultTable(arch,
			    tables[currTables[ARCH]].defaults,
			    tables[currTables[ARCH]].defaultsLength);
    }
    if (arch == NULL) return;	/* XXX can't happen */

    if (os == NULL) {
	os = host_os;
	if (tables[currTables[OS]].hasTranslate)
	    os = lookupInDefaultTable(os,
			    tables[currTables[OS]].defaults,
			    tables[currTables[OS]].defaultsLength);
    }
    if (os == NULL) return;	/* XXX can't happen */

    if (!current[ARCH] || strcmp(arch, current[ARCH])) {
	current[ARCH] = _free(current[ARCH]);
	current[ARCH] = xstrdup(arch);
	rebuildCompatTables(ARCH, host_cpu);
    }

    if (!current[OS] || strcmp(os, current[OS])) {
	char * t = xstrdup(os);
	current[OS] = _free(current[OS]);
	/*
	 * XXX Capitalizing the 'L' is needed to insure that old
	 * XXX os-from-uname (e.g. "Linux") is compatible with the new
	 * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
	 * XXX A copy of this string is embedded in headers and is
	 * XXX used by rpmInstallPackage->{os,arch}Okay->rpmMachineScore->
	 * XXX to verify correct arch/os from headers.
	 */
	if (!strcmp(t, "linux"))
	    *t = 'L';
	current[OS] = t;
	
	rebuildCompatTables(OS, host_os);
    }
}

static void rebuildCompatTables(int type, const char * name)
	/*@*/
{
    machFindEquivs(&tables[currTables[type]].cache,
		   &tables[currTables[type]].equiv,
		   name);
}

static void getMachineInfo(int type, /*@null@*/ /*@out@*/ const char ** name,
			/*@null@*/ /*@out@*/int * num)
	/*@modifies *name, *num @*/
{
    canonEntry canon;
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

void rpmGetArchInfo(const char ** name, int * num)
{
    getMachineInfo(ARCH, name, num);
}

void rpmGetOsInfo(const char ** name, int * num)
{
    getMachineInfo(OS, name, num);
}

void rpmRebuildTargetVars(const char ** target, const char ** canontarget)
{

    char *ca = NULL, *co = NULL, *ct = NULL;
    int x;

    /* Rebuild the compat table to recalculate the current target arch.  */

    rpmSetMachine(NULL, NULL);
    rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
    rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);

    /*@-branchstate@*/
    if (target && *target) {
	char *c;
	/* Set arch and os from specified build target */
	ca = xstrdup(*target);
	if ((c = strchr(ca, '-')) != NULL) {
	    *c++ = '\0';
	    
	    if ((co = strrchr(c, '-')) == NULL) {
		co = c;
	    } else {
		if (!xstrcasecmp(co, "-gnu"))
		    *co = '\0';
		if ((co = strrchr(c, '-')) == NULL)
		    co = c;
		else
		    co++;
	    }
	    if (co != NULL) co = xstrdup(co);
	}
    } else {
	const char *a = NULL;
	const char *o = NULL;
	/* Set build target from rpm arch and os */
	rpmGetArchInfo(&a, NULL);
	ca = (a) ? xstrdup(a) : NULL;
	rpmGetOsInfo(&o, NULL);
	co = (o) ? xstrdup(o) : NULL;
    }
    /*@=branchstate@*/

    /* If still not set, Set target arch/os from default uname(2) values */
    if (ca == NULL) {
	const char *a = NULL;
	defaultMachine(&a, NULL);
	ca = (a) ? xstrdup(a) : NULL;
    }
    for (x = 0; ca[x] != '\0'; x++)
	ca[x] = xtolower(ca[x]);

    if (co == NULL) {
	const char *o = NULL;
	defaultMachine(NULL, &o);
	co = (o) ? xstrdup(o) : NULL;
    }
    for (x = 0; co[x] != '\0'; x++)
	co[x] = xtolower(co[x]);

    /* XXX For now, set canonical target to arch-os */
    if (ct == NULL) {
	ct = xmalloc(strlen(ca) + sizeof("-") + strlen(co));
	sprintf(ct, "%s-%s", ca, co);
    }

/*
 * XXX All this macro pokery/jiggery could be achieved by doing a delayed
 *	rpmInitMacros(NULL, PER-PLATFORM-MACRO-FILE-NAMES);
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
  { const char *optflags = rpmGetVarArch(RPMVAR_OPTFLAGS, ca);
    if (optflags != NULL) {
	delMacro(NULL, "optflags");
	addMacro(NULL, "optflags", NULL, optflags, RMIL_RPMRC);
    }
  }

    /*@-branchstate@*/
    if (canontarget)
	*canontarget = ct;
    else
	ct = _free(ct);
    /*@=branchstate@*/
    ca = _free(ca);
    /*@-usereleased@*/
    co = _free(co);
    /*@=usereleased@*/
}

void rpmFreeRpmrc(void)
{
    int i, j, k;

    for (i = 0; i < RPM_MACHTABLE_COUNT; i++) {
	tableType t;
	t = tables + i;
	if (t->equiv.list) {
	    for (j = 0; j < t->equiv.count; j++)
		t->equiv.list[j].name = _free(t->equiv.list[j].name);
	    t->equiv.list = _free(t->equiv.list);
	    t->equiv.count = 0;
	}
	if (t->cache.cache) {
	    for (j = 0; j < t->cache.size; j++) {
		machCacheEntry e;
		e = t->cache.cache + j;
		if (e == NULL)
		    /*@innercontinue@*/ continue;
		e->name = _free(e->name);
		if (e->equivs) {
		    for (k = 0; k < e->count; k++)
			e->equivs[k] = _free(e->equivs[k]);
		    e->equivs = _free(e->equivs);
		}
	    }
	    t->cache.cache = _free(t->cache.cache);
	    t->cache.size = 0;
	}
	if (t->defaults) {
	    for (j = 0; j < t->defaultsLength; j++) {
		t->defaults[j].name = _free(t->defaults[j].name);
		t->defaults[j].defName = _free(t->defaults[j].defName);
	    }
	    t->defaults = _free(t->defaults);
	    t->defaultsLength = 0;
	}
	if (t->canons) {
	    for (j = 0; j < t->canonsLength; j++) {
		t->canons[j].name = _free(t->canons[j].name);
		t->canons[j].short_name = _free(t->canons[j].short_name);
	    }
	    t->canons = _free(t->canons);
	    t->canonsLength = 0;
	}
    }

    for (i = 0; i < RPMVAR_NUM; i++) {
	/*@only@*/ /*@null@*/ struct rpmvarValue * vp;
	while ((vp = values[i].next) != NULL) {
	    values[i].next = vp->next;
	    vp->value = _free(vp->value);
	    vp->arch = _free(vp->arch);
	    vp = _free(vp);
	}
	values[i].value = _free(values[i].value);
	values[i].arch = _free(values[i].arch);
    }
    current[OS] = _free(current[OS]);
    current[ARCH] = _free(current[ARCH]);
    defaultsInitialized = 0;
/*@-nullstate@*/ /* FIX: current may be NULL */
    return;
/*@=nullstate@*/
}

/** \ingroup rpmrc
 * Read rpmrc (and macro) configuration file(s).
 * @param rcfiles	colon separated files to read (NULL uses default)
 * @return		0 on succes
 */
static int rpmReadRC(/*@null@*/ const char * rcfiles)
	/*@globals rpmGlobalMacroContext, rpmCLIMacroContext,
		fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    char *myrcfiles, *r, *re;
    int rc;

    if (!defaultsInitialized) {
	setDefaults();
	defaultsInitialized = 1;
    }

    if (rcfiles == NULL)
	rcfiles = defrcfiles;

    /* Read each file in rcfiles. */
    rc = 0;
    for (r = myrcfiles = xstrdup(rcfiles); r && *r != '\0'; r = re) {
	char fn[4096];
	FD_t fd;

	/* Get pointer to rest of files */
	for (re = r; (re = strchr(re, ':')) != NULL; re++) {
	    if (!(re[1] == '/' && re[2] == '/'))
		/*@innerbreak@*/ break;
	}
	if (re && *re == ':')
	    *re++ = '\0';
	else
	    re = r + strlen(r);

	/* Expand ~/ to $HOME/ */
	fn[0] = '\0';
	if (r[0] == '~' && r[1] == '/') {
	    const char * home = getenv("HOME");
	    if (home == NULL) {
	    /* XXX Only /usr/lib/rpm/rpmrc must exist in default rcfiles list */
		if (rcfiles == defrcfiles && myrcfiles != r)
		    continue;
		rpmError(RPMERR_RPMRC, _("Cannot expand %s\n"), r);
		rc = 1;
		break;
	    }
	    if (strlen(home) > (sizeof(fn) - strlen(r))) {
		rpmError(RPMERR_RPMRC, _("Cannot read %s, HOME is too large.\n"),
				r);
		rc = 1;
		break;
	    }
	    strcpy(fn, home);
	    r++;
	}
	strncat(fn, r, sizeof(fn) - (strlen(fn) + 1));
	fn[sizeof(fn)-1] = '\0';

	/* Read another rcfile */
	fd = Fopen(fn, "r.fpio");
	if (fd == NULL || Ferror(fd)) {
	    /* XXX Only /usr/lib/rpm/rpmrc must exist in default rcfiles list */
	    if (rcfiles == defrcfiles && myrcfiles != r)
		continue;
	    rpmError(RPMERR_RPMRC, _("Unable to open %s for reading: %s.\n"),
		 fn, Fstrerror(fd));
	    rc = 1;
	    break;
	} else {
	    rc = doReadRC(fd, fn);
	}
 	if (rc) break;
    }
    myrcfiles = _free(myrcfiles);
    if (rc)
	return rc;

    rpmSetMachine(NULL, NULL);	/* XXX WTFO? Why bother? */

    {	const char *mfpath;
	/*@-branchstate@*/
	if ((mfpath = rpmGetVar(RPMVAR_MACROFILES)) != NULL) {
	    mfpath = xstrdup(mfpath);
	    rpmInitMacros(NULL, mfpath);
	    mfpath = _free(mfpath);
	}
	/*@=branchstate@*/
    }

    return rc;
}

int rpmReadConfigFiles(const char * file, const char * target)
{

    /* Preset target macros */
    /*@-nullstate@*/	/* FIX: target can be NULL */
    rpmRebuildTargetVars(&target, NULL);

    /* Read the files */
    if (rpmReadRC(file)) return -1;

    /* Reset target macros */
    rpmRebuildTargetVars(&target, NULL);
    /*@=nullstate@*/

    /* Finally set target platform */
    {	const char *cpu = rpmExpand("%{_target_cpu}", NULL);
	const char *os = rpmExpand("%{_target_os}", NULL);
	rpmSetMachine(cpu, os);
	cpu = _free(cpu);
	os = _free(os);
    }

    return 0;
}

int rpmShowRC(FILE * fp)
{
    struct rpmOption *opt;
    int i;
    machEquivTable equivTable;

    /* the caller may set the build arch which should be printed here */
    fprintf(fp, "ARCHITECTURE AND OS:\n");
    fprintf(fp, "build arch            : %s\n", current[ARCH]);

    fprintf(fp, "compatible build archs:");
    equivTable = &tables[RPM_MACHTABLE_BUILDARCH].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(fp," %s", equivTable->list[i].name);
    fprintf(fp, "\n");

    fprintf(fp, "build os              : %s\n", current[OS]);

    fprintf(fp, "compatible build os's :");
    equivTable = &tables[RPM_MACHTABLE_BUILDOS].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(fp," %s", equivTable->list[i].name);
    fprintf(fp, "\n");

    rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
    rpmSetMachine(NULL, NULL);	/* XXX WTFO? Why bother? */

    fprintf(fp, "install arch          : %s\n", current[ARCH]);
    fprintf(fp, "install os            : %s\n", current[OS]);

    fprintf(fp, "compatible archs      :");
    equivTable = &tables[RPM_MACHTABLE_INSTARCH].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(fp," %s", equivTable->list[i].name);
    fprintf(fp, "\n");

    fprintf(fp, "compatible os's       :");
    equivTable = &tables[RPM_MACHTABLE_INSTOS].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(fp," %s", equivTable->list[i].name);
    fprintf(fp, "\n");

    fprintf(fp, "\nRPMRC VALUES:\n");
    for (i = 0, opt = optionTable; i < optionTableSize; i++, opt++) {
	const char *s = rpmGetVar(opt->var);
	if (s != NULL || rpmIsVerbose())
	    fprintf(fp, "%-21s : %s\n", opt->name, s ? s : "(not set)");
    }
    fprintf(fp, "\n");

    fprintf(fp, "Features supported by rpmlib:\n");
    rpmShowRpmlibProvides(fp);
    fprintf(fp, "\n");

    rpmDumpMacroTable(NULL, fp);

    return 0;
}
/*@=mods@*/
