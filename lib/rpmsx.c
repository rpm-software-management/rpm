/** \ingroup rpmdep
 * \file lib/rpmsx.c
 */
#include "system.h"

#include <rpmlib.h>

#define	_RPMSX_INTERNAL
#include "rpmsx.h"

#include "debug.h"

/*@access regex_t @*/

/*@unchecked@*/
int _rpmsx_debug = 0;

/**
 */
static int rpmsxpCompare(const void* A, const void* B)
	/*@*/
{
    rpmsxp sxpA = (rpmsxp) A;
    rpmsxp sxpB = (rpmsxp) B;
    return (sxpB->hasMetaChars - sxpA->hasMetaChars);
}

/**
 * Sort the specifications with most general first.
 * @param sx		security context patterns
 */
static void rpmsxSort(rpmsx sx)
	/*@modifies sx @*/
{
    qsort(sx->sxp, sx->Count, sizeof(*sx->sxp), rpmsxpCompare);
}

/* Determine if the regular expression specification has any meta characters. */
static void rpmsxpHasMetaChars(rpmsxp sxp)
	/*@modifies sxp @*/
{
    const char * s = sxp->pattern;
    size_t ns = strlen(s);
    const char * se = s + ns;

    sxp->hasMetaChars = 0; 

    /* Look at each character in the RE specification string for a 
     * meta character. Return when any meta character reached. */
    while (s != se) {
	switch(*s) {
	case '.':
	case '^':
	case '$':
	case '?':
	case '*':
	case '+':
	case '|':
	case '[':
	case '(':
	case '{':
	    sxp->hasMetaChars = 1;
	    return;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case '\\':		/* skip the next character */
	    s++;
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;

	}
	s++;
    }
    return;
}

/**
 * Return the length of the text that can be considered the stem.
 * @return		stem length, 0 if no identifiable stem
 */
static size_t rpmsxsPStem(const char * const buf)
	/*@*/
{
    /*@observer@*/
    static const char * const regex_chars = ".^$?*+|[({";
    const char * tmp = strchr(buf, '/');
    const char * ind;

    if (!tmp)
	return 0;

    for (ind = buf; ind < tmp; ind++) {
	if (strchr(regex_chars, (int)*ind))
	    return 0;
    }
    return tmp - buf;
}

/**
 * Return the length of the text that is the stem of a file name.
 * @return		stem length, 0 if no identifiable stem
 */
static size_t rpmsxsFStem(const char * const buf)
	/*@*/
{
    const char * tmp = strchr(buf + 1, '/');

    if (!tmp)
	return 0;
    return tmp - buf;
}

/**
 * Find (or create) the stem of a file spec.
 * Error iff a file in the root directory or a regex that is too complex.
 *
 * @retval *bpp		ptr to text after stem.
 * @return		stem index, -1 on error
 */
static int rpmsxAdd(rpmsx sx, const char ** bpp)
	/*@modifies sx, *bpp @*/
{
    size_t stem_len = rpmsxsPStem(*bpp);
    rpmsxs sxs;
    int i;

    if (!stem_len)
	return -1;
    for (i = 0; i < sx->nsxs; i++) {
	sxs = sx->sxs + i;
	if (stem_len != sxs->len)
	    continue;
	if (strncmp(*bpp, sxs->stem, stem_len))
	    continue;
	*bpp += stem_len;
	return i;
    }

    if (sx->nsxs == sx->maxsxs) {
	sx->maxsxs = sx->maxsxs * 2 + 16;
	sx->sxs = xrealloc(sx->sxs, sizeof(*sx->sxs) * sx->maxsxs);
    }
    sxs = sx->sxs + sx->nsxs;
    sxs->len = stem_len;
/*@i@*/    sxs->stem = strndup(*bpp, stem_len);
    sx->nsxs++;
    *bpp += stem_len;
    return sx->nsxs - 1;
}

/**
 * Find the stem of a file name.
 * Error iff a file in the root directory or a regex that is too complex.
 *
 * @retval *bpp		ptr to text after stem.
 * @return		stem index, -1 on error
 */
static int rpmsxFind(const rpmsx sx, const char ** bpp)
	/*@modifies *bpp @*/
{
    size_t stem_len = rpmsxsFStem(*bpp);
    rpmsxs sxs;
    int i;

    if (stem_len)
    for (i = 0; i < sx->nsxs; i++) {
	sxs = sx->sxs + i;
	if (stem_len != sxs->len)
	    continue;
/*@i@*/	if (strncmp(*bpp, sxs->stem, stem_len))
	    continue;
	*bpp += stem_len;
	return i;
    }
    return -1;
}

rpmsx XrpmsxUnlink(rpmsx sx, const char * msg, const char * fn, unsigned ln)
{
    if (sx == NULL) return NULL;
/*@-modfilesys@*/
if (_rpmsx_debug && msg != NULL)
fprintf(stderr, "--> sx %p -- %d %s at %s:%u\n", sx, sx->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    sx->nrefs--;
    return NULL;
}

rpmsx XrpmsxLink(rpmsx sx, const char * msg, const char * fn, unsigned ln)
{
    if (sx == NULL) return NULL;
    sx->nrefs++;

/*@-modfilesys@*/
if (_rpmsx_debug && msg != NULL)
fprintf(stderr, "--> sx %p ++ %d %s at %s:%u\n", sx, sx->nrefs, msg, fn, ln);
/*@=modfilesys@*/

    /*@-refcounttrans@*/ return sx; /*@=refcounttrans@*/
}

rpmsx rpmsxFree(rpmsx sx)
{
    int i;

    if (sx == NULL)
	return NULL;

    if (sx->nrefs > 1)
	return rpmsxUnlink(sx, __func__);

/*@-modfilesys@*/
if (_rpmsx_debug < 0)
fprintf(stderr, "*** sx %p\t%s[%d]\n", sx, __func__, sx->Count);
/*@=modfilesys@*/

    /*@-branchstate@*/
    if (sx->Count > 0)
    for (i = 0; i < sx->Count; i++) {
	rpmsxp sxp = sx->sxp + i;
	sxp->pattern = _free(sxp->pattern);
	sxp->type = _free(sxp->type);
	sxp->context = _free(sxp->context);
/*@i@*/	regfree(sxp->preg);
/*@i@*/	sxp->preg = _free(sxp->preg);
    }
    sx->sxp = _free(sx->sxp);

    if (sx->nsxs > 0)
    for (i = 0; i < sx->nsxs; i++) {
	rpmsxs sxs = sx->sxs + i;
	sxs->stem = _free(sxs->stem);
    }
    sx->sxs = _free(sx->sxs);
    /*@=branchstate@*/

    (void) rpmsxUnlink(sx, __func__);
    /*@-refcounttrans -usereleased@*/
/*@-boundswrite@*/
    memset(sx, 0, sizeof(*sx));		/* XXX trash and burn */
/*@=boundswrite@*/
    sx = _free(sx);
    /*@=refcounttrans =usereleased@*/
    return NULL;
}

/**
 * Check for duplicate specifications. If a duplicate specification is found 
 * and the context is the same, give a warning to the user. If a duplicate 
 * specification is found and the context is different, give a warning
 * to the user (This could be changed to error). Return of non-zero is an error.
 *
 * @param sx		security context patterns
 * @return		0 on success
 */
static int rpmsxpCheckNoDupes(const rpmsx sx)
	/*@*/
{
    int i, j;
    int rc = 0;

    for (i = 0; i < sx->Count; i++) {
	rpmsxp sxpi = sx->sxp + i;
	for (j = i + 1; j < sx->Count; j++) { 
	    rpmsxp sxpj = sx->sxp + j;

	    /* Check if same RE string */
	    if (strcmp(sxpj->pattern, sxpi->pattern))
		/*@innercontinue@*/ continue;
	    if (sxpj->fmode && sxpi->fmode && sxpj->fmode != sxpi->fmode)
		/*@innercontinue@*/ continue;

	    /* Same RE string found */
	    if (strcmp(sxpj->context, sxpi->context)) {
		/* If different contexts, give warning */
/*@-modfilesys@*/
		fprintf(stderr,
		"ERROR: Multiple different specifications for %s  (%s and %s).\n",
			sxpi->pattern, sxpj->context, sxpi->context);
/*@=modfilesys@*/
		rc = -1;
	    } else {
		/* If same contexts give warning */
/*@-modfilesys@*/
		fprintf(stderr,
		"WARNING: Multiple same specifications for %s.\n",
			sxpi->pattern);
/*@=modfilesys@*/
	    }
	}
    }
    return rc;
}

int rpmsxParse(rpmsx sx, const char * fn)
{
    FILE * fp;
    char errbuf[255 + 1];
    char buf[255 + 1];
    char * bp;
    char * regex;
    char * type;
    char * context;
    char * anchored_regex;
    int items;
    int len;
    int lineno;
    int pass;
    int regerr;
    int nerr = 0;
#define	inc_err()	nerr++

/*@-branchstate@*/
    if (fn == NULL)
	fn = "/etc/security/selinux/src/policy/file_contexts/file_contexts";
/*@=branchstate@*/

    if ((fp = fopen(fn, "r")) == NULL) {
	perror(fn);
	return -1;
    }

    /* 
     * Perform two passes over the specification file.
     * The first pass counts the number of specifications and
     * performs simple validation of the input.  At the end
     * of the first pass, the spec array is allocated.
     * The second pass performs detailed validation of the input
     * and fills in the spec array.
     */
/*@-branchstate@*/
    for (pass = 0; pass < 2; pass++) {
	rpmsxp sxp;

	lineno = 0;
	sx->Count = 0;
	sxp = sx->sxp;
	while (fgets(buf, sizeof buf, fp)) {
	    lineno++;
	    len = strlen(buf);
	    if (buf[len - 1] != '\n') {
		fprintf(stderr,
			_("%s:  no newline on line number %d (only read %s)\n"),
			fn, lineno, buf);
		inc_err();
		/*@innercontinue@*/ continue;
	    }
	    buf[len - 1] = 0;
	    bp = buf;
	    while (isspace(*bp))
		bp++;
	    /* Skip comment lines and empty lines. */
	    if (*bp == '#' || *bp == 0)
		/*@innercontinue@*/ continue;
/*@-formatcode@*/
	    items = sscanf(buf, "%as %as %as", &regex, &type, &context);
/*@=formatcode@*/
	    if (items < 2) {
		fprintf(stderr,
			_("%s:  line number %d is missing fields (only read %s)\n"),
			fn, lineno, buf);
		inc_err();
		if (items == 1)
		    free(regex);
		/*@innercontinue@*/ continue;
	    } else if (items == 2) {
		/* The type field is optional. */
		free(context);
		context = type;
		type = 0;
	    }

	    /* On pass 2, compile and store the specification. */
	    if (pass == 1) {
		const char * reg_buf = regex;
		sxp->fstem = rpmsxAdd(sx, &reg_buf);
		sxp->pattern = regex;

		/* Anchor the regular expression. */
		len = strlen(reg_buf);
		anchored_regex = xmalloc(len + 3);
		sprintf(anchored_regex, "^%s$", reg_buf);

		/* Compile the regular expression. */
/*@i@*/		sxp->preg = xcalloc(1, sizeof(*sxp->preg));
		regerr = regcomp(sxp->preg, anchored_regex,
			    REG_EXTENDED | REG_NOSUB);
		if (regerr < 0) {
		    (void) regerror(regerr, sxp->preg, errbuf, sizeof errbuf);
		    fprintf(stderr,
			_("%s:  unable to compile regular expression %s on line number %d:  %s\n"),
			fn, regex, lineno,
			errbuf);
		    inc_err();
		}
		free(anchored_regex);

		/* Convert the type string to a mode format */
		sxp->type = type;
		sxp->fmode = 0;
		if (!type)
		    goto skip_type;
		len = strlen(type);
		if (type[0] != '-' || len != 2) {
		    fprintf(stderr,
			_("%s:  invalid type specifier %s on line number %d\n"),
			fn, type, lineno);
		    inc_err();
		    goto skip_type;
		}
		switch (type[1]) {
		case 'b':	sxp->fmode = S_IFBLK;	/*@switchbreak@*/ break;
		case 'c':	sxp->fmode = S_IFCHR;	/*@switchbreak@*/ break;
		case 'd':	sxp->fmode = S_IFDIR;	/*@switchbreak@*/ break;
		case 'p':	sxp->fmode = S_IFIFO;	/*@switchbreak@*/ break;
		case 'l':	sxp->fmode = S_IFLNK;	/*@switchbreak@*/ break;
/*@i@*/		case 's':	sxp->fmode = S_IFSOCK;	/*@switchbreak@*/ break;
		case '-':	sxp->fmode = S_IFREG;	/*@switchbreak@*/ break;
		default:
		    fprintf(stderr,
			_("%s:  invalid type specifier %s on line number %d\n"),
			fn, type, lineno);
		    inc_err();
		    /*@switchbreak@*/ break;
		}

	      skip_type:

		sxp->context = context;

		if (strcmp(context, "<<none>>")) {
		    if (security_check_context(context) < 0 && errno != ENOENT) {
			fprintf(stderr,
				_("%s:  invalid context %s on line number %d\n"),
				fn, context, lineno);
			inc_err();
		    }
		}

		/* Determine if specification has 
		 * any meta characters in the RE */
		rpmsxpHasMetaChars(sxp);
		sxp++;
	    }

	    sx->Count++;
	    if (pass == 0) {
/*@-kepttrans@*/
		free(regex);
		if (type)
		    free(type);
		free(context);
/*@=kepttrans@*/
	    }
	}

	if (nerr)
	    return -1;

	if (pass == 0) {
	    if (sx->Count == 0)
		return 0;
	    sx->sxp = xcalloc(sx->Count, sizeof(*sx->sxp));
	    rewind(fp);
	}
    }
/*@=branchstate@*/
    (void) fclose(fp);

    /* Sort the specifications with most general first */
    rpmsxSort(sx);

    /* Verify no exact duplicates */
    if (rpmsxpCheckNoDupes(sx) != 0)
	return -1;

    return 0;
}

rpmsx rpmsxNew(const char * fn)
{
    rpmsx sx;

    sx = xcalloc(1, sizeof(*sx));
    sx->sxp = NULL;
    sx->Count = 0;
    sx->i = -1;
    sx->sxs = NULL;
    sx->nsxs = 0;
    sx->maxsxs = 0;
    sx->reverse = 0;

    (void) rpmsxLink(sx, __func__);

    if (rpmsxParse(sx, fn) != 0)
	return rpmsxFree(sx);

    return sx;
}

int rpmsxCount(const rpmsx sx)
{
    return (sx != NULL ? sx->Count : 0);
}

int rpmsxIx(const rpmsx sx)
{
    return (sx != NULL ? sx->i : -1);
}

int rpmsxSetIx(rpmsx sx, int ix)
{
    int i = -1;

    if (sx != NULL) {
	i = sx->i;
	sx->i = ix;
    }
    return i;
}

const char * rpmsxPattern(const rpmsx sx)
{
    const char * pattern = NULL;

    if (sx != NULL && sx->i >= 0 && sx->i < sx->Count)
	pattern = (sx->sxp + sx->i)->pattern;
    return pattern;
}

const char * rpmsxType(const rpmsx sx)
{
    const char * type = NULL;

    if (sx != NULL && sx->i >= 0 && sx->i < sx->Count)
	type = (sx->sxp + sx->i)->type;
    return type;
}

const char * rpmsxContext(const rpmsx sx)
{
    const char * context = NULL;

    if (sx != NULL && sx->i >= 0 && sx->i < sx->Count)
	context = (sx->sxp + sx->i)->context;
    return context;
}

regex_t * rpmsxRE(const rpmsx sx)
{
    regex_t * preg = NULL;

    if (sx != NULL && sx->i >= 0 && sx->i < sx->Count)
	preg = (sx->sxp + sx->i)->preg;
    return preg;
}

mode_t rpmsxFMode(const rpmsx sx)
{
    mode_t fmode = 0;

    if (sx != NULL && sx->i >= 0 && sx->i < sx->Count)
	fmode = (sx->sxp + sx->i)->fmode;
    return fmode;
}

int rpmsxFStem(const rpmsx sx)
{
    int fstem = -1;

    if (sx != NULL && sx->i >= 0 && sx->i < sx->Count)
	fstem = (sx->sxp + sx->i)->fstem;
    return fstem;
}

int rpmsxNext(/*@null@*/ rpmsx sx)
	/*@modifies sx @*/
{
    int i = -1;

    if (sx != NULL) {
	if (sx->reverse != 0) {
	    i = --sx->i;
	    if (sx->i < 0) {
		sx->i = sx->Count;
		i = -1;
	    }
	} else {
	    i = ++sx->i;
	    if (sx->i >= sx->Count) {
		sx->i = -1;
		i = -1;
	    }
	}

/*@-modfilesys @*/
if (_rpmsx_debug  < 0 && i != -1) {
rpmsxp sxp = sx->sxp + i;
fprintf(stderr, "*** sx %p\t%s[%d]\t%s\t%s\n", sx, __func__, i, sxp->pattern, sxp->context);
/*@=modfilesys @*/
}

    }

    return i;
}

rpmsx rpmsxInit(/*@null@*/ rpmsx sx, int reverse)
	/*@modifies sx @*/
{
    if (sx != NULL) {
	sx->reverse = reverse;
	sx->i = (sx->reverse ? sx->Count : -1);
    }
    /*@-refcounttrans@*/
    return sx;
    /*@=refcounttrans@*/
}

const char * rpmsxFContext(rpmsx sx, const char * fn, mode_t fmode)
{
    const char * fcontext = NULL;
    const char * myfn = fn;
/*@-mods@*/
    int fstem = rpmsxFind(sx, &myfn);
/*@=mods@*/
    int i;

    sx = rpmsxInit(sx, 1);
    if (sx != NULL)
    while ((i = rpmsxNext(sx)) >= 0) {
	regex_t * preg;
	mode_t sxfmode;
	int sxfstem;
	int ret;

	sxfstem = rpmsxFStem(sx);
	if (sxfstem != -1 && sxfstem != fstem)
	    continue;

	sxfmode = rpmsxFMode(sx);
	if (sxfmode && (fmode & S_IFMT) != sxfmode)
	    continue;

	preg = rpmsxRE(sx);
	if (preg == NULL)
	    continue;

	ret = regexec(preg, (sxfstem == -1 ? fn : myfn), 0, NULL, 0);
	switch (ret) {
	case REG_NOMATCH:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case 0:
	    fcontext = rpmsxContext(sx);
	    /*@switchbreak@*/ break;
	default:
	  { static char errbuf[255 + 1];
	    (void) regerror(ret, preg, errbuf, sizeof errbuf);
/*@-modfilesys -nullpass @*/
	    fprintf(stderr, "unable to match %s against %s:  %s\n",
                fn, rpmsxPattern(sx), errbuf);
/*@=modfilesys =nullpass @*/
	  } /*@switchbreak@*/ break;
	}
	break;
    }

    return fcontext;
}
