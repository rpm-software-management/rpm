/* rpmheader: spit out the header portion of a package */

#include "system.h"

#include "../build/rpmbuild.h"
#include "../build/buildio.h"

#include "rpmlead.h"
#include "signature.h"
#include "header.h"

#include "intl.h"

#define	MYDEBUG	1

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

const char *progname = NULL;
int debug = MYDEBUG;
int verbose = 0;
int escape = 1;		/* use octal escape sequence for !isprint(c)? */
char *inputdir = "/mnt/redhat/comps/dist/5.2";
char *outputdir = "/tmp/OUT";

int gentran = 0;

static inline char *
basename(const char *file)
{
	char *fn = strrchr(file, '/');
	return fn ? fn+1 : (char *)file;
}

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

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (!strncmp(tname, t->name, strlen(t->name)))
	    return t->val;
    }
    return 0;
}

const struct headerTypeTableEntry {
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

static char **
headerGetLangs(Header h)
{
    char **s, *e, **table;
    int i, type, count;

    if (!headerGetRawEntry(h, HEADER_I18NTABLE, &type, (void **)&s, &count))
	return NULL;

    if ((table = (char **)calloc((count+1), sizeof(char *))) == NULL)
	return NULL;

    for (i = 0, e = *s; i < count > 0; i++, e += strlen(e)+1) {
	table[i] = e;
    }
    table[count] = NULL;

    return table;
}

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

/* ================================================================== */

static const char *
genSrpmFileName(Header h)
{
    char *name, *version, *release, *sourcerpm;
    char sfn[BUFSIZ], bfn[BUFSIZ];

    headerGetEntry(h, RPMTAG_NAME, NULL, (void **)&name, NULL);
    headerGetEntry(h, RPMTAG_VERSION, NULL, (void **)&version, NULL);
    headerGetEntry(h, RPMTAG_RELEASE, NULL, (void **)&release, NULL);
    sprintf(sfn, "%s-%s-%s.src.rpm", name, version, release);

    headerGetEntry(h, RPMTAG_SOURCERPM, NULL, (void **)&sourcerpm, NULL);

    if (strcmp(sourcerpm, sfn))
	return strdup(sourcerpm);

    return NULL;

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
gettextfile(int fd, const char *file, FILE *fp, int *poTags)
{
    struct rpmlead lead;
    Header h;
    int *tp;
    char **langs;
    const char *sourcerpm;
    char buf[BUFSIZ];
    
    DPRINTF(99, ("gettextfile(%d,\"%s\",%p,%p)\n", fd, file, fp, poTags));

    fprintf(fp, "\n#========================================================");

    readLead(fd, &lead);
    rpmReadSignature(fd, NULL, lead.signature_type);
    h = headerRead(fd, (lead.major >= 3) ?
		    HEADER_MAGIC_YES : HEADER_MAGIC_NO);

    if ((langs = headerGetLangs(h)) == NULL)
	return 1;

    sourcerpm = genSrpmFileName(h);

    for (tp = poTags; *tp != 0; tp++) {
	char **s, *e;
	int i, type, count;

	if (!headerGetRawEntry(h, *tp, &type, (void **)&s, &count))
	    continue;

	/* XXX skip untranslated RPMTAG_GROUP for now ... */
	switch (*tp) {
	case RPMTAG_GROUP:
	    if (count <= 1)
		continue;
	    break;
	default:
	    break;
	}

	/* Print xref comment */
	fprintf(fp, "\n#: %s:%s\n", basename(file), getTagString(*tp));
	if (sourcerpm)
	    fprintf(fp, "#: %s:%d\n", sourcerpm, 0);

	/* Print msgid */
	e = *s;
	expandRpmPO(buf, e);
	fprintf(fp, "msgid %s\n", buf);

	if (count <= 1)
	    fprintf(fp, "msgstr \"\"\n");
	for (i = 1, e += strlen(e)+1; i < count && e != NULL; i++, e += strlen(e)+1) {
		expandRpmPO(buf, e);
		fprintf(fp, "msgstr(%s) %s\n", langs[i], buf);
	}

	if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
	    FREE(s)
    }
    
    FREE((char *)sourcerpm);
    FREE(langs);

    return 0;
}

/* ================================================================== */
#define	xstrdup		strdup
#define	xmalloc		malloc
#define	xrealloc	realloc
#define	PARAMS(_x)	_x

#include "fstrcmp.c"

#include "str-list.c"

#include "rpmpo.h"

message_ty *
message_alloc (msgid)
     char *msgid;
{
  message_ty *mp;

  mp = xmalloc (sizeof (message_ty));
  mp->msgid = msgid;
  mp->comment = NULL;
  mp->comment_dot = NULL;
  mp->filepos_count = 0;
  mp->filepos = NULL;
  mp->variant_count = 0;
  mp->variant = NULL;
  mp->used = 0;
  mp->obsolete = 0;
  mp->is_fuzzy = 0;
  mp->is_c_format = undecided;
  mp->do_wrap = undecided;
  return mp;
}

void
message_free (mp)
     message_ty *mp;
{
  size_t j;

  if (mp->comment != NULL)
    string_list_free (mp->comment);
  if (mp->comment_dot != NULL)
    string_list_free (mp->comment_dot);
  free ((char *) mp->msgid);
  for (j = 0; j < mp->variant_count; ++j)
    free ((char *) mp->variant[j].msgstr);
  if (mp->variant != NULL)
    free (mp->variant);
  for (j = 0; j < mp->filepos_count; ++j)
    free ((char *) mp->filepos[j].file_name);
  if (mp->filepos != NULL)
    free (mp->filepos);
  free (mp);
}

message_variant_ty *
message_variant_search (mp, domain)
     message_ty *mp;
     const char *domain;
{
  size_t j;
  message_variant_ty *mvp;

  for (j = 0; j < mp->variant_count; ++j)
    {
      mvp = &mp->variant[j];
      if (0 == strcmp (domain, mvp->domain))
        return mvp;
    }
  return 0;
}

void
message_variant_append (mp, domain, msgstr, pp)
     message_ty *mp;
     const char *domain;
     const char *msgstr;
     const lex_pos_ty *pp;
{
  size_t nbytes;
  message_variant_ty *mvp;

  nbytes = (mp->variant_count + 1) * sizeof (mp->variant[0]);
  mp->variant = xrealloc (mp->variant, nbytes);
  mvp = &mp->variant[mp->variant_count++];
  mvp->domain = domain;
  mvp->msgstr = msgstr;
  mvp->pos = *pp;
}

void
message_comment_append (mp, s)
     message_ty *mp;
     const char *s;
{
  if (mp->comment == NULL)
    mp->comment = string_list_alloc ();
  string_list_append (mp->comment, s);
}

void
message_comment_dot_append (mp, s)
     message_ty *mp;
     const char *s;
{
  if (mp->comment_dot == NULL)
    mp->comment_dot = string_list_alloc ();
  string_list_append (mp->comment_dot, s);
}

void
message_comment_filepos (mp, name, line)
     message_ty *mp;
     const char *name;
     size_t line;
{
  size_t nbytes;
  lex_pos_ty *pp;
  int min, max;
  int j;

  /* See if we have this position already.  They are kept in sorted
     order, so use a binary chop.  */
  /* FIXME: use bsearch */
  min = 0;
  max = (int) mp->filepos_count - 1;
  while (min <= max)
    {
      int mid;
      int cmp;

      mid = (min + max) / 2;
      pp = &mp->filepos[mid];
      cmp = strcmp (pp->file_name, name);
      if (cmp == 0)
	cmp = (int) pp->line_number - line;
      if (cmp == 0)
	return;
      if (cmp < 0)
	min = mid + 1;
      else
	max = mid - 1;
    }

  /* Extend the list so that we can add an position to it.  */
  nbytes = (mp->filepos_count + 1) * sizeof (mp->filepos[0]);
  mp->filepos = xrealloc (mp->filepos, nbytes);

  /* Shuffle the rest of the list up one, so that we can insert the
     position at ``min''.  */
  /* FIXME: use memmove */
  for (j = mp->filepos_count; j > min; --j)
    mp->filepos[j] = mp->filepos[j - 1];
  mp->filepos_count++;

  /* Insert the postion into the empty slot.  */
  pp = &mp->filepos[min];
  pp->file_name = xstrdup (name);
  pp->line_number = line;
}

message_list_ty *
message_list_alloc ()
{
  message_list_ty *mlp;

  mlp = xmalloc (sizeof (message_list_ty));
  mlp->nitems = 0;
  mlp->nitems_max = 0;
  mlp->item = 0;
  return mlp;
}

void
message_list_append (mlp, mp)
     message_list_ty *mlp;
     message_ty *mp;
{
  if (mlp->nitems >= mlp->nitems_max)
    {
      size_t nbytes;

      mlp->nitems_max = mlp->nitems_max * 2 + 4;
      nbytes = mlp->nitems_max * sizeof (message_ty *);
      mlp->item = xrealloc (mlp->item, nbytes);
    }
  mlp->item[mlp->nitems++] = mp;
}

void
message_list_delete_nth (mlp, n)
     message_list_ty *mlp;
     size_t n;
{
  size_t j;

  if (n >= mlp->nitems)
    return;
  message_free (mlp->item[n]);
  for (j = n + 1; j < mlp->nitems; ++j)
    mlp->item[j - 1] = mlp->item[j];
  mlp->nitems--;
}

message_ty *
message_list_search (mlp, msgid)
     message_list_ty *mlp;
     const char *msgid;
{
  size_t j;

  for (j = 0; j < mlp->nitems; ++j)
    {
      message_ty *mp;

      mp = mlp->item[j];
      if (0 == strcmp (msgid, mp->msgid))
        return mp;
    }
  return 0;
}

message_ty *
message_list_search_fuzzy (mlp, msgid)
     message_list_ty *mlp;
     const char *msgid;
{
  size_t j;
  double best_weight;
  message_ty *best_mp;

  best_weight = 0.6;
  best_mp = NULL;
  for (j = 0; j < mlp->nitems; ++j)
    {
      size_t k;
      double weight;
      message_ty *mp;

      mp = mlp->item[j];

      for (k = 0; k < mp->variant_count; ++k)
        if (mp->variant[k].msgstr != NULL && mp->variant[k].msgstr[0] != '\0')
          break;
      if (k >= mp->variant_count)
        continue;

      weight = fstrcmp (msgid, mp->msgid);
      if (weight > best_weight)
        {
          best_weight = weight;
          best_mp = mp;
        }
    }
  return best_mp;
}

void
message_list_free (mlp)
     message_list_ty *mlp;
{
  size_t j;

  for (j = 0; j < mlp->nitems; ++j)
    message_free (mlp->item[j]);
  if (mlp->item)
    free (mlp->item);
  free (mlp);
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
	fprintf(stderr, "stat(%s): %s\n", file, strerror(errno));
	return 1;
    }

    nb = sb.st_size + 1;
    if ((ibuf = (char *)malloc(nb)) == NULL) {
	fprintf(stderr, "malloc(%d)\n", nb);
	return 2;
    }

    if ((fd = open(file, O_RDONLY)) < 0) {
	fprintf(stderr, "open(%s): %s\n", file, strerror(errno));
	return 3;
    }
    if ((nb = read(fd, ibuf, nb)) != sb.st_size) {
	fprintf(stderr, "read(%s): %s\n", file, strerror(errno));
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
	NULL
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
    KW_t *kw;
    char *lang;
    char *buf, *s, *se, *t, *f, *fe;
    size_t nb;
    int c, rc;
    int state = 1;
    int gotmsgstr = 0;

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
			fprintf(stderr, "non-alpha char at \"%.20s\"\n", se);
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
			case ':':
				f = s+2;
				while (*f && strchr(" \t", *f)) f++;
				fe = f;
				while (*fe && !strchr("\n:", *fe)) fe++;
				if (*fe != ':') {
					fprintf(stderr, "malformed #: xref at \"%.60s\"\n", s);
					break;
				}
				*fe++ = '\0';
				string_list_append_unique(flp, f);
				message_comment_filepos(mp, f, getTagVal(fe));
				break;
			case '.':
				message_comment_dot_append(mp, xstrdup(s));
				break;
			default:
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
			fprintf(stderr, "unknown keyword at \"%.20s\"\n", se);
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
				fprintf(stderr, "unclosed paren at \"%.20s\"\n", s);
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
		} else
			lang = "C";
DPRINTF(100, ("\n"));

		SKIPWHITE;
		if (*se != '"') {
			fprintf(stderr, "missing string at \"%.20s\"\n", s);
			se = s;
			NEXTLINE;
			break;
		}
		state = 2;
		break;
	case 2:		/* "...." */
		SKIPWHITE;
		if (c != '"') {
			fprintf(stderr, "not a string at \"%.20s\"\n", s);
			NEXTLINE;
			break;
		}

		t = tbuf;
		*t = '\0';
		do {
			s = se;
			s++;	/* skip open quote */
			if ((se = matchchar(s, c, c)) == NULL) {
				fprintf(stderr, "missing close %c at \"%.20s\"\n", c, s);
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
			FREE(((char *)mp->msgid));
			mp->msgid = xstrdup(tbuf);
		} else if (!strcmp(kw->name, "msgstr")) {
			static lex_pos_ty pos = { __FILE__, __LINE__ };
			message_variant_append(mp, xstrdup(lang), xstrdup(tbuf), &pos);
			lang = NULL;
		}
		/* XXX Peek to see if next item is comment */
		SKIPWHITE;
		if (*se == '#') {
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

static int
readRPM(char *fileName, Spec *specp, struct rpmlead *lead, Header *sigs, CSA_t *csa)
{
    int fdi = 0;
    Spec spec;
    int rc;

    DPRINTF(99, ("readRPM(\"%s\",%p,%p,%p,%p)\n", fileName, specp, lead, sigs, csa));

    if (fileName != NULL && (fdi = open(fileName, O_RDONLY, 0644)) < 0) {
	fprintf(stderr, "readRPM: open %s: %s\n", fileName, strerror(errno));
	exit(1);
    }

    /* Get copy of lead */
    if ((rc = read(fdi, lead, sizeof(*lead))) != sizeof(*lead)) {
	fprintf(stderr, "readRPM: read %s: %s\n", fileName, strerror(errno));
	exit(1);
    }
    lseek(fdi, 0, SEEK_SET);	/* XXX FIXME: EPIPE */

    /* Reallocate build data structures */
    spec = newSpec();
    spec->packages = newPackage(spec);

    /* XXX the header just allocated will be allocated again */
    if (spec->packages->header) {
	headerFree(spec->packages->header);
	spec->packages->header = NULL;
    }

   /* Read the rpm lead and header */
    rc = rpmReadPackageInfo(fdi, sigs, &spec->packages->header);
    switch (rc) {
    case 1:
	fprintf(stderr, _("error: %s is not an RPM package\n"), fileName);
	exit(1);
    case 0:
	break;
    default:
	fprintf(stderr, _("error: reading header from %s\n"), fileName);
	exit(1);
	break;
    }

    if (specp)
	*specp = spec;

    if (csa) {
	csa->cpioFdIn = fdi;
    } else if (fdi != 0) {
	close(fdi);
    }

    return 0;
}

/* For all poTags in h, if msgid is in msg list, then substitute msgstr's */
static int
headerInject(Header h, int *poTags, message_list_ty *mlp)
{
    message_ty *mp;
    message_variant_ty *mvp;
    int *tp;
    char **langs;
    char buf[BUFSIZ];
    
    DPRINTF(99, ("headerInject(%p,%p,%p)\n", h, poTags, mlp));

    if ((langs = headerGetLangs(h)) == NULL)
	return 1;

    for (tp = poTags; *tp != 0; tp++) {
	char **s, *e;
	int i, type, count;

	if (!headerGetRawEntry(h, *tp, &type, (void **)&s, &count))
	    continue;

	/* Search for the msgid */
	e = *s;
	if ((mp = message_list_search(mlp, e)) != NULL) {

DPRINTF(1, ("injecting %s msgid\n", getTagString(*tp)));
	    for (i = 0; i < count; i++) {
		if ((mvp = message_variant_search(mp, langs[i])) == NULL)
		    continue;

DPRINTF(1, ("\tmsgstr(%s)\n", langs[i]));
#if 1
		headerAddI18NString(h, *tp, (char *)mvp->msgstr, langs[i]);
#else
DPRINTF(1, ("headerAddI18NString(%x,%d,%x,\"%s\")\n%s\n",
h, *tp, mvp->msgstr, langs[i], mvp->msgstr));
#endif

	    }

#if 0
	    for (i = 1, e += strlen(e)+1; i < count && e != NULL; i++, e += strlen(e)+1) {
		expandRpmPO(buf, e);
		fprintf(fp, "msgstr(%s) %s\n", langs[i], buf);
	    }
#endif

	}

	if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
	    FREE(s)
    }

    FREE(langs);

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
    csa->cpioFdIn = -1;
    csa->cpioList = NULL;
    csa->cpioCount = 0;
    csa->lead = &lead;		/* XXX FIXME: exorcize lead/arch/os */

    /* Read rpm and (partially) recreate spec/pkg control structures */
    if ((rc = readRPM(fni, &spec, &lead, &sigs, csa)) != 0)
	return rc;

    /* Inject new strings into header tags */
    if ((rc = headerInject(spec->packages->header, poTags, mlp)) != 0)
	return rc;

    /* Rewrite the rpm */
    if (lead.type == RPMLEAD_SOURCE) {
	return writeRPM(spec->packages->header, fno, (int)lead.type,
		csa, spec->passPhrase, &(spec->cookie));
    } else {
	return writeRPM(spec->packages->header, fno, (int)lead.type,
		csa, spec->passPhrase, NULL);
    }
}

/* ================================================================== */

#define	STDINFN	"<stdin>"

#define	RPMGETTEXT	"rpmgettext"
static int
rpmgettext(int fd, const char *file, FILE *ofp)
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
		fprintf(stderr, "Can't open %s\n", fno);
		return 4;
	    }
	}

	if ((fd = open(fni, O_RDONLY, 0644)) < 0) {
	    fprintf(stderr, "rpmgettext: open %s: %s\n", fni, strerror(errno));
	    return 2;
	}

	if (gettextfile(fd, fni, ofp, poTags)) {
	    return 3;
	}

	if (ofp != stdout)
	    fclose(ofp);
	if (fd != 0)
	    close(fd);

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
rpmputtext(int fd, const char *file, FILE *ofp)
{
	string_list_ty *flp;
	message_list_ty *mlp;
	message_ty *mp;
	lex_pos_ty *pp;
	Header newh;
	char fn[BUFSIZ], fni[BUFSIZ], fno[BUFSIZ];
	int j, rc;

	DPRINTF(99, ("rpmputtext(%d,\"%s\",%p)\n", fd, file, ofp));

	if ((rc = parsepofile(file, &mlp, &flp)) != 0)
		return rc;

	for (j = 0; j < flp->nitems && rc == 0; j++) {
	    char *f, *fe;

	    f = fn;
	    strcpy(f, flp->item[j]);

	    /* Find the text after the name-version-release */
	    if ((fe = strrchr(f, '-')) == NULL ||
		(fe = strchr(fe, '.')) == NULL) {
		fprintf(stderr, "skipping malformed xref \"%s\"\n", fn);
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
		    fni[0] = '\0';
		    if (inputdir) {
			strcat(fni, inputdir);
			strcat(fni, "/");
			strcat(fni, arch);
			strcat(fni, "/");
		    }
		    strcat(fni, f);
	
		/* Build output "outputdir/arch/fn" */
		    fno[0] = '\0';
		    if (outputdir) {
			strcat(fno, outputdir);
			strcat(fno, "/");
			strcat(fno, arch);
			strcat(fno, "/");
		    }
		    strcat(fno, f);

		/* XXX skip over noarch/exclusivearch */
		    if (!access(fni, R_OK))
			rc = rewriteBinaryRPM(fni, fno, mlp);
		}
	    }
	}

	if (mlp)
		message_list_free(mlp);

	return rc;
}

#define	RPMCHKTEXT	"rpmchktext"
static int
rpmchktext(int fd, const char *file, FILE *ofp)
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

    progname = basename(argv[0]);

    while((c = getopt(argc, argv, "deI:O:Tv")) != EOF)
    switch (c) {
    case 'd':
	debug++;
	break;
    case 'e':
	escape = 0;
	break;
    case 'I':
	inputdir = optarg;
	break;
    case 'O':
	outputdir = optarg;
	break;
    case 'T':
	gentran++;
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

    /* XXX I don't want to read rpmrc yet */
    rpmSetVar(RPMVAR_TMPPATH, "/tmp");

    if (!strcmp(progname, RPMGETTEXT)) {
	if (optind == argc) {
	    rc = rpmgettext(0, STDINFN, stdout);
	} else {
	    for ( ; optind < argc; optind++ ) {
		if ((rc = rpmgettext(0, argv[optind], stdout)) != 0)
		    break;
	    }
	}
    } else if (!strcmp(progname, RPMPUTTEXT)) {
	if (optind == argc) {
	    rc = rpmputtext(0, STDINFN, stdout);
	} else {
	    for ( ; optind < argc; optind++ ) {
		if ((rc = rpmputtext(0, argv[optind], stdout)) != 0)
		    break;
	    }
	}
    } else if (!strcmp(progname, RPMCHKTEXT)) {
	if (optind == argc) {
	    rc = rpmchktext(0, STDINFN, stdout);
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
