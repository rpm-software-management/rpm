#include "system.h"

#include <rpmcli.h>

#include <rpmlead.h>
#include <signature.h>
#include "header_internal.h"
#include "misc.h"
#include "debug.h"

typedef enum rpmtoolComponentBits_e {
    RPMTOOL_NONE	= 0,
    RPMTOOL_LEAD	= (1 << 0),
    RPMTOOL_SHEADER	= (1 << 1),
    RPMTOOL_HEADER	= (1 << 2),
    RPMTOOL_DUMP	= (1 << 3),
    RPMTOOL_XML		= (1 << 4),
    RPMTOOL_PAYLOAD	= (1 << 5),
    RPMTOOL_UNCOMPRESS	= (1 << 6)
} rpmtoolComponentBits;

static rpmtoolComponentBits componentBits = RPMTOOL_NONE;
static const char * iav[] = { "-", NULL };
static const char * ipath = NULL;
static const char * ifmt = NULL;
static const char * oav[] = { "-", NULL };
static const char * opath = NULL;
static const char * ofmt = NULL;

static int _rpmtool_debug = 0;

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&_rpmtool_debug, -1,
	NULL, NULL },

 { "lead", 'L', POPT_BIT_SET,	&componentBits, RPMTOOL_LEAD,
	N_("extract lead"), NULL},
 { "sheader", 'S', POPT_BIT_SET,&componentBits, RPMTOOL_SHEADER,
	N_("extract signature header"), NULL},
 { "header", 'H', POPT_BIT_SET,	&componentBits, RPMTOOL_HEADER,
	N_("extract metadata header"), NULL},
 { "dump", 'D', POPT_BIT_SET,	&componentBits, (RPMTOOL_HEADER|RPMTOOL_DUMP),
	N_("dump metadata header"), NULL},
 { "xml", 'X', POPT_BIT_SET,	&componentBits, (RPMTOOL_HEADER|RPMTOOL_XML),
	N_("dump metadata header in xml"), NULL},
 { "archive", 'A', POPT_BIT_SET,&componentBits, RPMTOOL_PAYLOAD,
	N_("extract compressed payload"), NULL},
 { "payload", 'P', POPT_BIT_SET,&componentBits, RPMTOOL_PAYLOAD|RPMTOOL_UNCOMPRESS,
	N_("extract uncompressed payload"), NULL},

 { "opath", 0, POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &opath, 0,
	N_("output PATH"), N_("PATH") },

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
	componentBits = RPMTOOL_LEAD;
	ofmt = "lead";
    }
    if (!strcmp(__progname, "rpmsignature")) {
	componentBits = RPMTOOL_SHEADER;
	ofmt = NULL;
    }
    if (!strcmp(__progname, "rpmheader")) {
	componentBits = RPMTOOL_HEADER;
	ofmt = NULL;
    }
    if (!strcmp(__progname, "rpm2cpio")) {
	componentBits = RPMTOOL_PAYLOAD|RPMTOOL_UNCOMPRESS;
	ofmt = NULL;
    }
    if (!strcmp(__progname, "rpmarchive")) {
	componentBits = RPMTOOL_PAYLOAD;
	ofmt = NULL;
    }
    if (!strcmp(__progname, "dump")) {
	componentBits = RPMTOOL_HEADER | RPMTOOL_DUMP;
	ofmt = "dump";
    }
    if (!strcmp(__progname, "rpmdump")) {
	componentBits = RPMTOOL_HEADER | RPMTOOL_DUMP;
	ofmt = "dump";
    }
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
    const char * ifn = NULL;
    const char * ofn = NULL;
    const char * s;
    char * t, *te;
    poptContext optCon;
    rpmRC rc = RPMRC_OK;
    int ec = 0;
    int xx;

    initTool(argv[0]);       /* Retrofit glibc __progname */

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	exit(EXIT_FAILURE);

    av = poptGetArgs(optCon);
if (_rpmtool_debug)
fprintf(stderr, "*** av %p av[0] %p av[1] %p\n", av,
(av && av[0] ? av[0] : NULL),
(av && av[0] && av[1] ? av[1] : NULL));
    if (av == NULL) av = iav;

    while ((ifn = *av++) != NULL) {

	if (fdi == NULL) {
if (_rpmtool_debug)
fprintf(stderr, "*** Fopen(%s,r.ufdio)\n", ifn);
	    fdi = Fopen(ifn, "r.ufdio");
	    if (fdi == NULL || Ferror(fdi)) {
		fprintf(stderr, "%s: input  Fopen(%s, \"r\"): %s\n", __progname,
			ifn, Fstrerror(fdi));
		ec++;
		goto bottom;
	    }
	}

	/* Read package components. */
	if (readLead(fdi, &lead) != RPMRC_OK
	 || rpmReadSignature(fdi, &sigh, lead.signature_type, NULL) != RPMRC_OK)
	{
	    ec++;
	    goto bottom;
	}
	h = headerRead(fdi,
		(lead.major >= 3) ?  HEADER_MAGIC_YES : HEADER_MAGIC_NO);
	if (h == NULL) {
	    ec++;
	    goto bottom;
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
		    fprintf(stderr, "%s: headerSprintf(%s): %s\n", __progname,
				ofn, errstr);
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
		    ec++;
		    goto bottom;
		}
	    }
if (_rpmtool_debug)
fprintf(stderr, "*** Fopen(%s,w.ufdio)\n", (ofn != NULL ? ofn : "-"));
	    fdo = (ofn != NULL ? Fopen(ofn, "w.ufdio") : fdDup(STDOUT_FILENO));
	    if (fdo == NULL || Ferror(fdo)) {
		fprintf(stderr, "%s: output Fopen(%s, \"w\"): %s\n", __progname,
			(ofn != NULL ? ofn : "<stdout>"), Fstrerror(fdo));
		ec++;
		goto bottom;
	    }
	}

	/* Write package components. */
	if (componentBits & RPMTOOL_LEAD)
	    writeLead(fdo, &lead);

	if (componentBits & RPMTOOL_SHEADER)
	    rpmWriteSignature(fdo, sigh);

	if (componentBits & RPMTOOL_HEADER) {
	    if ((componentBits & RPMTOOL_DUMP)
	     || (ofmt != NULL && !strcmp(ofmt, "dump"))) {
		headerDump(h, stdout, HEADER_DUMP_INLINE, rpmTagTable);
	    } else if ((componentBits & RPMTOOL_XML)
	     || (ofmt != NULL && !strcmp(ofmt, "xml"))) {
		const char * errstr = NULL;

		s = "[%{*:xml}\n]";
		t = headerSprintf(h, s, rpmTagTable, rpmHeaderFormats, &errstr);
		
		if (t != NULL)
		    Fwrite(t, strlen(t), 1, fdo);
		t = _free(t);
	    } else
		headerWrite(fdo, h, HEADER_MAGIC_YES);
	}

	if (componentBits & RPMTOOL_PAYLOAD) {
	    if (componentBits & RPMTOOL_UNCOMPRESS) {
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
		    fprintf(stderr, "%s: output Fdopen(%s, \"%s\"): %s\n",
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

    return ec;
}
