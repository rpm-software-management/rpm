#include "system.h"

#include <rpmcli.h>
#include <rpmxp.h>

#include <rpmlead.h>
#include <signature.h>
#include "header_internal.h"
#include "misc.h"
#include "debug.h"

static int _rpmtool_debug = 0;

typedef struct rpmavi_s * rpmavi;

struct rpmavi_s {
    const char ** av;
    int flags;
};

static rpmavi rpmaviFree(rpmavi avi)
{
    if (avi) {
	memset(avi, 0, sizeof(*avi));
	avi = _free(avi);
    }
    return NULL;
}

static rpmavi rpmaviNew(const char ** av, int flags)
{
    rpmavi avi = xcalloc(1, sizeof(*avi));
    if (avi) {
	avi->av = av;
	avi->flags = flags;
    }
    return avi;
}

static const char * rpmaviNext(rpmavi avi)
{
    const char * a = NULL;

    if (avi && avi->av && *avi->av)
	a = *avi->av++;
    return a;
}

/*@observer@*/ /*@unchecked@*/
static const struct headerTagTableEntry_s rpmSTagTbl[] = {
	{ "RPMTAG_HEADERIMAGE", HEADER_IMAGE, RPM_BIN_TYPE },
	{ "RPMTAG_HEADERSIGNATURES", HEADER_SIGNATURES, RPM_BIN_TYPE },
	{ "RPMTAG_HEADERIMMUTABLE", HEADER_IMMUTABLE, RPM_BIN_TYPE },
	{ "RPMTAG_HEADERREGIONS", HEADER_REGIONS, RPM_BIN_TYPE },
	{ "RPMTAG_SIGSIZE", 999+1, RPM_INT32_TYPE },
	{ "RPMTAG_SIGLEMD5_1", 999+2, RPM_BIN_TYPE },
	{ "RPMTAG_SIGPGP", 999+3, RPM_BIN_TYPE },
	{ "RPMTAG_SIGLEMD5_2", 999+4, RPM_BIN_TYPE },
	{ "RPMTAG_SIGMD5", 999+5, RPM_BIN_TYPE },
	{ "RPMTAG_SIGGPG", 999+6, RPM_BIN_TYPE },
	{ "RPMTAG_SIGPGP5", 999+7, RPM_BIN_TYPE },
	{ "RPMTAG_SIGPAYLOADSIZE", 999+8, RPM_INT32_TYPE },
	{ "RPMTAG_BADSHA1_1", HEADER_SIGBASE+8, RPM_STRING_TYPE },
	{ "RPMTAG_BADSHA1_2", HEADER_SIGBASE+9, RPM_STRING_TYPE },
	{ "RPMTAG_PUBKEYS", HEADER_SIGBASE+10, RPM_STRING_ARRAY_TYPE },
	{ "RPMTAG_DSAHEADER", HEADER_SIGBASE+11, RPM_BIN_TYPE },
	{ "RPMTAG_RSAHEADER", HEADER_SIGBASE+12, RPM_BIN_TYPE },
	{ "RPMTAG_SHA1HEADER", HEADER_SIGBASE+13, RPM_STRING_TYPE },
	{ NULL, 0 }
};

/*@observer@*/ /*@unchecked@*/
const struct headerTagTableEntry_s * rpmSTagTable = rpmSTagTbl;


typedef enum rpmtoolIOBits_e {
    RPMIOBITS_NONE	= 0,
    RPMIOBITS_LEAD	= (1 <<  0),
    RPMIOBITS_SHEADER	= (1 <<  1),
    RPMIOBITS_HEADER	= (1 <<  2),
    RPMIOBITS_PAYLOAD	= (1 <<  3),
    RPMIOBITS_FDIO	= (1 <<  4),
    RPMIOBITS_UFDIO	= (1 <<  5),
    RPMIOBITS_GZDIO	= (1 <<  6),
    RPMIOBITS_BZDIO	= (1 <<  7),
    RPMIOBITS_UNCOMPRESS= (1 <<  8),
    RPMIOBITS_BINARY	= (1 <<  9),
    RPMIOBITS_DUMP	= (1 << 10),
    RPMIOBITS_XML	= (1 << 11)
} rpmtoolIOBits;

static const char * iav[] = { "-", NULL };
static const char * ifmt = NULL;
static const char * ipath = NULL;
static const char * imode = NULL;
static rpmtoolIOBits ibits = RPMIOBITS_NONE;

static const char * oav[] = { "-", NULL };
static const char * ofmt = NULL;
static const char * opath = NULL;
static const char * omode = NULL;
static rpmtoolIOBits obits = RPMIOBITS_NONE;

static struct iobits_s {
    const char * name;
    rpmtoolIOBits bits;
} iobits[] = {
    { "lead",		RPMIOBITS_LEAD },
    { "sheader",	RPMIOBITS_SHEADER },
    { "header",		RPMIOBITS_HEADER },
    { "payload",	RPMIOBITS_PAYLOAD },
#define	_RPMIOBITS_PKGMASK \
    (RPMIOBITS_LEAD|RPMIOBITS_SHEADER|RPMIOBITS_HEADER|RPMIOBITS_PAYLOAD)

    { "fdio",		RPMIOBITS_FDIO },
    { "ufdio",		RPMIOBITS_UFDIO },
    { "gzdio",		RPMIOBITS_GZDIO },
    { "bzdio",		RPMIOBITS_BZDIO },
#define	_RPMIOBITS_MODEMASK \
    (RPMIOBITS_FDIO|RPMIOBITS_UFDIO|RPMIOBITS_GZDIO|RPMIOBITS_BZDIO)

    { "uncompress",	RPMIOBITS_UNCOMPRESS },
    { "binary",		RPMIOBITS_BINARY },
    { "dump",		RPMIOBITS_DUMP },
    { "xml",		RPMIOBITS_XML },
    { NULL,	0 },
};

static int parseFmt(const char * fmt, rpmtoolIOBits * bits, const char ** mode)
{
    const char *f, *fe;

    if (fmt != NULL)
    for (f = fmt; f && *f; f = fe) {
	struct iobits_s * iop;
	size_t nfe;
	
	if ((fe = strchr(f, ',')) == NULL)
	    fe = f + strlen(f);
	nfe = (fe - f);

	for (iop = iobits; iop->name != NULL; iop++) {
	    if (strncmp(iop->name, f, nfe))
		continue;
	    *bits |= iop->bits;
	    if ((iop->bits & _RPMIOBITS_MODEMASK) && mode && *mode) {
		char * t = xmalloc(2 + strlen(iop->name) + 1);
		t[0] = (*mode)[0];
		t[1] = (*mode)[1];
		(void) stpcpy(t+2, iop->name);
		*mode = _free(*mode);
		*mode = t;
	    }
	    break;
	}

	if (iop->name == NULL) {
	    fprintf(stderr, _("%s: unknown format token \"%*s\", ignored\n"),
			__progname, nfe, f);
	}

	if (*fe == ',') fe++;	/* skip ',' */
    }
    return 0;
}


static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&_rpmtool_debug, -1,
	NULL, NULL },

 { "lead", 'L', POPT_BIT_SET,	&obits, RPMIOBITS_LEAD,
	N_("extract lead"), NULL},
 { "sheader", 'S', POPT_BIT_SET,&obits, RPMIOBITS_SHEADER,
	N_("extract signature header"), NULL},
 { "header", 'H', POPT_BIT_SET,	&obits, RPMIOBITS_HEADER,
	N_("extract metadata header"), NULL},
 { "dump", 'D', POPT_BIT_SET,	&obits, (RPMIOBITS_HEADER|RPMIOBITS_DUMP),
	N_("dump metadata header"), NULL},
 { "xml", 'X', POPT_BIT_SET,	&obits, (RPMIOBITS_HEADER|RPMIOBITS_XML),
	N_("dump metadata header in xml"), NULL},
 { "archive", 'A', POPT_BIT_SET,&obits, RPMIOBITS_PAYLOAD,
	N_("extract compressed payload"), NULL},
 { "payload", 'P', POPT_BIT_SET,&obits, RPMIOBITS_PAYLOAD|RPMIOBITS_UNCOMPRESS,
	N_("extract uncompressed payload"), NULL},

 { "ipath", 0, POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &ipath, 0,
	N_("input PATH"), N_("PATH") },
 { "imode", 0, POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &imode, 0,
	N_("input MODE"), N_("MODE") },
 { "ifmt", 'I', POPT_ARG_STRING,			&ifmt, 0,
	N_("input format, FMT is comma separated list of (lead|sheader|header|payload), (ufdio|gzdio|bzdio), (binary|xml)"), N_("FMT") },

 { "opath", 0, POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &opath, 0,
	N_("output PATH"), N_("PATH") },
 { "omode", 0, POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &omode, 0,
	N_("output MODE"), N_("MODE") },
 { "ofmt", 'O', POPT_ARG_STRING,			&ofmt, 0,
	N_("ouput format, FMT is comma separated list of (lead|sheader|header|payload), (ufdio|gzdio|bzdio), (binary|xml|dump)"), N_("FMT") },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL }, 

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

static void initTool(const char * argv0)
{
    setprogname(argv0);
    /* XXX glibc churn sanity */
    if (__progname == NULL) {
        if ((__progname = strrchr(argv0, '/')) != NULL) __progname++;
        else __progname = argv0;
    }

    if (!strcmp(__progname, "rpmlead")) {
	ibits = _RPMIOBITS_PKGMASK;
	obits = RPMIOBITS_LEAD;
    }
    if (!strcmp(__progname, "rpmsignature")) {
	ibits = _RPMIOBITS_PKGMASK;
	obits = RPMIOBITS_SHEADER;
    }
    if (!strcmp(__progname, "rpmheader")) {
	ibits = _RPMIOBITS_PKGMASK;
	obits = RPMIOBITS_HEADER;
    }
    if (!strcmp(__progname, "rpm2cpio")) {
	ibits = _RPMIOBITS_PKGMASK;
	obits = RPMIOBITS_PAYLOAD | RPMIOBITS_UNCOMPRESS;
    }
    if (!strcmp(__progname, "rpmarchive")) {
	ibits = _RPMIOBITS_PKGMASK;
	obits = RPMIOBITS_PAYLOAD;
    }
    if (!strcmp(__progname, "dump")) {
	ibits = _RPMIOBITS_PKGMASK;
	obits = RPMIOBITS_HEADER | RPMIOBITS_DUMP;
    }
    if (!strcmp(__progname, "rpmdump")) {
	ibits = _RPMIOBITS_PKGMASK;
	obits = RPMIOBITS_HEADER | RPMIOBITS_DUMP;
    }
}

static void spewHeader(FD_t fdo, Header h)
{
    headerTagTableEntry tbl = (headerIsEntry(h, RPMTAG_HEADERI18NTABLE))
				? rpmTagTable : rpmSTagTable;

    if (obits & RPMIOBITS_DUMP) {
	headerDump(h, stdout, HEADER_DUMP_INLINE, tbl);
    } else if (obits & RPMIOBITS_XML) {
	const char * errstr = NULL;
	const char * fmt = "[%{*:xml}\n]";
	char * t;

	t = headerSprintf(h, fmt, tbl, rpmHeaderFormats, &errstr);
		
	if (t != NULL) {
	    if (rpmxpDTD != NULL && *rpmxpDTD != '\0')
		Fwrite(rpmxpDTD, strlen(rpmxpDTD), 1, fdo);
	    Fwrite(t, strlen(t), 1, fdo);
	}
	t = _free(t);
    } else
	headerWrite(fdo, h, HEADER_MAGIC_YES);
}

int
main(int argc, char *const argv[])
{
    struct rpmlead lead;
    Header sigh = NULL;
    Header h = NULL;
    FD_t fdi = NULL;
    FD_t fdo = NULL;
    const char ** av = NULL;
    rpmavi avi = NULL;
    const char * ifn = NULL;
    const char * ofn = NULL;
    const char * s;
    char * t, *te;
    poptContext optCon;
    rpmRC rc = RPMRC_OK;
    int ec = 0;
    int xx;

    imode = xstrdup("r.ufdio");
    omode = xstrdup("w.ufdio");

    initTool(argv[0]);       /* Retrofit glibc __progname */

    /* Parse CLI options and args. */
    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	exit(EXIT_FAILURE);

    av = poptGetArgs(optCon);
if (_rpmtool_debug)
fprintf(stderr, "*** av %p av[0] %p av[1] %p\n", av,
(av && av[0] ? av[0] : NULL),
(av && av[0] && av[1] ? av[1] : NULL));
    if (av == NULL) av = iav;

    /* Parse input and output format. */
    parseFmt(ifmt, &ibits, &imode);
    if (!(ibits & _RPMIOBITS_PKGMASK))
	ibits |= _RPMIOBITS_PKGMASK;

    parseFmt(ofmt, &obits, &omode);
    if (!(obits & _RPMIOBITS_PKGMASK))
	obits |= RPMIOBITS_HEADER;

    /* Insure desired output is possible. */
    if (((ibits & obits) & _RPMIOBITS_PKGMASK) != (obits & _RPMIOBITS_PKGMASK))
    {
	fprintf(stderr, _("%s: no input(0x%x) for output(0x%x) components\n"),
		__progname, ibits, obits);
	ec++;
	goto bottom;
    }

    avi = rpmaviNew(av, 0);
    while ((ifn = rpmaviNext(avi)) != NULL) {

	/* Open input file. */
	if (fdi == NULL) {
if (_rpmtool_debug)
fprintf(stderr, "*** Fopen(%s,%s)\n", ifn, imode);
	    fdi = Fopen(ifn, imode);
	    if (fdi == NULL || Ferror(fdi)) {
		fprintf(stderr, _("%s: input  Fopen(%s, \"%s\"): %s\n"),
			__progname, ifn, imode, Fstrerror(fdi));
		ec++;
		goto bottom;
	    }
	}

	/* Read package components. */
	if (ibits & RPMIOBITS_LEAD) {
	    rc = readLead(fdi, &lead);
	    if (rc != RPMRC_OK) {
		fprintf(stderr, _("%s: readLead(%s) failed(%d)\n"),
			__progname, ifn, rc);
		ec++;
		goto bottom;
	    }
	}

	if (ibits & RPMIOBITS_SHEADER) {
	    const char * msg = NULL;
	    rc = rpmReadSignature(fdi, &sigh, RPMSIGTYPE_HEADERSIG, &msg);
	    if (rc != RPMRC_OK) {
		fprintf(stderr, _("%s: rpmReadSignature(%s) failed(%d): %s\n"),
			__progname, ifn, rc, msg);
		msg = _free(msg);
		ec++;
		goto bottom;
	    }
	}

	if (ibits & RPMIOBITS_HEADER) {
	    h = headerRead(fdi, HEADER_MAGIC_YES);
	    if (h == NULL) {
		fprintf(stderr, _("%s: headerRead(%s) failed\n"),
			__progname, ifn);
		ec++;
		goto bottom;
	    }
	}

	/* Determine output file name, and create directories in path. */
	if (fdo == NULL) {
	    if (opath != NULL) {
		const char * errstr = "(unkown error)";
		int ut;

		/* Macro and --queryformat expanded file path. */
		s = rpmGetPath(opath, NULL);
		ofn = headerSprintf(h, s, rpmTagTable, rpmHeaderFormats, &errstr);
		s = _free(s);
		if (ofn == NULL) {
		    fprintf(stderr, _("%s: headerSprintf(%s) failed: %s\n"),
				__progname, ofn, errstr);
		    ec++;
		    goto bottom;
		}

		/* Create directories in path. */
		s = xstrdup(ofn);
		ut = urlPath(s, (const char **)&t);
		if (t != NULL)
		for (te=strchr(t+1,'/'); (t=te) != NULL; te = strchr(t+1,'/')) {
		    *t = '\0';
		    rc = rpmMkdirPath(s, "opath");
		    *t = '/';
		    if (rc != RPMRC_OK) break;
		}
		s = _free(s);
		if (rc != RPMRC_OK) {
		    fprintf(stderr, _("%s: rpmMkdirPath(%s) failed.\n"),
				__progname, ofn);
		    ec++;
		    goto bottom;
		}
	    }

	    /* Open output file. */
if (_rpmtool_debug)
fprintf(stderr, "*** Fopen(%s,%s)\n", (ofn != NULL ? ofn : "-"), omode);
	    fdo = (ofn != NULL ? Fopen(ofn, omode) : fdDup(STDOUT_FILENO));
	    if (fdo == NULL || Ferror(fdo)) {
		fprintf(stderr, _("%s: output Fopen(%s, \"%s\"): %s\n"),
			__progname, (ofn != NULL ? ofn : "<stdout>"),
			omode, Fstrerror(fdo));
		ec++;
		goto bottom;
	    }
	}

	/* Write package components. */
	if (obits & RPMIOBITS_LEAD)
	    writeLead(fdo, &lead);

	if (obits & RPMIOBITS_SHEADER)
	    spewHeader(fdo, sigh);

	if (obits & RPMIOBITS_HEADER)
	    spewHeader(fdo, h);

	if (obits & RPMIOBITS_PAYLOAD)
	{
	    if (obits & RPMIOBITS_UNCOMPRESS) {
		const char * payload_compressor = NULL;
		const char * rpmio_flags;
		FD_t gzdi;

		/* Retrieve type of payload compression. */
		if (!headerGetEntry(h, RPMTAG_PAYLOADCOMPRESSOR, NULL,
			    (void **) &payload_compressor, NULL))
		    payload_compressor = "gzip";
		rpmio_flags = t = alloca(sizeof("r.gzdio"));
		*t++ = 'r';
		if (!strcmp(payload_compressor, "gzip"))
		    t = stpcpy(t, ".gzdio");
		if (!strcmp(payload_compressor, "bzip2"))
		    t = stpcpy(t, ".bzdio");

		gzdi = Fdopen(fdi, rpmio_flags);	/* XXX gzdi == fdi */
		if (gzdi == NULL) {
		    fprintf(stderr, _("%s: output Fdopen(%s, \"%s\"): %s\n"),
			__progname, (ofn != NULL ? ofn : "-"),
			rpmio_flags, Fstrerror(fdo));
		    ec++;
		    goto bottom;
		}

		rc = ufdCopy(gzdi, fdo);
		xx = Fclose(gzdi);	/* XXX gzdi == fdi */
		if (rc <= 0) {
		    ec++;
		    goto bottom;
		}
	    } else {
		char buffer[BUFSIZ];
		int ct;
		while ((ct = Fread(buffer, sizeof(buffer), 1, fdi)))
		    Fwrite(buffer, ct, 1, fdo);
	    }
	}

bottom:
	sigh = headerFree(sigh);
	h = headerFree(h);
	ofn = _free(ofn);

	if (fdi != NULL && Fileno(fdi) != STDIN_FILENO) {
	    xx = Fclose(fdi);
	    fdi = NULL;
	}
	if (fdo != NULL && Fileno(fdo) != STDOUT_FILENO) {
	    xx = Fclose(fdo);
	    fdo = NULL;
	}
    }
    avi = rpmaviFree(avi);

exit:
    sigh = headerFree(sigh);
    h = headerFree(h);
    ofn = _free(ofn);

    if (fdi != NULL && Fileno(fdi) != STDIN_FILENO) {
	xx = Fclose(fdi);
	fdi = NULL;
    }
    if (fdo != NULL && Fileno(fdo) != STDOUT_FILENO) {
	xx = Fclose(fdi);
	fdi = NULL;
    }
    optCon = rpmcliFini(optCon);

    imode = _free(imode);
    omode = _free(omode);

    return ec;
}
