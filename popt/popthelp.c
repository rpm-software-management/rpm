/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/** \ingroup popt
 * \file popt/popthelp.c
 */

/* (C) 1998-2000 Red Hat, Inc. -- Licensing details are in the COPYING
   file accompanying popt source distributions, available from 
   ftp://ftp.rpm.org/pub/rpm/dist. */

#include "system.h"
#include "poptint.h"

static void displayArgs(poptContext con,
		/*@unused@*/ enum poptCallbackReason foo,
		struct poptOption * key, 
		/*@unused@*/ const char * arg, /*@unused@*/ void * data)
{
    if (key->shortName == '?')
	poptPrintHelp(con, stdout, 0);
    else
	poptPrintUsage(con, stdout, 0);
    exit(0);
}

/*@-castfcnptr@*/
struct poptOption poptHelpOptions[] = {
    { NULL, '\0', POPT_ARG_CALLBACK, (void *)&displayArgs, '\0', NULL, NULL },
    { "help", '?', 0, NULL, '?', N_("Show this help message"), NULL },
    { "usage", '\0', 0, NULL, 'u', N_("Display brief usage message"), NULL },
    POPT_TABLEEND
} ;
/*@=castfcnptr@*/

/*@observer@*/ /*@null@*/ static const char *const
getTableTranslationDomain(/*@null@*/ const struct poptOption *table)
	/*@*/
{
    const struct poptOption *opt;

    if (table != NULL)
    for (opt = table; opt->longName || opt->shortName || opt->arg; opt++) {
	if (opt->argInfo == POPT_ARG_INTL_DOMAIN)
	    return opt->arg;
    }
    return NULL;
}

/*@observer@*/ /*@null@*/ static const char *const
getArgDescrip(const struct poptOption * opt,
		/*@-paramuse@*/		/* FIX: wazzup? */
		/*@null@*/ const char * translation_domain)
		/*@=paramuse@*/
	/*@*/
{
    if (!(opt->argInfo & POPT_ARG_MASK)) return NULL;

    if (opt == (poptHelpOptions + 1) || opt == (poptHelpOptions + 2))
	if (opt->argDescrip) return POPT_(opt->argDescrip);

    if (opt->argDescrip) return D_(translation_domain, opt->argDescrip);

    switch (opt->argInfo & POPT_ARG_MASK) {
    case POPT_ARG_NONE:		return POPT_("NONE");
    case POPT_ARG_VAL:		return POPT_("VAL");
    case POPT_ARG_INT:		return POPT_("INT");
    case POPT_ARG_LONG:		return POPT_("LONG");
    case POPT_ARG_STRING:	return POPT_("STRING");
    case POPT_ARG_FLOAT:	return POPT_("FLOAT");
    case POPT_ARG_DOUBLE:	return POPT_("DOUBLE");
    default:			return POPT_("ARG");
    }
}

static void singleOptionHelp(FILE * fp, int maxLeftCol, 
		const struct poptOption * opt,
		/*@null@*/ const char *translation_domain)
	/*@modifies *fp, fileSystem @*/
{
    int indentLength = maxLeftCol + 5;
    int lineLength = 79 - indentLength;
    const char * help = D_(translation_domain, opt->descrip);
    const char * argDescrip = getArgDescrip(opt, translation_domain);
    int helpLength;
    char * left;
    int nb = maxLeftCol + 1;

    /* Make sure there's more than enough room in target buffer. */
    if (opt->longName)	nb += strlen(opt->longName);
    if (argDescrip)	nb += strlen(argDescrip);

    left = malloc(nb);
    if (left == NULL) return;	/* XXX can't happen */
    left[0] = '\0';
    left[maxLeftCol] = '\0';

    if (opt->longName && opt->shortName)
	sprintf(left, "-%c, %s%s", opt->shortName,
		((opt->argInfo & POPT_ARGFLAG_ONEDASH) ? "-" : "--"),
		opt->longName);
    else if (opt->shortName != '\0') 
	sprintf(left, "-%c", opt->shortName);
    else if (opt->longName)
	sprintf(left, "%s%s",
		((opt->argInfo & POPT_ARGFLAG_ONEDASH) ? "-" : "--"),
		opt->longName);
    if (!*left) goto out;
    if (argDescrip) {
	char * le = left + strlen(left);
	if (opt->argInfo & POPT_ARGFLAG_OPTIONAL)
	    *le++ = '[';
	if (opt->argDescrip == NULL) {
	    switch (opt->argInfo & POPT_ARG_MASK) {
	    case POPT_ARG_NONE:
		sprintf(le, "[true]");
		break;
	    case POPT_ARG_VAL:
	    {   long aLong = opt->val;

		if (opt->argInfo & POPT_ARGFLAG_NOT) aLong = ~aLong;
		switch (opt->argInfo & POPT_ARGFLAG_LOGICALOPS) {
		case POPT_ARGFLAG_OR:
		    sprintf(le, "[|=0x%lx]", (unsigned long)aLong);	break;
		case POPT_ARGFLAG_AND:
		    sprintf(le, "[&=0x%lx]", (unsigned long)aLong);	break;
		case POPT_ARGFLAG_XOR:
		    sprintf(le, "[^=0x%lx]", (unsigned long)aLong);	break;
		default:
		    if (!(aLong == 0L || aLong == 1L || aLong == -1L))
			sprintf(le, "[=%ld]", aLong);
		    break;
		}
	    }	break;
	    case POPT_ARG_INT:
	    case POPT_ARG_LONG:
	    case POPT_ARG_STRING:
	    case POPT_ARG_FLOAT:
	    case POPT_ARG_DOUBLE:
		sprintf(le, "=%s", argDescrip);
		break;
	    }
	} else {
	    sprintf(le, "=%s", argDescrip);
	}
	le += strlen(le);
	if (opt->argInfo & POPT_ARGFLAG_OPTIONAL)
	    *le++ = ']';
	*le = '\0';
    }

    if (help)
	fprintf(fp,"  %-*s   ", maxLeftCol, left);
    else {
	fprintf(fp,"  %s\n", left); 
	goto out;
    }

    helpLength = strlen(help);
    while (helpLength > lineLength) {
	const char * ch;
	char format[10];

	ch = help + lineLength - 1;
	while (ch > help && !isspace(*ch)) ch--;
	if (ch == help) break;		/* give up */
	while (ch > (help + 1) && isspace(*ch)) ch--;
	ch++;

	sprintf(format, "%%.%ds\n%%%ds", (int) (ch - help), indentLength);
	fprintf(fp, format, help, " ");
	help = ch;
	while (isspace(*help) && *help) help++;
	helpLength = strlen(help);
    }

    if (helpLength) fprintf(fp, "%s\n", help);

out:
    left = _free(left);
}

static int maxArgWidth(const struct poptOption * opt,
		       /*@null@*/ const char * translation_domain)
	/*@*/
{
    int max = 0;
    int len = 0;
    const char * s;
    
    if (opt != NULL)
    while (opt->longName || opt->shortName || opt->arg) {
	if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE) {
	    if (opt->arg)	/* XXX program error */
	    len = maxArgWidth(opt->arg, translation_domain);
	    if (len > max) max = len;
	} else if (!(opt->argInfo & POPT_ARGFLAG_DOC_HIDDEN)) {
	    len = sizeof("  ")-1;
	    if (opt->shortName != '\0') len += sizeof("-X")-1;
	    if (opt->shortName != '\0' && opt->longName) len += sizeof(", ")-1;
	    if (opt->longName) {
		len += ((opt->argInfo & POPT_ARGFLAG_ONEDASH)
			? sizeof("-")-1 : sizeof("--")-1);
		len += strlen(opt->longName);
	    }

	    s = getArgDescrip(opt, translation_domain);
	    if (s)
		len += sizeof("=")-1 + strlen(s);
	    if (opt->argInfo & POPT_ARGFLAG_OPTIONAL) len += sizeof("[]")-1;
	    if (len > max) max = len;
	}

	opt++;
    }
    
    return max;
}

static void singleTableHelp(FILE * fp,
		/*@null@*/ const struct poptOption * table, int left,
		/*@null@*/ const char * translation_domain)
	/*@modifies *fp, fileSystem @*/
{
    const struct poptOption * opt;
    const char *sub_transdom;

    if (table != NULL)
    for (opt = table; (opt->longName || opt->shortName || opt->arg); opt++) {
	if ((opt->longName || opt->shortName) && 
	    !(opt->argInfo & POPT_ARGFLAG_DOC_HIDDEN))
	    singleOptionHelp(fp, left, opt, translation_domain);
    }

    if (table != NULL)
    for (opt = table; (opt->longName || opt->shortName || opt->arg); opt++) {
	if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE) {
	    sub_transdom = getTableTranslationDomain(opt->arg);
	    if (sub_transdom == NULL)
		sub_transdom = translation_domain;
	    
	    if (opt->descrip)
		fprintf(fp, "\n%s\n", D_(sub_transdom, opt->descrip));

	    singleTableHelp(fp, opt->arg, left, sub_transdom);
	}
    }
}

static int showHelpIntro(poptContext con, FILE * fp)
	/*@modifies *fp, fileSystem @*/
{
    int len = 6;
    const char * fn;

    fprintf(fp, POPT_("Usage:"));
    if (!(con->flags & POPT_CONTEXT_KEEP_FIRST)) {
	/*@-nullderef@*/	/* LCL: wazzup? */
	fn = con->optionStack->argv[0];
	/*@=nullderef@*/
	if (fn == NULL) return len;
	if (strchr(fn, '/')) fn = strrchr(fn, '/') + 1;
	fprintf(fp, " %s", fn);
	len += strlen(fn) + 1;
    }

    return len;
}

void poptPrintHelp(poptContext con, FILE * fp, /*@unused@*/ int flags)
{
    int leftColWidth;

    (void) showHelpIntro(con, fp);
    if (con->otherHelp)
	fprintf(fp, " %s\n", con->otherHelp);
    else
	fprintf(fp, " %s\n", POPT_("[OPTION...]"));

    leftColWidth = maxArgWidth(con->options, NULL);
    singleTableHelp(fp, con->options, leftColWidth, NULL);
}

static int singleOptionUsage(FILE * fp, int cursor, 
		const struct poptOption * opt,
		/*@null@*/ const char *translation_domain)
	/*@modifies *fp, fileSystem @*/
{
    int len = 3;
    char shortStr[2] = { '\0', '\0' };
    const char * item = shortStr;
    const char * argDescrip = getArgDescrip(opt, translation_domain);

    if (opt->shortName!= '\0' ) {
	if (!(opt->argInfo & POPT_ARG_MASK)) 
	    return cursor;	/* we did these already */
	len++;
	shortStr[0] = opt->shortName;
	shortStr[1] = '\0';
    } else if (opt->longName) {
	len += 1 + strlen(opt->longName);
	item = opt->longName;
    }

    if (len == 3) return cursor;

    if (argDescrip) 
	len += strlen(argDescrip) + 1;

    if ((cursor + len) > 79) {
	fprintf(fp, "\n       ");
	cursor = 7;
    } 

    fprintf(fp, " [-%s%s%s%s]",
	((opt->shortName || (opt->argInfo & POPT_ARGFLAG_ONEDASH)) ? "" : "-"),
	item,
	(argDescrip ? (opt->shortName != '\0' ? " " : "=") : ""),
	(argDescrip ? argDescrip : ""));

    return cursor + len + 1;
}

static int singleTableUsage(FILE * fp,
		int cursor, const struct poptOption * opt,
		/*@null@*/ const char * translation_domain)
	/*@modifies *fp, fileSystem @*/
{
    /*@-branchstate@*/		/* FIX: W2DO? */
    if (opt != NULL)
    for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
        if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INTL_DOMAIN) {
	    translation_domain = (const char *)opt->arg;
	} else if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE) {
	    if (opt->arg)	/* XXX program error */
	    cursor = singleTableUsage(fp, cursor, opt->arg, translation_domain);
	} else if ((opt->longName || opt->shortName) &&
		 !(opt->argInfo & POPT_ARGFLAG_DOC_HIDDEN)) {
	    cursor = singleOptionUsage(fp, cursor, opt, translation_domain);
	}
    }
    /*@=branchstate@*/

    return cursor;
}

static int showShortOptions(const struct poptOption * opt, FILE * fp,
		/*@null@*/ char * str)
	/*@modifies *str, *fp, fileSystem @*/
{
    char * s = alloca(300);	/* larger then the ascii set */

    s[0] = '\0';
    /*@-branchstate@*/		/* FIX: W2DO? */
    if (str == NULL) {
	memset(s, 0, sizeof(s));
	str = s;
    }
    /*@=branchstate@*/

    if (opt != NULL)
    for (; (opt->longName || opt->shortName || opt->arg); opt++) {
	if (opt->shortName && !(opt->argInfo & POPT_ARG_MASK))
	    str[strlen(str)] = opt->shortName;
	else if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE)
	    if (opt->arg)	/* XXX program error */
		(void) showShortOptions(opt->arg, fp, str);
    } 

    if (s != str || *s != '\0')
	return 0;

    fprintf(fp, " [-%s]", s);
    return strlen(s) + 4;
}

void poptPrintUsage(poptContext con, FILE * fp, /*@unused@*/ int flags)
{
    int cursor;

    cursor = showHelpIntro(con, fp);
    cursor += showShortOptions(con->options, fp, NULL);
    (void) singleTableUsage(fp, cursor, con->options, NULL);

    if (con->otherHelp) {
	cursor += strlen(con->otherHelp) + 1;
	if (cursor > 79) fprintf(fp, "\n       ");
	fprintf(fp, " %s", con->otherHelp);
    }

    fprintf(fp, "\n");
}

void poptSetOtherOptionHelp(poptContext con, const char * text)
{
    con->otherHelp = _free(con->otherHelp);
    con->otherHelp = xstrdup(text);
}
