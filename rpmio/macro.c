/** \ingroup rpmrc rpmio
 * \file rpmio/macro.c
 */

#include "system.h"
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#if HAVE_SCHED_GETAFFINITY
#include <sched.h>
#endif

#if !defined(isblank)
#define	isblank(_c)	((_c) == ' ' || (_c) == '\t')
#endif
#define	iseol(_c)	((_c) == '\n' || (_c) == '\r')

#define	STREQ(_t, _f, _fn)	((_fn) == (sizeof(_t)-1) && rstreqn((_t), (_f), (_fn)))

#define MACROBUFSIZ (BUFSIZ * 2)

#include <rpm/rpmio.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/argv.h>

#ifdef	WITH_LUA
#include "rpmio/rpmlua.h"
#endif

#include "debug.h"

enum macroFlags_e {
    ME_NONE	= 0,
    ME_AUTO	= (1 << 0),
    ME_USED	= (1 << 1),
};

/*! The structure used to store a macro. */
struct rpmMacroEntry_s {
    struct rpmMacroEntry_s *prev;/*!< Macro entry stack. */
    const char *name;  	/*!< Macro name. */
    const char *opts;  	/*!< Macro parameters (a la getopt) */
    const char *body;	/*!< Macro body. */
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
typedef struct MacroBuf_s {
    char * buf;			/*!< Expansion buffer. */
    size_t tpos;		/*!< Current position in expansion buffer */
    size_t nb;			/*!< No. bytes remaining in expansion buffer. */
    int depth;			/*!< Current expansion depth. */
    int level;			/*!< Current scoping level */
    int error;			/*!< Errors encountered during expansion? */
    int macro_trace;		/*!< Pre-print macro to expand? */
    int expand_trace;		/*!< Post-print macro expansion? */
    int escape;			/*!< Preserve '%%' during expansion? */
    int flags;			/*!< Flags to control behavior */
    rpmMacroContext mc;
} * MacroBuf;

#define	_MAX_MACRO_DEPTH	64
static int max_macro_depth = _MAX_MACRO_DEPTH;

#define	_PRINT_MACRO_TRACE	0
static int print_macro_trace = _PRINT_MACRO_TRACE;

#define	_PRINT_EXPAND_TRACE	0
static int print_expand_trace = _PRINT_EXPAND_TRACE;

typedef void (*macroFunc)(MacroBuf mb, int chkexist, int negate,
			const char * f, size_t fn, const char * g, size_t gn);
typedef const char *(*parseFunc)(MacroBuf mb, const char * se, size_t slen);

/* forward ref */
static int expandMacro(MacroBuf mb, const char *src, size_t slen);
static void pushMacro(rpmMacroContext mc,
	const char * n, const char * o, const char * b, int level, int flags);
static void popMacro(rpmMacroContext mc, const char * n);
static int loadMacroFile(rpmMacroContext mc, const char * fn);
static void doFoo(MacroBuf mb, int chkexist, int negate,
		    const char * f, size_t fn, const char * g, size_t gn);
static void doLoad(MacroBuf mb, int chkexist, int negate,
		    const char * f, size_t fn, const char * g, size_t gn);
static void doLua(MacroBuf mb, int chkexist, int negate,
		    const char * f, size_t fn, const char * g, size_t gn);
static void doOutput(MacroBuf mb, int chkexist, int negate,
		    const char * f, size_t fn, const char * g, size_t gn);
static void doTrace(MacroBuf mb, int chkexist, int negate,
		    const char * f, size_t fn, const char * g, size_t gn);

static const char * doDef(MacroBuf mb, const char * se, size_t slen);
static const char * doGlobal(MacroBuf mb, const char * se, size_t slen);
static const char * doDump(MacroBuf mb, const char * se, size_t slen);
static const char * doUndefine(MacroBuf mb, const char * se, size_t slen);
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
    char *q = buf - 1;		/* initialize just before buffer. */
    size_t nb = 0;
    size_t nread = 0;
    int pc = 0, bc = 0;
    int nlines = 0;
    char *p = buf;

    if (f != NULL)
    do {
	*(++q) = '\0';			/* terminate and move forward. */
	if (fgets(q, size, f) == NULL)	/* read next line. */
	    break;
	nlines++;
	nb = strlen(q);
	nread += nb;			/* trim trailing \r and \n */
	for (q += nb - 1; nb > 0 && iseol(*q); q--)
	    nb--;
	for (; p <= q; p++) {
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
			case '%': p++; break;
		    }
		    break;
		case '{': if (bc > 0) bc++; break;
		case '}': if (bc > 0) bc--; break;
		case '(': if (pc > 0) pc++; break;
		case ')': if (pc > 0) pc--; break;
	    }
	}
	if (nb == 0 || (*q != '\\' && !bc && !pc) || *(q+1) == '\0') {
	    *(++q) = '\0';		/* trim trailing \r, \n */
	    break;
	}
	q++; nb++;			/* copy newline too */
	size -= nb;
	if (*q == '\r')			/* XXX avoid \r madness */
	    *q = '\n';
    } while (size > 0);
    return nlines;
}

/**
 * Return text between pl and matching pr characters.
 * @param p		start of text
 * @param pl		left char, i.e. '[', '(', '{', etc.
 * @param pr		right char, i.e. ']', ')', '}', etc.
 * @return		address of last char before pr (or NULL)
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
	    if (--lvl <= 0)	return --p;
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
printExpansion(MacroBuf mb, const char * t, const char * te)
{
    if (!(te > t)) {
	rpmlog(RPMLOG_DEBUG, _("%3d<%*s(empty)\n"), mb->depth, (2 * mb->depth + 1), "");
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

    rpmlog(RPMLOG_DEBUG,"%3d<%*s", mb->depth, (2 * mb->depth + 1), "");
    if (te > t)
	rpmlog(RPMLOG_DEBUG, "%.*s", (int)(te - t), t);
    rpmlog(RPMLOG_DEBUG, "\n");
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
 * @retval target	pointer to expanded string (malloced)
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

static const char * doDnl(MacroBuf mb, const char * se, size_t slen)
{
    const char *s = se;
    while (*s && !iseol(*s))
	s++;
    return (*s != '\0') ? s + 1 : s;
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
	mb->error = 1;
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

#define STR_AND_LEN(_str) (_str), sizeof((_str))-1

/* Names in the table must be in ASCII-code order */
static struct builtins_s {
    const char * name;
    size_t len;
    macroFunc func;
    parseFunc parse;
} const builtinmacros[] = {
    { STR_AND_LEN("F"),		doFoo,		NULL },
    { STR_AND_LEN("P"),		doFoo,		NULL },
    { STR_AND_LEN("S"),		doFoo,		NULL },
    { STR_AND_LEN("basename"),	doFoo,		NULL },
    { STR_AND_LEN("define"),	NULL,		doDef },
    { STR_AND_LEN("dirname"),	doFoo,		NULL },
    { STR_AND_LEN("dnl"),	NULL,		doDnl },
    { STR_AND_LEN("dump"), 	NULL,		doDump },
    { STR_AND_LEN("echo"),	doOutput,	NULL },
    { STR_AND_LEN("error"),	doOutput,	NULL },
    { STR_AND_LEN("expand"),	doFoo,		NULL },
    { STR_AND_LEN("expr"),	doFoo,		NULL },
    { STR_AND_LEN("getconfdir"),doFoo,		NULL },
    { STR_AND_LEN("getenv"),	doFoo,		NULL },
    { STR_AND_LEN("getncpus"),	doFoo,		NULL },
    { STR_AND_LEN("global"),	NULL,		doGlobal },
    { STR_AND_LEN("load"),	doLoad,		NULL },
    { STR_AND_LEN("lua"),	doLua,		NULL },
    { STR_AND_LEN("quote"),	doFoo,		NULL },
    { STR_AND_LEN("shrink"),	doFoo,		NULL },
    { STR_AND_LEN("suffix"),	doFoo,		NULL },
    { STR_AND_LEN("trace"),	doTrace,	NULL },
    { STR_AND_LEN("u2p"),	doFoo,		NULL },
    { STR_AND_LEN("uncompress"),doFoo,		NULL },
    { STR_AND_LEN("undefine"),	NULL,		doUndefine },
    { STR_AND_LEN("url2path"),	doFoo,		NULL },
    { STR_AND_LEN("verbose"),	doFoo,		NULL },
    { STR_AND_LEN("warn"),	doOutput,	NULL },
};
static const size_t numbuiltins = sizeof(builtinmacros)/sizeof(*builtinmacros);

static int namecmp(const void *name1, const void *name2)
{
    struct builtins_s *n1 = (struct builtins_s *)name1;
    struct builtins_s *n2 = (struct builtins_s *)name2;

    int rc = strncmp(n1->name, n2->name, n1->len);
    if (rc == 0)
	rc = n1->len - n2->len;
    return rc;
}

/**
 * Return a pointer to the built-in macro with the given name
 * @param name		macro name
 * @param nlen		name length
 * @return		pointer to the built-in macro or NULL if not found
 */
static const struct builtins_s* lookupBuiltin(const char *name, size_t nlen)
{
    struct builtins_s macro = {
	.name = name,
	.len = nlen,
    };

    return bsearch(&macro, builtinmacros, numbuiltins, sizeof(*builtinmacros),
		    namecmp);
}

static const int
validName(MacroBuf mb, const char *name, size_t namelen, const char *action)
{
    int rc = 0;
    int c;

    /* Names must start with alphabetic or _ and be at least 3 chars */
    if (!((c = *name) && (risalpha(c) || c == '_') && (namelen) > 2)) {
	mbErr(mb, 1, _("Macro %%%s has illegal name (%s)\n"), name, action);
	goto exit;
    }

    if (lookupBuiltin(name, namelen)) {
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
 * @param slen		length of se argument
 * @param level		macro recursion level
 * @param expandbody	should body be expanded?
 * @return		address to continue parsing
 */
static const char *
doDefine(MacroBuf mb, const char * se, size_t slen, int level, int expandbody)
{
    const char *s = se;
    char *buf = xmalloc(slen + 3); /* Some leeway for termination issues... */
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
	strncpy(b, s, (se - s));
	b[se - s] = '\0';
	be += strlen(b);
	se++;	/* XXX skip } */
	s = se;	/* move scan forward */
    } else {	/* otherwise free-field */
	int bc = 0, pc = 0;
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
			case '%': *be++ = *s++; break;
		    }
		    break;
		case '{': if (bc > 0) bc++; break;
		case '}': if (bc > 0) bc--; break;
		case '(': if (pc > 0) pc++; break;
		case ')': if (pc > 0) pc--; break;
	    }
	    *be++ = *s++;
	}
	*be = '\0';

	if (bc || pc) {
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
    return se;
}

/**
 * Parse (and execute) macro undefinition.
 * @param mb		macro expansion state
 * @param se		macro name to undefine
 * @param slen		length of se argument
 * @return		address to continue parsing
 */
static const char *
doUndefine(MacroBuf mb, const char * se, size_t slen)
{
    const char *s = se;
    char *buf = xmalloc(slen + 1);
    char *n = buf, *ne = n;
    int c;

    COPYNAME(ne, s, c);

    /* Move scan over body */
    while (iseol(*s))
	s++;
    se = s;

    if (!validName(mb, n, ne - n, "%undefine")) {
	mb->error = 1;
	goto exit;
    }

    popMacro(mb->mc, n);

exit:
    _free(buf);
    return se;
}

static const char * doDef(MacroBuf mb, const char * se, size_t slen)
{
    return doDefine(mb, se, slen, mb->level, 0);
}

static const char * doGlobal(MacroBuf mb, const char * se, size_t slen)
{
    return doDefine(mb, se, slen, RMIL_GLOBAL, 1);
}

static const char * doDump(MacroBuf mb, const char * se, size_t slen)
{
    rpmDumpMacroTable(mb->mc, NULL);
    while (iseol(*se))
	se++;
    return se;
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
 * @todo Use popt rather than getopt to parse args.
 * @param mb		macro expansion state
 * @param me		macro entry slot
 * @param se		arguments to parse
 * @param lastc		stop parsing at lastc
 * @return		address to continue parsing
 */
static const char *
grabArgs(MacroBuf mb, const rpmMacroEntry me, const char * se,
		const char * lastc)
{
    const char *cont = NULL;
    const char *opts;
    char *args = NULL;
    ARGV_t argv = NULL;
    int argc = 0;
    int c;

    /* 
     * Prepare list of call arguments, starting with macro name as argv[0].
     * Make a copy of se up to lastc string that we can pass to argvSplit().
     * Append the results to main argv. 
     */
    argvAdd(&argv, me->name);
    if (lastc) {
	int oescape = mb->escape;
	char *s = NULL;

	/* Expand possible macros in arguments */
	mb->escape = 1;
	expandThis(mb, se, lastc-se, &s);
	mb->escape = oescape;

	splitQuoted(&argv, s, " \t");
	free(s);

	cont = ((*lastc == '\0' || *lastc == '\n') && *(lastc-1) != '\\') ?
	       lastc : lastc + 1;
    }

    /* Bump call depth on entry before first macro define */
    mb->level++;

    /* Setup macro name as %0 */
    pushMacro(mb->mc, "0", NULL, me->name, mb->level, ME_AUTO);

    /*
     * The macro %* analoguous to the shell's $* means "Pass all non-macro
     * parameters." Consequently, there needs to be a macro that means "Pass all
     * (including macro parameters) options". This is useful for verifying
     * parameters during expansion and yet transparently passing all parameters
     * through for higher level processing (e.g. %description and/or %setup).
     * This is the (potential) justification for %{**} ...
    */
    args = argvJoin(argv + 1, " ");
    pushMacro(mb->mc, "**", NULL, args, mb->level, ME_AUTO);
    free(args);

    /*
     * POSIX states optind must be 1 before any call but glibc uses 0
     * to (re)initialize getopt structures, eww.
     */
#ifdef __GLIBC__
    optind = 0;
#else
    optind = 1;
#endif

    opts = me->opts;
    argc = argvCount(argv);

    /* Define option macros. */
    while ((c = getopt(argc, argv, opts)) != -1)
    {
	char *name = NULL, *body = NULL;
	if (c == '?' || strchr(opts, c) == NULL) {
	    mbErr(mb, 1, _("Unknown option %c in %s(%s)\n"),
			(char)optopt, me->name, opts);
	    goto exit;
	}

	rasprintf(&name, "-%c", c);
	if (optarg) {
	    rasprintf(&body, "-%c %s", c, optarg);
	} else {
	    rasprintf(&body, "-%c", c);
	}
	pushMacro(mb->mc, name, NULL, body, mb->level, ME_AUTO);
	free(name);
	free(body);

	if (optarg) {
	    rasprintf(&name, "-%c*", c);
	    pushMacro(mb->mc, name, NULL, optarg, mb->level, ME_AUTO);
	    free(name);
	}
    }

    /* Add argument count (remaining non-option items) as macro. */
    {	char *ac = NULL;
    	rasprintf(&ac, "%d", (argc - optind));
	pushMacro(mb->mc, "#", NULL, ac, mb->level, ME_AUTO);
	free(ac);
    }

    /* Add macro for each argument */
    if (argc - optind) {
	for (c = optind; c < argc; c++) {
	    char *name = NULL;
	    rasprintf(&name, "%d", (c - optind + 1));
	    pushMacro(mb->mc, name, NULL, argv[c], mb->level, ME_AUTO);
	    free(name);
	}
    }

    /* Add concatenated unexpanded arguments as yet another macro. */
    args = argvJoin(argv + optind, " ");
    pushMacro(mb->mc, "*", NULL, args ? args : "", mb->level, ME_AUTO);
    free(args);

exit:
    argvFree(argv);
    return cont;
}

static void doOutput(MacroBuf mb, int chkexist, int negate, const char * f, size_t fn, const char * g, size_t gn)
{
    char *buf = NULL;
    int loglevel = RPMLOG_NOTICE; /* assume echo */
    if (STREQ("error", f, fn)) {
	loglevel = RPMLOG_ERR;
	mb->error = 1;
    } else if (STREQ("warn", f, fn)) {
	loglevel = RPMLOG_WARNING;
    }
    if (gn == 0)
	g = "";

    (void) expandThis(mb, g, gn, &buf);
    rpmlog(loglevel, "%s\n", buf);
    _free(buf);
}

static void doLua(MacroBuf mb, int chkexist, int negate, const char * f, size_t fn, const char * g, size_t gn)
{
#ifdef WITH_LUA
    rpmlua lua = NULL; /* Global state. */
    char *scriptbuf = xmalloc(gn + 1);
    char *printbuf;
    rpmMacroContext mc = mb->mc;
    int odepth = mc->depth;
    int olevel = mc->level;

    if (g != NULL && gn > 0)
	memcpy(scriptbuf, g, gn);
    scriptbuf[gn] = '\0';
    rpmluaPushPrintBuffer(lua);
    mc->depth = mb->depth;
    mc->level = mb->level;
    if (rpmluaRunScript(lua, scriptbuf, NULL) == -1)
	mb->error = 1;
    mc->depth = odepth;
    mc->level = olevel;
    printbuf = rpmluaPopPrintBuffer(lua);
    if (printbuf) {
	mbAppendStr(mb, printbuf);
	free(printbuf);
    }
    free(scriptbuf);
#else
    mbErr(mb, 1, _("<lua> scriptlet support not built in\n"));
#endif
}

/**
 * Execute macro primitives.
 * @param mb		macro expansion state
 * @param chkexist	unused
 * @param negate	should logic be inverted?
 * @param f		beginning of field f
 * @param fn		length of field f
 * @param g		beginning of field g
 * @param gn		length of field g
 */
static void
doFoo(MacroBuf mb, int chkexist, int negate, const char * f, size_t fn,
		const char * g, size_t gn)
{
    char *buf = NULL;
    char *b = NULL, *be;
    int c;
    int verbose = (rpmIsVerbose() != 0);
    int expand = (g != NULL && gn > 0);
    int expandagain = 1;

    /* Don't expand %{verbose:...} argument on false condition */
    if (STREQ("verbose", f, fn) && (verbose == negate))
	expand = 0;

    if (expand) {
	(void) expandThis(mb, g, gn, &buf);
    } else {
	buf = xmalloc(MACROBUFSIZ + fn + gn);
	buf[0] = '\0';
    }
    if (STREQ("basename", f, fn)) {
	if ((b = strrchr(buf, '/')) == NULL)
	    b = buf;
	else
	    b++;
    } else if (STREQ("dirname", f, fn)) {
	if ((b = strrchr(buf, '/')) != NULL)
	    *b = '\0';
	b = buf;
    } else if (STREQ("shrink", f, fn)) {
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
    } else if (STREQ("quote", f, fn)) {
	char *quoted = NULL;
	rasprintf(&quoted, "%c%s%c", 0x1f, buf, 0x1f);
	free(buf);
	b = buf = quoted;
    } else if (STREQ("suffix", f, fn)) {
	if ((b = strrchr(buf, '.')) != NULL)
	    b++;
    } else if (STREQ("expand", f, fn) || STREQ("verbose", f, fn)) {
	b = buf;
    } else if (STREQ("expr", f, fn)) {
	char *expr = rpmExprStr(buf);
	if (expr) {
	    free(buf);
	    b = buf = expr;
	    expandagain = 0;
	} else {
	    mb->error = 1;
	}
    } else if (STREQ("url2path", f, fn) || STREQ("u2p", f, fn)) {
	(void)urlPath(buf, (const char **)&b);
	if (*b == '\0') b = "/";
    } else if (STREQ("uncompress", f, fn)) {
	rpmCompressedMagic compressed = COMPRESSED_OTHER;
	for (b = buf; (c = *b) && isblank(c);)
	    b++;
	for (be = b; (c = *be) && !isblank(c);)
	    be++;
	*be++ = '\0';
	if (rpmFileIsCompressed(b, &compressed))
	    mb->error = 1;
	switch (compressed) {
	default:
	case COMPRESSED_NOT:
	    sprintf(be, "%%__cat %s", b);
	    break;
	case COMPRESSED_OTHER:
	    sprintf(be, "%%__gzip -dc %s", b);
	    break;
	case COMPRESSED_BZIP2:
	    sprintf(be, "%%__bzip2 -dc %s", b);
	    break;
	case COMPRESSED_ZIP:
	    sprintf(be, "%%__unzip %s", b);
	    break;
        case COMPRESSED_LZMA:
        case COMPRESSED_XZ:
            sprintf(be, "%%__xz -dc %s", b);
            break;
	case COMPRESSED_LZIP:
	    sprintf(be, "%%__lzip -dc %s", b);
	    break;
	case COMPRESSED_LRZIP:
	    sprintf(be, "%%__lrzip -dqo- %s", b);
	    break;
	case COMPRESSED_7ZIP:
	    sprintf(be, "%%__7zip x %s", b);
	    break;
	case COMPRESSED_ZSTD:
	    sprintf(be, "%%__zstd -dc %s", b);
	    break;
	}
	b = be;
    } else if (STREQ("getenv", f, fn)) {
	b = getenv(buf);
    } else if (STREQ("getconfdir", f, fn)) {
	sprintf(buf, "%s", rpmConfigDir());
	b = buf;
    } else if (STREQ("getncpus", f, fn)) {
	sprintf(buf, "%u", getncpus());
	b = buf;
    } else if (STREQ("S", f, fn)) {
	for (b = buf; (c = *b) && risdigit(c);)
	    b++;
	if (!c) {	/* digit index */
	    b++;
	    sprintf(b, "%%SOURCE%s", buf);
	} else
	    b = buf;
    } else if (STREQ("P", f, fn)) {
	for (b = buf; (c = *b) && risdigit(c);)
	    b++;
	if (!c) {	/* digit index */
	    b++;
	    sprintf(b, "%%PATCH%s", buf);
	} else
			b = buf;
    } else if (STREQ("F", f, fn)) {
	b = buf + strlen(buf) + 1;
	sprintf(b, "file%s.file", buf);
    }

    if (b) {
	if (expandagain) {
	    (void) expandMacro(mb, b, 0);
	} else {
	    mbAppendStr(mb, b);
	}
    }
    free(buf);
}

static void doLoad(MacroBuf mb, int chkexist, int negate,
		    const char * f, size_t fn, const char * g, size_t gn)
{
    char *arg = NULL;
    if (g && gn > 0 && expandThis(mb, g, gn, &arg) == 0) {
	/* Print failure iff %{load:...} or %{!?load:...} */
	if (loadMacroFile(mb->mc, arg) && chkexist == negate) {
	    mbErr(mb, 1, _("failed to load macro file %s\n"), arg);
	}
    }
    free(arg);
}

static void doTrace(MacroBuf mb, int chkexist, int negate,
		    const char * f, size_t fn, const char * g, size_t gn)
{
    mb->expand_trace = mb->macro_trace = (negate ? 0 : mb->depth);
    if (mb->depth == 1) {
	print_macro_trace = mb->macro_trace;
	print_expand_trace = mb->expand_trace;
    }
}

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
    rpmMacroEntry me;
    const char *s = src, *se;
    const char *f, *fe;
    const char *g, *ge;
    size_t fn, gn, tpos;
    int c;
    int negate;
    const char * lastc;
    int chkexist;
    char *source = NULL;
    int store_macro_trace;
    int store_expand_trace;

    /*
     * Always make a (terminated) copy of the source string.
     * Beware: avoiding the copy when src is known \0-terminated seems like
     * an obvious opportunity for optimization, but doing that breaks
     * a special case of macro undefining itself.
     */
    if (!slen)
	slen = strlen(src);
    source = xmalloc(slen + 1);
    strncpy(source, src, slen);
    source[slen] = '\0';
    s = source;

    if (mb->buf == NULL) {
	size_t blen = MACROBUFSIZ + slen;
	mb->buf = xmalloc(blen + 1);
	mb->buf[0] = '\0';
	mb->tpos = 0;
	mb->nb = blen;
    }
    tpos = mb->tpos; /* save expansion pointer for printExpand */
    store_macro_trace = mb->macro_trace;
    store_expand_trace = mb->expand_trace;

    if (++mb->depth > max_macro_depth) {
	mbErr(mb, 1,
		_("Too many levels of recursion in macro expansion. It is likely caused by recursive macro declaration.\n"));
	mb->depth--;
	mb->expand_trace = 1;
	goto exit;
    }

    while (mb->error == 0 && (c = *s) != '\0') {
	const struct builtins_s* builtin = NULL;
	s++;
	/* Copy text until next macro */
	switch (c) {
	case '%':
	    if (*s) {	/* Ensure not end-of-string. */
		if (*s != '%')
		    break;
		s++;	/* skip first % in %% */
		if (mb->escape)
		    mbAppend(mb, c);
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
	    tpos = mb->tpos;	/* save expansion pointer for printExpand */
	lastc = NULL;
	switch ((c = *s)) {
	default:		/* %name substitution */
	    s = setNegateAndCheck(s, &negate, &chkexist);
	    f = se = s;
	    if (*se == '-')
		se++;
	    while ((c = *se) && (risalnum(c) || c == '_'))
		se++;
	    /* Recognize non-alnum macros too */
	    switch (*se) {
	    case '*':
		se++;
		if (*se == '*') se++;
		break;
	    case '#':
		se++;
		break;
	    default:
		break;
	    }
	    fe = se;
	    /* For "%name " macros ... */
	    if ((c = *fe) && isblank(c))
		if ((lastc = strchr(fe,'\n')) == NULL)
		    lastc = strchr(fe, '\0');
	    break;
	case '(':		/* %(...) shell escape */
	    if ((se = matchchar(s, c, ')')) == NULL) {
		mbErr(mb, 1, _("Unterminated %c: %s\n"), (char)c, s);
		continue;
	    }
	    if (mb->macro_trace)
		printMacro(mb, s, se+1);

	    s++;	/* skip ( */
	    doShellEscape(mb, s, (se - s));
	    se++;	/* skip ) */

	    s = se;
	    continue;
	    break;
	case '{':		/* %{...}/%{...:...} substitution */
	    if ((se = matchchar(s, c, '}')) == NULL) {
		mbErr(mb, 1, _("Unterminated %c: %s\n"), (char)c, s);
		continue;
	    }
	    f = s+1;/* skip { */
	    se++;	/* skip } */
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
	    s = se;
	    continue;
	}

	if (mb->macro_trace)
	    printMacro(mb, s, se);

	/* Expand builtin macros */
	if ((builtin = lookupBuiltin(f, fn))) {
	    if (builtin->parse) {
		s = builtin->parse(mb, se, slen - (se - s));
	    } else {
		builtin->func(mb, chkexist, negate, f, fn, g, gn);
		s = se;
	    }
	    continue;
	}

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

	/* XXX Special processing for flags */
	if (*f == '-') {
	    if ((me == NULL && !negate) ||	/* Without -f, skip %{-f...} */
		    (me != NULL && negate)) {	/* With -f, skip %{!-f...} */
		s = se;
		continue;
	    }

	    if (g && g < ge) {		/* Expand X in %{-f:X} */
		expandMacro(mb, g, gn);
	    } else
		if (me && me->body && *me->body) {/* Expand %{-f}/%{-f*} */
		    expandMacro(mb, me->body, 0);
		}
	    s = se;
	    continue;
	}

	/* XXX Special processing for macro existence */
	if (chkexist) {
	    if ((me == NULL && !negate) ||	/* Without -f, skip %{?f...} */
		    (me != NULL && negate)) {	/* With -f, skip %{!?f...} */
		s = se;
		continue;
	    }
	    if (g && g < ge) {		/* Expand X in %{?f:X} */
		expandMacro(mb, g, gn);
	    } else
		if (me && me->body && *me->body) { /* Expand %{?f}/%{?f*} */
		    expandMacro(mb, me->body, 0);
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

	/* Setup args for "%name " macros with opts */
	if (me && me->opts != NULL) {
	    const char *xe = grabArgs(mb, me, fe, lastc);
	    if (xe != NULL)
		se = xe;
	}

	/* Recursively expand body of macro */
	if (me->body && *me->body) {
	    expandMacro(mb, me->body, 0);
	}

	/* Free args for "%name " macros with opts */
	if (me->opts != NULL)
	    freeArgs(mb);

	s = se;
    }

    mb->buf[mb->tpos] = '\0';
    mb->depth--;
    if (mb->error != 0 || mb->expand_trace)
	printExpansion(mb, mb->buf+tpos, mb->buf+mb->tpos);
    mb->macro_trace = store_macro_trace;
    mb->expand_trace = store_expand_trace;
exit:
    _free(source);
    return mb->error;
}


/* =============================================================== */

static int doExpandMacros(rpmMacroContext mc, const char *src, int flags,
			char **target)
{
    MacroBuf mb = xcalloc(1, sizeof(*mb));
    int rc = 0;

    mb->buf = NULL;
    mb->depth = mc->depth;
    mb->level = mc->level;
    mb->macro_trace = print_macro_trace;
    mb->expand_trace = print_expand_trace;
    mb->mc = mc;
    mb->flags = flags;

    rc = expandMacro(mb, src, 0);

    mb->buf[mb->tpos] = '\0';	/* XXX just in case */
    /* expanded output is usually much less than alloced buffer, downsize */
    *target = xrealloc(mb->buf, mb->tpos + 1);

    _free(mb);
    return rc;
}

static void pushMacro(rpmMacroContext mc,
	const char * n, const char * o, const char * b, int level, int flags)
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
    me->flags = flags;
    me->flags &= ~(ME_USED);
    me->level = level;
    /* push over previous definition */
    me->prev = *mep;
    *mep = me;
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
    (void) doDefine(mb, macro, strlen(macro), level, 0);
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

    pushMacro(mc, "__file_name", NULL, fn, RMIL_MACROFILES, ME_NONE);

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
	pushMacro(mc, "__file_lineno", NULL, lnobuf, RMIL_MACROFILES, ME_NONE);
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

int rpmPushMacro(rpmMacroContext mc,
	      const char * n, const char * o, const char * b, int level)
{
    mc = rpmmctxAcquire(mc);
    pushMacro(mc, n, o, b, level, ME_NONE);
    rpmmctxRelease(mc);
    return 0;
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

    if (macrofiles == NULL)
	return;

    argvSplit(&globs, macrofiles, ":");
    mc = rpmmctxAcquire(mc);
    for (pattern = globs; *pattern; pattern++) {
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
