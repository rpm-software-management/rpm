/** \ingroup rpmrc rpmio
 * \file rpmio/macro.c
 */

#include "system.h"
#include <stdarg.h>

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

/*! The structure used to store a macro. */
struct rpmMacroEntry_s {
    struct rpmMacroEntry_s *prev;/*!< Macro entry stack. */
    char *name;   	/*!< Macro name. */
    char *opts;   	/*!< Macro parameters (a la getopt) */
    char *body;   	/*!< Macro body. */
    int used;           /*!< No. of expansions. */
    int level;          /*!< Scoping level. */
};

/*! The structure used to store the set of macros in a context. */
struct rpmMacroContext_s {
    rpmMacroEntry *macroTable;  /*!< Macro entry table for context. */
    int macrosAllocated;/*!< No. of allocated macros. */
    int firstFree;      /*!< No. of macros. */
};


static struct rpmMacroContext_s rpmGlobalMacroContext_s;
rpmMacroContext rpmGlobalMacroContext = &rpmGlobalMacroContext_s;

static struct rpmMacroContext_s rpmCLIMacroContext_s;
rpmMacroContext rpmCLIMacroContext = &rpmCLIMacroContext_s;

/**
 * Macro expansion state.
 */
typedef struct MacroBuf_s {
    const char * s;		/*!< Text to expand. */
    char * t;			/*!< Expansion buffer. */
    size_t nb;			/*!< No. bytes remaining in expansion buffer. */
    int depth;			/*!< Current expansion depth. */
    int macro_trace;		/*!< Pre-print macro to expand? */
    int expand_trace;		/*!< Post-print macro expansion? */
    void * spec;		/*!< (future) %file expansion info?. */
    rpmMacroContext mc;
} * MacroBuf;

#define SAVECHAR(_mb, _c) { *(_mb)->t = (_c), (_mb)->t++, (_mb)->nb--; }


#define	_MAX_MACRO_DEPTH	16
static int max_macro_depth = _MAX_MACRO_DEPTH;

#define	_PRINT_MACRO_TRACE	0
static int print_macro_trace = _PRINT_MACRO_TRACE;

#define	_PRINT_EXPAND_TRACE	0
static int print_expand_trace = _PRINT_EXPAND_TRACE;

#define	MACRO_CHUNK_SIZE	16

/* forward ref */
static int expandMacro(MacroBuf mb);

/* =============================================================== */

/**
 * Compare macro entries by name (qsort/bsearch).
 * @param ap		1st macro entry
 * @param bp		2nd macro entry
 * @return		result of comparison
 */
static int
compareMacroName(const void * ap, const void * bp)
{
    rpmMacroEntry ame = *((const rpmMacroEntry *)ap);
    rpmMacroEntry bme = *((const rpmMacroEntry *)bp);

    if (ame == NULL && bme == NULL)
	return 0;
    if (ame == NULL)
	return 1;
    if (bme == NULL)
	return -1;
    return strcmp(ame->name, bme->name);
}

/**
 * Enlarge macro table.
 * @param mc		macro context
 */
static void
expandMacroTable(rpmMacroContext mc)
{
    if (mc->macroTable == NULL) {
	mc->macrosAllocated = MACRO_CHUNK_SIZE;
	mc->macroTable = (rpmMacroEntry *)
	    xmalloc(sizeof(*(mc->macroTable)) * mc->macrosAllocated);
	mc->firstFree = 0;
    } else {
	mc->macrosAllocated += MACRO_CHUNK_SIZE;
	mc->macroTable = (rpmMacroEntry *)
	    xrealloc(mc->macroTable, sizeof(*(mc->macroTable)) *
			mc->macrosAllocated);
    }
    memset(&mc->macroTable[mc->firstFree], 0, MACRO_CHUNK_SIZE * sizeof(*(mc->macroTable)));
}

/**
 * Sort entries in macro table.
 * @param mc		macro context
 */
static void
sortMacroTable(rpmMacroContext mc)
{
    int i;

    if (mc == NULL || mc->macroTable == NULL)
	return;

    qsort(mc->macroTable, mc->firstFree, sizeof(*(mc->macroTable)),
		compareMacroName);

    /* Empty pointers are now at end of table. Reset first free index. */
    for (i = 0; i < mc->firstFree; i++) {
	if (mc->macroTable[i] != NULL)
	    continue;
	mc->firstFree = i;
	break;
    }
}

void
rpmDumpMacroTable(rpmMacroContext mc, FILE * fp)
{
    int nempty = 0;
    int nactive = 0;

    if (mc == NULL) mc = rpmGlobalMacroContext;
    if (fp == NULL) fp = stderr;
    
    fprintf(fp, "========================\n");
    if (mc->macroTable != NULL) {
	int i;
	for (i = 0; i < mc->firstFree; i++) {
	    rpmMacroEntry me;
	    if ((me = mc->macroTable[i]) == NULL) {
		/* XXX this should never happen */
		nempty++;
		continue;
	    }
	    fprintf(fp, "%3d%c %s", me->level,
			(me->used > 0 ? '=' : ':'), me->name);
	    if (me->opts && *me->opts)
		    fprintf(fp, "(%s)", me->opts);
	    if (me->body && *me->body)
		    fprintf(fp, "\t%s", me->body);
	    fprintf(fp, "\n");
	    nactive++;
	}
    }
    fprintf(fp, _("======================== active %d empty %d\n"),
		nactive, nempty);
}

/**
 * Find entry in macro table.
 * @param mc		macro context
 * @param name		macro name
 * @param namelen	no. of bytes
 * @return		address of slot in macro table with name (or NULL)
 */
static rpmMacroEntry *
findEntry(rpmMacroContext mc, const char * name, size_t namelen)
{
    rpmMacroEntry key, *ret;
    struct rpmMacroEntry_s keybuf;
    char *namebuf = NULL;

    if (mc == NULL) mc = rpmGlobalMacroContext;
    if (mc->macroTable == NULL || mc->firstFree == 0)
	return NULL;

    if (namelen > 0) {
	namebuf = xcalloc(namelen + 1, sizeof(*namebuf));
	strncpy(namebuf, name, namelen);
	namebuf[namelen] = '\0';
	name = namebuf;
    }
    
    key = &keybuf;
    memset(key, 0, sizeof(*key));
    key->name = (char *)name;
    ret = (rpmMacroEntry *) bsearch(&key, mc->macroTable, mc->firstFree,
			sizeof(*(mc->macroTable)), compareMacroName);
    _free(namebuf);
    /* XXX TODO: find 1st empty slot and return that */
    return ret;
}

/* =============================================================== */

/**
 * fgets(3) analogue that reads \ continuations. Last newline always trimmed.
 * @param buf		input buffer
 * @param size		inbut buffer size (bytes)
 * @param fd		file handle
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
	fprintf(stderr, _("%3d<%*s(empty)\n"), mb->depth, (2 * mb->depth + 1), "");
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

    fprintf(stderr, "%3d<%*s", mb->depth, (2 * mb->depth + 1), "");
    if (te > t)
	fprintf(stderr, "%.*s%s", (int)(te - t), t, ellipsis);
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
 * Save source and expand field into target.
 * @param mb		macro expansion state
 * @param f		field
 * @param flen		no. bytes in field
 * @return		result of expansion
 */
static int
expandT(MacroBuf mb, const char * f, size_t flen)
{
    char *sbuf;
    const char *s = mb->s;
    int rc;

    sbuf = xcalloc(flen + 1, sizeof(*sbuf));

    strncpy(sbuf, f, flen);
    sbuf[flen] = '\0';
    mb->s = sbuf;
    rc = expandMacro(mb);
    mb->s = s;

    _free(sbuf);

    return rc;
}

/**
 * Save source/target and expand macro in u.
 * @param mb		macro expansion state
 * @param u		input macro, output expansion
 * @param ulen		no. bytes in u buffer
 * @return		result of expansion
 */
static int
expandU(MacroBuf mb, char * u, size_t ulen)
{
    const char *s = mb->s;
    char *t = mb->t;
    size_t nb = mb->nb;
    char *tbuf;
    int rc;

    tbuf = xcalloc(ulen + 1, sizeof(*tbuf));

    mb->s = u;
    mb->t = tbuf;
    mb->nb = ulen;
    rc = expandMacro(mb);

    tbuf[ulen] = '\0';	/* XXX just in case */
    if (ulen > mb->nb)
	strncpy(u, tbuf, (ulen - mb->nb + 1));

    mb->s = s;
    mb->t = t;
    mb->nb = nb;

    _free(tbuf);

    return rc;
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
    size_t blen = MACROBUFSIZ + clen;
    char *buf = xmalloc(blen);
    FILE *shf;
    int rc = 0;
    int c;

    strncpy(buf, cmd, clen);
    buf[clen] = '\0';
    rc = expandU(mb, buf, blen);
    if (rc)
	goto exit;

    if ((shf = popen(buf, "r")) == NULL) {
	rc = 1;
	goto exit;
    }
    while((c = fgetc(shf)) != EOF) {
	if (mb->nb > 1) {
	    SAVECHAR(mb, c);
	} 
    }
    (void) pclose(shf);

    /* XXX delete trailing \r \n */
    while (iseol(mb->t[-1])) {
	*(mb->t--) = '\0';
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
 * @param level		macro recursion level
 * @param expandbody	should body be expanded?
 * @return		address to continue parsing
 */
static const char *
doDefine(MacroBuf mb, const char * se, int level, int expandbody)
{
    const char *s = se;
    size_t blen = MACROBUFSIZ;
    char *buf = xmalloc(blen);
    char *n = buf, *ne = n;
    char *o = NULL, *oe;
    char *b, *be;
    int c;
    int oc = ')';

    /* Copy name */
    COPYNAME(ne, s, c);

    /* Copy opts (if present) */
    oe = ne + 1;
    if (*s == '(') {
	s++;	/* skip ( */
	o = oe;
	COPYOPTS(oe, s, oc);
	s++;	/* skip ) */
    }

    /* Copy body, skipping over escaped newlines */
    b = be = oe + 1;
    SKIPBLANK(s, c);
    if (c == '{') {	/* XXX permit silent {...} grouping */
	if ((se = matchchar(s, c, '}')) == NULL) {
	    rpmlog(RPMLOG_ERR,
		_("Macro %%%s has unterminated body\n"), n);
	    se = s;	/* XXX W2DO? */
	    return se;
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
	    return se;
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
	return se;
    }

    /* Options must be terminated with ')' */
    if (o && oc != ')') {
	rpmlog(RPMLOG_ERR, _("Macro %%%s has unterminated opts\n"), n);
	goto exit;
    }

    if ((be - b) < 1) {
	rpmlog(RPMLOG_ERR, _("Macro %%%s has empty body\n"), n);
	goto exit;
    }

    if (expandbody && expandU(mb, b, (&buf[blen] - b))) {
	rpmlog(RPMLOG_ERR, _("Macro %%%s failed to expand\n"), n);
	goto exit;
    }

    addMacro(mb->mc, n, o, b, (level - 1));

exit:
    _free(buf);
    return se;
}

/**
 * Parse (and execute) macro undefinition.
 * @param mc		macro context
 * @param se		macro name to undefine
 * @return		address to continue parsing
 */
static const char *
doUndefine(rpmMacroContext mc, const char * se)
{
    const char *s = se;
    char *buf = xmalloc(MACROBUFSIZ);
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

    delMacro(mc, n);

exit:
    _free(buf);
    return se;
}

/**
 * Push new macro definition onto macro entry stack.
 * @param mep		address of macro entry slot
 * @param n		macro name
 * @param o		macro parameters (NULL if none)
 * @param b		macro body (NULL becomes "")
 * @param level		macro recursion level
 */
static void
pushMacro(rpmMacroEntry * mep,
		const char * n, const char * o,
		const char * b, int level)
{
    rpmMacroEntry prev = (mep && *mep ? *mep : NULL);
    rpmMacroEntry me = (rpmMacroEntry) xmalloc(sizeof(*me));

    me->prev = prev;
    me->name = (prev ? prev->name : xstrdup(n));
    me->opts = (o ? xstrdup(o) : NULL);
    me->body = xstrdup(b ? b : "");
    me->used = 0;
    me->level = level;
    if (mep)
	*mep = me;
    else
	me = _free(me);
}

/**
 * Pop macro definition from macro entry stack.
 * @param mep		address of macro entry slot
 */
static void
popMacro(rpmMacroEntry * mep)
{
	rpmMacroEntry me = (*mep ? *mep : NULL);

	if (me) {
		/* XXX cast to workaround const */
		if ((*mep = me->prev) == NULL)
			me->name = _free(me->name);
		me->opts = _free(me->opts);
		me->body = _free(me->body);
		me = _free(me);
	}
}

/**
 * Free parsed arguments for parameterized macro.
 * @param mb		macro expansion state
 */
static void
freeArgs(MacroBuf mb)
{
    rpmMacroContext mc = mb->mc;
    int ndeleted = 0;
    int i;

    if (mc == NULL || mc->macroTable == NULL)
	return;

    /* Delete dynamic macro definitions */
    for (i = 0; i < mc->firstFree; i++) {
	rpmMacroEntry *mep, me;
	int skiptest = 0;
	mep = &mc->macroTable[i];
	me = *mep;

	if (me == NULL)		/* XXX this should never happen */
	    continue;
	if (me->level < mb->depth)
	    continue;
	if (strlen(me->name) == 1 && strchr("#*0", *me->name)) {
	    if (*me->name == '*' && me->used > 0)
		skiptest = 1; /* XXX skip test for %# %* %0 */
	} else if (!skiptest && me->used <= 0) {
#if NOTYET
	    rpmlog(RPMLOG_ERR,
			_("Macro %%%s (%s) was not used below level %d\n"),
			me->name, me->body, me->level);
#endif
	}
	popMacro(mep);
	if (!(mep && *mep))
	    ndeleted++;
    }

    /* If any deleted macros, sort macro table */
    if (ndeleted)
	sortMacroTable(mc);
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
    addMacro(mb->mc, "0", NULL, me->name, mb->depth);
    
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
    addMacro(mb->mc, "**", NULL, args, mb->depth);
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
			(char)c, me->name, opts);
	    goto exit;
	}

	rasprintf(&name, "-%c", c);
	if (optarg) {
	    rasprintf(&body, "-%c %s", c, optarg);
	} else {
	    rasprintf(&body, "-%c", c);
	}
	addMacro(mb->mc, name, NULL, body, mb->depth);
	free(name);
	free(body);

	if (optarg) {
	    rasprintf(&name, "-%c*", c);
	    addMacro(mb->mc, name, NULL, optarg, mb->depth);
	    free(name);
	}
    }

    /* Add argument count (remaining non-option items) as macro. */
    {	char *ac = NULL;
    	rasprintf(&ac, "%d", (argc - optind));
    	addMacro(mb->mc, "#", NULL, ac, mb->depth);
	free(ac);
    }

    /* Add macro for each argument */
    if (argc - optind) {
	for (c = optind; c < argc; c++) {
	    char *name = NULL;
	    rasprintf(&name, "%d", (c - optind + 1));
	    addMacro(mb->mc, name, NULL, argv[c], mb->depth);
	    free(name);
	}
    }

    /* Add concatenated unexpanded arguments as yet another macro. */
    args = argvJoin(argv + optind, " ");
    addMacro(mb->mc, "*", NULL, args ? args : "", mb->depth);
    free(args);

exit:
    argvFree(argv);
    return *lastc ? lastc + 1 : lastc; 
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
    size_t blen = MACROBUFSIZ + msglen;
    char *buf = xmalloc(blen);

    strncpy(buf, msg, msglen);
    buf[msglen] = '\0';
    (void) expandU(mb, buf, blen);
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
    size_t blen = MACROBUFSIZ + fn + gn;
    char *buf = xmalloc(blen);
    char *b = NULL, *be;
    int c;

    buf[0] = '\0';
    if (g != NULL) {
	strncpy(buf, g, gn);
	buf[gn] = '\0';
	(void) expandU(mb, buf, blen);
    }
    if (STREQ("basename", f, fn)) {
	if ((b = strrchr(buf, '/')) == NULL)
	    b = buf;
	else
	    b++;
#if NOTYET
    /* XXX watchout for conflict with %dir */
    } else if (STREQ("dirname", f, fn)) {
	if ((b = strrchr(buf, '/')) != NULL)
	    *b = '\0';
	b = buf;
#endif
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
	(void) expandT(mb, b, strlen(b));
    }
    free(buf);
}

/**
 * The main macro recursion loop.
 * @todo Dynamically reallocate target buffer.
 * @param mb		macro expansion state
 * @return		0 on success, 1 on failure
 */
static int
expandMacro(MacroBuf mb)
{
    rpmMacroEntry *mep;
    rpmMacroEntry me;
    const char *s = mb->s, *se;
    const char *f, *fe;
    const char *g, *ge;
    size_t fn, gn;
    char *t = mb->t;	/* save expansion pointer for printExpand */
    int c;
    int rc = 0;
    int negate;
    const char * lastc;
    int chkexist;

    if (++mb->depth > max_macro_depth) {
	rpmlog(RPMLOG_ERR,
		_("Recursion depth(%d) greater than max(%d)\n"),
		mb->depth, max_macro_depth);
	mb->depth--;
	mb->expand_trace = 1;
	return 1;
    }

    while (rc == 0 && mb->nb > 0 && (c = *s) != '\0') {
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
		SAVECHAR(mb, c);
		continue;
		break;
	}

	/* Expand next macro */
	f = fe = NULL;
	g = ge = NULL;
	if (mb->depth > 1)	/* XXX full expansion for outermost level */
		t = mb->t;	/* save expansion pointer for printExpand */
	negate = 0;
	lastc = NULL;
	chkexist = 0;
	switch ((c = *s)) {
	default:		/* %name substitution */
		while (strchr("!?", *s) != NULL) {
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
		SAVECHAR(mb, c);
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
	if (STREQ("global", f, fn)) {
		s = doDefine(mb, se, RMIL_GLOBAL, 1);
		continue;
	}
	if (STREQ("define", f, fn)) {
		s = doDefine(mb, se, mb->depth, 0);
		continue;
	}
	if (STREQ("undefine", f, fn)) {
		s = doUndefine(mb->mc, se);
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
		const char *ls = s+sizeof("{lua:")-1;
		const char *lse = se-sizeof("}")+1;
		char *scriptbuf = (char *)xmalloc((lse-ls)+1);
		const char *printbuf;
		memcpy(scriptbuf, ls, lse-ls);
		scriptbuf[lse-ls] = '\0';
		rpmluaSetPrintBuffer(lua, 1);
		if (rpmluaRunScript(lua, scriptbuf, NULL) == -1)
		    rc = 1;
		printbuf = rpmluaGetPrintBuffer(lua);
		if (printbuf) {
		    size_t len = strlen(printbuf);
		    if (len > mb->nb)
			len = mb->nb;
		    memcpy(mb->t, printbuf, len);
		    mb->t += len;
		    mb->nb -= len;
		}
		rpmluaSetPrintBuffer(lua, 0);
		free(scriptbuf);
		s = se;
		continue;
	}
#endif

	/* XXX necessary but clunky */
	if (STREQ("basename", f, fn) ||
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
	mep = findEntry(mb->mc, f, fn);
	me = (mep ? *mep : NULL);

	/* XXX Special processing for flags */
	if (*f == '-') {
		if (me)
			me->used++;	/* Mark macro as used */
		if ((me == NULL && !negate) ||	/* Without -f, skip %{-f...} */
		    (me != NULL && negate)) {	/* With -f, skip %{!-f...} */
			s = se;
			continue;
		}

		if (g && g < ge) {		/* Expand X in %{-f:X} */
			rc = expandT(mb, g, gn);
		} else
		if (me && me->body && *me->body) {/* Expand %{-f}/%{-f*} */
			rc = expandT(mb, me->body, strlen(me->body));
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
			rc = expandT(mb, g, gn);
		} else
		if (me && me->body && *me->body) { /* Expand %{?f}/%{?f*} */
			rc = expandT(mb, me->body, strlen(me->body));
		}
		s = se;
		continue;
	}
	
	if (me == NULL) {	/* leave unknown %... as is */
#ifndef HACK
#if DEAD
		/* XXX hack to skip over empty arg list */
		if (fn == 1 && *f == '*') {
			s = se;
			continue;
		}
#endif
		/* XXX hack to permit non-overloaded %foo to be passed */
		c = '%';	/* XXX only need to save % */
		SAVECHAR(mb, c);
#else
		rpmlog(RPMLOG_ERR,
			_("Macro %%%.*s not found, skipping\n"), fn, f);
		s = se;
#endif
		continue;
	}

	/* Setup args for "%name " macros with opts */
	if (me && me->opts != NULL) {
		if (lastc != NULL) {
			se = grabArgs(mb, me, fe, lastc);
		} else {
			addMacro(mb->mc, "**", NULL, "", mb->depth);
			addMacro(mb->mc, "*", NULL, "", mb->depth);
			addMacro(mb->mc, "#", NULL, "0", mb->depth);
			addMacro(mb->mc, "0", NULL, me->name, mb->depth);
		}
	}

	/* Recursively expand body of macro */
	if (me->body && *me->body) {
		mb->s = me->body;
		rc = expandMacro(mb);
		if (rc == 0)
			me->used++;	/* Mark macro as used */
	}

	/* Free args for "%name " macros with opts */
	if (me->opts != NULL)
		freeArgs(mb);

	s = se;
    }

    *mb->t = '\0';
    mb->s = s;
    mb->depth--;
    if (rc != 0 || mb->expand_trace)
	printExpansion(mb, t, mb->t);
    return rc;
}


/* =============================================================== */

int
expandMacros(void * spec, rpmMacroContext mc, char * sbuf, size_t slen)
{
    MacroBuf mb = xcalloc(1, sizeof(*mb));
    char *tbuf = NULL;
    int rc = 0;

    if (sbuf == NULL || slen == 0) 
	goto exit;

    if (mc == NULL) mc = rpmGlobalMacroContext;

    tbuf = xcalloc(slen + 1, sizeof(*tbuf));

    mb->s = sbuf;
    mb->t = tbuf;
    mb->nb = slen;
    mb->depth = 0;
    mb->macro_trace = print_macro_trace;
    mb->expand_trace = print_expand_trace;

    mb->spec = spec;	/* (future) %file expansion info */
    mb->mc = mc;

    rc = expandMacro(mb);

    if (mb->nb == 0)
	rpmlog(RPMLOG_ERR, _("Target buffer overflow\n"));

    tbuf[slen] = '\0';	/* XXX just in case */
    strncpy(sbuf, tbuf, (slen - mb->nb + 1));

exit:
    _free(mb);
    _free(tbuf);
    return rc;
}

void
addMacro(rpmMacroContext mc,
	const char * n, const char * o, const char * b, int level)
{
    rpmMacroEntry * mep;

    if (mc == NULL) mc = rpmGlobalMacroContext;

    /* If new name, expand macro table */
    if ((mep = findEntry(mc, n, 0)) == NULL) {
	if (mc->firstFree == mc->macrosAllocated)
	    expandMacroTable(mc);
	if (mc->macroTable != NULL)
	    mep = mc->macroTable + mc->firstFree++;
    }

    if (mep != NULL) {
	/* Push macro over previous definition */
	pushMacro(mep, n, o, b, level);

	/* If new name, sort macro table */
	if ((*mep)->prev == NULL)
	    sortMacroTable(mc);
    }
}

void
delMacro(rpmMacroContext mc, const char * n)
{
    rpmMacroEntry * mep;

    if (mc == NULL) mc = rpmGlobalMacroContext;
    /* If name exists, pop entry */
    if ((mep = findEntry(mc, n, 0)) != NULL) {
	popMacro(mep);
	/* If deleted name, sort macro table */
	if (!(mep && *mep))
	    sortMacroTable(mc);
    }
}

int
rpmDefineMacro(rpmMacroContext mc, const char * macro, int level)
{
    MacroBuf mb = xcalloc(1, sizeof(*mb));

    /* XXX just enough to get by */
    mb->mc = (mc ? mc : rpmGlobalMacroContext);
    (void) doDefine(mb, macro, level, 0);
    _free(mb);
    return 0;
}

void
rpmLoadMacros(rpmMacroContext mc, int level)
{

    if (mc == NULL || mc == rpmGlobalMacroContext)
	return;

    if (mc->macroTable != NULL) {
	int i;
	for (i = 0; i < mc->firstFree; i++) {
	    rpmMacroEntry *mep, me;
	    mep = &mc->macroTable[i];
	    me = *mep;

	    if (me == NULL)		/* XXX this should never happen */
		continue;
	    addMacro(NULL, me->name, me->opts, me->body, (level - 1));
	}
    }
}

int
rpmLoadMacroFile(rpmMacroContext mc, const char * fn)
{
    FILE *fd = fopen(fn, "r");
    size_t blen = MACROBUFSIZ;
    char *buf = xmalloc(blen);
    int rc = -1;

    if (fd == NULL || ferror(fd)) {
	if (fd) (void) fclose(fd);
	goto exit;
    }

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
	rc = rpmDefineMacro(mc, n, RMIL_MACROFILES);
    }
    rc = fclose(fd);

exit:
    _free(buf);
    return rc;
}

void
rpmInitMacros(rpmMacroContext mc, const char * macrofiles)
{
    ARGV_t pattern, globs = NULL;

    if (macrofiles == NULL)
	return;

    argvSplit(&globs, macrofiles, ":");
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
	    (void) rpmLoadMacroFile(mc, *path);
	}
	argvFree(files);
    }
    argvFree(globs);

    /* Reload cmdline macros */
    rpmLoadMacros(rpmCLIMacroContext, RMIL_CMDLINE);
}

void
rpmFreeMacros(rpmMacroContext mc)
{
    
    if (mc == NULL) mc = rpmGlobalMacroContext;

    if (mc->macroTable != NULL) {
	int i;
	for (i = 0; i < mc->firstFree; i++) {
	    rpmMacroEntry me;
	    while ((me = mc->macroTable[i]) != NULL) {
		/* XXX cast to workaround const */
		if ((mc->macroTable[i] = me->prev) == NULL)
		    me->name = _free(me->name);
		me->opts = _free(me->opts);
		me->body = _free(me->body);
		me = _free(me);
	    }
	}
	mc->macroTable = _free(mc->macroTable);
    }
    memset(mc, 0, sizeof(*mc));
}

char * 
rpmExpand(const char *arg, ...)
{
    size_t blen = MACROBUFSIZ;
    char *buf = NULL;
    char *pe;
    const char *s;
    va_list ap;

    if (arg == NULL) {
	buf = xstrdup("");
	goto exit;
    }

    /* precalculate unexpanded size on top of MACROBUFSIZ */
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

    (void) expandMacros(NULL, NULL, buf, blen);

    /* expanded output is usually much less than alloced buffer, downsize */
    blen = strlen(buf);
    buf = xrealloc(buf, blen + 1);

exit:
    return buf;
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
    val = _free(val);

    return rc;
}
