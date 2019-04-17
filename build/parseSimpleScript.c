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
    int res = PART_ERROR;
    
    if (*sbp != NULL) {
	rpmlog(RPMLOG_ERR, _("line %d: second %s\n"),
		spec->lineNum, name);
	goto exit;
    }
    
    /* There are no options to %build, %install, %check, or %clean */
    res = parseLines(spec, STRIP_NOTHING, NULL, sbp);
    
exit:

    return res;
}
