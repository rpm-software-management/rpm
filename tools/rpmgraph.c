#include "system.h"

#include <string.h>
#include <glob.h>

#include <rpm/rpmcli.h>
#include <rpm/rpmlib.h>		/* rpmReadPackageFile */
#include <rpm/rpmdb.h>
#include <rpm/rpmps.h>
#include <rpm/rpmte.h>
#include <rpm/rpmts.h>
#include <rpm/rpmds.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>

#include "lib/manifest.h"
#include "debug.h"

static int noDeps = 1;

static rpmVSFlags vsflags = 0;

static int
rpmGraph(rpmts ts, struct rpmInstallArguments_s * ia, const char ** fileArgv)
{
    char ** pkgURL = NULL;
    char * pkgState = NULL;
    const char ** fnp;
    char * fileURL = NULL;
    int numPkgs = 0;
    int numFailed = 0;
    int prevx = 0;
    int pkgx = 0;
    char ** argv = NULL;
    int argc = 0;
    char ** av = NULL;
    int ac = 0;
    Header h;
    rpmRC rpmrc;
    int rc = 0;
    rpmVSFlags ovsflags;
    int i;

    if (fileArgv == NULL)
	return 0;

    /* Build fully globbed list of arguments in argv[argc]. */
    for (fnp = fileArgv; *fnp; fnp++) {
	av = _free(av);
	ac = 0;
	rc = rpmGlob(*fnp, GLOB_NOMAGIC, &ac, &av);
	if (rc || ac == 0) continue;

	argv = xrealloc(argv, (argc+2) * sizeof(*argv));
	memcpy(argv+argc, av, ac * sizeof(*av));
	argc += ac;
	argv[argc] = NULL;
    }
    av = _free(av);	ac = 0;

restart:
    /* Allocate sufficient storage for next set of args. */
    if (pkgx >= numPkgs) {
	numPkgs = pkgx + argc;
	pkgURL = xrealloc(pkgURL, (numPkgs + 1) * sizeof(*pkgURL));
	memset(pkgURL + pkgx, 0, ((argc + 1) * sizeof(*pkgURL)));
	pkgState = xrealloc(pkgState, (numPkgs + 1) * sizeof(*pkgState));
	memset(pkgState + pkgx, 0, ((argc + 1) * sizeof(*pkgState)));
    }

    /* Copy next set of args. */
    for (i = 0; i < argc; i++) {
	fileURL = _free(fileURL);
	fileURL = argv[i];
	argv[i] = NULL;

	pkgURL[pkgx] = fileURL;
	fileURL = NULL;
	pkgx++;
    }
    fileURL = _free(fileURL);

    /* Continue processing file arguments, building transaction set. */
    for (fnp = (const char **) pkgURL+prevx; *fnp != NULL; fnp++, prevx++) {
	const char * fileName;
	FD_t fd;

	(void) urlPath(*fnp, &fileName);

	/* Try to read the header from a package file. */
	fd = Fopen(*fnp, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) {
		Fclose(fd);
		fd = NULL;
	    }
	    numFailed++; *fnp = NULL;
	    continue;
	}

	/* Read the header, verifying signatures (if present). */
	ovsflags = rpmtsSetVSFlags(ts, vsflags);
	rpmrc = rpmReadPackageFile(ts, fd, *fnp, &h);
	rpmtsSetVSFlags(ts, ovsflags);
	Fclose(fd);
	fd = NULL;

	switch (rpmrc) {
	case RPMRC_FAIL:
	default:
	    rpmlog(RPMLOG_ERR, _("%s cannot be installed\n"), *fnp);
	    numFailed++; *fnp = NULL;
	    break;
	case RPMRC_OK:
	    rc = rpmtsAddInstallElement(ts, h, (fnpyKey)fileName, 0, NULL);
	    break;
	case RPMRC_NOTFOUND:
	    goto maybe_manifest;
	    break;
	}
	h = headerFree(h); 
	continue;

maybe_manifest:
	/* Try to read a package manifest. */
	fd = Fopen(*fnp, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) {
		Fclose(fd);
		fd = NULL;
	    }
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Read list of packages from manifest. */
	rc = rpmReadPackageManifest(fd, &argc, &argv);
	if (rc)
	    rpmlog(RPMLOG_NOTICE, _("%s: read manifest failed: %s\n"),
			fileURL, Fstrerror(fd));
	Fclose(fd);
	fd = NULL;

	/* If successful, restart the query loop. */
	if (rc == 0) {
	    prevx++;
	    goto restart;
	}

	numFailed++; *fnp = NULL;
	break;
    }

    if (numFailed > 0) goto exit;

    if (!noDeps) {
	rpmps ps;
	rc = rpmtsCheck(ts);
	if (rc) {
	    numFailed += numPkgs;
	    goto exit;
	}
	ps = rpmtsProblems(ts);
	if (rpmpsNumProblems(ps) > 0) {
	    rpmlog(RPMLOG_ERR, _("Failed dependencies:\n"));
	    rpmpsPrint(NULL, ps);
	    numFailed += numPkgs;
	}
	rpmpsFree(ps);
    }

    rc = rpmtsOrder(ts);
    if (rc)
	goto exit;

    {	rpmtsi pi;
	rpmte p;
	rpmte q;
	int oType = TR_ADDED;

	fprintf(stdout, "digraph XXX {\n");

	fprintf(stdout, "  rankdir=LR\n");

	fprintf(stdout, "//===== Packages:\n");
	pi = rpmtsiInit(ts);
	while ((p = rpmtsiNext(pi, oType)) != NULL) {
	    q = rpmteParent(p);
	    if (q != NULL)
		fprintf(stdout, "  \"%s\" -> \"%s\"\n", rpmteN(p), rpmteN(q));
	    else {
		fprintf(stdout, "  \"%s\"\n", rpmteN(p));
		fprintf(stdout, "  { rank=max ; \"%s\" }\n", rpmteN(p));
	    }
	}
	rpmtsiFree(pi);

	fprintf(stdout, "}\n");
    }

    rc = 0;

exit:
    for (i = 0; i < numPkgs; i++)
        pkgURL[i] = _free(pkgURL[i]);
    pkgState = _free(pkgState);
    pkgURL = _free(pkgURL);
    argv = _free(argv);

    return rc;
}

static struct poptOption optionsTable[] = {
 { "check", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noDeps, 0,
	N_("don't verify package dependencies"), NULL },
 { "nolegacy", '\0', POPT_BIT_SET,	&vsflags, RPMVSF_NEEDPAYLOAD,
        N_("don't verify header+payload signature"), NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL }, 

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmts ts = NULL;
    struct rpmInstallArguments_s * ia = &rpmIArgs;
    poptContext optCon;
    int ec = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	exit(EXIT_FAILURE);

    ts = rpmtsCreate();
    vsflags |= rpmcliVSFlags;
    (void) rpmtsSetVSFlags(ts, vsflags);

    ec = rpmGraph(ts, ia, poptGetArgs(optCon));

    rpmtsFree(ts);

    rpmcliFini(optCon);

    return ec;
}
