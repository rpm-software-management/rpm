/* rpmheader: spit out the header portion of a package */

#define	_GNU_SOURCE	1

#include "system.h"

#include "../build/rpmbuild.h"
#include "../build/buildio.h"

#include "rpmlead.h"
#include "signature.h"
#include "header.h"

#if !defined(HAVE_BASENAME)
extern char *basename (const char *__filename);
#endif

#define	MYDEBUG	2

#ifdef	MYDEBUG
#include <stdarg.h>
static void dpf(char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fflush(stderr);
}

#define	DPRINTF(_lvl, _fmt)	if ((_lvl) <= debug) dpf _fmt
#else
#define	DPRINTF(_lvl, _fmt)
#endif

int debug = MYDEBUG;
int verbose = 0;
char *inputdir = NULL;
char *outputdir = NULL;
int msgid_too = 0;
int gottalang = 0;
int nlangs = 0;
char *onlylang[128];
int metamsgid = 0;
int nogroups = 1;
char *mastercatalogue = NULL;

int gentran = 0;

static const char *
getTagString(int tval)
{
    const struct headerTagTableEntry *t;
    static char buf[128];

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (t->val == tval)
	    return t->name;
    }
    sprintf(buf, "<RPMTAG_%d>", tval);
    return buf;
}

static int
getTagVal(const char *tname)
{
    const struct headerTagTableEntry *t;
    int tval;

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (!strncmp(tname, t->name, strlen(t->name)))
	    return t->val;
    }
    sscanf(tname, "%d", &tval);
    return tval;
}

static const struct headerTypeTableEntry {
    char *name;
    int val;
} rpmTypeTable[] = {
    {"RPM_NULL_TYPE",	0},
    {"RPM_CHAR_TYPE",	1},
    {"RPM_INT8_TYPE",	2},
    {"RPM_INT16_TYPE",	3},
    {"RPM_INT32_TYPE",	4},
    {"RPM_INT64_TYPE",	5},
    {"RPM_STRING_TYPE",	6},
    {"RPM_BIN_TYPE",	7},
    {"RPM_STRING_ARRAY_TYPE",	8},
    {"RPM_I18NSTRING_TYPE",	9},
    {NULL,	0}
};

static char *
getTypeString(int tval)
{
    const struct headerTypeTableEntry *t;
    static char buf[128];

    for (t = rpmTypeTable; t->name != NULL; t++) {
	if (t->val == tval)
	    return t->name;
    }
    sprintf(buf, "<RPM_%d_TYPE>", tval);
    return buf;
}

/* ================================================================== */

static const char *
genSrpmFileName(Header h)
{
    char *name, *version, *release, *sourcerpm;
    char sfn[BUFSIZ];

    headerGetEntry(h, RPMTAG_NAME, NULL, (void **)&name, NULL);
    headerGetEntry(h, RPMTAG_VERSION, NULL, (void **)&version, NULL);
    headerGetEntry(h, RPMTAG_RELEASE, NULL, (void **)&release, NULL);
    sprintf(sfn, "%s-%s-%s.src.rpm", name, version, release);

    headerGetEntry(h, RPMTAG_SOURCERPM, NULL, (void **)&sourcerpm, NULL);

#if 0
    if (strcmp(sourcerpm, sfn))
	return xstrdup(sourcerpm);

    return NULL;
#else
    return xstrdup(sourcerpm);
#endif

}

static const char *
hasLang(const char *onlylang, char **langs, char **s)
{
	const char *e = *s;
	int i = 0;

	while(langs[i] && strcmp(langs[i], onlylang)) {
		i++;
		e += strlen(e) + 1;
	}
#if 0
	if (langs[i] && *e)
		return e;
	return NULL;
#else
	return onlylang;
#endif
}

/* ================================================================== */
/* XXX stripped down gettext environment */

#if !defined(PARAMS)
#define	PARAMS(_x)	_x
#endif

/* Length from which starting on warnings about too long strings are given.
   Several systems have limits for strings itself, more have problems with
   strings in their tools (important here: gencat).  1024 bytes is a
   conservative limit.  Because many translation let the message size grow
   (German translations are always bigger) choose a length < 1024.  */
#if !defined(WARN_ID_LEN)
#define WARN_ID_LEN 900
#endif

/* This is the page width for the message_print function.  It should
   not be set to more than 79 characters (Emacs users will appreciate
   it).  It is used to wrap the msgid and msgstr strings, and also to
   wrap the file position (#:) comments.  */
#if !defined(PAGE_WIDTH)
#define PAGE_WIDTH 79
#endif

#include "fstrcmp.c"

#include "str-list.c"

#define	_LIBGETTEXT_H	1	/* XXX WTFO? _LIBINTL_H is undef? */
#undef	_			/* XXX WTFO? */

#include "message.c"

/* ================================================================== */

/* XXX cribbed from gettext/src/message.c */
static const char escapes[] = "\b\f\n\r\t";
static const char escape_names[] = "bfnrt";

static void
expandRpmPO(char *t, const char *s)
{
    const char *esc;
    int c;

    *t++ = '"';
    while((c = *s++) != '\0') {
	esc = strchr(escapes, c);
	if (esc == NULL && (!escape || isprint(c))) {
		if (c == '\\' || c == '"')
			*t++ = '\\';
		*t++ = c;
	} else if (esc != NULL) {
		*t++ = '\\';
		*t++ = escape_names[esc - escapes];
		if (c == '\n') {
			strcpy(t, "\"\n\"");
			t += strlen(t);
		}
	} else {
		*t++ = '\\';
		sprintf(t, "%3.3o", (c & 0377));
		t += strlen(t);
	}
    }
    *t++ = '"';
    *t = '\0';
}

static void
contractRpmPO(char *t, const char *s)
{
    int instring = 0;
    const char *esc;
    int c;

    while((c = *s++) != '\0') {
	if (!(c == '"' || !(instring & 1)))
		continue;
	switch(c) {
	case '\\':
		if (strchr("0123467", *s)) {
			char *se;
			*t++ = strtol(s, &se, 8);
			s = se;
		} else if ((esc = strchr(escape_names, *s)) != NULL) {
			*t++ = escapes[esc - escape_names];
			s++;
		} else {
			*t++ = *s++;
		}
		break;
	case '"':
		instring++;
		break;
	default:
		*t++ = c;
		break;
	}
    }
    *t = '\0';
}

static int poTags[] = {
    RPMTAG_DESCRIPTION,
    RPMTAG_GROUP,
    RPMTAG_SUMMARY,
#ifdef NOTYET
    RPMTAG_DISTRIBUTION,
    RPMTAG_VENDOR,
    RPMTAG_LICENSE,
    RPMTAG_PACKAGER,
#endif
    0
};

static int
gettextfile(FD_t fd, const char *file, FILE *fp, int *poTags)
{
    struct rpmlead lead;
    Header h;
    int *tp;
    char **langs;
    const char *sourcerpm;
    char buf[BUFSIZ];
    
    DPRINTF(99, ("gettextfile(%d,\"%s\",%p,%p)\n", fd, file, fp, poTags));

    fprintf(fp, "\n# ========================================================");

    readLead(fd, &lead);
    rpmReadSignature(fd, NULL, lead.signature_type);
    h = headerRead(fd, (lead.major >= 3) ?
		    HEADER_MAGIC_YES : HEADER_MAGIC_NO);

    if ((langs = headerGetLangs(h)) == NULL)
	return 1;

    sourcerpm = genSrpmFileName(h);

    for (tp = poTags; *tp != 0; tp++) {
	const char *onlymsgstr;
	char **s, *e;
	int i, type, count, nmsgstrs;

	if (!headerGetRawEntry(h, *tp, &type, (void **)&s, &count))
	    continue;

	/* XXX skip untranslated RPMTAG_GROUP for now ... */
    if (nogroups) {
	switch (*tp) {
	case RPMTAG_GROUP:
	    if (count <= 1)
		continue;
	    break;
	default:
	    break;
	}
    }

	/* XXX generate catalog for single language */
	onlymsgstr = NULL;
	if (nlangs > 0 && onlylang[0] != NULL &&
	   (onlymsgstr = hasLang(onlylang[0], langs, s)) == NULL)
		continue;
	

	/* Print xref comment */
	fprintf(fp, "\n#: %s:%d\n", basename(file), *tp);
	if (sourcerpm)
	    fprintf(fp, "#: %s:%d\n", sourcerpm, *tp);

	/* Print msgid */
	e = *s;
	expandRpmPO(buf, e);

	if (metamsgid) {
	    char name[1024], *np;
	    char lctag[128], *lctp;
	    strcpy(name, basename(file));
	    if ((np = strrchr(name, '-')) != NULL) {
		*np = '\0';
		if ((np = strrchr(name, '-')) != NULL) {
		    *np = '\0';
		}
	    }
	    strcpy(lctag, getTagString(*tp));
	    for (lctp = lctag; *lctp; lctp++)
		*lctp = tolower(*lctp);
	    if ((lctp = strchr(lctag, '_')) != NULL)
		lctp++;
	    else
		lctp = lctag;
	
	    *lctp = toupper(*lctp);
	    fprintf(fp, "msgid \"%s(%s)\"\n", name, lctp);
	    fprintf(fp, "msgstr");
	    fprintf(fp, " %s\n", buf);
	    continue;
	}

	fprintf(fp, "msgid %s\n", buf);

	nmsgstrs = 0;
	for (i = 1, e += strlen(e)+1; i < count && e != NULL; i++, e += strlen(e)+1) {
		if (!(onlymsgstr == NULL || onlymsgstr == e))
			continue;
		nmsgstrs++;
		expandRpmPO(buf, e);
		fprintf(fp, "msgstr");
		if (onlymsgstr == NULL)
			fprintf(fp, "(%s)", langs[i]);
		fprintf(fp, " %s\n", buf);
	}

	if (nmsgstrs < 1)
	    fprintf(fp, "msgstr \"\"\n");

	if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
	    FREE(s)
    }
    
    FREE(sourcerpm);
    FREE(langs);

    return 0;
}

/* ================================================================== */

static int
slurp(const char *file, char **ibufp, size_t *nbp)
{
    struct stat sb;
    char *ibuf;
    size_t nb;
    int fd;

    DPRINTF(99, ("slurp(\"%s\",%p,%p)\n", file, ibufp, nbp));

    if (ibufp)
	*ibufp = NULL;
    if (nbp)
	*nbp = 0;

    if (stat(file, &sb) < 0) {
	fprintf(stderr, _("stat(%s): %s\n"), file, strerror(errno));
	return 1;
    }

    nb = sb.st_size + 1;
    if ((ibuf = (char *)malloc(nb)) == NULL) {
	fprintf(stderr, _("malloc(%d)\n"), nb);
	return 2;
    }

    if ((fd = open(file, O_RDONLY)) < 0) {
	fprintf(stderr, _("open(%s): %s\n"), file, strerror(errno));
	return 3;
    }
    if ((nb = read(fd, ibuf, nb)) != sb.st_size) {
	fprintf(stderr, _("read(%s): %s\n"), file, strerror(errno));
	return 4;
    }
    close(fd);
    ibuf[nb] = '\0';

    if (ibufp)
	*ibufp = ibuf;
    if (nbp)
	*nbp = nb;

    return 0;
}

static char *
matchchar(const char *p, char pl, char pr)
{
	int lvl = 0;
	char c;

	while ((c = *p++) != '\0') {
		if (c == '\\') {	/* escaped chars */
			p++;
			continue;
		}
		if (pl == pr && c == pl && *p == pr) {	/* doubled quotes */
			p++;
			continue;
		}
		if (c == pr) {
			if (--lvl <= 0) return (char *)--p;
		} else if (c == pl)
			lvl++;
	}
	return NULL;
}

typedef struct {
	char *name;
	int len;
	int haslang;
} KW_t;

KW_t keywords[] = {
	{ "domain",	6, 0 },
	{ "msgid",	5, 0 },
	{ "msgstr",	6, 1 },
	{ NULL, 0, 0 }
};

#define	SKIPWHITE {while ((c = *se) && strchr("\b\f\n\r\t ", c)) se++;}
#define	NEXTLINE  {state = 0; while ((c = *se) && c != '\n') se++; if (c == '\n') se++;}

static int
parsepofile(const char *file, message_list_ty **mlpp, string_list_ty **flpp)
{
    char tbuf[BUFSIZ];
    string_list_ty *flp;
    message_list_ty *mlp;
    message_ty *mp;
    KW_t *kw = NULL;
    char *lang = NULL;
    char *buf, *s, *se, *t;
    size_t nb;
    int c, rc, tval;
    int state = 1;

    DPRINTF(99, ("parsepofile(\"%s\",%p,%p)\n", file, mlpp, flpp));

DPRINTF(100, ("================ %s\n", file));

    if ((rc = slurp(file, &buf, &nb)) != 0)
	return rc;

    flp = string_list_alloc();
    mlp = message_list_alloc();
    mp = NULL;
    s = buf;
    while ((c = *s) != '\0') {
	se = s;
	switch (state) {
	case 0:		/* free wheeling */
	case 1:		/* comment "domain" "msgid" "msgstr" */
		state = 1;
		SKIPWHITE;
		s = se;
		if (!(isalpha(c) || c == '#')) {
			fprintf(stderr, _("non-alpha char at \"%.20s\"\n"), se);
			NEXTLINE;
			break;
		}

		if (mp == NULL) {
			mp = message_alloc(NULL);
		}

		if (c == '#') {
			while ((c = *se) && c != '\n') se++;
	/* === s:se has comment */
DPRINTF(100, ("%.*s\n", (int)(se-s), s));

			if (c)
				*se = '\0';	/* zap \n */
			switch (s[1]) {
			case '~':	/* archival translations */
				break;
			case ':':	/* file cross reference */
			{   char *f, *fe, *g, *ge;
			    for (f = s+2; f < se; f = ge) {
				while (*f && strchr(" \t", *f)) f++;
				fe = f;
				while (*fe && !strchr(": \t", *fe)) fe++;
				if (*fe != ':') {
					fprintf(stderr, _("malformed #: xref at \"%.60s\"\n"), s);
					break;
				}
				*fe++ = '\0';
				g = ge = fe;
				while (*ge && !strchr(" \t", *ge)) ge++;
				*ge++ = '\0';
				tval = getTagVal(g);
				string_list_append_unique(flp, f);
				message_comment_filepos(mp, f, tval);
			    }
			}	break;
			case '.':	/* automatic comments */
				if (*s == '#') {
					s++;
					if (*s == '.') s++;
				}
				while (*s && strchr(" \t", *s)) s++;
				message_comment_dot_append(mp, xstrdup(s));
				break;
			case ',':	/* flag... */
				mp->is_fuzzy = ((strstr(s,"fuzzy")!=NULL) ?1:0);
				mp->is_c_format = parse_c_format_description_string(s);
				mp->do_wrap = parse_c_width_description_string(s);
				break;
			default:
				/* XXX might want to fix and/or warn here */
			case ' ':	/* comment...*/
				if (*s == '#') s++;
				while (*s && strchr(" \t", *s)) s++;
				message_comment_append(mp, xstrdup(s));
				break;
			}
			if (c)
				se++;	/* skip \n */
			break;
		}

		for (kw = keywords; kw->name != NULL; kw++) {
			if (!strncmp(s, kw->name, kw->len)) {
				se += kw->len;
				break;
			}
		}
		if (kw == NULL || kw->name == NULL) {
			fprintf(stderr, _("unknown keyword at \"%.20s\"\n"), se);
			NEXTLINE;
			break;
		}
	/* === s:se has keyword */
DPRINTF(100, ("%.*s", (int)(se-s), s));

		SKIPWHITE;
		s = se;
		if (kw->haslang && *se == '(') {
			while ((c = *se) && c != ')') se++;
			if (c != ')') {
				fprintf(stderr, _("unclosed paren at \"%.20s\"\n"), s);
				se = s;
				NEXTLINE;
				break;
			}
			s++;	/* skip ( */
	/* === s:se has lang */
DPRINTF(100, ("(%.*s)", (int)(se-s), s));
			*se = '\0';
			lang = s;
			if (c)
				se++;	/* skip ) */
		} else {
			lang = "C";
		}
DPRINTF(100, ("\n"));

		SKIPWHITE;
		if (*se != '"') {
			fprintf(stderr, _("missing string at \"%.20s\"\n"), s);
			se = s;
			NEXTLINE;
			break;
		}
		state = 2;
		break;
	case 2:		/* "...." */
		SKIPWHITE;
		if (c != '"') {
			fprintf(stderr, _("not a string at \"%.20s\"\n"), s);
			NEXTLINE;
			break;
		}

		t = tbuf;
		*t = '\0';
		do {
			s = se;
			s++;	/* skip open quote */
			if ((se = matchchar(s, c, c)) == NULL) {
				fprintf(stderr, _("missing close %c at \"%.20s\"\n"), c, s);
				se = s;
				NEXTLINE;
				break;
			}

	/* === s:se has next part of string */
			*se = '\0';
			contractRpmPO(t, s);
			t += strlen(t);
			*t = '\0';

			se++;	/* skip close quote */
			SKIPWHITE;
		} while (c == '"');

		if (!strcmp(kw->name, "msgid")) {
			FREE(mp->msgid);
			mp->msgid = xstrdup(tbuf);
		} else if (!strcmp(kw->name, "msgstr")) {
			static lex_pos_ty pos = { __FILE__, __LINE__ };
			message_variant_append(mp, xstrdup(lang), xstrdup(tbuf), &pos);
			if (!gottalang) {
				int l;
				for (l = 0; l < nlangs; l++) {
					if (!strcmp(onlylang[l], lang))
						break;
				}
				if (l == nlangs)
					onlylang[nlangs++] = xstrdup(lang);
			}
			lang = NULL;
		}
		/* XXX Peek to see if next item is comment */
		SKIPWHITE;
		if (*se == '#' || *se == '\0') {
			message_list_append(mlp, mp);
			mp = NULL;
		}
		state = 1;
		break;
	}
	s = se;
    }

    if (mlpp)
	*mlpp = mlp;
    else
	message_list_free(mlp);

    if (flpp)
	*flpp = flp;
    else
	string_list_free(flp);

    FREE(buf);
    return rc;
}

/* ================================================================== */

/* For all poTags in h, if msgid is in msg list, then substitute msgstr's */
static int
headerInject(Header h, int *poTags, message_list_ty *mlp)
{
    message_ty *mp;
    message_variant_ty *mvp;
    int *tp;
    
    DPRINTF(99, ("headerInject(%p,%p,%p)\n", h, poTags, mlp));

    for (tp = poTags; *tp != 0; tp++) {
	char **s, *e;
	int n, type, count;

	if (!headerGetRawEntry(h, *tp, &type, (void **)&s, &count))
	    continue;

	/* Only I18N strings can be injected */
	if (type != RPM_I18NSTRING_TYPE) {
	    if (type == RPM_STRING_ARRAY_TYPE)
		FREE(s);
	    continue;
	}

	e = *s;

	/* Search for the msgid ... */
	if ((mp = message_list_search(mlp, e)) == NULL)
		goto bottom;
DPRINTF(1, ("%s\n\tmsgid", getTagString(*tp)));
	if (msgid_too) {
DPRINTF(1, (" (drilled)"));
	    headerAddI18NString(h, *tp, e, "C");
	}
DPRINTF(1, ("\n"));

	/* Skip fuzzy ... */
	if (mp->is_fuzzy) {
DPRINTF(1, ("\t(fuzzy)\n"));
		goto bottom;
	}

	for (n = 0; n < nlangs; n++) {
	    /* Search for the msgstr ... */
	    if ((mvp = message_variant_search(mp, onlylang[n])) == NULL) {
DPRINTF(1, ("\t(%s not found)\n", onlylang[n]));
		continue;
	    }

	    /* Skip untranslated ... */
	    if (strlen(mvp->msgstr) <= 0) {
DPRINTF(1, ("\t(%s untranslated)\n", onlylang[n]));
		continue;
	    }

DPRINTF(1, ("\tmsgstr(%s)\n", onlylang[n]));
	    headerAddI18NString(h, *tp, (char *)mvp->msgstr, onlylang[n]);
	}

bottom:
	if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
	    FREE(s)
    }

    return 0;
}

static int
rewriteBinaryRPM(char *fni, char *fno, message_list_ty *mlp)
{
    struct rpmlead lead;	/* XXX FIXME: exorcize lead/arch/os */
    Header sigs;
    Spec spec;
    CSA_t csabuf, *csa = &csabuf;
    int rc;

    DPRINTF(99, ("rewriteBinaryRPM(\"%s\",\"%s\",%p)\n", fni, fno, mlp));

    csa->cpioArchiveSize = 0;
    csa->cpioFdIn = fdNew(fdio, "init (rewriteBinary)");
    csa->cpioList = NULL;
    csa->cpioCount = 0;
    csa->lead = &lead;		/* XXX FIXME: exorcize lead/arch/os */

    /* Read rpm and (partially) recreate spec/pkg control structures */
    if ((rc = readRPM(fni, &spec, &lead, &sigs, csa)) != 0)
	return rc;

    /* Inject new strings into header tags */
    if ((rc = headerInject(spec->packages->header, poTags, mlp)) != 0)
	goto exit;

    /* Rewrite the rpm */
    if (lead.type == RPMLEAD_SOURCE) {
	rc = writeRPM(spec->packages->header, fno, (int)lead.type,
		csa, spec->passPhrase, &(spec->cookie));
    } else {
	rc = writeRPM(spec->packages->header, fno, (int)lead.type,
		csa, spec->passPhrase, NULL);
    }

exit:
    Fclose(csa->cpioFdIn);
    return rc;

}

/* ================================================================== */

#define	STDINFN	"<stdin>"

#define	RPMGETTEXT	"rpmgettext"
static int
rpmgettext(FD_t fd, const char *file, FILE *ofp)
{
	char fni[BUFSIZ], fno[BUFSIZ];
	const char *fn;

	DPRINTF(99, ("rpmgettext(%d,\"%s\",%p)\n", fd, file, ofp));

	if (file == NULL)
	    file = STDINFN;

	if (!strcmp(file, STDINFN))
	    return gettextfile(fd, file, ofp, poTags);

	fni[0] = '\0';
	if (inputdir && *file != '/') {
	    strcpy(fni, inputdir);
	    strcat(fni, "/");
	}
	strcat(fni, file);

	if (gentran) {
	    char *op;
	    fno[0] = '\0';
	    fn = file;
	    if (outputdir) {
		strcpy(fno, outputdir);
		strcat(fno, "/");
		fn = basename(file);
	    }
	    strcat(fno, fn);

	    if ((op = strrchr(fno, '-')) != NULL &&
		(op = strchr(op, '.')) != NULL)
		    strcpy(op, ".tran");

	    if ((ofp = fopen(fno, "w")) == NULL) {
		fprintf(stderr, _("Can't open %s\n"), fno);
		return 4;
	    }
	}

	fd = fdio->open(fni, O_RDONLY, 0644);
	if (Ferror(fd)) {
	    /* XXX Fstrerror */
	    fprintf(stderr, _("rpmgettext: open %s: %s\n"), fni, strerror(errno));
	    return 2;
	}

	if (gettextfile(fd, fni, ofp, poTags)) {
	    return 3;
	}

	if (ofp != stdout)
	    fclose(ofp);
	Fclose(fd);

	return 0;
}

static char *archs[] = {
	"noarch",
	"i386",
	"alpha",
	"sparc",
	NULL
};

#define	RPMPUTTEXT	"rpmputtext"
static int
rpmputtext(FD_t fd, const char *file, FILE *ofp, string_list_ty *drillp)
{
	string_list_ty *flp;
	message_list_ty *mlp;
	char fn[BUFSIZ];
	char fnib[BUFSIZ], *fni;
	char fnob[BUFSIZ], *fno;
	int j, rc;
	int deletefni = 0;
	int deletefno = 0;

	DPRINTF(99, ("rpmputtext(%x,\"%s\",%p)\n", fd, file, ofp));

	/* Read the po file, parsing out xref files */
	if ((rc = parsepofile(file, &mlp, &flp)) != 0)
		return rc;
	if (drillp) {
		string_list_free(flp);
		flp = drillp;
	}

#if 0
{ int l;
  fprintf(stderr, "Drilling %d langs:", nlangs);
  for (l = 0; l < nlangs; l++)
    fprintf(stderr, " %s", onlylang[l]);
}
#endif

	/* For all xref files ... */
	for (j = 0; j < flp->nitems && rc == 0; j++) {
	    char *f, *fe;

	    f = fn;
	    strcpy(f, flp->item[j]);

	    /* Find the text after the name-version-release */
	    if ((fe = strrchr(f, '-')) == NULL ||
		(fe = strchr(fe, '.')) == NULL) {
		fprintf(stderr, _("rpmputtext: skipping malformed xref \"%s\"\n"), fn);
		continue;
	    }
	    fe++;	/* skip . */

	    if ( !strcmp(fe, "src.rpm") ) {
	    } else {
		char **a, *arch;

		for (a = archs; (arch = *a) != NULL && rc == 0; a++) {

		/* Append ".arch.rpm" */
		    *fe = '\0';
		    strcat(fe, arch);
		    strcat(fe, ".rpm");

		/* Build input "inputdir/arch/fn" */
		    fni = fnib;
		    fni[0] = '\0';
		    if (inputdir) {
			strcat(fni, inputdir);
			strcat(fni, "/");
#ifdef	ADD_BUILD_SUBDIRS
			strcat(fni, arch);
			strcat(fni, "/");
#endif	/* ADD_BUILD_SUBDIRS */
		    }
		    strcat(fni, f);
	
		/* Build output "outputdir/arch/fn" */
		    fno = fnob;
		    fno[0] = '\0';
		    if (outputdir) {
			strcat(fno, outputdir);
			strcat(fno, "/");
#ifdef	ADD_BUILD_SUBDIRS
			strcat(fno, arch);
			strcat(fno, "/");
#endif	/* ADD_BUILD_SUBDIRS */
		    }
		    strcat(fno, f);

		/* XXX skip over noarch/exclusivearch missing inputs */
		    if (access(fni, R_OK))
				continue;

		/* XXX previously rewritten output rpm takes precedence
		 * XXX over input rpm.
		 */
		    deletefni = 0;
		    if (!access(fno, R_OK)) {
			deletefni = 1;
			strcpy(fni, fno);
			strcat(fni, "-DELETE");
			if (rename(fno, fni) < 0) {
			    fprintf(stderr, _("rpmputtext: rename(%s,%s) failed: %s\n"),
				fno, fni, strerror(errno));
			    continue;
			}
		    }

		    rc = rewriteBinaryRPM(fni, fno, mlp);

		    if (deletefni && unlink(fni) < 0) {
			 fprintf(stderr, _("rpmputtext: unlink(%s) failed: %s\n"),
				fni, strerror(errno));
		    }
		    if (deletefno && unlink(fno) < 0) {
			fprintf(stderr, _("rpmputtext: unlink(%s) failed: %s\n"),
				fno, strerror(errno));
		    }
		}
	    }
	}

	if (mlp)
		message_list_free(mlp);

	return rc;
}

#define	RPMCHKTEXT	"rpmchktext"
static int
rpmchktext(FD_t fd, const char *file, FILE *ofp)
{
	DPRINTF(99, ("rpmchktext(%d,\"%s\",%p)\n", fd, file, ofp));
	return parsepofile(file, NULL, NULL);
}

int
main(int argc, char **argv)
{
    int rc = 0;
    int c;
    extern char *optarg;
    extern int optind;
    int errflg = 0;
    FD_t fdi;

    setprogname(argv[0]);	/* Retrofit glibc __progname */

    while((c = getopt(argc, argv, "defgEMl:C:I:O:Tv")) != EOF)
    switch (c) {
    case 'C':
	mastercatalogue = xstrdup(optarg);
	break;
    case 'd':
	debug++;
	break;
    case 'e':
	message_print_style_escape(0);
	break;
    case 'E':
	message_print_style_escape(1);
	break;
    case 'f':
	msgid_too++;
	break;
    case 'l':
	gottalang = 1;
	onlylang[nlangs++] = xstrdup(optarg);
	break;
    case 'I':
	inputdir = optarg;
	break;
    case 'O':
	outputdir = optarg;
	break;
    case 'T':
	gentran++;
	break;
    case 'M':
	metamsgid++;
	break;
    case 'g':
	nogroups = 0;
	break;
    case 'v':
	verbose++;
	break;
    case '?':
    default:
	errflg++;
	break;
    }

    if (errflg) {
	exit(1);
    }

    /* XXX I don't want to read rpmrc */
    addMacro(NULL, "_tmppath", NULL, "/tmp", RMIL_DEFAULT);

    fdi = fdDup(STDIN_FILENO);

    if (!strcmp(__progname, RPMGETTEXT)) {
	if (optind == argc) {
	    rc = rpmgettext(fdi, STDINFN, stdout);
	} else {
	    for ( ; optind < argc; optind++ ) {
		if ((rc = rpmgettext(fdi, argv[optind], stdout)) != 0)
		    break;
	    }
	}
    } else if (!strcmp(__progname, RPMPUTTEXT)) {
	if (mastercatalogue == NULL) {
		fprintf(stderr, _("%s: must specify master PO catalogue with -C\n"),
			__progname);
		exit(1);
        }
	if (optind == argc) {
		fprintf(stderr, _("%s: no binary rpms on cmd line\n"),
			__progname);
		exit(1);
	} else {
	    string_list_ty *drillp = string_list_alloc();
	    for ( ; optind < argc; optind++ ) {
		string_list_append_unique(drillp, argv[optind]);
	    }
	    rc = rpmputtext(fdi, mastercatalogue, stdout, drillp);
	    string_list_free(drillp);
	}
    } else if (!strcmp(__progname, RPMCHKTEXT)) {
	if (optind == argc) {
	    rc = rpmchktext(fdi, STDINFN, stdout);
	} else {
	    for ( ; optind < argc; optind++ ) {
		if ((rc = rpmchktext(0, argv[optind], stdout)) != 0)
		    break;
	    }
	}
    } else {
	rc = 1;
    } 

    return rc;
}
