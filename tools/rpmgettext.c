/* rpmheader: spit out the header portion of a package */

#include "system.h"

#include "../build/rpmbuild.h"
#include "../build/buildio.h"

#include "rpmlead.h"
#include "signature.h"
#include "header.h"

#include "intl.h"

static int escape = 1;	/* use octal escape sequence for !isprint(c)? */

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

int debug = 0;
int verbose = 0;
char *inputdir = NULL;
char *outputdir = NULL;
int gentran = 0;

#define	STDINFN	"<stdin>"

static int
rpmgettext(int fd, const char *file, FILE *ofp)
{
	char fni[BUFSIZ], fno[BUFSIZ];

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
	    if (outputdir && *file != '/') {
		strcpy(fno, outputdir);
		strcat(fno, "/");
	    }
	    strcat(fno, file);

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

static int
rpmputtext(const char *file)
{
	char fni[BUFSIZ], fno[BUFSIZ];

	fni[0] = '\0';
	if (inputdir && *file != '/') {
	    strcpy(fni, inputdir);
	    strcat(fni, "/");
	}
	strcat(fni, file);

	fno[0] = '\0';
	if (outputdir && *file != '/') {
	    strcpy(fno, outputdir);
	    strcat(fno, "/");
	    strcat(fno, file);
	} else {
	    strcat(fno, file);
	    strcat(fno, ".out");
	}

	return rewriteBinaryRPM(fni, fno);

}

int
main(int argc, char **argv)
{
    int rc = 0;
    int c;
    extern char *optarg;
    extern int optind;
    int errflg = 0;

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

    if (optind == argc) {
	rc = rpmgettext(0, STDINFN, stdout);
    } else {
	for ( ; optind < argc; optind++ ) {
	    if ((rc = rpmgettext(0, argv[optind], stdout)) != 0)
		break;
	}
    }

    return rc;
}
