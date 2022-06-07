/** \ingroup rpmrc rpmio
 * \file rpmio/macro.c
 */

#include "system.h"
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#if HAVE_SCHED_GETAFFINITY
#include <sched.h>
#endif

#if !defined(isblank)
#define	isblank(_c)	((_c) == ' ' || (_c) == '\t')
#endif
#define	iseol(_c)	((_c) == '\n' || (_c) == '\r')

#define MACROBUFSIZ (BUFSIZ * 2)

#include <rpm/rpmio.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/argv.h>

#include "rpmio/rpmlua.h"
#include "rpmio/rpmmacro_internal.h"
#include "debug.h"

enum macroFlags_e {
    ME_NONE	= 0,
    ME_AUTO	= (1 << 0),
    ME_USED	= (1 << 1),
    ME_LITERAL	= (1 << 2),
    ME_PARSE	= (1 << 3),
    ME_FUNC	= (1 << 4),
};

typedef struct MacroBuf_s *MacroBuf;
typedef void (*macroFunc)(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed);

/*! The structure used to store a macro. */
struct rpmMacroEntry_s {
    struct rpmMacroEntry_s *prev;/*!< Macro entry stack. */
    const char *name;  	/*!< Macro name. */
    const char *opts;  	/*!< Macro parameters (a la getopt) */
    const char *body;	/*!< Macro body. */
    macroFunc func;	/*!< Macro function (builtin macros) */
    int nargs;		/*!< Number of required args */
    int flags;		/*!< Macro state bits. */
    int level;          /*!< Scoping level. */
    char arena[];   	/*!< String arena. */
};

/*! The structure used to store the set of macros in a context. */
struct rpmMacroContext_s {
    rpmMacroEntry *tab;  /*!< Macro entry table (array of pointers). */
    int n;      /*!< No. of macros. */
    int depth;		 /*!< Depth tracking when recursing from Lua  */
    int level;		 /*!< Scope level tracking when recursing from Lua  */
    pthread_mutex_t lock;
    pthread_mutexattr_t lockattr;
};


static struct rpmMacroContext_s rpmGlobalMacroContext_s;
rpmMacroContext rpmGlobalMacroContext = &rpmGlobalMacroContext_s;

static struct rpmMacroContext_s rpmCLIMacroContext_s;
rpmMacroContext rpmCLIMacroContext = &rpmCLIMacroContext_s;

/*
 * The macro engine internals do not require recursive mutexes but Lua
 * macro bindings which can get called from the internals use the external
 * interfaces which do perform locking. Until that is fixed somehow
 * we'll just have to settle for recursive mutexes.
 * Unfortunately POSIX doesn't specify static initializers for recursive
 * mutexes so we need to have a separate PTHREAD_ONCE initializer just
 * to initialize the otherwise static macro context mutexes. Pooh.
 */
static pthread_once_t locksInitialized = PTHREAD_ONCE_INIT;

static void initLocks(void)
{
    rpmMacroContext mcs[] = { rpmGlobalMacroContext, rpmCLIMacroContext, NULL };

    for (rpmMacroContext *mcp = mcs; *mcp; mcp++) {
	rpmMacroContext mc = *mcp;
	pthread_mutexattr_init(&mc->lockattr);
	pthread_mutexattr_settype(&mc->lockattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mc->lock, &mc->lockattr);
    }
}

/**
 * Macro expansion state.
 */
struct MacroBuf_s {
    char * buf;			/*!< Expansion buffer. */
    size_t tpos;		/*!< Current position in expansion buffer */
    size_t nb;			/*!< No. bytes remaining in expansion buffer. */
    int depth;			/*!< Current expansion depth. */
    int level;			/*!< Current scoping level */
    int error;			/*!< Errors encountered during expansion? */
    int macro_trace;		/*!< Pre-print macro to expand? */
    int expand_trace;		/*!< Post-print macro expansion? */
    int flags;			/*!< Flags to control behavior */
    rpmMacroEntry me;		/*!< Current macro (or NULL if anonymous) */
    ARGV_t args;		/*!< Current macro arguments (or NULL) */
    rpmMacroContext mc;
};

/**
  * Expansion data for a scoping level
  */
typedef struct MacroExpansionData_s {
    size_t tpos;
    int macro_trace;
    int expand_trace;
} MacroExpansionData;

#define	_MAX_MACRO_DEPTH	64
static int max_macro_depth = _MAX_MACRO_DEPTH;

#define	_PRINT_MACRO_TRACE	0
static int print_macro_trace = _PRINT_MACRO_TRACE;

#define	_PRINT_EXPAND_TRACE	0
static int print_expand_trace = _PRINT_EXPAND_TRACE;

/* forward ref */
static int expandMacro(MacroBuf mb, const char *src, size_t slen);
static void pushMacro(rpmMacroContext mc,
	const char * n, const char * o, const char * b, int level, int flags);
static void popMacro(rpmMacroContext mc, const char * n);
static int loadMacroFile(rpmMacroContext mc, const char * fn);
/* =============================================================== */

static rpmMacroContext rpmmctxAcquire(rpmMacroContext mc)
{
    if (mc == NULL)
	mc = rpmGlobalMacroContext;
    pthread_once(&locksInitialized, initLocks);
    pthread_mutex_lock(&mc->lock);
    return mc;
}

static rpmMacroContext rpmmctxRelease(rpmMacroContext mc)
{
    pthread_mutex_unlock(&mc->lock);
    return NULL;
}

/**
 * Find entry in macro table.
 * @param mc		macro context
 * @param name		macro name
 * @param namelen	no. of bytes
 * @param pos		found/insert position
 * @return		address of slot in macro table with name (or NULL)
 */
static rpmMacroEntry *
findEntry(rpmMacroContext mc, const char *name, size_t namelen, size_t *pos)
{
    /* bsearch */
    int cmp = 1;
    size_t l = 0;
    size_t u = mc->n;
    size_t i = 0;
    while (l < u) {
	i = (l + u) / 2;
	rpmMacroEntry me = mc->tab[i];
	if (namelen == 0)
	    cmp = strcmp(me->name, name);
	else {
	    cmp = strncmp(me->name, name, namelen);
	    /* longer me->name compares greater */
	    if (cmp == 0)
		cmp = (me->name[namelen] != '\0');
	}
	if (cmp < 0)
	    l = i + 1;
	else if (cmp > 0)
	    u = i;
	else
	    break;
    }

    if (pos)
	*pos = (cmp < 0) ? i + 1 : i;
    if (cmp == 0)
	return &mc->tab[i];
    return NULL;
}

/* =============================================================== */

/**
 * fgets(3) analogue that reads \ continuations. Last newline always trimmed.
 * @param buf		input buffer
 * @param size		inbut buffer size (bytes)
 * @param f		file handle
 * @return		number of lines read, or 0 on end-of-file
 */
static int
rdcl(char * buf, size_t size, FILE *f)
{
    size_t nb = 0;
    int pc = 0, bc = 0, xc = 0;
    int nlines = 0;
    char *p = buf;
    char *q = buf;

    if (f != NULL)
    do {
	*q = '\0';			/* terminate */
	if (fgets(q, size, f) == NULL)	/* read next line. */
	    break;
	nlines++;
	nb = strlen(q);
	for (q += nb; nb > 0 && iseol(q[-1]); q--)
	    nb--;
	if (*q == 0)
	    break;			/* no newline found, EOF */
	if (p == buf) {
            while (*p && isblank(*p))
                p++;
            if (*p != '%') {		/* only parse actual macro */
                *q = '\0';		/* trim trailing \r, \n */
                break;
            }
        }
	for (; p < q; p++) {
	    switch (*p) {
		case '\\':
		    switch (*(p+1)) {
			case '\0': break;
			default: p++; break;
		    }
		    break;
		case '%':
		    switch (*(p+1)) {
			case '{': p++, bc++; break;
			case '(': p++, pc++; break;
			case '[': p++, xc++; break;
			case '%': p++; break;
		    }
		    break;
		case '{': if (bc > 0) bc++; break;
		case '}': if (bc > 0) bc--; break;
		case '(': if (pc > 0) pc++; break;
		case ')': if (pc > 0) pc--; break;
		case '[': if (xc > 0) xc++; break;
		case ']': if (xc > 0) xc--; break;
	    }
	}
	if ((nb == 0 || q[-1] != '\\') && !bc && !pc && !xc) {
	    *q = '\0';		/* trim trailing \r, \n */
	    break;
	}
	q++; nb++;			/* copy newline too */
	size -= nb;
	if (q[-1] == '\r')			/* XXX avoid \r madness */
	    q[-1] = '\n';
    } while (size > 0);
    return nlines;
}

/**
 * Return text between pl and matching pr characters.
 * @param p		start of text
 * @param pl		left char, i.e. '[', '(', '{', etc.
 * @param pr		right char, i.e. ']', ')', '}', etc.
 * @return		address of char after pr (or NULL)
 */
static const char *
matchchar(const char * p, char pl, char pr)
{
    int lvl = 0;
    char c;

    while ((c = *p++) != '\0') {
	if (c == '\\') {		/* Ignore escaped chars */
	    p++;
	    continue;
	}
	if (c == pr) {
	    if (--lvl <= 0)	return p;
	} else if (c == pl)
	    lvl++;
    }
    return (const char *)NULL;
}

static void mbErr(MacroBuf mb, int error, const char *fmt, ...)
{
    char *emsg = NULL;
    int n;
    va_list ap;

    va_start(ap, fmt);
    n = rvasprintf(&emsg, fmt, ap);
    va_end(ap);

    if (n >= -1) {
	/* XXX should have an non-locking version for this */
	char *pfx = rpmExpand("%{?__file_name:%{__file_name}: }",
			      "%{?__file_lineno:line %{__file_lineno}: }",
			      NULL);
	rpmlog(error ? RPMLOG_ERR : RPMLOG_WARNING, "%s%s", pfx, emsg);
	free(pfx);
    }

    if (error)
	mb->error = error;

    free(emsg);
}

/**
 * Pre-print macro expression to be expanded.
 * @param mb		macro expansion state
 * @param s		current expansion string
 * @param se		end of string
 */
static void
printMacro(MacroBuf mb, const char * s, const char * se)
{
    const char *senl;

    if (s >= se) {	/* XXX just in case */
	fprintf(stderr, _("%3d>%*s(empty)\n"), mb->depth,
		(2 * mb->depth + 1), "");
	return;
    }

    if (s[-1] == '{')
	s--;

    /* Print only to first end-of-line (or end-of-string). */
    for (senl = se; *senl && !iseol(*senl); senl++)
	{};

    /* Substitute caret at end-of-macro position */
    fprintf(stderr, "%3d>%*s%%%.*s^", mb->depth,
	(2 * mb->depth + 1), "", (int)(se - s), s);
    if (se[0] != '\0' && se[1] != '\0' && (senl - (se+1)) > 0)
	fprintf(stderr, "%-.*s", (int)(senl - (se+1)), se+1);
    fprintf(stderr, "\n");
}

/**
 * Post-print expanded macro expression.
 * @param mb		macro expansion state
 * @param t		current expansion string result
 * @param te		end of string
 */
static void
printExpansion(MacroBuf mb, rpmMacroEntry me, const char * t, const char * te)
{
    const char *mname = me ? me->name : "";
    if (!(te > t)) {
	fprintf(stderr, "%3d<%*s (%%%s)\n", mb->depth, (2 * mb->depth + 1), "", mname);
	return;
    }

    /* Shorten output which contains newlines */
    while (te > t && iseol(te[-1]))
	te--;
    if (mb->depth > 0) {
	const char *tenl;

	/* Skip to last line of expansion */
	while ((tenl = strchr(t, '\n')) && tenl < te)
	    t = ++tenl;

    }

    fprintf(stderr, "%3d<%*s (%%%s)\n", mb->depth, (2 * mb->depth + 1), "", mname);
    if (te > t)
	fprintf(stderr, "%.*s", (int)(te - t), t);
    fprintf(stderr, "\n");
}

#define	SKIPBLANK(_s, _c)	\
	while (((_c) = *(_s)) && isblank(_c)) \
		(_s)++;		\

#define	SKIPNONBLANK(_s, _c)	\
	while (((_c) = *(_s)) && !(isblank(_c) || iseol(_c))) \
		(_s)++;		\

#define	COPYNAME(_ne, _s, _c)	\
    {	SKIPBLANK(_s,_c);	\
	while (((_c) = *(_s)) && (risalnum(_c) || (_c) == '_')) \
		*(_ne)++ = *(_s)++; \
	*(_ne) = '\0';		\
    }

#define	COPYOPTS(_oe, _s, _c)	\
    { \
	while (((_c) = *(_s)) && (_c) != ')') \
		*(_oe)++ = *(_s)++; \
	*(_oe) = '\0';		\
    }

/**
 * Macro-expand string src, return result in dynamically allocated buffer.
 * @param mb		macro expansion state
 * @param src		string to expand
 * @param slen		input string length (or 0 for strlen())
 * @param[out] target	pointer to expanded string (malloced)
 * @return		result of expansion
 */
static int
expandThis(MacroBuf mb, const char * src, size_t slen, char **target)
{
    struct MacroBuf_s umb;

    /* Copy other state from "parent", but we want a buffer of our own */
    umb = *mb;
    umb.buf = NULL;
    umb.error = 0;
    /* In case of error, flag it in the "parent"... */
    if (expandMacro(&umb, src, slen))
	mb->error = 1;
    *target = umb.buf;

    /* ...but return code for this operation specifically */
    return umb.error;
}

static MacroBuf mbCreate(rpmMacroContext mc, int flags)
{
    MacroBuf mb = xcalloc(1, sizeof(*mb));
    mb->buf = NULL;
    mb->depth = mc->depth;
    mb->level = mc->level;
    mb->macro_trace = print_macro_trace;
    mb->expand_trace = print_expand_trace;
    mb->mc = mc;
    mb->flags = flags;
    return mb;
}

static void mbAllocBuf(MacroBuf mb, size_t slen)
{
    size_t blen = MACROBUFSIZ + slen;
    mb->buf = xmalloc(blen + 1);
    mb->buf[0] = '\0';
    mb->tpos = 0;
    mb->nb = blen;
}

static int mbInit(MacroBuf mb, MacroExpansionData *med, size_t slen)
{
    if (mb->buf == NULL)
	mbAllocBuf(mb, slen);
    if (++mb->depth > max_macro_depth) {
	mbErr(mb, 1,
		_("Too many levels of recursion in macro expansion. It is likely caused by recursive macro declaration.\n"));
	mb->depth--;
	return -1;
    }
    med->tpos = mb->tpos; /* save expansion pointer for printExpand */
    med->macro_trace = mb->macro_trace;
    med->expand_trace = mb->expand_trace;
    return 0;
}

static void mbFini(MacroBuf mb, rpmMacroEntry me, MacroExpansionData *med)
{
    mb->buf[mb->tpos] = '\0';
    mb->depth--;
    if (mb->error && rpmIsVerbose())
	mb->expand_trace = 1;
    if (mb->expand_trace)
	printExpansion(mb, me, mb->buf + med->tpos, mb->buf + mb->tpos);
    mb->macro_trace = med->macro_trace;
    mb->expand_trace = med->expand_trace;
}

static void mbAppend(MacroBuf mb, char c)
{
    if (mb->nb < 1) {
	mb->buf = xrealloc(mb->buf, mb->tpos + MACROBUFSIZ + 1);
	mb->nb += MACROBUFSIZ;
    }
    mb->buf[mb->tpos++] = c;
    mb->buf[mb->tpos] = '\0';
    mb->nb--;
}

static void mbAppendStr(MacroBuf mb, const char *str)
{
    size_t len = strlen(str);
    if (len > mb->nb) {
	mb->buf = xrealloc(mb->buf, mb->tpos + mb->nb + MACROBUFSIZ + len + 1);
	mb->nb += MACROBUFSIZ + len;
    }
    memcpy(mb->buf+mb->tpos, str, len + 1);
    mb->tpos += len;
    mb->nb -= len;
}

static void doDnl(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    const char *se = argv[1];
    const char *start = se, *end;
    const char *s = se;
    while (*s && !iseol(*s))
	s++;
    end = (*s != '\0') ? s + 1 : s;
    if (parsed)
	*parsed += end - start;
}

/**
 * Expand output of shell command into target buffer.
 * @param mb		macro expansion state
 * @param cmd		shell command
 * @param clen		no. bytes in shell command
 */
static void
doShellEscape(MacroBuf mb, const char * cmd, size_t clen)
{
    char *buf = NULL;
    FILE *shf;
    int c;

    if (expandThis(mb, cmd, clen, &buf))
	goto exit;

    if ((shf = popen(buf, "r")) == NULL) {
	mbErr(mb, 1, _("Failed to open shell expansion pipe for command: "
		"%s: %m \n"), buf);
	goto exit;
    }

    size_t tpos = mb->tpos;
    while ((c = fgetc(shf)) != EOF) {
	mbAppend(mb, c);
    }
    (void) pclose(shf);

    /* Delete trailing \r \n */
    while (mb->tpos > tpos && iseol(mb->buf[mb->tpos-1])) {
	mb->buf[--mb->tpos] = '\0';
	mb->nb++;
    }

exit:
    _free(buf);
}

/**
 * Expand an expression into target buffer.
 * @param mb		macro expansion state
 * @param expr		expression
 * @param len		no. bytes in expression
 */
static void doExpressionExpansion(MacroBuf mb, const char * expr, size_t len)
{
    char *buf = rstrndup(expr, len);
    char *result;
    result = rpmExprStrFlags(buf, RPMEXPR_EXPAND);
    if (!result) {
	mb->error = 1;
	goto exit;
    }
    mbAppendStr(mb, result);
    free(result);
exit:
    _free(buf);
}

static unsigned int getncpus(void)
{
    unsigned int ncpus = 0;
#if HAVE_SCHED_GETAFFINITY
    cpu_set_t set;
    if (sched_getaffinity (0, sizeof(set), &set) == 0)
	ncpus = CPU_COUNT(&set);
#endif
    /* Fallback to sysconf() if the above isn't supported or didn't work */
    if (ncpus < 1)
	ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    /* If all else fails, there's always the one we're running on... */
    if (ncpus < 1)
	ncpus = 1;
    return ncpus;
}

static int
validName(MacroBuf mb, const char *name, size_t namelen, const char *action)
{
    rpmMacroEntry *mep;
    int rc = 0;
    int c;

    /* Names must start with alphabetic or _ and be at least 3 chars */
    if (!((c = *name) && (risalpha(c) || c == '_') && (namelen) > 2)) {
	mbErr(mb, 1, _("Macro %%%s has illegal name (%s)\n"), name, action);
	goto exit;
    }

    mep = findEntry(mb->mc, name, namelen, NULL);
    if (mep && (*mep)->flags & (ME_FUNC|ME_AUTO)) {
	mbErr(mb, 1, _("Macro %%%s is a built-in (%s)\n"), name, action);
	goto exit;
    }

    rc = 1;

exit:
    return rc;
}

/**
 * Parse (and execute) new macro definition.
 * @param mb		macro expansion state
 * @param se		macro definition to parse
 * @param level		macro recursion level
 * @param expandbody	should body be expanded?
 * @return		number of consumed characters
 */
static void
doDefine(MacroBuf mb, const char * se, int level, int expandbody, size_t *parsed)
{
    const char *start = se;
    const char *s = se;
    char *buf = xmalloc(strlen(s) + 3); /* Some leeway for termination issues... */
    char *n = buf, *ne = n;
    char *o = NULL, *oe;
    char *b, *be, *ebody = NULL;
    int c;
    int oc = ')';
    const char *sbody; /* as-is body start */
    int rc = 1; /* assume failure */

    /* Copy name */
    COPYNAME(ne, s, c);

    /* Copy opts (if present) */
    oe = ne + 1;
    if (*s == '(') {
	s++;	/* skip ( */
	/* Options must be terminated with ')' */
	if (strchr(s, ')')) {
	    o = oe;
	    COPYOPTS(oe, s, oc);
	    s++;	/* skip ) */
	} else {
	    mbErr(mb, 1, _("Macro %%%s has unterminated opts\n"), n);
	    goto exit;
	}
    }

    /* Copy body, skipping over escaped newlines */
    b = be = oe + 1;
    sbody = s;
    SKIPBLANK(s, c);
    if (c == '{') {	/* XXX permit silent {...} grouping */
	if ((se = matchchar(s, c, '}')) == NULL) {
	    mbErr(mb, 1, _("Macro %%%s has unterminated body\n"), n);
	    se = s;	/* XXX W2DO? */
	    goto exit;
	}
	s++;	/* XXX skip { */
	strncpy(b, s, (se - 1 - s));
	b[se - 1 - s] = '\0';
	be += strlen(b);
	s = se;	/* move scan forward */
    } else {	/* otherwise free-field */
	int bc = 0, pc = 0, xc = 0;
	while (*s && (bc || pc || !iseol(*s))) {
	    switch (*s) {
		case '\\':
		    switch (*(s+1)) {
			case '\0': break;
			default: s++; break;
		    }
		    break;
		case '%':
		    switch (*(s+1)) {
			case '{': *be++ = *s++; bc++; break;
			case '(': *be++ = *s++; pc++; break;
			case '[': *be++ = *s++; xc++; break;
			case '%': *be++ = *s++; break;
		    }
		    break;
		case '{': if (bc > 0) bc++; break;
		case '}': if (bc > 0) bc--; break;
		case '(': if (pc > 0) pc++; break;
		case ')': if (pc > 0) pc--; break;
		case '[': if (xc > 0) xc++; break;
		case ']': if (xc > 0) xc--; break;
	    }
	    *be++ = *s++;
	}
	*be = '\0';

	if (bc || pc || xc) {
	    mbErr(mb, 1, _("Macro %%%s has unterminated body\n"), n);
	    se = s;	/* XXX W2DO? */
	    goto exit;
	}

	/* Trim trailing blanks/newlines */
	while (--be >= b && (c = *be) && (isblank(c) || iseol(c)))
	    {};
	*(++be) = '\0';	/* one too far */
    }

    /* Move scan over body */
    while (iseol(*s))
	s++;
    se = s;

    if (!validName(mb, n, ne - n, expandbody ? "%global": "%define"))
	goto exit;

    if ((be - b) < 1) {
	mbErr(mb, 1, _("Macro %%%s has empty body\n"), n);
	goto exit;
    }

    if (!isblank(*sbody) && !(*sbody == '\\' && iseol(sbody[1])))
	mbErr(mb, 0, _("Macro %%%s needs whitespace before body\n"), n);

    if (expandbody) {
	if (expandThis(mb, b, 0, &ebody)) {
	    mbErr(mb, 1, _("Macro %%%s failed to expand\n"), n);
	    goto exit;
	}
	b = ebody;
    }

    pushMacro(mb->mc, n, o, b, level, ME_NONE);
    rc = 0;

exit:
    if (rc)
	mb->error = 1;
    _free(buf);
    _free(ebody);
    if (parsed)
	*parsed += se - start;
}

/**
 * Parse (and execute) macro undefinition.
 * @param mb		macro expansion state
 * @param argv		macro arguments
 * @return		number of consumed characters
 */
static void
doUndefine(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    const char *n = argv[1];
    if (!validName(mb, n, strlen(n), "%undefine")) {
	mb->error = 1;
    } else {
	popMacro(mb->mc, n);
    }
}

static void doArgvDefine(MacroBuf mb, ARGV_t argv, int level, int expand, size_t *parsed)
{
    char *args = NULL;
    const char *se = argv[1];

    /* handle the "programmatic" case where macro name is arg1 and body arg2 */
    if (argv[2])
	se = args = rstrscat(NULL, argv[1], " ", argv[2], NULL);

    doDefine(mb, se, level, expand, parsed);
    free(args);
}

static void doDef(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    doArgvDefine(mb, argv, mb->level, 0, parsed);
}

static void doGlobal(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    doArgvDefine(mb, argv, RMIL_GLOBAL, 1, parsed);
}

static void doDump(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    rpmDumpMacroTable(mb->mc, NULL);
}


static int mbopt(int c, const char *oarg, int oint, void *data)
{
    MacroBuf mb = data;
    char *name = NULL, *body = NULL;

    /* Define option macros. */
    rasprintf(&name, "-%c", c);
    if (oarg) {
	rasprintf(&body, "-%c %s", c, oarg);
    } else {
	rasprintf(&body, "-%c", c);
    }
    pushMacro(mb->mc, name, NULL, body, mb->level, ME_AUTO | ME_LITERAL);
    free(name);
    free(body);

    if (oarg) {
	rasprintf(&name, "-%c*", c);
	pushMacro(mb->mc, name, NULL, oarg, mb->level, ME_AUTO | ME_LITERAL);
	free(name);
    }
    return 0;
}

/**
 * Setup arguments for parameterized macro.
 * @todo Use popt rather than getopt to parse args.
 * @param mb		macro expansion state
 * @param me		macro entry slot
 * @param argv		parsed arguments for the macro
 */
static void
setupArgs(MacroBuf mb, const rpmMacroEntry me, ARGV_t argv)
{
    char *args = NULL;
    int argc;
    int ind;
    int c;

    /* Bump call depth on entry before first macro define */
    mb->level++;

    /* Setup macro name as %0 */
    pushMacro(mb->mc, "0", NULL, me->name, mb->level, ME_AUTO | ME_LITERAL);

    /*
     * The macro %* analoguous to the shell's $* means "Pass all non-macro
     * parameters." Consequently, there needs to be a macro that means "Pass all
     * (including macro parameters) options". This is useful for verifying
     * parameters during expansion and yet transparently passing all parameters
     * through for higher level processing (e.g. %description and/or %setup).
     * This is the (potential) justification for %{**} ...
    */
    args = argvJoin(argv + 1, " ");
    pushMacro(mb->mc, "**", NULL, args, mb->level, ME_AUTO | ME_LITERAL);
    free(args);

    argc = argvCount(argv);
    ind = rgetopt(argc, argv, me->opts, mbopt, mb);

    if (ind < 0) {
	mbErr(mb, 1, _("Unknown option %c in %s(%s)\n"), -ind,
		me->name, me->opts);
	goto exit;
    }

    /* Add argument count (remaining non-option items) as macro. */
    {	char *ac = NULL;
	rasprintf(&ac, "%d", (argc - ind));
	pushMacro(mb->mc, "#", NULL, ac, mb->level, ME_AUTO | ME_LITERAL);
	free(ac);
    }

    /* Add macro for each argument */
    if (argc - ind) {
	for (c = ind; c < argc; c++) {
	    char *name = NULL;
	    rasprintf(&name, "%d", (c - ind + 1));
	    pushMacro(mb->mc, name, NULL, argv[c], mb->level, ME_AUTO | ME_LITERAL);
	    free(name);
	}
    }

    /* Add concatenated unexpanded arguments as yet another macro. */
    args = argvJoin(argv + ind, " ");
    pushMacro(mb->mc, "*", NULL, args ? args : "", mb->level, ME_AUTO | ME_LITERAL);
    free(args);

exit:
    mb->me = me;
    mb->args = argv;
}

/**
 * Free parsed arguments for parameterized macro.
 * @param mb		macro expansion state
 */
static void
freeArgs(MacroBuf mb)
{
    rpmMacroContext mc = mb->mc;

    /* Delete dynamic macro definitions */
    for (int i = 0; i < mc->n; i++) {
	rpmMacroEntry me = mc->tab[i];
	assert(me);
	if (me->level < mb->level)
	    continue;
	/* Warn on defined but unused non-automatic, scoped macros */
	if (!(me->flags & (ME_AUTO|ME_USED))) {
	    mbErr(mb, 0, _("Macro %%%s defined but not used within scope\n"),
			me->name);
	    /* Only whine once */
	    me->flags |= ME_USED;
	}

	/* compensate if the slot is to go away */
	if (me->prev == NULL)
	    i--;
	popMacro(mc, me->name);
    }
    mb->level--;
    mb->args = NULL;
    mb->me = NULL;
}

static void splitQuoted(ARGV_t *av, const char * str, const char * seps)
{
    const int qchar = 0x1f; /* ASCII unit separator */
    const char *s = str;
    const char *start = str;
    int quoted = 0;

    while (start != NULL) {
	if (!quoted && strchr(seps, *s)) {
	    size_t slen = s - start;
	    /* quoted arguments are always kept, otherwise skip empty args */
	    if (slen > 0) {
		char *d, arg[slen + 1];
		const char *t;
		for (d = arg, t = start; t - start < slen; t++) {
		    if (*t == qchar)
			continue;
		    *d++ = *t;
		}
		arg[d - arg] = '\0';
		argvAdd(av, arg);
	    }
	    start = s + 1;
	}
	if (*s == qchar)
	    quoted = !quoted;
	else if (*s == '\0')
	    start = NULL;
	s++;
    }
}

/**
 * Parse arguments (to next new line) for parameterized macro.
 * @param mb		macro expansion state
 * @param me		macro entry slot
 * @param argvp		pointer to argv array to store the result
 * @param se		arguments to parse
 * @param lastc		stop parsing at lastc
 * @param splitargs	split into words
 * @return		address to continue parsing
 */
static const char *
grabArgs(MacroBuf mb, const rpmMacroEntry me, ARGV_t *argvp,
		const char * se, const char * lastc, int splitargs)
{
    if ((me->flags & ME_PARSE) != 0) {
	/* Always unsplit and unexpanded */
	argvAddN(argvp, se, lastc - se);
    } else {
	char *s = NULL;
	/* Expand possible macros in arguments */
	expandThis(mb, lastc == se ? "" : se, lastc - se, &s);
	if (splitargs)
	    splitQuoted(argvp, s, " \t");
	else
	    argvAdd(argvp, s);
	free(s);
    }
    return (*lastc == '\0') || (*lastc == '\n' && *(lastc-1) != '\\') ?
	   lastc : lastc + 1;
}

static void doBody(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    if (*argv[1]) {
	rpmMacroEntry *mep = findEntry(mb->mc, argv[1], 0, NULL);
	if (mep) {
	    mbAppendStr(mb, (*mep)->body);
	} else {
	    mbErr(mb, 1, _("no such macro: '%s'\n"), argv[1]);
	}
    }
}

static void doOutput(MacroBuf mb,  rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    int loglevel = RPMLOG_NOTICE; /* assume echo */
    if (rstreq("error", me->name)) {
	loglevel = RPMLOG_ERR;
	mb->error = 1;
    } else if (rstreq("warn", me->name)) {
	loglevel = RPMLOG_WARNING;
    }
    rpmlog(loglevel, "%s\n", argv[1]);
}

static void doLua(MacroBuf mb,  rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    rpmlua lua = NULL; /* Global state. */
    const char *scriptbuf = argv[1];
    char *printbuf;
    rpmMacroContext mc = mb->mc;
    rpmMacroEntry mbme = mb->me;
    int odepth = mc->depth;
    int olevel = mc->level;
    const char *opts = NULL;
    const char *name = NULL;
    ARGV_t args = NULL;

    if (mbme) {
	opts = mbme->opts;
	name = mbme->name;
	if (mb->args)
	    args = mb->args;
    }

    rpmluaPushPrintBuffer(lua);
    mc->depth = mb->depth;
    mc->level = mb->level;
    if (rpmluaRunScript(lua, scriptbuf, name, opts, args) == -1)
	mb->error = 1;
    mc->depth = odepth;
    mc->level = olevel;
    printbuf = rpmluaPopPrintBuffer(lua);
    if (printbuf) {
	mbAppendStr(mb, printbuf);
	free(printbuf);
    }
}

static void doSP(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    char *s = NULL;

    s = rstrscat(NULL, (*(me->name) == 'S') ? "%SOURCE" : "%PATCH", argv[1], NULL);
    expandMacro(mb, s, 0);
    free(s);
}

static void doUncompress(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    rpmCompressedMagic compressed = COMPRESSED_OTHER;
    char *b, *be, *buf = NULL;
    int c;

    if (!*argv[1])
	goto exit;
    buf = xstrdup(argv[1]);
    for (b = buf; (c = *b) && isblank(c);)
	b++;
    for (be = b; (c = *be) && !isblank(c);)
	be++;
    *be = '\0';

    if (*b == '\0')
	goto exit;

    if (rpmFileIsCompressed(b, &compressed))
	mb->error = 1;

    switch (compressed) {
    default:
    case COMPRESSED_NOT:
	expandMacro(mb, "%__cat ", 0);
	break;
    case COMPRESSED_OTHER:
	expandMacro(mb, "%__gzip -dc ", 0);
	break;
    case COMPRESSED_BZIP2:
	expandMacro(mb, "%__bzip2 -dc ", 0);
	break;
    case COMPRESSED_ZIP:
	expandMacro(mb, "%__unzip ", 0);
	break;
    case COMPRESSED_LZMA:
    case COMPRESSED_XZ:
	expandMacro(mb, "%__xz -dc ", 0);
	break;
    case COMPRESSED_LZIP:
	expandMacro(mb, "%__lzip -dc ", 0);
	break;
    case COMPRESSED_LRZIP:
	expandMacro(mb, "%__lrzip -dqo- ", 0);
	break;
    case COMPRESSED_7ZIP:
	expandMacro(mb, "%__7zip x ", 0);
	break;
    case COMPRESSED_ZSTD:
	expandMacro(mb, "%__zstd -dc ", 0);
	break;
    }
    mbAppendStr(mb, buf);

exit:
    free(buf);
}

static void doExpand(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    if (*argv[1])
	expandMacro(mb, argv[1], 0);
}

static void doVerbose(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    mbAppend(mb, rpmIsVerbose() ? '1' : '0');
}

static void doFoo(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    char *buf = NULL;
    char *b = NULL;

    if (argv && argv[1])
	buf = xstrdup(argv[1]);
    if (rstreq("basename", me->name)) {
	if ((b = strrchr(buf, '/')) == NULL)
	    b = buf;
	else
	    b++;
    } else if (rstreq("dirname", me->name)) {
	if ((b = strrchr(buf, '/')) != NULL)
	    *b = '\0';
	b = buf;
    } else if (rstreq("shrink", me->name)) {
	/*
	 * shrink body by removing all leading and trailing whitespaces and
	 * reducing intermediate whitespaces to a single space character.
	 */
	size_t i = 0, j = 0;
	size_t buflen = strlen(buf);
	int was_space = 0;
	while (i < buflen) {
	    if (risspace(buf[i])) {
		was_space = 1;
		i++;
		continue;
	    } else if (was_space) {
		was_space = 0;
		if (j > 0) /* remove leading blanks at all */
		    buf[j++] = ' ';
	    }
	    buf[j++] = buf[i++];
	}
	buf[j] = '\0';
	b = buf;
    } else if (rstreq("quote", me->name)) {
	char *quoted = NULL;
	rasprintf(&quoted, "%c%s%c", 0x1f, buf, 0x1f);
	free(buf);
	b = buf = quoted;
    } else if (rstreq("suffix", me->name)) {
	if ((b = strrchr(buf, '.')) != NULL)
	    b++;
    } else if (rstreq("expr", me->name)) {
	char *expr = rpmExprStrFlags(buf, 0);
	if (expr) {
	    free(buf);
	    b = buf = expr;
	} else {
	    mb->error = 1;
	}
    } else if (rstreq("url2path", me->name) || rstreq("u2p", me->name)) {
	(void)urlPath(buf, (const char **)&b);
	if (*b == '\0') b = "/";
    } else if (rstreq("getenv", me->name)) {
	b = getenv(buf);
    } else if (rstreq("getconfdir", me->name)) {
	free(buf);
	buf = xmalloc(MACROBUFSIZ);
	sprintf(buf, "%s", rpmConfigDir());
	b = buf;
    } else if (rstreq("getncpus", me->name)) {
	free(buf);
	buf = xmalloc(MACROBUFSIZ);
	sprintf(buf, "%u", getncpus());
	b = buf;
    } else if (rstreq("exists", me->name)) {
	b = (access(buf, F_OK) == 0) ? "1" : "0";
    }

    if (b) {
	mbAppendStr(mb, b);
    }
    free(buf);
}

static void doLoad(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    if (loadMacroFile(mb->mc, argv[1])) {
	mbErr(mb, 1, _("failed to load macro file %s\n"), argv[1]);
    }
}

static void doTrace(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed)
{
    mb->expand_trace = mb->macro_trace = mb->depth;
    if (mb->depth == 1) {
	print_macro_trace = mb->macro_trace;
	print_expand_trace = mb->expand_trace;
    }
}

static struct builtins_s {
    const char * name;
    macroFunc func;
    int nargs;
    int flags;
} const builtinmacros[] = {
    { "P",		doSP,		1,	0 },
    { "S",		doSP,		1,	0 },
    { "basename",	doFoo,		1,	0 },
    { "define",		doDef,		-1,	ME_PARSE },
    { "dirname",	doFoo,		1,	0 },
    { "dnl",		doDnl,		-1,	ME_PARSE },
    { "dump", 		doDump,		0,	0 },
    { "echo",		doOutput,	1,	0 },
    { "error",		doOutput,	1,	0 },
    { "exists",		doFoo,		1,	0 },
    { "expand",		doExpand,	1,	0 },
    { "expr",		doFoo,		1,	0 },
    { "getconfdir",	doFoo,		0,	0 },
    { "getenv",		doFoo,		1,	0 },
    { "getncpus",	doFoo,		0,	0 },
    { "global",		doGlobal,	-1,	ME_PARSE },
    { "load",		doLoad,		1,	0 },
    { "lua",		doLua,		-1,	ME_PARSE },
    { "macrobody",	doBody,		1,	0 },
    { "quote",		doFoo,		1,	0 },
    { "shrink",		doFoo,		1,	0 },
    { "suffix",		doFoo,		1,	0 },
    { "trace",		doTrace,	0,	0 },
    { "u2p",		doFoo,		1,	0 },
    { "uncompress",	doUncompress,	1,	0 },
    { "undefine",	doUndefine,	1,	0 },
    { "url2path",	doFoo,		1,	0 },
    { "verbose",	doVerbose,	0,	0 },
    { "warn",		doOutput,	1,	0 },
    { NULL,		NULL,		0 }
};

static const char *setNegateAndCheck(const char *str, int *pnegate, int *pchkexist) {

    *pnegate = 0;
    *pchkexist = 0;
    while ((*str == '?') || (*str == '!')) {
	if (*str == '!')
	    *pnegate = !*pnegate;
	else
	    (*pchkexist)++;
	str++;
    }
    return str;
}

/**
 * Find the end of a macro call
 * @param str           pointer to the character after the initial '%'
 * @return              pointer to the next character after the macro
 */
RPM_GNUC_INTERNAL
const char *findMacroEnd(const char *str)
{
    int c;
    if (*str == '(')
	return matchchar(str, *str, ')');
    if (*str == '{')
	return matchchar(str, *str, '}');
    if (*str == '[')
	return matchchar(str, *str, ']');
    while (*str == '?' || *str == '!')
	str++;
    if (*str == '-')				/* %-f */
	str++;
    while ((c = *str) && (risalnum(c) || c == '_'))
	str++;
    if (*str == '*' && str[1] == '*')		/* %** */
	str += 2;
    else if (*str == '*' || *str == '#')	/* %* and %# */
	str++;
    return str;
}

/**
 * Expand a single macro entry
 * @param mb		macro expansion state
 * @param me		macro entry slot
 * @param args		arguments for parametric macros
 * @param parsed	how many characters ME_PARSE parsed (or NULL)
 */
static void
doExpandThisMacro(MacroBuf mb, rpmMacroEntry me, ARGV_t args, size_t *parsed)
{
    rpmMacroEntry prevme = mb->me;
    ARGV_t prevarg = mb->args;

    /* Recursively expand body of macro */
    if (me->flags & ME_FUNC) {
	int nargs = argvCount(args) - 1;
	int needarg = (me->nargs != 0);
	int havearg = (nargs > 0);
	if (needarg != havearg) {
	    mbErr(mb, 1, "%%%s: %s\n", me->name, needarg ?
		    _("argument expected") : _("unexpected argument"));
	    goto exit;
	}
	me->func(mb, me, args, parsed);
    } else if (me->body && *me->body) {
	/* Setup args for "%name " macros with opts */
	if (args != NULL)
	    setupArgs(mb, me, args);

	if ((me->flags & ME_LITERAL) != 0)
	    mbAppendStr(mb, me->body);
	else
	    expandMacro(mb, me->body, 0);
	/* Free args for "%name " macros with opts */
	if (args != NULL)
	    freeArgs(mb);
    }
exit:
    mb->args = prevarg;
    mb->me = prevme;
}

/**
 * The main macro recursion loop.
 * @param mb		macro expansion state
 * @param src		string to expand
 * @param slen		length of string buffer
 * @return		0 on success, 1 on failure
 */
static int
expandMacro(MacroBuf mb, const char *src, size_t slen)
{
    rpmMacroEntry *mep;
    rpmMacroEntry me = NULL;
    const char *s = src, *se;
    const char *f, *fe;
    const char *g, *ge;
    size_t fn, gn;
    int c;
    int negate;
    const char * lastc;
    int chkexist;
    char *source = NULL;
    MacroExpansionData med;

    /*
     * Always make a (terminated) copy of the source string.
     * Beware: avoiding the copy when src is known \0-terminated seems like
     * an obvious opportunity for optimization, but doing that breaks
     * a special case of macro undefining itself.
     */
    if (!slen)
	slen = strlen(src);
    source = rstrndup(src, slen);
    s = source;

    if (mbInit(mb, &med, slen) != 0)
	goto exit;

    while (mb->error == 0 && (c = *s) != '\0') {
	s++;
	/* Copy text until next macro */
	switch (c) {
	case '%':
	    if (*s) {	/* Ensure not end-of-string. */
		if (*s != '%')
		    break;
		s++;	/* skip first % in %% */
	    }
	default:
	    mbAppend(mb, c);
	    continue;
	    break;
	}

	/* Expand next macro */
	f = fe = NULL;
	g = ge = NULL;
	if (mb->depth > 1)	/* XXX full expansion for outermost level */
	    med.tpos = mb->tpos;	/* save expansion pointer for printExpand */
	lastc = NULL;
	if ((se = findMacroEnd(s)) == NULL) {
	    mbErr(mb, 1, _("Unterminated %c: %s\n"), (char)*s, s);
	    continue;
	}

	switch (*s) {
	default:		/* %name substitution */
	    f = s = setNegateAndCheck(s, &negate, &chkexist);
	    fe = se;
	    /* For "%name " macros ... */
	    if ((c = *fe) && isblank(c))
		if ((lastc = strchr(fe,'\n')) == NULL)
		    lastc = strchr(fe, '\0');
	    break;
	case '(':		/* %(...) shell escape */
	    if (mb->macro_trace)
		printMacro(mb, s, se);
	    s++;	/* skip ( */
	    doShellEscape(mb, s, (se - 1 - s));
	    s = se;
	    continue;
	case '[':		/* %[...] expression expansion */
	    if (mb->macro_trace)
		printMacro(mb, s, se);
	    s++;	/* skip [ */
	    doExpressionExpansion(mb, s, (se - 1 - s));
	    s = se;
	    continue;
	case '{':		/* %{...}/%{...:...} substitution */
	    f = s+1;	/* skip { */
	    f = setNegateAndCheck(f, &negate, &chkexist);
	    for (fe = f; (c = *fe) && !strchr(" :}", c);)
		fe++;
	    switch (c) {
	    case ':':
		g = fe + 1;
		ge = se - 1;
		break;
	    case ' ':
		lastc = se-1;
		break;
	    default:
		break;
	    }
	    break;
	}

	/* XXX Everything below expects fe > f */
	fn = (fe - f);
	gn = (ge - g);
	if ((fe - f) <= 0) {
	    /* XXX Process % in unknown context */
	    c = '%';	/* XXX only need to save % */
	    mbAppend(mb, c);
#if 0
	    mbErr(mb, 1,
		    _("A %% is followed by an unparseable macro\n"));
#endif
	    continue;
	}

	if (mb->macro_trace)
	    printMacro(mb, s, se);

	/* Expand defined macros */
	mep = findEntry(mb->mc, f, fn, NULL);
	me = (mep ? *mep : NULL);

	if (me) {
	    if ((me->flags & ME_AUTO) && mb->level > me->level) {
		/* Ignore out-of-scope automatic macros */
		me = NULL;
	    } else {
		/* If we looked up a macro, consider it used */
		me->flags |= ME_USED;
	    }
	}

	/* XXX Special processing for flags and existance test */
	if (*f == '-' || chkexist) {
	    if ((me == NULL && !negate) ||	/* Without existance, skip %{?...} */
		    (me != NULL && negate)) {	/* With existance, skip %{!?...} */
		s = se;
		continue;
	    }

	    if (g && g < ge) {		/* Expand X in %{...:X} */
		expandMacro(mb, g, gn);
	    } else if (me) {
		doExpandThisMacro(mb, me, NULL, NULL);
	    }
	    s = se;
	    continue;
	}

	if (me == NULL) {	/* leave unknown %... as is */
	    /* XXX hack to permit non-overloaded %foo to be passed */
	    c = '%';	/* XXX only need to save % */
	    mbAppend(mb, c);
	    continue;
	}

	/* Grab args for parametric macros */
	ARGV_t args = NULL;
	size_t fwd = 0;
	if (me->opts != NULL) {
	    argvAdd(&args, me->name);
	    if (g) {
		grabArgs(mb, me, &args, g, ge, 0);
	    } else if (me->flags & ME_PARSE) {
		argvAdd(&args, se);
	    } else if (lastc) {
		fwd = (grabArgs(mb, me, &args, fe, lastc, 1) - se);
	    }
	}
	doExpandThisMacro(mb, me, args, &fwd);
	if (args != NULL)
	    argvFree(args);
	s = se + (g ? 0 : fwd);
    }

    mbFini(mb, me, &med);
exit:
    _free(source);
    return mb->error;
}

/**
 * Expand a single macro
 * @param mb		macro expansion state
 * @param me		macro entry slot
 * @param args		arguments for parametric macros
 * @param flags		expandsion flags
 * @return		0 on success, 1 on failure
 */
static int
expandThisMacro(MacroBuf mb, rpmMacroEntry me, ARGV_const_t args, int flags)
{
    MacroExpansionData med;
    ARGV_t optargs = NULL;

    if (mbInit(mb, &med, 0) != 0)
	goto exit;

    if (mb->macro_trace) {
	ARGV_const_t av = args;
	fprintf(stderr, "%3d>%*s (%%%s)", mb->depth, (2 * mb->depth + 1), "", me->name);
	for (av = args; av && *av; av++)
	    fprintf(stderr, " %s", *av);
	fprintf(stderr, "\n");
    }

    /* prepare arguments for parametric macros */
    if (me->opts) {
	argvAdd(&optargs, me->name);
	if ((flags & RPMEXPAND_EXPAND_ARGS) != 0) {
	    ARGV_const_t av = args;
	    for (av = args; av && *av; av++) {
		char *s = NULL;
		expandThis(mb, *av, 0, &s);
		argvAdd(&optargs, s);
		free(s);
	    }
	} else {
	    argvAppend(&optargs, args);
	}
    }

    doExpandThisMacro(mb, me, optargs, NULL);
    if (optargs)
	argvFree(optargs);

    mbFini(mb, me, &med);
exit:
    return mb->error;
}

/* =============================================================== */

static int doExpandMacros(rpmMacroContext mc, const char *src, int flags,
			char **target)
{
    MacroBuf mb = mbCreate(mc, flags);
    int rc = 0;

    rc = expandMacro(mb, src, 0);

    mb->buf[mb->tpos] = '\0';	/* XXX just in case */
    /* expanded output is usually much less than alloced buffer, downsize */
    *target = xrealloc(mb->buf, mb->tpos + 1);

    _free(mb);
    return rc;
}

static void pushMacroAny(rpmMacroContext mc,
	const char * n, const char * o, const char * b,
	macroFunc f, int nargs, int level, int flags)
{
    /* new entry */
    rpmMacroEntry me;
    /* pointer into me */
    char *p;
    /* calculate sizes */
    size_t olen = o ? strlen(o) : 0;
    size_t blen = b ? strlen(b) : 0;
    size_t mesize = sizeof(*me) + blen + 1 + (olen ? olen + 1 : 0);

    size_t pos;
    rpmMacroEntry *mep = findEntry(mc, n, 0, &pos);
    if (mep) {
	/* entry with shared name */
	me = xmalloc(mesize);
	/* copy body */
	me->body = p = me->arena;
	if (blen)
	    memcpy(p, b, blen + 1);
	else
	    *p = '\0';
	p += blen + 1;
	/* set name */
	me->name = (*mep)->name;
    }
    else {
	/* extend macro table */
	const int delta = 256;
	if (mc->n % delta == 0)
	    mc->tab = xrealloc(mc->tab, sizeof(me) * (mc->n + delta));
	/* shift pos+ entries to the right */
	memmove(mc->tab + pos + 1, mc->tab + pos, sizeof(me) * (mc->n - pos));
	mc->n++;
	/* make slot */
	mc->tab[pos] = NULL;
	mep = &mc->tab[pos];
	/* entry with new name */
	size_t nlen = strlen(n);
	me = xmalloc(mesize + nlen + 1);
	/* copy body */
	me->body = p = me->arena;
	if (blen)
	    memcpy(p, b, blen + 1);
	else
	    *p = '\0';
	p += blen + 1;
	/* copy name */
	me->name = memcpy(p, n, nlen + 1);
	p += nlen + 1;
    }

    /* copy options */
    if (olen)
	me->opts = memcpy(p, o, olen + 1);
    else
	me->opts = o ? "" : NULL;
    /* initialize */
    me->func = f;
    me->nargs = nargs;
    me->flags = flags;
    me->flags &= ~(ME_USED);
    me->level = level;
    /* push over previous definition */
    me->prev = *mep;
    *mep = me;
}

static void pushMacro(rpmMacroContext mc,
	const char * n, const char * o, const char * b, int level, int flags)
{
    return pushMacroAny(mc, n, o, b, NULL, 0, level, flags);
}

static void popMacro(rpmMacroContext mc, const char * n)
{
    size_t pos;
    rpmMacroEntry *mep = findEntry(mc, n, 0, &pos);
    if (mep == NULL)
	return;
    /* parting entry */
    rpmMacroEntry me = *mep;
    assert(me);
    /* detach/pop definition */
    mc->tab[pos] = me->prev;
    /* shrink macro table */
    if (me->prev == NULL) {
	mc->n--;
	/* move pos+ elements to the left */
	memmove(mc->tab + pos, mc->tab + pos + 1, sizeof(me) * (mc->n - pos));
	/* deallocate */
	if (mc->n == 0)
	    mc->tab = _free(mc->tab);
    }
    /* comes in a single chunk */
    free(me);
}

static int defineMacro(rpmMacroContext mc, const char * macro, int level)
{
    MacroBuf mb = xcalloc(1, sizeof(*mb));
    int rc;

    /* XXX just enough to get by */
    mb->mc = mc;
    doDefine(mb, macro, level, 0, NULL);
    rc = mb->error;
    _free(mb);
    return rc;
}

static int loadMacroFile(rpmMacroContext mc, const char * fn)
{
    FILE *fd = fopen(fn, "r");
    size_t blen = MACROBUFSIZ;
    char *buf = xmalloc(blen);
    int rc = -1;
    int nfailed = 0;
    int lineno = 0;
    int nlines = 0;

    if (fd == NULL)
	goto exit;

    pushMacro(mc, "__file_name", NULL, fn, RMIL_MACROFILES, ME_LITERAL);

    buf[0] = '\0';
    while ((nlines = rdcl(buf, blen, fd)) > 0) {
	char c, *n;
	char lnobuf[16];

	lineno += nlines;
	n = buf;
	SKIPBLANK(n, c);

	if (c != '%')
		continue;
	n++;	/* skip % */

	snprintf(lnobuf, sizeof(lnobuf), "%d", lineno);
	pushMacro(mc, "__file_lineno", NULL, lnobuf, RMIL_MACROFILES, ME_LITERAL);
	if (defineMacro(mc, n, RMIL_MACROFILES))
	    nfailed++;
	popMacro(mc, "__file_lineno");
    }
    fclose(fd);
    popMacro(mc, "__file_name");

    rc = (nfailed > 0);

exit:
    _free(buf);
    return rc;
}

static void copyMacros(rpmMacroContext src, rpmMacroContext dst, int level)
{
    for (int i = 0; i < src->n; i++) {
	rpmMacroEntry me = src->tab[i];
	assert(me);
	pushMacro(dst, me->name, me->opts, me->body, level, me->flags);
    }
}

/* External interfaces */

int rpmExpandMacros(rpmMacroContext mc, const char * sbuf, char ** obuf, int flags)
{
    char *target = NULL;
    int rc;

    mc = rpmmctxAcquire(mc);
    rc = doExpandMacros(mc, sbuf, flags, &target);
    rpmmctxRelease(mc);

    if (rc) {
	free(target);
	return -1;
    } else {
	*obuf = target;
	return 1;
    }
}

int rpmExpandThisMacro(rpmMacroContext mc, const char *n,  ARGV_const_t args, char ** obuf, int flags)
{
    rpmMacroEntry *mep;
    char *target = NULL;
    int rc = 1; /* assume failure */

    mc = rpmmctxAcquire(mc);
    mep = findEntry(mc, n, 0, NULL);
    if (mep) {
	MacroBuf mb = mbCreate(mc, flags);
	rc = expandThisMacro(mb, *mep, args, flags);
	mb->buf[mb->tpos] = '\0';	/* XXX just in case */
	target = xrealloc(mb->buf, mb->tpos + 1);
	_free(mb);
    }
    rpmmctxRelease(mc);
    if (rc) {
	free(target);
	return -1;
    } else {
	*obuf = target;
	return 1;
    }
}

void
rpmDumpMacroTable(rpmMacroContext mc, FILE * fp)
{
    mc = rpmmctxAcquire(mc);
    if (fp == NULL) fp = stderr;
    
    fprintf(fp, "========================\n");
    for (int i = 0; i < mc->n; i++) {
	rpmMacroEntry me = mc->tab[i];
	assert(me);
	fprintf(fp, "%3d%c %s", me->level,
		    ((me->flags & ME_USED) ? '=' : ':'), me->name);
	if (me->opts && *me->opts)
		fprintf(fp, "(%s)", me->opts);
	if (me->body && *me->body)
		fprintf(fp, "\t%s", me->body);
	fprintf(fp, "\n");
    }
    fprintf(fp, _("======================== active %d empty %d\n"),
		mc->n, 0);
    rpmmctxRelease(mc);
}

int rpmPushMacroFlags(rpmMacroContext mc,
	      const char * n, const char * o, const char * b,
	      int level, rpmMacroFlags flags)
{
    mc = rpmmctxAcquire(mc);
    pushMacro(mc, n, o, b, level, flags & RPMMACRO_LITERAL ? ME_LITERAL : ME_NONE);
    rpmmctxRelease(mc);
    return 0;
}

int rpmPushMacro(rpmMacroContext mc,
	      const char * n, const char * o, const char * b, int level)
{
    return rpmPushMacroFlags(mc, n, o, b, level, RPMMACRO_DEFAULT);
}

int rpmPopMacro(rpmMacroContext mc, const char * n)
{
    mc = rpmmctxAcquire(mc);
    popMacro(mc, n);
    rpmmctxRelease(mc);
    return 0;
}

int
rpmDefineMacro(rpmMacroContext mc, const char * macro, int level)
{
    int rc;
    mc = rpmmctxAcquire(mc);
    rc = defineMacro(mc, macro, level);
    rpmmctxRelease(mc);
    return rc;
}

int rpmMacroIsDefined(rpmMacroContext mc, const char *n)
{
    int defined = 0;
    if ((mc = rpmmctxAcquire(mc)) != NULL) {
	if (findEntry(mc, n, 0, NULL))
	    defined = 1;
	rpmmctxRelease(mc);
    }
    return defined;
}

int rpmMacroIsParametric(rpmMacroContext mc, const char *n)
{
    int parametric = 0;
    if ((mc = rpmmctxAcquire(mc)) != NULL) {
	rpmMacroEntry *mep = findEntry(mc, n, 0, NULL);
	if (mep && (*mep)->opts)
	    parametric = 1;
	rpmmctxRelease(mc);
    }
    return parametric;
}

void
rpmLoadMacros(rpmMacroContext mc, int level)
{
    rpmMacroContext gmc;
    if (mc == NULL || mc == rpmGlobalMacroContext)
	return;

    gmc = rpmmctxAcquire(NULL);
    mc = rpmmctxAcquire(mc);

    copyMacros(mc, gmc, level);

    rpmmctxRelease(mc);
    rpmmctxRelease(gmc);
}

int
rpmLoadMacroFile(rpmMacroContext mc, const char * fn)
{
    int rc;

    mc = rpmmctxAcquire(mc);
    rc = loadMacroFile(mc, fn);
    rpmmctxRelease(mc);

    return rc;
}

void
rpmInitMacros(rpmMacroContext mc, const char * macrofiles)
{
    ARGV_t pattern, globs = NULL;
    rpmMacroContext climc;
    mc = rpmmctxAcquire(mc);

    /* Define built-in macros */
    for (const struct builtins_s *b = builtinmacros; b->name; b++) {
	pushMacroAny(mc, b->name, "", "<builtin>", b->func, b->nargs,
			RMIL_BUILTIN, b->flags | ME_FUNC);
    }

    argvSplit(&globs, macrofiles, ":");
    for (pattern = globs; pattern && *pattern; pattern++) {
	ARGV_t path, files = NULL;
    
	/* Glob expand the macro file path element, expanding ~ to $HOME. */
	if (rpmGlob(*pattern, NULL, &files) != 0) {
	    continue;
	}

	/* Read macros from each file. */
	for (path = files; *path; path++) {
	    if (rpmFileHasSuffix(*path, ".rpmnew") || 
		rpmFileHasSuffix(*path, ".rpmsave") ||
		rpmFileHasSuffix(*path, ".rpmorig")) {
		continue;
	    }
	    (void) loadMacroFile(mc, *path);
	}
	argvFree(files);
    }
    argvFree(globs);

    /* Reload cmdline macros */
    climc = rpmmctxAcquire(rpmCLIMacroContext);
    copyMacros(climc, mc, RMIL_CMDLINE);
    rpmmctxRelease(climc);

    rpmmctxRelease(mc);
}

void
rpmFreeMacros(rpmMacroContext mc)
{
    mc = rpmmctxAcquire(mc);
    while (mc->n > 0) {
	/* remove from the end to avoid memmove */
	rpmMacroEntry me = mc->tab[mc->n - 1];
	popMacro(mc, me->name);
    }
    rpmmctxRelease(mc);
}

char * 
rpmExpand(const char *arg, ...)
{
    size_t blen = 0;
    char *buf = NULL, *ret = NULL;
    char *pe;
    const char *s;
    va_list ap;
    rpmMacroContext mc;

    if (arg == NULL) {
	ret = xstrdup("");
	goto exit;
    }

    /* precalculate unexpanded size */
    va_start(ap, arg);
    for (s = arg; s != NULL; s = va_arg(ap, const char *))
	blen += strlen(s);
    va_end(ap);

    buf = xmalloc(blen + 1);
    buf[0] = '\0';

    va_start(ap, arg);
    for (pe = buf, s = arg; s != NULL; s = va_arg(ap, const char *))
	pe = stpcpy(pe, s);
    va_end(ap);

    mc = rpmmctxAcquire(NULL);
    (void) doExpandMacros(mc, buf, 0, &ret);
    rpmmctxRelease(mc);

    free(buf);
exit:
    return ret;
}

int
rpmExpandNumeric(const char *arg)
{
    char *val;
    int rc;

    if (arg == NULL)
	return 0;

    val = rpmExpand(arg, NULL);
    if (!(val && *val != '%'))
	rc = 0;
    else if (*val == 'Y' || *val == 'y')
	rc = 1;
    else if (*val == 'N' || *val == 'n')
	rc = 0;
    else {
	char *end;
	rc = strtol(val, &end, 0);
	if (!(end && *end == '\0'))
	    rc = 0;
    }
    free(val);

    return rc;
}
