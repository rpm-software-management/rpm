/** \ingroup rpmdep
 * \file lib/rpmsx.c
 */
#include "system.h"

#include <rpmlib.h>

#define	_RPMSX_INTERNAL
#include "rpmsx.h"

#include "debug.h"

/*@unchecked@*/
int _rpmsx_debug = 0;

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
	regfree(sxp->preg);
	sxp->preg = _free(sxp->preg);
    }
    /*@=branchstate@*/

    sx->sxp = _free(sx->sxp);

    (void) rpmsxUnlink(sx, __func__);
    /*@-refcounttrans -usereleased@*/
/*@-bounsxwrite@*/
    memset(sx, 0, sizeof(*sx));		/* XXX trash and burn */
/*@=bounsxwrite@*/
    sx = _free(sx);
    /*@=refcounttrans =usereleased@*/
    return NULL;
}

static int rpmsxpCompare(const void* A, const void* B)
{
    rpmsxp sxpA = (rpmsxp) A;
    rpmsxp sxpB = (rpmsxp) B;
    return (sxpA->hasMetaChars - sxpB->hasMetaChars);
}

/* Determine if the regular expression specification has any meta characters. */
static void rpmsxpHasMetaChars(rpmsxp sxp)
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
	    break;
	case '\\':		/* skip the next character */
	    s++;
	    break;
	default:
	    break;

	}
	s++;
    }
    return;
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
{
    int i, j;
    int rc = 0;

    for (i = 0; i < sx->Count; i++) {
	rpmsxp sxpi = sx->sxp + i;
	for (j = i + 1; j < sx->Count; j++) { 
	    rpmsxp sxpj = sx->sxp + j;

	    /* Check if same RE string */
	    if (strcmp(sxpj->pattern, sxpi->pattern))
		continue;
	    if (sxpj->mode && sxpi->mode && sxpj->mode != sxpi->mode)
		continue;

	    /* Same RE string found */
	    if (strcmp(sxpj->context, sxpi->context)) {
		/* If different contexts, give warning */
		fprintf(stderr,
		"ERROR: Multiple different specifications for %s  (%s and %s).\n",
			sxpi->pattern, sxpj->context, sxpi->context);
		rc = -1;
	    } else {
		/* If same contexts give warning */
		fprintf(stderr,
		"WARNING: Multiple same specifications for %s.\n",
			sxpi->pattern);
	    }
	}
    }
    return rc;
}

static int nerr;
#define	inc_err()	nerr++

int rpmsxParse(rpmsx sx, const char *fn)
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

    if (fn == NULL)
	fn = "/etc/security/selinux/src/policy/file_contexts/file_contexts";

    if ((fp = fopen(fn, "r")) == NULL) {
	perror(fn);
	return -1;
    }

    nerr = 0;

    /* 
     * Perform two passes over the specification file.
     * The first pass counts the number of specifications and
     * performs simple validation of the input.  At the end
     * of the first pass, the spec array is allocated.
     * The second pass performs detailed validation of the input
     * and fills in the spec array.
     */
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
		continue;
	    }
	    buf[len - 1] = 0;
	    bp = buf;
	    while (isspace(*bp))
		bp++;
	    /* Skip comment lines and empty lines. */
	    if (*bp == '#' || *bp == 0)
		continue;
	    items = sscanf(buf, "%as %as %as", &regex, &type, &context);
	    if (items < 2) {
		fprintf(stderr,
			_("%s:  line number %d is missing fields (only read %s)\n"),
			fn, lineno, buf);
		inc_err();
		if (items == 1)
		    free(regex);
		continue;
	    } else if (items == 2) {
		/* The type field is optional. */
		free(context);
		context = type;
		type = 0;
	    }

	    if (pass == 1) {
		/* On the second pass, compile and store the specification in spec. */
		const char *reg_buf = regex;
#ifdef	NOTYET
		sxp->stem_id = find_stem_from_spec(&reg_buf);
#else
		sxp->stem_id = -1;
#endif
		sxp->pattern = regex;

		/* Anchor the regular expression. */
		len = strlen(reg_buf);
		anchored_regex = xmalloc(len + 3);
		sprintf(anchored_regex, "^%s$", reg_buf);

		/* Compile the regular expression. */
		sxp->preg = xcalloc(1, sizeof(*sxp->preg));
		regerr = regcomp(sxp->preg, anchored_regex,
			    REG_EXTENDED | REG_NOSUB);
		if (regerr < 0) {
		    regerror(regerr, sxp->preg, errbuf, sizeof errbuf);
		    fprintf(stderr,
			_("%s:  unable to compile regular expression %s on line number %d:  %s\n"),
			fn, regex, lineno,
			errbuf);
		    inc_err();
		}
		free(anchored_regex);

		/* Convert the type string to a mode format */
		sxp->type = type;
		sxp->mode = 0;
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
		case 'b':
		    sxp->mode = S_IFBLK;
		    break;
		case 'c':
		    sxp->mode = S_IFCHR;
		    break;
		case 'd':
		    sxp->mode = S_IFDIR;
		    break;
		case 'p':
		    sxp->mode = S_IFIFO;
		    break;
		case 'l':
		    sxp->mode = S_IFLNK;
		    break;
		case 's':
		    sxp->mode = S_IFSOCK;
		    break;
		case '-':
		    sxp->mode = S_IFREG;
		    break;
		default:
		    fprintf(stderr,
			_("%s:  invalid type specifier %s on line number %d\n"),
			fn, type, lineno);
		    inc_err();
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
		free(regex);
		if (type)
		    free(type);
		free(context);
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
    fclose(fp);

    /* Sort the specifications with most general first */
    qsort(sx->sxp, sx->Count, sizeof(*sx->sxp), rpmsxpCompare);

    /* Verify no exact duplicates */
    if (rpmsxpCheckNoDupes(sx) != 0)
	return -1;

    return 0;
}

rpmsx rpmsxNew(const char * fn)
{
    rpmsx sx;

    sx = xcalloc(1, sizeof(*sx));
    sx->Count = 0;
    sx->i = -1;
    sx->sxp = NULL;

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

const char * rpmsxApply(rpmsx sx, const char * fn)
{
   const char * context = NULL;

    sx = rpmsxInit(sx, 1);
    if (sx != NULL)
    while (rpmsxNext(sx) >= 0) {
    }

    return context;
}
