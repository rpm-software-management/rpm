/**
 * \file rpmdb/tagname.c
 */

#include "system.h"

#include <rpmlib.h>
#include "debug.h"

const char *const tagName(int tag)
{
    int i;
    static char nameBuf[128];	/* XXX yuk */
    char *s;

    switch (tag) {
    case RPMDBI_PACKAGES:
	strcpy(nameBuf, "Packages");
	break;
    case RPMDBI_DEPENDS:
	strcpy(nameBuf, "Depends");
	break;
    case RPMDBI_ADDED:
	strcpy(nameBuf, "Added");
	break;
    case RPMDBI_REMOVED:
	strcpy(nameBuf, "Removed");
	break;
    case RPMDBI_AVAILABLE:
	strcpy(nameBuf, "Available");
	break;
    case RPMDBI_HDLIST:
	strcpy(nameBuf, "Hdlist");
	break;
    case RPMDBI_ARGLIST:
	strcpy(nameBuf, "Arglist");
	break;
    case RPMDBI_FTSWALK:
	strcpy(nameBuf, "Ftswalk");
	break;
    default:
	strcpy(nameBuf, "(unknown)");
/*@-boundswrite@*/
	for (i = 0; i < rpmTagTableSize; i++) {
	    if (tag != rpmTagTable[i].val)
		continue;
	    nameBuf[0] = nameBuf[1] = '\0';
	    if (rpmTagTable[i].name != NULL)	/* XXX programmer error. */
		strcpy(nameBuf, rpmTagTable[i].name + (sizeof("RPMTAG_")-1));
	    for (s = nameBuf+1; *s != '\0'; s++)
		*s = xtolower(*s);
	    /*@loopbreak@*/ break;
	}
/*@=boundswrite@*/
	break;
    }
    return nameBuf;
}

int tagValue(const char * tagstr)
{
    const struct headerTagTableEntry_s *t;

    if (!xstrcasecmp(tagstr, "Packages"))
	return RPMDBI_PACKAGES;
    if (!xstrcasecmp(tagstr, "Depends"))
	return RPMDBI_DEPENDS;
    if (!xstrcasecmp(tagstr, "Added"))
	return RPMDBI_ADDED;
    if (!xstrcasecmp(tagstr, "Removed"))
	return RPMDBI_REMOVED;
    if (!xstrcasecmp(tagstr, "Available"))
	return RPMDBI_AVAILABLE;
    if (!xstrcasecmp(tagstr, "Hdlist"))
	return RPMDBI_HDLIST;
    if (!xstrcasecmp(tagstr, "Arglist"))
	return RPMDBI_ARGLIST;
    if (!xstrcasecmp(tagstr, "Ftswalk"))
	return RPMDBI_FTSWALK;

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (!xstrcasecmp(t->name + (sizeof("RPMTAG_")-1), tagstr))
	    return t->val;
    }
    return -1;
}
