/** \ingroup rpmbuild
 * \file build/parseBuildInstallClean.c
 *  Parse %build/%install/%clean section from spec file.
 */
#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

int parseBuildInstallClean(Spec spec, rpmParseState parsePart)
{
    int nextPart, rc;
    StringBuf *sbp = NULL;
    const char *name = NULL;

    if (parsePart == PART_BUILD) {
	sbp = &(spec->build);
	name = "%build";
    } else if (parsePart == PART_INSTALL) {
	sbp = &(spec->install);
	name = "%install";
    } else if (parsePart == PART_CLEAN) {
	sbp = &(spec->clean);
	name = "%clean";
    }
    
    if (*sbp != NULL) {
	rpmError(RPMERR_BADSPEC, _("line %d: second %s"), spec->lineNum, name);
	return RPMERR_BADSPEC;
    }
    
    *sbp = newStringBuf();

    /* There are no options to %build, %install, or %clean */
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0)
	return PART_NONE;
    if (rc)
	return rc;
    
    while (! (nextPart = isPart(spec->line))) {
	appendStringBuf(*sbp, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0)
	    return PART_NONE;
	if (rc)
	    return rc;
    }

    return nextPart;
}
