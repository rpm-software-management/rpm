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
    unsigned int defcnt; /*!< Non-global define tracking */
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
    int macro_trace;		/*!< Pre-print macro to expand? */
    int expand_trace;		/*!< Post-print macro expansion? */
    rpmMacroContext mc;
} * MacroBuf;

#define	_MAX_MACRO_DEPTH	16
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
 * @return		buffer, or NULL on end-of-file
 */
static char *
rdcl(char * buf, size_t size, FILE *f)
{
    char *q = buf - 1;		/* initialize just before buffer. */
    size_t nb = 0;
    size_t nread = 0;
    int pc = 0, bc = 0;
    char *p = buf;

    if (f != NULL)
    do {
	*(++q) = '\0';			/* terminate and move forward. */
	if (fgets(q, size, f) == NULL)	/* read next line. */
	    break;
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
	q++; p++; nb++;			/* copy newline too */
	size -= nb;
	if (*q == '\r')			/* XXX avoid \r madness */
	    *q = '\n';
    } while (size > 0);
    return (nread > 0 ? buf : NULL);
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
    const char *ellipsis;
    int choplen;

    if (s >= se) {	/* XXX just in case */
	fprintf(stderr, _("%3d>%*s(empty)"), mb->depth,
		(2 * mb->depth + 1), "");
	return;
    }

    if (s[-1] == '{')
	s--;

    /* Print only to first end-of-line (or end-of-string). */
    for (senl = se; *senl && !iseol(*senl); senl++)
	{};

    /* Limit trailing non-trace output */
    choplen = 61 - (2 * mb->depth);
    if ((senl - s) > choplen) {
	senl = s + choplen;
	ellipsis = "...";
    } else
	ellipsis = "";

    /* Substitute caret at end-of-macro position */
    fprintf(stderr, "%3d>%*s%%%.*s^", mb->depth,
	(2 * mb->depth + 1), "", (int)(se - s), s);
    if (se[1] != '\0' && (senl - (se+1)) > 0)
	fprintf(stderr, "%-.*s%s", (int)(senl - (se+1)), se+1, ellipsis);
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
    const char *ellipsis;
    int choplen;

    if (!(te > t)) {
	rpmlog(RPMLOG_DEBUG, _("%3d<%*s(empty)\n"), mb->depth, (2 * mb->depth + 1), "");
	return;
    }

    /* Shorten output which contains newlines */
    while (te > t && iseol(te[-1]))
	te--;
    ellipsis = "";
    if (mb->depth > 0) {
	const char *tenl;

	/* Skip to last line of expansion */
	while ((tenl = strchr(t, '\n')) && tenl < te)
	    t = ++tenl;

	/* Limit expand output */
	choplen = 61 - (2 * mb->depth);
	if ((te - t) > choplen) {
	    te = t + choplen;
	    ellipsis = "...";
	}
    }

    rpmlog(RPMLOG_DEBUG,"%3d<%*s", mb->depth, (2 * mb->depth + 1), "");
    if (te > t)
	rpmlog(RPMLOG_DEBUG, "%.*s%s", (int)(te - t), t, ellipsis);
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
	while(((_c) = *(_s)) && (risalnum(_c) || (_c) == '_')) \
		*(_ne)++ = *(_s)++; \
	*(_ne) = '\0';		\
    }

#define	COPYOPTS(_oe, _s, _c)	\
    { \
	while(((_c) = *(_s)) && (_c) != ')') \
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
    int rc;

    /* Copy other state from "parent", but we want a buffer of our own */
    umb = *mb;
    umb.buf = NULL;
    rc = expandMacro(&umb, src, slen);
    *target = umb.buf;

    return rc;
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
/**
 * Expand output of shell command into target buffer.
 * @param mb		macro expansion state
 * @param cmd		shell command
 * @param clen		no. bytes in shell command
 * @return		result of expansion
 */
static int
doShellEscape(MacroBuf mb, const char * cmd, size_t clen)
{
    char *buf = NULL;
    FILE *shf;
    int rc = 0;
    int c;

    rc = expandThis(mb, cmd, clen, &buf);
    if (rc)
	goto exit;

    if ((shf = popen(buf, "r")) == NULL) {
	rc = 1;
	goto exit;
    }

    size_t tpos = mb->tpos;
    while((c = fgetc(shf)) != EOF) {
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
	    rpmlog(RPMLOG_ERR, _("Macro %%%s has unterminated opts\n"), n);
	    goto exit;
	}
    }

    /* Copy body, skipping over escaped newlines */
    b = be = oe + 1;
    sbody = s;
    SKIPBLANK(s, c);
    if (c == '{') {	/* XXX permit silent {...} grouping */
	if ((se = matchchar(s, c, '}')) == NULL) {
	    rpmlog(RPMLOG_ERR,
		_("Macro %%%s has unterminated body\n"), n);
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
	    rpmlog(RPMLOG_ERR,
		_("Macro %%%s has unterminated body\n"), n);
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

    /* Names must start with alphabetic or _ and be at least 3 chars */
    if (!((c = *n) && (risalpha(c) || c == '_') && (ne - n) > 2)) {
	rpmlog(RPMLOG_ERR,
		_("Macro %%%s has illegal name (%%define)\n"), n);
	goto exit;
    }

    if ((be - b) < 1) {
	rpmlog(RPMLOG_ERR, _("Macro %%%s has empty body\n"), n);
	goto exit;
    }

    if (!isblank(*sbody) && !(*sbody == '\\' && iseol(sbody[1])))
	rpmlog(RPMLOG_WARNING, _("Macro %%%s needs whitespace before body\n"), n);

    if (expandbody) {
	if (expandThis(mb, b, 0, &ebody)) {
	    rpmlog(RPMLOG_ERR, _("Macro %%%s failed to expand\n"), n);
	    goto exit;
	}
	b = ebody;
    }

    pushMacro(mb->mc, n, o, b, (level - 1), ME_NONE);

exit:
    _free(buf);
    _free(ebody);
    return se;
}

/**
 * Parse (and execute) macro undefinition.
 * @param mc		macro context
 * @param se		macro name to undefine
 * @param slen		length of se argument
 * @return		address to continue parsing
 */
static const char *
doUndefine(rpmMacroContext mc, const char * se, size_t slen)
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

    /* Names must start with alphabetic or _ and be at least 3 chars */
    if (!((c = *n) && (risalpha(c) || c == '_') && (ne - n) > 2)) {
	rpmlog(RPMLOG_ERR,
		_("Macro %%%s has illegal name (%%undefine)\n"), n);
	goto exit;
    }

    popMacro(mc, n);

exit:
    _free(buf);
    return se;
}

/**
 * Free parsed arguments for parameterized macro.
 * @param mb		macro expansion state
 * @param delete	
 */
static void
freeArgs(MacroBuf mb, int delete)
{
    rpmMacroContext mc = mb->mc;

    /* Delete dynamic macro definitions */
    for (int i = 0; i < mc->n; i++) {
	rpmMacroEntry me = mc->tab[i];
	assert(me);
	if (me->level < mb->depth)
	    continue;
	/* Warn on defined but unused non-automatic, scoped macros */
	if (!(me->flags & (ME_AUTO|ME_USED))) {
	    rpmlog(RPMLOG_WARNING,
			_("Macro %%%s defined but not used within scope\n"),
			me->name);
	    /* Only whine once */
	    me->flags |= ME_USED;
	}
	if (!delete)
	    continue;

	/* compensate if the slot is to go away */
	if (me->prev == NULL)
	    i--;
	popMacro(mc, me->name);
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
    const char *opts;
    char *args = NULL;
    ARGV_t argv = NULL;
    int argc = 0;
    int c;

    /* Copy macro name as argv[0] */
    argvAdd(&argv, me->name);
    pushMacro(mb->mc, "0", NULL, me->name, mb->depth, ME_AUTO);
    
    /* 
     * Make a copy of se up to lastc string that we can pass to argvSplit().
     * Append the results to main argv. 
     */
    {	ARGV_t av = NULL;
	char *s = xcalloc((lastc-se)+1, sizeof(*s));
	memcpy(s, se, (lastc-se));

	argvSplit(&av, s, " \t");
	argvAppend(&argv, av);

	argvFree(av);
	free(s);
    }

    /*
     * The macro %* analoguous to the shell's $* means "Pass all non-macro
     * parameters." Consequently, there needs to be a macro that means "Pass all
     * (including macro parameters) options". This is useful for verifying
     * parameters during expansion and yet transparently passing all parameters
     * through for higher level processing (e.g. %description and/or %setup).
     * This is the (potential) justification for %{**} ...
    */
    args = argvJoin(argv + 1, " ");
    pushMacro(mb->mc, "**", NULL, args, mb->depth, ME_AUTO);
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
    while((c = getopt(argc, argv, opts)) != -1)
    {
	char *name = NULL, *body = NULL;
	if (c == '?' || strchr(opts, c) == NULL) {
	    rpmlog(RPMLOG_ERR, _("Unknown option %c in %s(%s)\n"),
			(char)optopt, me->name, opts);
	    goto exit;
	}

	rasprintf(&name, "-%c", c);
	if (optarg) {
	    rasprintf(&body, "-%c %s", c, optarg);
	} else {
	    rasprintf(&body, "-%c", c);
	}
	pushMacro(mb->mc, name, NULL, body, mb->depth, ME_AUTO);
	free(name);
	free(body);

	if (optarg) {
	    rasprintf(&name, "-%c*", c);
	    pushMacro(mb->mc, name, NULL, optarg, mb->depth, ME_AUTO);
	    free(name);
	}
    }

    /* Add argument count (remaining non-option items) as macro. */
    {	char *ac = NULL;
    	rasprintf(&ac, "%d", (argc - optind));
    	pushMacro(mb->mc, "#", NULL, ac, mb->depth, ME_AUTO);
	free(ac);
    }

    /* Add macro for each argument */
    if (argc - optind) {
	for (c = optind; c < argc; c++) {
	    char *name = NULL;
	    rasprintf(&name, "%d", (c - optind + 1));
	    pushMacro(mb->mc, name, NULL, argv[c], mb->depth, ME_AUTO);
	    free(name);
	}
    }

    /* Add concatenated unexpanded arguments as yet another macro. */
    args = argvJoin(argv + optind, " ");
    pushMacro(mb->mc, "*", NULL, args ? args : "", mb->depth, ME_AUTO);
    free(args);

exit:
    argvFree(argv);
    return ((*lastc == '\0' || *lastc == '\n') && *(lastc-1) != '\\') ?
	   lastc : lastc + 1;
}

/**
 * Perform macro message output
 * @param mb		macro expansion state
 * @param waserror	use rpmlog()?
 * @param msg		message to ouput
 * @param msglen	no. of bytes in message
 */
static void
doOutput(MacroBuf mb, int waserror, const char * msg, size_t msglen)
{
    char *buf = NULL;

    (void) expandThis(mb, msg, msglen, &buf);
    if (waserror)
	rpmlog(RPMLOG_ERR, "%s\n", buf);
    else
	fprintf(stderr, "%s", buf);
    _free(buf);
}

/**
 * Execute macro primitives.
 * @param mb		macro expansion state
 * @param negate	should logic be inverted?
 * @param f		beginning of field f
 * @param fn		length of field f
 * @param g		beginning of field g
 * @param gn		length of field g
 */
static void
doFoo(MacroBuf mb, int negate, const char * f, size_t fn,
		const char * g, size_t gn)
{
    char *buf = NULL;
    char *b = NULL, *be;
    int c;

    if (g != NULL && gn > 0) {
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
    } else if (STREQ("suffix", f, fn)) {
	if ((b = strrchr(buf, '.')) != NULL)
	    b++;
    } else if (STREQ("expand", f, fn)) {
	b = buf;
    } else if (STREQ("verbose", f, fn)) {
	if (negate)
	    b = (rpmIsVerbose() ? NULL : buf);
	else
	    b = (rpmIsVerbose() ? buf : NULL);
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
	(void) rpmFileIsCompressed(b, &compressed);
	switch(compressed) {
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
	}
	b = be;
    } else if (STREQ("getenv", f, fn)) {
	b = getenv(buf);
    } else if (STREQ("getconfdir", f, fn)) {
	sprintf(buf, "%s", rpmConfigDir());
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
	(void) expandMacro(mb, b, 0);
    }
    free(buf);
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
    rpmMacroContext mc = mb->mc;
    rpmMacroEntry *mep;
    rpmMacroEntry me;
    const char *s = src, *se;
    const char *f, *fe;
    const char *g, *ge;
    size_t fn, gn, tpos;
    int c;
    int rc = 0;
    int negate;
    const char * lastc;
    int chkexist;
    char *source = NULL;
    unsigned int prevcnt = mc->defcnt;

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

    if (++mb->depth > max_macro_depth) {
	rpmlog(RPMLOG_ERR,
		_("Too many levels of recursion in macro expansion. It is likely caused by recursive macro declaration.\n"));
	mb->depth--;
	mb->expand_trace = 1;
	_free(source);
	return 1;
    }

    while (rc == 0 && (c = *s) != '\0') {
	s++;
	/* Copy text until next macro */
	switch(c) {
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
	    tpos = mb->tpos;	/* save expansion pointer for printExpand */
	negate = 0;
	lastc = NULL;
	chkexist = 0;
	switch ((c = *s)) {
	default:		/* %name substitution */
		while (*s != '\0' && strchr("!?", *s) != NULL) {
			switch(*s++) {
			case '!':
				negate = ((negate + 1) % 2);
				break;
			case '?':
				chkexist++;
				break;
			}
		}
		f = se = s;
		if (*se == '-')
			se++;
		while((c = *se) && (risalnum(c) || c == '_'))
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
			rpmlog(RPMLOG_ERR,
				_("Unterminated %c: %s\n"), (char)c, s);
			rc = 1;
			continue;
		}
		if (mb->macro_trace)
			printMacro(mb, s, se+1);

		s++;	/* skip ( */
		rc = doShellEscape(mb, s, (se - s));
		se++;	/* skip ) */

		s = se;
		continue;
		break;
	case '{':		/* %{...}/%{...:...} substitution */
		if ((se = matchchar(s, c, '}')) == NULL) {
			rpmlog(RPMLOG_ERR,
				_("Unterminated %c: %s\n"), (char)c, s);
			rc = 1;
			continue;
		}
		f = s+1;/* skip { */
		se++;	/* skip } */
		while (strchr("!?", *f) != NULL) {
			switch(*f++) {
			case '!':
				negate = ((negate + 1) % 2);
				break;
			case '?':
				chkexist++;
				break;
			}
		}
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
		rpmlog(RPMLOG_ERR,
			_("A %% is followed by an unparseable macro\n"));
#endif
		s = se;
		continue;
	}

	if (mb->macro_trace)
		printMacro(mb, s, se);

	/* Expand builtin macros */
	if (STREQ("load", f, fn)) {
		char *arg = NULL;
		if (g && gn > 0 && expandThis(mb, g, gn, &arg) == 0) {
			/* Print failure iff %{load:...} or %{!?load:...} */
			if (loadMacroFile(mb->mc, arg) && chkexist == negate) {
				rpmlog(RPMLOG_ERR,
				       _("failed to load macro file %s"), arg);
			}
		}
		free(arg);
		s = se;
		continue;
	}
	if (STREQ("global", f, fn)) {
		s = doDefine(mb, se, slen - (se - s), RMIL_GLOBAL, 1);
		continue;
	}
	if (STREQ("define", f, fn)) {
		s = doDefine(mb, se, slen - (se - s), mb->depth, 0);
		continue;
	}
	if (STREQ("undefine", f, fn)) {
		s = doUndefine(mb->mc, se, slen - (se - s));
		continue;
	}

	if (STREQ("echo", f, fn) ||
	    STREQ("warn", f, fn) ||
	    STREQ("error", f, fn)) {
		int waserror = 0;
		if (STREQ("error", f, fn))
			waserror = 1;
		if (g != NULL && g < ge)
			doOutput(mb, waserror, g, gn);
		else
			doOutput(mb, waserror, f, fn);
		s = se;
		continue;
	}

	if (STREQ("trace", f, fn)) {
		/* XXX TODO restore expand_trace/macro_trace to 0 on return */
		mb->expand_trace = mb->macro_trace = (negate ? 0 : mb->depth);
		if (mb->depth == 1) {
			print_macro_trace = mb->macro_trace;
			print_expand_trace = mb->expand_trace;
		}
		s = se;
		continue;
	}

	if (STREQ("dump", f, fn)) {
		rpmDumpMacroTable(mb->mc, NULL);
		while (iseol(*se))
			se++;
		s = se;
		continue;
	}

#ifdef	WITH_LUA
	if (STREQ("lua", f, fn)) {
		rpmlua lua = NULL; /* Global state. */
		char *scriptbuf = xmalloc(gn + 1);
		char *printbuf;
		if (g != NULL && gn > 0) 
		    memcpy(scriptbuf, g, gn);
		scriptbuf[gn] = '\0';
		rpmluaPushPrintBuffer(lua);
		if (rpmluaRunScript(lua, scriptbuf, NULL) == -1)
		    rc = 1;
		printbuf = rpmluaPopPrintBuffer(lua);
		if (printbuf) {
		    mbAppendStr(mb, printbuf);
		    free(printbuf);
		}
		free(scriptbuf);
		s = se;
		continue;
	}
#endif

	/* XXX necessary but clunky */
	if (STREQ("basename", f, fn) ||
	    STREQ("dirname", f, fn) ||
	    STREQ("suffix", f, fn) ||
	    STREQ("expand", f, fn) ||
	    STREQ("verbose", f, fn) ||
	    STREQ("uncompress", f, fn) ||
	    STREQ("url2path", f, fn) ||
	    STREQ("u2p", f, fn) ||
	    STREQ("getenv", f, fn) ||
	    STREQ("getconfdir", f, fn) ||
	    STREQ("S", f, fn) ||
	    STREQ("P", f, fn) ||
	    STREQ("F", f, fn)) {
		/* FIX: verbose may be set */
		doFoo(mb, negate, f, fn, g, gn);
		s = se;
		continue;
	}

	/* Expand defined macros */
	mep = findEntry(mb->mc, f, fn, NULL);
	me = (mep ? *mep : NULL);

	/* If we looked up a macro, consider it used */
	if (me)
	    me->flags |= ME_USED;

	/* XXX Special processing for flags */
	if (*f == '-') {
		if ((me == NULL && !negate) ||	/* Without -f, skip %{-f...} */
		    (me != NULL && negate)) {	/* With -f, skip %{!-f...} */
			s = se;
			continue;
		}

		if (g && g < ge) {		/* Expand X in %{-f:X} */
			rc = expandMacro(mb, g, gn);
		} else
		if (me && me->body && *me->body) {/* Expand %{-f}/%{-f*} */
			rc = expandMacro(mb, me->body, 0);
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
			rc = expandMacro(mb, g, gn);
		} else
		if (me && me->body && *me->body) { /* Expand %{?f}/%{?f*} */
			rc = expandMacro(mb, me->body, 0);
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
		if (lastc != NULL) {
			se = grabArgs(mb, me, fe, lastc);
		} else {
			pushMacro(mb->mc, "**", NULL, "", mb->depth, ME_AUTO);
			pushMacro(mb->mc, "*", NULL, "", mb->depth, ME_AUTO);
			pushMacro(mb->mc, "#", NULL, "0", mb->depth, ME_AUTO);
			pushMacro(mb->mc, "0", NULL, me->name, mb->depth, ME_AUTO);
		}
	}

	/* Recursively expand body of macro */
	if (me->body && *me->body) {
		rc = expandMacro(mb, me->body, 0);
	}

	/* Free args for "%name " macros with opts */
	if (me->opts != NULL)
		freeArgs(mb, 1);

	s = se;
    }

    mb->buf[mb->tpos] = '\0';
    /* Warn on unused non-global macros */
    if (prevcnt != mc->defcnt)
	freeArgs(mb, 0);
    mb->depth--;
    if (rc != 0 || mb->expand_trace)
	printExpansion(mb, mb->buf+tpos, mb->buf+mb->tpos);
    _free(source);
    return rc;
}


/* =============================================================== */

static int doExpandMacros(rpmMacroContext mc, const char *src, char **target)
{
    MacroBuf mb = xcalloc(1, sizeof(*mb));
    int rc = 0;

    mb->buf = NULL;
    mb->depth = 0;
    mb->macro_trace = print_macro_trace;
    mb->expand_trace = print_expand_trace;
    mb->mc = mc;

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

    /* Track for non-global definitions */
    if (level > 0)
	mc->defcnt++;
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

    /* XXX just enough to get by */
    mb->mc = mc;
    (void) doDefine(mb, macro, strlen(macro), level, 0);
    _free(mb);
    return 0;
}

static int loadMacroFile(rpmMacroContext mc, const char * fn)
{
    FILE *fd = fopen(fn, "r");
    size_t blen = MACROBUFSIZ;
    char *buf = xmalloc(blen);
    int rc = -1;

    if (fd == NULL)
	goto exit;

    /* XXX Assume new fangled macro expansion */
    max_macro_depth = 16;

    buf[0] = '\0';
    while(rdcl(buf, blen, fd) != NULL) {
	char c, *n;

	n = buf;
	SKIPBLANK(n, c);

	if (c != '%')
		continue;
	n++;	/* skip % */
	rc = defineMacro(mc, n, RMIL_MACROFILES);
    }

    rc = fclose(fd);

exit:
    _free(buf);
    return rc;
}

static void copyMacros(rpmMacroContext src, rpmMacroContext dst, int level)
{
    for (int i = 0; i < src->n; i++) {
	rpmMacroEntry me = src->tab[i];
	assert(me);
	pushMacro(dst, me->name, me->opts, me->body, (level - 1), me->flags);
    }
}

/* External interfaces */

int expandMacros(void * spec, rpmMacroContext mc, char * sbuf, size_t slen)
{
    char *target = NULL;
    int rc;

    mc = rpmmctxAcquire(mc);
    rc = doExpandMacros(mc, sbuf, &target);
    rpmmctxRelease(mc);

    rstrlcpy(sbuf, target, slen);
    free(target);
    return rc;
}

int rpmExpandMacros(rpmMacroContext mc, const char * sbuf, char ** obuf, int flags)
{
    char *target = NULL;
    int rc;

    mc = rpmmctxAcquire(mc);
    rc = doExpandMacros(mc, sbuf, &target);
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

void addMacro(rpmMacroContext mc,
	      const char * n, const char * o, const char * b, int level)
{
    mc = rpmmctxAcquire(mc);
    pushMacro(mc, n, o, b, level, ME_NONE);
    rpmmctxRelease(mc);
}

void delMacro(rpmMacroContext mc, const char * n)
{
    mc = rpmmctxAcquire(mc);
    popMacro(mc, n);
    rpmmctxRelease(mc);
}

int
rpmDefineMacro(rpmMacroContext mc, const char * macro, int level)
{
    mc = rpmmctxAcquire(mc);
    (void) defineMacro(mc, macro, level);
    rpmmctxRelease(mc);
    return 0;
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
    (void) doExpandMacros(mc, buf, &ret);
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
