/** \ingroup rpmbuild
 * \file build/parseBuildInstallClean.c
 *  Parse %build/%install/%clean section from spec file.
 */
#include "system.h"

#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include "debug.h"


int parseBuildInstallClean(rpmSpec spec, rpmParseState parsePart)
{
    int nextPart, rc, res = PART_ERROR;
    StringBuf *sbp = NULL;
    const char *name = NULL;

    if (parsePart == PART_BUILD) {
	sbp = &(spec->build);
	name = "%build";
    } else if (parsePart == PART_INSTALL) {
	sbp = &(spec->install);
	name = "%install";
    } else if (parsePart == PART_CHECK) {
	sbp = &(spec->check);
	name = "%check";
    } else if (parsePart == PART_CLEAN) {
	sbp = &(spec->clean);
	name = "%clean";
    } else {
	goto exit; /* programmer error */
    }
    
    if (*sbp != NULL) {
	rpmlog(RPMLOG_ERR, _("line %d: second %s\n"),
		spec->lineNum, name);
	goto exit;
    }
    
    *sbp = newStringBuf();

    /* There are no options to %build, %install, %check, or %clean */
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	res = PART_NONE;
	goto exit;
    } else if (rc < 0) {
	goto exit;
    }
    
    while (! (nextPart = isPart(spec->line))) {
	appendStringBuf(*sbp, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	    nextPart = PART_NONE;
	    break;
	} else if (rc < 0) {
	    goto exit;
	}
    }
    res = nextPart;
    
exit:

    return res;
}
