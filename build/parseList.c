/** \ingroup rpmbuild
 * \file build/parseBuildInstallClean.c
 *  Parse %build/%install/%clean section from spec file.
 */
#include "system.h"

#include <rpm/rpmlog.h>
#include "build/rpmbuild_internal.h"
#include "debug.h"

static int addLinesFromFile(rpmSpec spec, const char * const fn, rpmTagVal tag)
{
    int nlines = 0;
    int flags = STRIP_COMMENTS | STRIP_TRAILINGSPACE;
    ARGV_t argv = NULL;

    int terminate = rpmExpandNumeric(tag == RPMTAG_SOURCE
				     ? "%{?_empty_sourcelist_terminate_build}"
				     : "%{?_empty_patchlist_terminate_build}");

    if (!terminate)
	flags |= ALLOW_EMPTY;


    nlines = readManifest(spec, fn, tag == RPMTAG_SOURCE ? "%sourcelist" : "%patchlist", flags, &argv, NULL);

    for (ARGV_t av = argv; *av; av++) {
	addSource(spec, 0, *av, tag);
    }

    argvFree(argv);

    if (nlines <= 0) {
	rpmlog(terminate ? RPMLOG_ERR : RPMLOG_WARNING,
	       _("Empty %s file %s\n"),
	       tag == RPMTAG_SOURCE ? "%sourcelist" : "%patchlist",
	       fn);
    }

    return (nlines < 0) ? RPMRC_FAIL : RPMRC_OK;
}

int parseList(rpmSpec spec, const char *name, rpmTagVal tag)
{
    int res = PART_ERROR;
    ARGV_t lst = NULL;
    int rc, argc = 0;
    int arg;
    const char **argv = NULL;
    poptContext optCon = NULL;
    struct poptOption optionsTable[] = {
	{ NULL, 'f', POPT_ARG_STRING, NULL, 'f', NULL, NULL},
	{ 0, 0, 0, 0, 0, NULL, NULL}
    };
    char * file = NULL;

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("line %d: Error parsing %%%s: %s\n"),
	       spec->lineNum, name, poptStrerror(rc));
	goto exit;
    }

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	;
    }

    if (arg < -1) {
	rpmlog(RPMLOG_ERR, _("line %d: Bad option %s: %s\n"),
	       spec->lineNum,
	       poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
	       spec->line);
	goto exit;
    }

    lst = argvNew();

    for (arg = 1; arg < argc; arg++) {
	if (rstreq(argv[arg], "-f")) {

	    if (!argv[arg+1]) {
		rpmlog(RPMLOG_ERR,
		       _("line %d: \"%%%s -f\" requires an argument.\n"),
		       spec->lineNum, name);
		goto exit;
	    }

	    addSource(spec, 0, argv[arg+1], RPMTAG_SOURCE);

	    if (argv[arg+1][0] == '/')
		file = rpmGetPath(argv[arg+1], NULL);
	    else
		file = rpmGetPath("%{_sourcedir}/", argv[arg+1], NULL);

	    res = addLinesFromFile(spec, file, tag);
	    if (res < 0) {
		rpmlog(RPMLOG_ERR, _("line %d: %%%s: Error parsing %s\n"),
		       spec->lineNum, name, argv[arg+1]);
		goto exit;
	    }
	}
    }

    if ((res = parseLines(spec, (STRIP_TRAILINGSPACE | STRIP_COMMENTS),
			  &lst, NULL)) == PART_ERROR) {
	goto exit;
    }

    for (ARGV_const_t l = lst; l && *l; l++) {
	if (rstreq(*l, ""))
	    continue;
	addSource(spec, 0, *l, tag);
    }

exit:
    free(file);
    argvFree(lst);
    free(argv);
    poptFreeContext(optCon);
    return res;
}
