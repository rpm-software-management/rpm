/**
 * \file lib/tagName.c
 */

#include "system.h"

#include <rpmlib.h>

const char *const tagName(int tag)
{
    int i;
    static char nameBuf[128];	/* XXX yuk */
    char *s;

    switch (tag) {
    case RPMDBI_PACKAGES:
	strcpy(nameBuf, "Packages");
	return nameBuf;
	/*@notreached@*/ break;
    case RPMDBI_DEPENDS:
	strcpy(nameBuf, "Depends");
	return nameBuf;
	/*@notreached@*/ break;
    case RPMDBI_ADDED:
	strcpy(nameBuf, "Added");
	return nameBuf;
	/*@notreached@*/ break;
    case RPMDBI_REMOVED:
	strcpy(nameBuf, "Removed");
	return nameBuf;
	/*@notreached@*/ break;
    case RPMDBI_AVAILABLE:
	strcpy(nameBuf, "Available");
	return nameBuf;
	/*@notreached@*/ break;
    }

    strcpy(nameBuf, "(unknown)");
    for (i = 0; i < rpmTagTableSize; i++) {
	if (tag != rpmTagTable[i].val)
	    continue;
	strcpy(nameBuf, rpmTagTable[i].name + 7);
	for (s = nameBuf+1; *s; s++)
	    *s = tolower(*s);
	break;
    }
    return nameBuf;
}

int tagValue(const char * tagstr)
{
    const struct headerTagTableEntry *t;

    if (!strcmp(tagstr, "Packages"))
	return RPMDBI_PACKAGES;
    if (!strcmp(tagstr, "Depends"))
	return RPMDBI_DEPENDS;
    if (!strcmp(tagstr, "Added"))
	return RPMDBI_ADDED;
    if (!strcmp(tagstr, "Removed"))
	return RPMDBI_REMOVED;
    if (!strcmp(tagstr, "Available"))
	return RPMDBI_AVAILABLE;

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (!strcasecmp(t->name + 7, tagstr))
	    return t->val;
    }
    return -1;
}
