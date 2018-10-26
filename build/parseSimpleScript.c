/** \ingroup rpmbuild
 * \file build/parseBuildInstallClean.c
 *  Parse %build/%install/%clean section from spec file.
 */
#include "system.h"

#include <rpm/rpmlog.h>
#include "build/rpmbuild_internal.h"
#include "debug.h"


int parseSimpleScript(rpmSpec spec, const char * name, StringBuf *sbp)
{
    int nextPart, rc, res = PART_ERROR;
    
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
