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
    char buf[PATH_MAX+1];
    char *expanded;
    FILE *f;
    int res = -1;
    int lineno = 0, nlines = 0;

    f = fopen(fn, "r");
    if (f == NULL)
	return -1;

    buf[PATH_MAX] = '\0';

    while (fgets(buf, sizeof(buf), f)) {
	char *c;
	lineno++;

	c = strrchr(buf, '\n');
	if (c)
	    *c = '\0';

	if (handleComments(buf))
	    continue;
	if (specExpand(spec, lineno, buf, &expanded))
	    goto exit;

	addSource(spec, 0, expanded, tag);
	free(expanded);
	nlines++;
    }

    if (nlines == 0) {
	int terminate =
                rpmExpandNumeric(tag == RPMTAG_SOURCE
				 ? "%{?_empty_sourcelist_terminate_build}"
				 : "%{?_empyy_patchlist_terminate_build}");
        rpmlog(terminate ? RPMLOG_ERR : RPMLOG_WARNING,
               _("Empty %s file %s\n"),
	       tag == RPMTAG_SOURCE ? "%sourcelist" : "%patchlist",
	       fn);
        if (terminate)
                goto exit;
    }

    if (ferror(f))
        rpmlog(RPMLOG_ERR, _("Error reading %s file %s: %m\n"),
	       tag == RPMTAG_SOURCE ? "%sourcelist" : "%patchlist",
	       fn);
    else
	res = 0;

exit:
    fclose(f);
    return res;
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
