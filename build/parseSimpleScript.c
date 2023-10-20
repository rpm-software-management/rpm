/** \ingroup rpmbuild
 * \file build/parseBuildInstallClean.c
 *  Parse %build/%install/%clean section from spec file.
 */
#include "system.h"

#include <rpm/rpmlog.h>
#include "rpmbuild_internal.h"
#include "debug.h"


int parseSimpleScript(rpmSpec spec, const char * name, StringBuf *sbp)
{
    int res = PART_ERROR;
    poptContext optCon = NULL;
    int argc;
    const char **argv = NULL;
    StringBuf *target = sbp;
    StringBuf buf = NULL;
    int rc, append = 0, prepend = 0;
    struct poptOption optionsTable[] = {
	{ NULL, 'a', POPT_ARG_NONE, &append, 'a', NULL, NULL },
	{ NULL, 'p', POPT_ARG_NONE, &prepend, 'p', NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL }
    };


    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("line %d: Error parsing script: %s: %s\n"),
		spec->lineNum, spec->line, poptStrerror(rc));
	goto exit;
    }

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((rc = poptGetNextOpt(optCon)) > 0) {};
    if (rc < -1) {
	rpmlog(RPMLOG_ERR, _("line %d: Bad %s option %s: %s\n"),
		spec->lineNum, argv[0],
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(rc));
	goto exit;
    }

    if (*sbp != NULL && append == 0 && prepend == 0) {
	rpmlog(RPMLOG_ERR, _("line %d: second %s\n"),
		spec->lineNum, name);
	goto exit;
    }

    if (append && prepend) {
	rpmlog(RPMLOG_ERR, _("line %d: append and prepend specified: %s\n"),
		spec->lineNum, spec->line);
	goto exit;
    }

    /* Prepend is only special if the section already exists */
    if (prepend && *sbp) {
	buf = newStringBuf();
	target = &buf;
    }

    res = parseLines(spec, STRIP_NOTHING, NULL, target);

    if (buf) {
	appendStringBuf(buf, getStringBuf(*sbp));
	freeStringBuf(*sbp);
	*sbp = buf;
    }

exit:
    free(argv);
    poptFreeContext(optCon);

    return res;
}
