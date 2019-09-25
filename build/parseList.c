/** \ingroup rpmbuild
 * \file build/parseBuildInstallClean.c
 *  Parse %build/%install/%clean section from spec file.
 */
#include "system.h"

#include <rpm/rpmlog.h>
#include "build/rpmbuild_internal.h"
#include "debug.h"

static int addLinesFromFile(rpmSpec spec, const char * const file, rpmTagVal tag)
{
    char buf[PATH_MAX+1], *c;
    FILE *f;
    int res = 0;

    f = fopen(file, "r");
    if (f == NULL)
	return -1;

    buf[PATH_MAX] = '\0';

    do {
	c = fgets(buf, PATH_MAX+1, f);
	if (c == NULL)
	    continue;

	c = strchrnul(buf, '\n');
	*c = '\0';

	addSource(spec, 0, buf, tag);
    } while (c);

    if (!c && ferror(f))
	res = -1;

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
	if (rstreq(argv[arg], "-f") && argv[arg+1]) {
	    char * const file = rpmGetPath(argv[arg+1], NULL);
	    addSource(spec, 0, file, RPMTAG_SOURCE);
	    res = addLinesFromFile(spec, file, tag);
	    free(file);
	    if (res < 0) {
		rpmlog(RPMLOG_ERR, _("line %d: Error parsing %%%s\n"),
		       spec->lineNum, file);
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
    argvFree(lst);
    free(argv);
    poptFreeContext(optCon);
    return res;
}
