/* rpmheader: spit out the header portion of a package */

#include "system.h"

#include "../build/rpmbuild.h"
#include "../build/buildio.h"

#include "rpmlead.h"
#include "signature.h"
#include "header.h"

#include "intl.h"

static int escape = 1;	/* use octal escape sequence for !isprint(c)? */

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
gettextfile(int fd, const char *file, FILE *fp)
{
    struct rpmlead lead;
    Header h;
    int *tp;
    char **langs;
    char buf[BUFSIZ];
    
    fprintf(fp, "\n#========================================================");

    readLead(fd, &lead);
    rpmReadSignature(fd, NULL, lead.signature_type);
    h = headerRead(fd, (lead.major >= 3) ?
		    HEADER_MAGIC_YES : HEADER_MAGIC_NO);

    if ((langs = headerGetLangs(h)) == NULL)
	return 1;

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
	fprintf(fp, "\n#: %s:%s\n", file, getTagString(*tp));

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
    
    FREE(langs);

    return 0;
}

/* ================================================================== */

static int
readRPM(char *fileName, Spec *specp, struct rpmlead *lead, Header *sigs, CSA_t *csa)
{
    int fdi = 0;
    Spec spec;
    int rc;

    if (fileName != NULL && (fdi = open(fileName, O_RDONLY, 0644)) < 0) {
	perror("cannot open package");
	exit(1);
    }

    /* Get copy of lead */
    if ((rc = read(fdi, lead, sizeof(*lead))) != sizeof(*lead)) {
	perror("cannot read lead");
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

    if (specp)		*specp = spec;

    if (csa) {
	csa->cpioFdIn = fdi;
    } else if (fdi != 0) {
	close(fdi);
    }

    return 0;
}

static int
rewriteBinaryRPM(char *fni, char *fno)
{
    struct rpmlead lead;	/* XXX FIXME: exorcize lead/arch/os */
    Header sigs;
    Spec spec;
    CSA_t csabuf, *csa = &csabuf;
    int rc;

    csa->cpioArchiveSize = 0;
    csa->cpioFdIn = -1;
    csa->cpioList = NULL;
    csa->cpioCount = 0;
    csa->lead = &lead;		/* XXX FIXME: exorcize lead/arch/os */

    /* Read rpm and (partially) recreate spec/pkg control structures */
    rc = readRPM(fni, &spec, &lead, &sigs, csa);
    if (rc)
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
#define	xstrdup		strdup
#define	xmalloc		malloc
#define	xrealloc	realloc
#define	PARAMS(_x)	_x

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

    if (ibufp)
	*ibufp = NULL;
    if (nbp)
	*nbp = 0;

    if (stat(file, &sb) < 0) {
	perror(file);
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

#define	SKIPWHITE {while ((c = *se) && strchr("\b\f\n\r\t ", c)) se++;}
#define	NEXTLINE  {state = 0; while ((c = *se) && c != '\n') se++; if (c == '\n') se++;}

static int
parsepofile(const char *file, message_list_ty **mlpp)
{
    message_list_ty *mlp;
    KW_t *kw;
    char *buf, *s, *se;
    size_t nb;
    int c, rc;
    int state = 0;

fprintf(stderr, "================ %s\n", file);
    if ((rc = slurp(file, &buf, &nb)) != 0)
	return rc;

    mlp = message_list_alloc();
    s = buf;
    while ((c = *s) != '\0') {
	se = s;
	switch (state) {
	case 0:		/* free wheeling */
		SKIPWHITE;
		if (c) state = 1;
		break;
	case 1:		/* comment "domain" "msgid" "msgstr" */
		SKIPWHITE;
		s = se;
		if (!(isalpha(c) || c == '#')) {
			fprintf(stderr, "non-alpha char at \"%.20s\"\n", se);
			NEXTLINE;
			break;
		}
		if (c == '#') {
			NEXTLINE;
	/* === s:se has comment */
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
			se++;	/* skip ) */
		}

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
		s = se;
		do {
			s++;	/* skip open quote */
			if ((se = matchchar(s, c, c)) == NULL) {
				fprintf(stderr, "missing close %c at \"%.20s\"\n", c, s);
				se = s;
				NEXTLINE;
				break;
			}
	/* === s:se has next part of string */
			se++;	/* skip close quote */
			SKIPWHITE;
		} while (c == '"');
		state = 0;
		break;
	}
	s = se;
    }

    if (mlpp)
	*mlpp = mlp;
    else
	message_list_free(mlp);

    FREE(buf);
    return rc;
}

/* ================================================================== */

const char *progname = NULL;
int debug = 0;
int verbose = 0;
char *inputdir = NULL;
char *outputdir = NULL;
int gentran = 0;

#define	STDINFN	"<stdin>"

#define	RPMGETTEXT	"rpmgettext"
static int
rpmgettext(int fd, const char *file, FILE *ofp)
{
	char fni[BUFSIZ], fno[BUFSIZ];
	const char *fn;

	if (file == NULL)
	    file = STDINFN;

	if (!strcmp(file, STDINFN))
	    return gettextfile(fd, file, ofp);

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
	    perror(fni);
	    return 2;
	}

	if (gettextfile(fd, fni, ofp)) {
	    return 3;
	}

	if (ofp != stdout)
	    fclose(ofp);
	if (fd != 0)
	    close(fd);

	return 0;
}

#define	RPMPUTTEXT	"rpmputtext"
static int
rpmputtext(int fd, const char *file, FILE *ofp)
{
	char fni[BUFSIZ], fno[BUFSIZ];

	fni[0] = '\0';
	if (inputdir && *file != '/') {
	    strcpy(fni, inputdir);
	    strcat(fni, "/");
	}
	strcat(fni, file);

	fno[0] = '\0';
	if (outputdir) {
	    strcpy(fno, outputdir);
	    strcat(fno, "/");
	    strcat(fno, basename(file));
	} else {
	    strcat(fno, file);
	    strcat(fno, ".out");
	}

	return rewriteBinaryRPM(fni, fno);
}

#define	RPMCHKTEXT	"rpmchktext"
static int
rpmchktext(int fd, const char *file, FILE *ofp)
{
	return parsepofile(file, NULL);
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
	rc = 0;
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
