/*@-boundsread@*/
/** \ingroup rpmrc rpmio
 * \file rpmio/macro.c
 */

#include "system.h"
#include <stdarg.h>

#if !defined(isblank)
#define	isblank(_c)	((_c) == ' ' || (_c) == '\t')
#endif
#define	iseol(_c)	((_c) == '\n' || (_c) == '\r')

#define	STREQ(_t, _f, _fn)	((_fn) == (sizeof(_t)-1) && !strncmp((_t), (_f), (_fn)))

#ifdef DEBUG_MACROS
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define rpmError fprintf
#define RPMERR_BADSPEC stderr
#undef	_
#define	_(x)	x

#define	vmefail()		(exit(1), NULL)
#define	urlPath(_xr, _r)	*(_r) = (_xr)

typedef	FILE * FD_t;
#define Fopen(_path, _fmode)	fopen(_path, "r");
#define	Ferror			ferror
#define Fstrerror(_fd)		strerror(errno)
#define	Fread			fread
#define	Fclose			fclose

#define	fdGetFILE(_fd)		(_fd)

#else

#include <rpmio_internal.h>
#include <rpmmessages.h>
#include <rpmerr.h>
#include <rpmlua.h>

#endif

#include <rpmmacro.h>

#include "debug.h"

#if defined(__LCLINT__)
/*@-exportheader@*/
extern const unsigned short int **__ctype_b_loc (void) /*@*/;
/*@=exportheader@*/
#endif

/*@access FD_t@*/		/* XXX compared with NULL */
/*@access MacroContext@*/
/*@access MacroEntry@*/
/*@access rpmlua @*/

static struct MacroContext_s rpmGlobalMacroContext_s;
/*@-compmempass@*/
MacroContext rpmGlobalMacroContext = &rpmGlobalMacroContext_s;
/*@=compmempass@*/

static struct MacroContext_s rpmCLIMacroContext_s;
/*@-compmempass@*/
MacroContext rpmCLIMacroContext = &rpmCLIMacroContext_s;
/*@=compmempass@*/

/**
 * Macro expansion state.
 */
typedef /*@abstract@*/ struct MacroBuf_s {
/*@kept@*/ /*@exposed@*/
    const char * s;		/*!< Text to expand. */
/*@shared@*/
    char * t;			/*!< Expansion buffer. */
    size_t nb;			/*!< No. bytes remaining in expansion buffer. */
    int depth;			/*!< Current expansion depth. */
    int macro_trace;		/*!< Pre-print macro to expand? */
    int expand_trace;		/*!< Post-print macro expansion? */
/*@kept@*/ /*@exposed@*/ /*@null@*/
    void * spec;		/*!< (future) %file expansion info?. */
/*@kept@*/ /*@exposed@*/
    MacroContext mc;
} * MacroBuf;

#define SAVECHAR(_mb, _c) { *(_mb)->t = (_c), (_mb)->t++, (_mb)->nb--; }

/*@-exportlocal -exportheadervar@*/

#define	_MAX_MACRO_DEPTH	16
/*@unchecked@*/
int max_macro_depth = _MAX_MACRO_DEPTH;

#define	_PRINT_MACRO_TRACE	0
/*@unchecked@*/
int print_macro_trace = _PRINT_MACRO_TRACE;

#define	_PRINT_EXPAND_TRACE	0
/*@unchecked@*/
int print_expand_trace = _PRINT_EXPAND_TRACE;
/*@=exportlocal =exportheadervar@*/

#define	MACRO_CHUNK_SIZE	16

/* forward ref */
static int expandMacro(MacroBuf mb)
	/*@globals rpmGlobalMacroContext,
		print_macro_trace, print_expand_trace, h_errno, fileSystem @*/
	/*@modifies mb, rpmGlobalMacroContext,
		print_macro_trace, print_expand_trace, fileSystem @*/;

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ const void * p)
	/*@modifies p@*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

/* =============================================================== */

/**
 * Compare macro entries by name (qsort/bsearch).
 * @param ap		1st macro entry
 * @param bp		2nd macro entry
 * @return		result of comparison
 */
static int
compareMacroName(const void * ap, const void * bp)
	/*@*/
{
    MacroEntry ame = *((MacroEntry *)ap);
    MacroEntry bme = *((MacroEntry *)bp);

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
/*@-boundswrite@*/
static void
expandMacroTable(MacroContext mc)
	/*@modifies mc @*/
{
    if (mc->macroTable == NULL) {
	mc->macrosAllocated = MACRO_CHUNK_SIZE;
	mc->macroTable = (MacroEntry *)
	    xmalloc(sizeof(*(mc->macroTable)) * mc->macrosAllocated);
	mc->firstFree = 0;
    } else {
	mc->macrosAllocated += MACRO_CHUNK_SIZE;
	mc->macroTable = (MacroEntry *)
	    xrealloc(mc->macroTable, sizeof(*(mc->macroTable)) *
			mc->macrosAllocated);
    }
    memset(&mc->macroTable[mc->firstFree], 0, MACRO_CHUNK_SIZE * sizeof(*(mc->macroTable)));
}
/*@=boundswrite@*/

/**
 * Sort entries in macro table.
 * @param mc		macro context
 */
static void
sortMacroTable(MacroContext mc)
	/*@modifies mc @*/
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
rpmDumpMacroTable(MacroContext mc, FILE * fp)
{
    int nempty = 0;
    int nactive = 0;

    if (mc == NULL) mc = rpmGlobalMacroContext;
    if (fp == NULL) fp = stderr;
    
    fprintf(fp, "========================\n");
    if (mc->macroTable != NULL) {
	int i;
	for (i = 0; i < mc->firstFree; i++) {
	    MacroEntry me;
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
/*@-boundswrite@*/
/*@dependent@*/ /*@null@*/
static MacroEntry *
findEntry(MacroContext mc, const char * name, size_t namelen)
	/*@*/
{
    MacroEntry key, *ret;
    struct MacroEntry_s keybuf;
    char namebuf[1024];

/*@-globs@*/
    if (mc == NULL) mc = rpmGlobalMacroContext;
/*@=globs@*/
    if (mc->macroTable == NULL || mc->firstFree == 0)
	return NULL;

/*@-branchstate@*/
    if (namelen > 0) {
	strncpy(namebuf, name, namelen);
	namebuf[namelen] = '\0';
	name = namebuf;
    }
/*@=branchstate@*/
    
    key = &keybuf;
    memset(key, 0, sizeof(*key));
    /*@-temptrans -assignexpose@*/
    key->name = (char *)name;
    /*@=temptrans =assignexpose@*/
    ret = (MacroEntry *) bsearch(&key, mc->macroTable, mc->firstFree,
			sizeof(*(mc->macroTable)), compareMacroName);
    /* XXX TODO: find 1st empty slot and return that */
    return ret;
}
/*@=boundswrite@*/

/* =============================================================== */

/**
 * fgets(3) analogue that reads \ continuations. Last newline always trimmed.
 * @param buf		input buffer
 * @param size		inbut buffer size (bytes)
 * @param fd		file handle
 * @param escapes	permit escaped newlines?
 * @return		buffer, or NULL on end-of-file
 */
/*@-boundswrite@*/
/*@null@*/
static char *
rdcl(/*@returned@*/ char * buf, size_t size, FD_t fd)
	/*@globals fileSystem @*/
	/*@modifies buf, fileSystem @*/
{
    char *q = buf - 1;		/* initialize just before buffer. */
    size_t nb = 0;
    size_t nread = 0;
    FILE * f = fdGetFILE(fd);
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
			case '\0': /*@switchbreak@*/ break;
			default: p++; /*@switchbreak@*/ break;
		    }
		    /*@switchbreak@*/ break;
		case '%':
		    switch (*(p+1)) {
			case '{': p++, bc++; /*@switchbreak@*/ break;
			case '(': p++, pc++; /*@switchbreak@*/ break;
			case '%': p++; /*@switchbreak@*/ break;
		    }
		    /*@switchbreak@*/ break;
		case '{': if (bc > 0) bc++; /*@switchbreak@*/ break;
		case '}': if (bc > 0) bc--; /*@switchbreak@*/ break;
		case '(': if (pc > 0) pc++; /*@switchbreak@*/ break;
		case ')': if (pc > 0) pc--; /*@switchbreak@*/ break;
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
/*@=boundswrite@*/

/**
 * Return text between pl and matching pr characters.
 * @param p		start of text
 * @param pl		left char, i.e. '[', '(', '{', etc.
 * @param pr		right char, i.e. ']', ')', '}', etc.
 * @return		address of last char before pr (or NULL)
 */
/*@null@*/
static const char *
matchchar(const char * p, char pl, char pr)
	/*@*/
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
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
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
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
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
	/*@-globs@*/	/* FIX: __ctype_b */ \
	while (((_c) = *(_s)) && isblank(_c)) \
		(_s)++;		\
	/*@=globs@*/

#define	SKIPNONBLANK(_s, _c)	\
	/*@-globs@*/	/* FIX: __ctype_b */ \
	while (((_c) = *(_s)) && !(isblank(_c) || iseol(_c))) \
		(_s)++;		\
	/*@=globs@*/

#define	COPYNAME(_ne, _s, _c)	\
    {	SKIPBLANK(_s,_c);	\
	/*@-boundswrite@*/	\
	while(((_c) = *(_s)) && (xisalnum(_c) || (_c) == '_')) \
		*(_ne)++ = *(_s)++; \
	*(_ne) = '\0';		\
	/*@=boundswrite@*/	\
    }

#define	COPYOPTS(_oe, _s, _c)	\
    {	/*@-boundswrite@*/	\
	while(((_c) = *(_s)) && (_c) != ')') \
		*(_oe)++ = *(_s)++; \
	*(_oe) = '\0';		\
	/*@=boundswrite@*/	\
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
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem@*/
	/*@modifies mb, rpmGlobalMacroContext, fileSystem @*/
{
    char *sbuf;
    const char *s = mb->s;
    int rc;

    sbuf = alloca(flen + 1);
    memset(sbuf, 0, (flen + 1));

    strncpy(sbuf, f, flen);
    sbuf[flen] = '\0';
    mb->s = sbuf;
    rc = expandMacro(mb);
    mb->s = s;
    return rc;
}

#if 0
/**
 * Save target and expand sbuf into target.
 * @param mb		macro expansion state
 * @param tbuf		target buffer
 * @param tbuflen	no. bytes in target buffer
 * @return		result of expansion
 */
static int
expandS(MacroBuf mb, char * tbuf, size_t tbuflen)
	/*@globals rpmGlobalMacroContext, fileSystem@*/
	/*@modifies mb, *tbuf, rpmGlobalMacroContext, fileSystem @*/
{
    const char *t = mb->t;
    size_t nb = mb->nb;
    int rc;

    mb->t = tbuf;
    mb->nb = tbuflen;
    rc = expandMacro(mb);
    mb->t = t;
    mb->nb = nb;
    return rc;
}
#endif

/**
 * Save source/target and expand macro in u.
 * @param mb		macro expansion state
 * @param u		input macro, output expansion
 * @param ulen		no. bytes in u buffer
 * @return		result of expansion
 */
/*@-boundswrite@*/
static int
expandU(MacroBuf mb, char * u, size_t ulen)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem@*/
	/*@modifies mb, *u, rpmGlobalMacroContext, fileSystem @*/
{
    const char *s = mb->s;
    char *t = mb->t;
    size_t nb = mb->nb;
    char *tbuf;
    int rc;

    tbuf = alloca(ulen + 1);
    memset(tbuf, 0, (ulen + 1));

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

    return rc;
}
/*@=boundswrite@*/

/**
 * Expand output of shell command into target buffer.
 * @param mb		macro expansion state
 * @param cmd		shell command
 * @param clen		no. bytes in shell command
 * @return		result of expansion
 */
/*@-boundswrite@*/
static int
doShellEscape(MacroBuf mb, const char * cmd, size_t clen)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem @*/
	/*@modifies mb, rpmGlobalMacroContext, fileSystem @*/
{
    char pcmd[BUFSIZ];
    FILE *shf;
    int rc;
    int c;

    strncpy(pcmd, cmd, clen);
    pcmd[clen] = '\0';
    rc = expandU(mb, pcmd, sizeof(pcmd));
    if (rc)
	return rc;

    if ((shf = popen(pcmd, "r")) == NULL)
	return 1;
    while(mb->nb > 0 && (c = fgetc(shf)) != EOF)
	SAVECHAR(mb, c);
    (void) pclose(shf);

    /* XXX delete trailing \r \n */
    while (iseol(mb->t[-1])) {
	*(mb->t--) = '\0';
	mb->nb++;
    }
    return 0;
}
/*@=boundswrite@*/

/**
 * Parse (and execute) new macro definition.
 * @param mb		macro expansion state
 * @param se		macro definition to parse
 * @param level		macro recursion level
 * @param expandbody	should body be expanded?
 * @return		address to continue parsing
 */
/*@dependent@*/ static const char *
doDefine(MacroBuf mb, /*@returned@*/ const char * se, int level, int expandbody)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies mb, rpmGlobalMacroContext @*/
{
    const char *s = se;
    char buf[BUFSIZ], *n = buf, *ne = n;
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
	    rpmError(RPMERR_BADSPEC,
		_("Macro %%%s has unterminated body\n"), n);
	    se = s;	/* XXX W2DO? */
	    return se;
	}
	s++;	/* XXX skip { */
/*@-boundswrite@*/
	strncpy(b, s, (se - s));
	b[se - s] = '\0';
/*@=boundswrite@*/
	be += strlen(b);
	se++;	/* XXX skip } */
	s = se;	/* move scan forward */
    } else {	/* otherwise free-field */
/*@-boundswrite@*/
	int bc = 0, pc = 0;
	while (*s && (bc || pc || !iseol(*s))) {
	    switch (*s) {
		case '\\':
		    switch (*(s+1)) {
			case '\0': /*@switchbreak@*/ break;
			default: s++; /*@switchbreak@*/ break;
		    }
		    /*@switchbreak@*/ break;
		case '%':
		    switch (*(s+1)) {
			case '{': *be++ = *s++; bc++; /*@switchbreak@*/ break;
			case '(': *be++ = *s++; pc++; /*@switchbreak@*/ break;
			case '%': *be++ = *s++; /*@switchbreak@*/ break;
		    }
		    /*@switchbreak@*/ break;
		case '{': if (bc > 0) bc++; /*@switchbreak@*/ break;
		case '}': if (bc > 0) bc--; /*@switchbreak@*/ break;
		case '(': if (pc > 0) pc++; /*@switchbreak@*/ break;
		case ')': if (pc > 0) pc--; /*@switchbreak@*/ break;
	    }
	    *be++ = *s++;
	}
	*be = '\0';

	if (bc || pc) {
	    rpmError(RPMERR_BADSPEC,
		_("Macro %%%s has unterminated body\n"), n);
	    se = s;	/* XXX W2DO? */
	    return se;
	}

	/* Trim trailing blanks/newlines */
/*@-globs@*/
	while (--be >= b && (c = *be) && (isblank(c) || iseol(c)))
	    {};
/*@=globs@*/
	*(++be) = '\0';	/* one too far */
/*@=boundswrite@*/
    }

    /* Move scan over body */
    while (iseol(*s))
	s++;
    se = s;

    /* Names must start with alphabetic or _ and be at least 3 chars */
    if (!((c = *n) && (xisalpha(c) || c == '_') && (ne - n) > 2)) {
	rpmError(RPMERR_BADSPEC,
		_("Macro %%%s has illegal name (%%define)\n"), n);
	return se;
    }

    /* Options must be terminated with ')' */
    if (o && oc != ')') {
	rpmError(RPMERR_BADSPEC, _("Macro %%%s has unterminated opts\n"), n);
	return se;
    }

    if ((be - b) < 1) {
	rpmError(RPMERR_BADSPEC, _("Macro %%%s has empty body\n"), n);
	return se;
    }

/*@-modfilesys@*/
    if (expandbody && expandU(mb, b, (&buf[sizeof(buf)] - b))) {
	rpmError(RPMERR_BADSPEC, _("Macro %%%s failed to expand\n"), n);
	return se;
    }
/*@=modfilesys@*/

    addMacro(mb->mc, n, o, b, (level - 1));

    return se;
}

/**
 * Parse (and execute) macro undefinition.
 * @param mc		macro context
 * @param se		macro name to undefine
 * @return		address to continue parsing
 */
/*@dependent@*/ static const char *
doUndefine(MacroContext mc, /*@returned@*/ const char * se)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies mc, rpmGlobalMacroContext @*/
{
    const char *s = se;
    char buf[BUFSIZ], *n = buf, *ne = n;
    int c;

    COPYNAME(ne, s, c);

    /* Move scan over body */
    while (iseol(*s))
	s++;
    se = s;

    /* Names must start with alphabetic or _ and be at least 3 chars */
    if (!((c = *n) && (xisalpha(c) || c == '_') && (ne - n) > 2)) {
	rpmError(RPMERR_BADSPEC,
		_("Macro %%%s has illegal name (%%undefine)\n"), n);
	return se;
    }

    delMacro(mc, n);

    return se;
}

#ifdef	DYING
static void
dumpME(const char * msg, MacroEntry me)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    if (msg)
	fprintf(stderr, "%s", msg);
    fprintf(stderr, "\tme %p", me);
    if (me)
	fprintf(stderr,"\tname %p(%s) prev %p",
		me->name, me->name, me->prev);
    fprintf(stderr, "\n");
}
#endif

/**
 * Push new macro definition onto macro entry stack.
 * @param mep		address of macro entry slot
 * @param n		macro name
 * @param o		macro parameters (NULL if none)
 * @param b		macro body (NULL becomes "")
 * @param level		macro recursion level
 */
static void
pushMacro(/*@out@*/ MacroEntry * mep,
		const char * n, /*@null@*/ const char * o,
		/*@null@*/ const char * b, int level)
	/*@modifies *mep @*/
{
    MacroEntry prev = (mep && *mep ? *mep : NULL);
    MacroEntry me = (MacroEntry) xmalloc(sizeof(*me));

    /*@-assignexpose@*/
    me->prev = prev;
    /*@=assignexpose@*/
    me->name = (prev ? prev->name : xstrdup(n));
    me->opts = (o ? xstrdup(o) : NULL);
    me->body = xstrdup(b ? b : "");
    me->used = 0;
    me->level = level;
/*@-boundswrite@*/
/*@-branchstate@*/
    if (mep)
	*mep = me;
    else
	me = _free(me);
/*@=branchstate@*/
/*@=boundswrite@*/
}

/**
 * Pop macro definition from macro entry stack.
 * @param mep		address of macro entry slot
 */
static void
popMacro(MacroEntry * mep)
	/*@modifies *mep @*/
{
	MacroEntry me = (*mep ? *mep : NULL);

/*@-branchstate@*/
	if (me) {
		/* XXX cast to workaround const */
		/*@-onlytrans@*/
/*@-boundswrite@*/
		if ((*mep = me->prev) == NULL)
			me->name = _free(me->name);
/*@=boundswrite@*/
		me->opts = _free(me->opts);
		me->body = _free(me->body);
		me = _free(me);
		/*@=onlytrans@*/
	}
/*@=branchstate@*/
}

/**
 * Free parsed arguments for parameterized macro.
 * @param mb		macro expansion state
 */
static void
freeArgs(MacroBuf mb)
	/*@modifies mb @*/
{
    MacroContext mc = mb->mc;
    int ndeleted = 0;
    int i;

    if (mc == NULL || mc->macroTable == NULL)
	return;

    /* Delete dynamic macro definitions */
    for (i = 0; i < mc->firstFree; i++) {
	MacroEntry *mep, me;
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
	    rpmError(RPMERR_BADSPEC,
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
/*@-bounds@*/
/*@dependent@*/ static const char *
grabArgs(MacroBuf mb, const MacroEntry me, /*@returned@*/ const char * se,
		const char * lastc)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies mb, rpmGlobalMacroContext @*/
{
    char buf[BUFSIZ], *b, *be;
    char aname[16];
    const char *opts, *o;
    int argc = 0;
    const char **argv;
    int c;

    /* Copy macro name as argv[0], save beginning of args.  */
    buf[0] = '\0';
    b = be = stpcpy(buf, me->name);

    addMacro(mb->mc, "0", NULL, buf, mb->depth);
    
    argc = 1;	/* XXX count argv[0] */

    /* Copy args into buf until lastc */
    *be++ = ' ';
    while ((c = *se++) != '\0' && (se-1) != lastc) {
/*@-globs@*/
	if (!isblank(c)) {
	    *be++ = c;
	    continue;
	}
/*@=globs@*/
	/* c is blank */
	if (be[-1] == ' ')
	    continue;
	/* a word has ended */
	*be++ = ' ';
	argc++;
    }
    if (c == '\0') se--;	/* one too far */
    if (be[-1] != ' ')
	argc++, be++;		/* last word has not trailing ' ' */
    be[-1] = '\0';
    if (*b == ' ') b++;		/* skip the leading ' ' */

/*
 * The macro %* analoguous to the shell's $* means "Pass all non-macro
 * parameters." Consequently, there needs to be a macro that means "Pass all
 * (including macro parameters) options". This is useful for verifying
 * parameters during expansion and yet transparently passing all parameters
 * through for higher level processing (e.g. %description and/or %setup).
 * This is the (potential) justification for %{**} ...
 */
    /* Add unexpanded args as macro */
    addMacro(mb->mc, "**", NULL, b, mb->depth);

#ifdef NOTYET
    /* XXX if macros can be passed as args ... */
    expandU(mb, buf, sizeof(buf));
#endif

    /* Build argv array */
    argv = (const char **) alloca((argc + 1) * sizeof(*argv));
    be[-1] = ' '; /* assert((be - 1) == (b + strlen(b) == buf + strlen(buf))) */
    be[0] = '\0';
    b = buf;
    for (c = 0; c < argc; c++) {
	argv[c] = b;
	b = strchr(b, ' ');
	*b++ = '\0';
    }
    /* assert(b == be);  */
    argv[argc] = NULL;

    /* Citation from glibc/posix/getopt.c:
     *    Index in ARGV of the next element to be scanned.
     *    This is used for communication to and from the caller
     *    and for communication between successive calls to `getopt'.
     *
     *    On entry to `getopt', zero means this is the first call; initialize.
     *
     *    When `getopt' returns -1, this is the index of the first of the
     *    non-option elements that the caller should itself scan.
     *
     *    Otherwise, `optind' communicates from one call to the next
     *    how much of ARGV has been scanned so far.
     */
    /* 1003.2 says this must be 1 before any call.  */

#ifdef __GLIBC__
    /*@-mods@*/
    optind = 0;		/* XXX but posix != glibc */
    /*@=mods@*/
#else
    optind = 1;
#endif

    opts = me->opts;

    /* Define option macros. */
/*@-nullstate@*/ /* FIX: argv[] can be NULL */
    while((c = getopt(argc, (char **)argv, opts)) != -1)
/*@=nullstate@*/
    {
	if (c == '?' || (o = strchr(opts, c)) == NULL) {
	    rpmError(RPMERR_BADSPEC, _("Unknown option %c in %s(%s)\n"),
			(char)c, me->name, opts);
	    return se;
	}
	*be++ = '-';
	*be++ = c;
	if (o[1] == ':') {
	    *be++ = ' ';
	    be = stpcpy(be, optarg);
	}
	*be++ = '\0';
	aname[0] = '-'; aname[1] = c; aname[2] = '\0';
	addMacro(mb->mc, aname, NULL, b, mb->depth);
	if (o[1] == ':') {
	    aname[0] = '-'; aname[1] = c; aname[2] = '*'; aname[3] = '\0';
	    addMacro(mb->mc, aname, NULL, optarg, mb->depth);
	}
	be = b; /* reuse the space */
    }

    /* Add arg count as macro. */
    sprintf(aname, "%d", (argc - optind));
    addMacro(mb->mc, "#", NULL, aname, mb->depth);

    /* Add macro for each arg. Concatenate args for %*. */
    if (be) {
	*be = '\0';
	for (c = optind; c < argc; c++) {
	    sprintf(aname, "%d", (c - optind + 1));
	    addMacro(mb->mc, aname, NULL, argv[c], mb->depth);
	    if (be != b) *be++ = ' '; /* Add space between args */
/*@-nullpass@*/ /* FIX: argv[] can be NULL */
	    be = stpcpy(be, argv[c]);
/*@=nullpass@*/
	}
    }

    /* Add unexpanded args as macro. */
    addMacro(mb->mc, "*", NULL, b, mb->depth);

    return se;
}
/*@=bounds@*/

/**
 * Perform macro message output
 * @param mb		macro expansion state
 * @param waserror	use rpmError()?
 * @param msg		message to ouput
 * @param msglen	no. of bytes in message
 */
static void
doOutput(MacroBuf mb, int waserror, const char * msg, size_t msglen)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem @*/
	/*@modifies mb, rpmGlobalMacroContext, fileSystem @*/
{
    char buf[BUFSIZ];

    strncpy(buf, msg, msglen);
    buf[msglen] = '\0';
    (void) expandU(mb, buf, sizeof(buf));
    if (waserror)
	rpmError(RPMERR_BADSPEC, "%s\n", buf);
    else
	fprintf(stderr, "%s", buf);
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
		/*@null@*/ const char * g, size_t gn)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies mb, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    char buf[BUFSIZ], *b = NULL, *be;
    int c;

    buf[0] = '\0';
    if (g != NULL) {
	strncpy(buf, g, gn);
	buf[gn] = '\0';
	(void) expandU(mb, buf, sizeof(buf));
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
/*@-branchstate@*/
	if (*b == '\0') b = "/";
/*@=branchstate@*/
    } else if (STREQ("uncompress", f, fn)) {
	rpmCompressedMagic compressed = COMPRESSED_OTHER;
/*@-globs@*/
	for (b = buf; (c = *b) && isblank(c);)
	    b++;
	for (be = b; (c = *be) && !isblank(c);)
	    be++;
/*@=globs@*/
	*be++ = '\0';
#ifndef	DEBUG_MACROS
	(void) isCompressed(b, &compressed);
#endif
	switch(compressed) {
	default:
	case 0:	/* COMPRESSED_NOT */
	    sprintf(be, "%%_cat %s", b);
	    break;
	case 1:	/* COMPRESSED_OTHER */
	    sprintf(be, "%%_gzip -dc %s", b);
	    break;
	case 2:	/* COMPRESSED_BZIP2 */
	    sprintf(be, "%%_bzip2 %s", b);
	    break;
	case 3:	/* COMPRESSED_ZIP */
	    sprintf(be, "%%_unzip %s", b);
	    break;
	}
	b = be;
    } else if (STREQ("S", f, fn)) {
	for (b = buf; (c = *b) && xisdigit(c);)
	    b++;
	if (!c) {	/* digit index */
	    b++;
	    sprintf(b, "%%SOURCE%s", buf);
	} else
	    b = buf;
    } else if (STREQ("P", f, fn)) {
	for (b = buf; (c = *b) && xisdigit(c);)
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
}

/**
 * The main macro recursion loop.
 * @todo Dynamically reallocate target buffer.
 * @param mb		macro expansion state
 * @return		0 on success, 1 on failure
 */
static int
expandMacro(MacroBuf mb)
	/*@globals rpmGlobalMacroContext,
		print_macro_trace, print_expand_trace, h_errno, fileSystem @*/
	/*@modifies mb, rpmGlobalMacroContext,
		print_macro_trace, print_expand_trace, fileSystem @*/
{
    MacroEntry *mep;
    MacroEntry me;
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
	rpmError(RPMERR_BADSPEC,
		_("Recursion depth(%d) greater than max(%d)\n"),
		mb->depth, max_macro_depth);
	mb->depth--;
	mb->expand_trace = 1;
	return 1;
    }

/*@-branchstate@*/
    while (rc == 0 && mb->nb > 0 && (c = *s) != '\0') {
	s++;
	/* Copy text until next macro */
	switch(c) {
	case '%':
		if (*s != '%')
			/*@switchbreak@*/ break;
		s++;	/* skip first % in %% */
		/*@fallthrough@*/
	default:
		SAVECHAR(mb, c);
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
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
				/*@switchbreak@*/ break;
			case '?':
				chkexist++;
				/*@switchbreak@*/ break;
			}
		}
		f = se = s;
		if (*se == '-')
			se++;
		while((c = *se) && (xisalnum(c) || c == '_'))
			se++;
		/* Recognize non-alnum macros too */
		switch (*se) {
		case '*':
			se++;
			if (*se == '*') se++;
			/*@innerbreak@*/ break;
		case '#':
			se++;
			/*@innerbreak@*/ break;
		default:
			/*@innerbreak@*/ break;
		}
		fe = se;
		/* For "%name " macros ... */
/*@-globs@*/
		if ((c = *fe) && isblank(c))
			if ((lastc = strchr(fe,'\n')) == NULL)
                lastc = strchr(fe, '\0');
/*@=globs@*/
		/*@switchbreak@*/ break;
	case '(':		/* %(...) shell escape */
		if ((se = matchchar(s, c, ')')) == NULL) {
			rpmError(RPMERR_BADSPEC,
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
		/*@notreached@*/ /*@switchbreak@*/ break;
	case '{':		/* %{...}/%{...:...} substitution */
		if ((se = matchchar(s, c, '}')) == NULL) {
			rpmError(RPMERR_BADSPEC,
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
				/*@switchbreak@*/ break;
			case '?':
				chkexist++;
				/*@switchbreak@*/ break;
			}
		}
		for (fe = f; (c = *fe) && !strchr(" :}", c);)
			fe++;
		switch (c) {
		case ':':
			g = fe + 1;
			ge = se - 1;
			/*@innerbreak@*/ break;
		case ' ':
			lastc = se-1;
			/*@innerbreak@*/ break;
		default:
			/*@innerbreak@*/ break;
		}
		/*@switchbreak@*/ break;
	}

	/* XXX Everything below expects fe > f */
	fn = (fe - f);
	gn = (ge - g);
	if ((fe - f) <= 0) {
/* XXX Process % in unknown context */
		c = '%';	/* XXX only need to save % */
		SAVECHAR(mb, c);
#if 0
		rpmError(RPMERR_BADSPEC,
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
		    int len = strlen(printbuf);
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

	/* XXX necessary but clunky */
	if (STREQ("basename", f, fn) ||
	    STREQ("suffix", f, fn) ||
	    STREQ("expand", f, fn) ||
	    STREQ("verbose", f, fn) ||
	    STREQ("uncompress", f, fn) ||
	    STREQ("url2path", f, fn) ||
	    STREQ("u2p", f, fn) ||
	    STREQ("S", f, fn) ||
	    STREQ("P", f, fn) ||
	    STREQ("F", f, fn)) {
		/*@-internalglobs@*/ /* FIX: verbose may be set */
		doFoo(mb, negate, f, fn, g, gn);
		/*@=internalglobs@*/
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
		rpmError(RPMERR_BADSPEC,
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
/*@=branchstate@*/

    *mb->t = '\0';
    mb->s = s;
    mb->depth--;
    if (rc != 0 || mb->expand_trace)
	printExpansion(mb, t, mb->t);
    return rc;
}

/* =============================================================== */
/* XXX dupe'd to avoid change in linkage conventions. */

#define POPT_ERROR_NOARG        -10     /*!< missing argument */
#define POPT_ERROR_BADQUOTE     -15     /*!< error in paramter quoting */
#define POPT_ERROR_MALLOC       -21     /*!< memory allocation failed */

#define POPT_ARGV_ARRAY_GROW_DELTA 5

/*@-boundswrite@*/
static int XpoptDupArgv(int argc, const char **argv,
		int * argcPtr, const char *** argvPtr)
	/*@modifies *argcPtr, *argvPtr @*/
{
    size_t nb = (argc + 1) * sizeof(*argv);
    const char ** argv2;
    char * dst;
    int i;

    if (argc <= 0 || argv == NULL)	/* XXX can't happen */
	return POPT_ERROR_NOARG;
    for (i = 0; i < argc; i++) {
	if (argv[i] == NULL)
	    return POPT_ERROR_NOARG;
	nb += strlen(argv[i]) + 1;
    }
	
    dst = malloc(nb);
    if (dst == NULL)			/* XXX can't happen */
	return POPT_ERROR_MALLOC;
    argv2 = (void *) dst;
    dst += (argc + 1) * sizeof(*argv);

    /*@-branchstate@*/
    for (i = 0; i < argc; i++) {
	argv2[i] = dst;
	dst += strlen(strcpy(dst, argv[i])) + 1;
    }
    /*@=branchstate@*/
    argv2[argc] = NULL;

    if (argvPtr) {
	*argvPtr = argv2;
    } else {
	free(argv2);
	argv2 = NULL;
    }
    if (argcPtr)
	*argcPtr = argc;
    return 0;
}
/*@=boundswrite@*/

/*@-bounds@*/
static int XpoptParseArgvString(const char * s, int * argcPtr, const char *** argvPtr)
	/*@modifies *argcPtr, *argvPtr @*/
{
    const char * src;
    char quote = '\0';
    int argvAlloced = POPT_ARGV_ARRAY_GROW_DELTA;
    const char ** argv = malloc(sizeof(*argv) * argvAlloced);
    int argc = 0;
    int buflen = strlen(s) + 1;
    char * buf = memset(alloca(buflen), 0, buflen);
    int rc = POPT_ERROR_MALLOC;

    if (argv == NULL) return rc;
    argv[argc] = buf;

    for (src = s; *src != '\0'; src++) {
	if (quote == *src) {
	    quote = '\0';
	} else if (quote != '\0') {
	    if (*src == '\\') {
		src++;
		if (!*src) {
		    rc = POPT_ERROR_BADQUOTE;
		    goto exit;
		}
		if (*src != quote) *buf++ = '\\';
	    }
	    *buf++ = *src;
	} else if (isspace(*src)) {
	    if (*argv[argc] != '\0') {
		buf++, argc++;
		if (argc == argvAlloced) {
		    argvAlloced += POPT_ARGV_ARRAY_GROW_DELTA;
		    argv = realloc(argv, sizeof(*argv) * argvAlloced);
		    if (argv == NULL) goto exit;
		}
		argv[argc] = buf;
	    }
	} else switch (*src) {
	  case '"':
	  case '\'':
	    quote = *src;
	    /*@switchbreak@*/ break;
	  case '\\':
	    src++;
	    if (!*src) {
		rc = POPT_ERROR_BADQUOTE;
		goto exit;
	    }
	    /*@fallthrough@*/
	  default:
	    *buf++ = *src;
	    /*@switchbreak@*/ break;
	}
    }

    if (strlen(argv[argc])) {
	argc++, buf++;
    }

    rc = XpoptDupArgv(argc, argv, argcPtr, argvPtr);

exit:
    if (argv) free(argv);
    return rc;
}
/*@=bounds@*/
/* =============================================================== */
/*@unchecked@*/
static int _debug = 0;

int rpmGlob(const char * patterns, int * argcPtr, const char *** argvPtr)
{
    int ac = 0;
    const char ** av = NULL;
    int argc = 0;
    const char ** argv = NULL;
    char * globRoot = NULL;
#ifdef ENABLE_NLS	
    const char * old_collate = NULL;
    const char * old_ctype = NULL;
    const char * t;
#endif
	size_t maxb, nb;
    int i, j;
    int rc;

    rc = XpoptParseArgvString(patterns, &ac, &av);
    if (rc)
	return rc;
#ifdef ENABLE_NLS
/*@-branchstate@*/
	t = setlocale(LC_COLLATE, NULL);
	if (t)
	    old_collate = xstrdup(t);
	t = setlocale(LC_CTYPE, NULL);
	if (t)
	    old_ctype = xstrdup(t);
/*@=branchstate@*/
	(void) setlocale(LC_COLLATE, "C");
	(void) setlocale(LC_CTYPE, "C");
#endif
	
    if (av != NULL)
    for (j = 0; j < ac; j++) {
	const char * globURL;
	const char * path;
	int ut = urlPath(av[j], &path);
	glob_t gl;

	if (!Glob_pattern_p(av[j], 0) && strchr(path, '~') == NULL) {
	    argv = xrealloc(argv, (argc+2) * sizeof(*argv));
	    argv[argc] = xstrdup(av[j]);
if (_debug)
fprintf(stderr, "*** rpmGlob argv[%d] \"%s\"\n", argc, argv[argc]);
	    argc++;
	    continue;
	}
	
	gl.gl_pathc = 0;
	gl.gl_pathv = NULL;
	rc = Glob(av[j], GLOB_TILDE, Glob_error, &gl);
	if (rc)
	    goto exit;

	/* XXX Prepend the URL leader for globs that have stripped it off */
	maxb = 0;
	for (i = 0; i < gl.gl_pathc; i++) {
	    if ((nb = strlen(&(gl.gl_pathv[i][0]))) > maxb)
		maxb = nb;
	}
	
	nb = ((ut > URL_IS_DASH && ut != URL_IS_FTP) ? (path - av[j]) : 0);
	maxb += nb;
	maxb += 1;
	globURL = globRoot = xmalloc(maxb);

	switch (ut) {
	case URL_IS_HTTP:
	case URL_IS_PATH:
	case URL_IS_DASH:
	    strncpy(globRoot, av[j], nb);
	    /*@switchbreak@*/ break;
	case URL_IS_FTP:
	case URL_IS_UNKNOWN:
	    /*@switchbreak@*/ break;
	}
	globRoot += nb;
	*globRoot = '\0';
if (_debug)
fprintf(stderr, "*** GLOB maxb %d diskURL %d %*s globURL %p %s\n", (int)maxb, (int)nb, (int)nb, av[j], globURL, globURL);
	
	argv = xrealloc(argv, (argc+gl.gl_pathc+1) * sizeof(*argv));

	if (argv != NULL)
	for (i = 0; i < gl.gl_pathc; i++) {
	    const char * globFile = &(gl.gl_pathv[i][0]);
	    if (globRoot > globURL && globRoot[-1] == '/')
		while (*globFile == '/') globFile++;
	    strcpy(globRoot, globFile);
if (_debug)
fprintf(stderr, "*** rpmGlob argv[%d] \"%s\"\n", argc, globURL);
	    argv[argc++] = xstrdup(globURL);
	}
	/*@-immediatetrans@*/
	Globfree(&gl);
	/*@=immediatetrans@*/
	globURL = _free(globURL);
    }

    if (argv != NULL && argc > 0) {
	argv[argc] = NULL;
	if (argvPtr)
	    *argvPtr = argv;
	if (argcPtr)
	    *argcPtr = argc;
	rc = 0;
    } else
	rc = 1;


exit:
#ifdef ENABLE_NLS
/*@-branchstate@*/
    if (old_collate) {
	(void) setlocale(LC_COLLATE, old_collate);
	old_collate = _free(old_collate);
    }
    if (old_ctype) {
	(void) setlocale(LC_CTYPE, old_ctype);
	old_ctype = _free(old_ctype);
    }
/*@=branchstate@*/
#endif
	av = _free(av);
/*@-branchstate@*/
    if (rc || argvPtr == NULL) {
/*@-dependenttrans -unqualifiedtrans@*/
	if (argv != NULL)
	for (i = 0; i < argc; i++)
	    argv[i] = _free(argv[i]);
	argv = _free(argv);
/*@=dependenttrans =unqualifiedtrans@*/
    }
/*@=branchstate@*/
    return rc;
}

/* =============================================================== */

int
expandMacros(void * spec, MacroContext mc, char * sbuf, size_t slen)
{
    MacroBuf mb = alloca(sizeof(*mb));
    char *tbuf;
    int rc;

    if (sbuf == NULL || slen == 0)
	return 0;
    if (mc == NULL) mc = rpmGlobalMacroContext;

    tbuf = alloca(slen + 1);
    memset(tbuf, 0, (slen + 1));

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
	rpmError(RPMERR_BADSPEC, _("Target buffer overflow\n"));

    tbuf[slen] = '\0';	/* XXX just in case */
    strncpy(sbuf, tbuf, (slen - mb->nb + 1));

    return rc;
}

void
addMacro(MacroContext mc,
	const char * n, const char * o, const char * b, int level)
{
    MacroEntry * mep;

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
delMacro(MacroContext mc, const char * n)
{
    MacroEntry * mep;

    if (mc == NULL) mc = rpmGlobalMacroContext;
    /* If name exists, pop entry */
    if ((mep = findEntry(mc, n, 0)) != NULL) {
	popMacro(mep);
	/* If deleted name, sort macro table */
	if (!(mep && *mep))
	    sortMacroTable(mc);
    }
}

/*@-mustmod@*/ /* LCL: mc is modified through mb->mc, mb is abstract */
int
rpmDefineMacro(MacroContext mc, const char * macro, int level)
{
    MacroBuf mb = alloca(sizeof(*mb));

    memset(mb, 0, sizeof(*mb));
    /* XXX just enough to get by */
    mb->mc = (mc ? mc : rpmGlobalMacroContext);
    (void) doDefine(mb, macro, level, 0);
    return 0;
}
/*@=mustmod@*/

void
rpmLoadMacros(MacroContext mc, int level)
{

    if (mc == NULL || mc == rpmGlobalMacroContext)
	return;

    if (mc->macroTable != NULL) {
	int i;
	for (i = 0; i < mc->firstFree; i++) {
	    MacroEntry *mep, me;
	    mep = &mc->macroTable[i];
	    me = *mep;

	    if (me == NULL)		/* XXX this should never happen */
		continue;
	    addMacro(NULL, me->name, me->opts, me->body, (level - 1));
	}
    }
}

int
rpmLoadMacroFile(MacroContext mc, const char * fn)
{
    FD_t fd = Fopen(fn, "r.fpio");
    char buf[BUFSIZ];
    int rc = -1;

    if (fd == NULL || Ferror(fd)) {
	if (fd) (void) Fclose(fd);
	return rc;
    }

    /* XXX Assume new fangled macro expansion */
    /*@-mods@*/
    max_macro_depth = 16;
    /*@=mods@*/

    buf[0] = '\0';
    while(rdcl(buf, sizeof(buf), fd) != NULL) {
	char c, *n;

	n = buf;
	SKIPBLANK(n, c);

	if (c != '%')
		continue;
	n++;	/* skip % */
	rc = rpmDefineMacro(mc, n, RMIL_MACROFILES);
    }
    rc = Fclose(fd);
    return rc;
}

void
rpmInitMacros(MacroContext mc, const char * macrofiles)
{
    char *mfiles, *m, *me;

    if (macrofiles == NULL)
	return;
#ifdef	DYING
    if (mc == NULL) mc = rpmGlobalMacroContext;
#endif

    mfiles = xstrdup(macrofiles);
    for (m = mfiles; m && *m != '\0'; m = me) {
	const char ** av;
	int ac;
	int i;

	for (me = m; (me = strchr(me, ':')) != NULL; me++) {
	    /* Skip over URI's. */
	    if (!(me[1] == '/' && me[2] == '/'))
		/*@innerbreak@*/ break;
	}

	if (me && *me == ':')
	    *me++ = '\0';
	else
	    me = m + strlen(m);

	/* Glob expand the macro file path element, expanding ~ to $HOME. */
	ac = 0;
	av = NULL;
	i = rpmGlob(m, &ac, &av);
        if (i != 0)
	    continue;

	/* Read macros from each file. */
	for (i = 0; i < ac; i++)
	    (void) rpmLoadMacroFile(mc, av[i]);
	av = _free(av);
    }
    mfiles = _free(mfiles);

    /* Reload cmdline macros */
    /*@-mods@*/
    rpmLoadMacros(rpmCLIMacroContext, RMIL_CMDLINE);
    /*@=mods@*/
}

/*@-globstate@*/
void
rpmFreeMacros(MacroContext mc)
{
    
    if (mc == NULL) mc = rpmGlobalMacroContext;

    if (mc->macroTable != NULL) {
	int i;
	for (i = 0; i < mc->firstFree; i++) {
	    MacroEntry me;
	    while ((me = mc->macroTable[i]) != NULL) {
		/* XXX cast to workaround const */
		/*@-onlytrans@*/
		if ((mc->macroTable[i] = me->prev) == NULL)
		    me->name = _free(me->name);
		/*@=onlytrans@*/
		me->opts = _free(me->opts);
		me->body = _free(me->body);
		me = _free(me);
	    }
	}
	mc->macroTable = _free(mc->macroTable);
    }
    memset(mc, 0, sizeof(*mc));
}
/*@=globstate@*/

/* =============================================================== */
int isCompressed(const char * file, rpmCompressedMagic * compressed)
{
    FD_t fd;
    ssize_t nb;
    int rc = -1;
    unsigned char magic[4];

    *compressed = COMPRESSED_NOT;

    fd = Fopen(file, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	/* XXX Fstrerror */
	rpmError(RPMERR_BADSPEC, _("File %s: %s\n"), file, Fstrerror(fd));
	if (fd) (void) Fclose(fd);
	return 1;
    }
    nb = Fread(magic, sizeof(magic[0]), sizeof(magic), fd);
    if (nb < 0) {
	rpmError(RPMERR_BADSPEC, _("File %s: %s\n"), file, Fstrerror(fd));
	rc = 1;
    } else if (nb < sizeof(magic)) {
	rpmError(RPMERR_BADSPEC, _("File %s is smaller than %u bytes\n"),
		file, (unsigned)sizeof(magic));
	rc = 0;
    }
    (void) Fclose(fd);
    if (rc >= 0)
	return rc;

    rc = 0;

    if ((magic[0] == 'B') && (magic[1] == 'Z')) {
	*compressed = COMPRESSED_BZIP2;
    } else if ((magic[0] == 0120) && (magic[1] == 0113) &&
	 (magic[2] == 0003) && (magic[3] == 0004)) {	/* pkzip */
	*compressed = COMPRESSED_ZIP;
    } else if (((magic[0] == 0037) && (magic[1] == 0213)) || /* gzip */
	((magic[0] == 0037) && (magic[1] == 0236)) ||	/* old gzip */
	((magic[0] == 0037) && (magic[1] == 0036)) ||	/* pack */
	((magic[0] == 0037) && (magic[1] == 0240)) ||	/* SCO lzh */
	((magic[0] == 0037) && (magic[1] == 0235))	/* compress */
	) {
	*compressed = COMPRESSED_OTHER;
    }

    return rc;
}

/* =============================================================== */

/*@-modfilesys@*/
char * 
rpmExpand(const char *arg, ...)
{
    char buf[BUFSIZ], *p, *pe;
    const char *s;
    va_list ap;

    if (arg == NULL)
	return xstrdup("");

    buf[0] = '\0';
    p = buf;
    pe = stpcpy(p, arg);

    va_start(ap, arg);
    while ((s = va_arg(ap, const char *)) != NULL)
	pe = stpcpy(pe, s);
    va_end(ap);
    (void) expandMacros(NULL, NULL, buf, sizeof(buf));
    return xstrdup(buf);
}
/*@=modfilesys@*/

int
rpmExpandNumeric(const char *arg)
{
    const char *val;
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

/* @todo "../sbin/./../bin/" not correct. */
char *rpmCleanPath(char * path)
{
    const char *s;
    char *se, *t, *te;
    int begin = 1;

    if (path == NULL)
	return NULL;

/*fprintf(stderr, "*** RCP %s ->\n", path); */
    s = t = te = path;
    while (*s != '\0') {
/*fprintf(stderr, "*** got \"%.*s\"\trest \"%s\"\n", (t-path), path, s); */
	switch(*s) {
	case ':':			/* handle url's */
	    if (s[1] == '/' && s[2] == '/') {
		*t++ = *s++;
		*t++ = *s++;
		/*@switchbreak@*/ break;
	    }
	    begin=1;
	    /*@switchbreak@*/ break;
	case '/':
	    /* Move parent dir forward */
	    for (se = te + 1; se < t && *se != '/'; se++)
		{};
	    if (se < t && *se == '/') {
		te = se;
/*fprintf(stderr, "*** next pdir \"%.*s\"\n", (te-path), path); */
	    }
	    while (s[1] == '/')
		s++;
	    while (t > path && t[-1] == '/')
		t--;
	    /*@switchbreak@*/ break;
	case '.':
	    /* Leading .. is special */
 	    /* Check that it is ../, so that we don't interpret */
	    /* ..?(i.e. "...") or ..* (i.e. "..bogus") as "..". */
	    /* in the case of "...", this ends up being processed*/
	    /* as "../.", and the last '.' is stripped.  This   */
	    /* would not be correct processing.                 */
	    if (begin && s[1] == '.' && (s[2] == '/' || s[2] == '\0')) {
/*fprintf(stderr, "    leading \"..\"\n"); */
		*t++ = *s++;
		/*@switchbreak@*/ break;
	    }
	    /* Single . is special */
	    if (begin && s[1] == '\0') {
		/*@switchbreak@*/ break;
	    }
	    /* Trim embedded ./ , trailing /. */
	    if ((t[-1] == '/' && s[1] == '\0') || (t != path && s[1] == '/')) {
		s++;
		continue;
	    }
	    /* Trim embedded /../ and trailing /.. */
	    if (!begin && t > path && t[-1] == '/' && s[1] == '.' && (s[2] == '/' || s[2] == '\0')) {
		t = te;
		/* Move parent dir forward */
		if (te > path)
		    for (--te; te > path && *te != '/'; te--)
			{};
/*fprintf(stderr, "*** prev pdir \"%.*s\"\n", (te-path), path); */
		s++;
		s++;
		continue;
	    }
	    /*@switchbreak@*/ break;
	default:
	    begin = 0;
	    /*@switchbreak@*/ break;
	}
	*t++ = *s++;
    }

    /* Trim trailing / (but leave single / alone) */
    if (t > &path[1] && t[-1] == '/')
	t--;
    *t = '\0';

/*fprintf(stderr, "\t%s\n", path); */
    return path;
}

/* Return concatenated and expanded canonical path. */

const char *
rpmGetPath(const char *path, ...)
{
    char buf[BUFSIZ];
    const char * s;
    char * t, * te;
    va_list ap;

    if (path == NULL)
	return xstrdup("");

    buf[0] = '\0';
    t = buf;
    te = stpcpy(t, path);
    *te = '\0';

    va_start(ap, path);
    while ((s = va_arg(ap, const char *)) != NULL) {
	te = stpcpy(te, s);
	*te = '\0';
    }
    va_end(ap);
/*@-modfilesys@*/
    (void) expandMacros(NULL, NULL, buf, sizeof(buf));
/*@=modfilesys@*/

    (void) rpmCleanPath(buf);
    return xstrdup(buf);	/* XXX xstrdup has side effects. */
}

/* Merge 3 args into path, any or all of which may be a url. */

const char * rpmGenPath(const char * urlroot, const char * urlmdir,
		const char *urlfile)
{
/*@owned@*/ const char * xroot = rpmGetPath(urlroot, NULL);
/*@dependent@*/ const char * root = xroot;
/*@owned@*/ const char * xmdir = rpmGetPath(urlmdir, NULL);
/*@dependent@*/ const char * mdir = xmdir;
/*@owned@*/ const char * xfile = rpmGetPath(urlfile, NULL);
/*@dependent@*/ const char * file = xfile;
    const char * result;
    const char * url = NULL;
    int nurl = 0;
    int ut;

#if 0
if (_debug) fprintf(stderr, "*** RGP xroot %s xmdir %s xfile %s\n", xroot, xmdir, xfile);
#endif
    ut = urlPath(xroot, &root);
    if (url == NULL && ut > URL_IS_DASH) {
	url = xroot;
	nurl = root - xroot;
#if 0
if (_debug) fprintf(stderr, "*** RGP ut %d root %s nurl %d\n", ut, root, nurl);
#endif
    }
    if (root == NULL || *root == '\0') root = "/";

    ut = urlPath(xmdir, &mdir);
    if (url == NULL && ut > URL_IS_DASH) {
	url = xmdir;
	nurl = mdir - xmdir;
#if 0
if (_debug) fprintf(stderr, "*** RGP ut %d mdir %s nurl %d\n", ut, mdir, nurl);
#endif
    }
    if (mdir == NULL || *mdir == '\0') mdir = "/";

    ut = urlPath(xfile, &file);
    if (url == NULL && ut > URL_IS_DASH) {
	url = xfile;
	nurl = file - xfile;
#if 0
if (_debug) fprintf(stderr, "*** RGP ut %d file %s nurl %d\n", ut, file, nurl);
#endif
    }

/*@-branchstate@*/
    if (url && nurl > 0) {
	char *t = strncpy(alloca(nurl+1), url, nurl);
	t[nurl] = '\0';
	url = t;
    } else
	url = "";
/*@=branchstate@*/

    result = rpmGetPath(url, root, "/", mdir, "/", file, NULL);

    xroot = _free(xroot);
    xmdir = _free(xmdir);
    xfile = _free(xfile);
#if 0
if (_debug) fprintf(stderr, "*** RGP result %s\n", result);
#endif
    return result;
}

/* =============================================================== */

#if defined(DEBUG_MACROS)

#if defined(EVAL_MACROS)

char *macrofiles = "/usr/lib/rpm/macros:/etc/rpm/macros:~/.rpmmacros";

int
main(int argc, char *argv[])
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optind;

    while ((c = getopt(argc, argv, "f:")) != EOF ) {
	switch (c) {
	case 'f':
	    macrofiles = optarg;
	    break;
	case '?':
	default:
	    errflg++;
	    break;
	}
    }
    if (errflg || optind >= argc) {
	fprintf(stderr, "Usage: %s [-f macropath ] macro ...\n", argv[0]);
	exit(1);
    }

    rpmInitMacros(NULL, macrofiles);
    for ( ; optind < argc; optind++) {
	const char *val;

	val = rpmGetPath(argv[optind], NULL);
	if (val) {
	    fprintf(stdout, "%s:\t%s\n", argv[optind], val);
	    val = _free(val);
	}
    }
    rpmFreeMacros(NULL);
    return 0;
}

#else	/* !EVAL_MACROS */

char *macrofiles = "../macros:./testmacros";
char *testfile = "./test";

int
main(int argc, char *argv[])
{
    char buf[BUFSIZ];
    FILE *fp;
    int x;

    rpmInitMacros(NULL, macrofiles);
    rpmDumpMacroTable(NULL, NULL);

    if ((fp = fopen(testfile, "r")) != NULL) {
	while(rdcl(buf, sizeof(buf), fp)) {
	    x = expandMacros(NULL, NULL, buf, sizeof(buf));
	    fprintf(stderr, "%d->%s\n", x, buf);
	    memset(buf, 0, sizeof(buf));
	}
	fclose(fp);
    }

    while(rdcl(buf, sizeof(buf), stdin)) {
	x = expandMacros(NULL, NULL, buf, sizeof(buf));
	fprintf(stderr, "%d->%s\n <-\n", x, buf);
	memset(buf, 0, sizeof(buf));
    }
    rpmFreeMacros(NULL);

    return 0;
}
#endif	/* EVAL_MACROS */
#endif	/* DEBUG_MACROS */
/*@=boundsread@*/
