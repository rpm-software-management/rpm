#include "system.h"

#include <rpmlib.h>

const char *const tagName(int tag)
{
    int i;
    static char nameBuf[128];	/* XXX yuk */
    char *s;

    switch (tag) {
    case 0:
	strcpy(nameBuf, "Packages");
	return nameBuf;
	/*@notreached@*/ break;
    case 1:
	strcpy(nameBuf, "Depends");
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
	return 0;
    if (!strcmp(tagstr, "Depends"))
	return 1;

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (!strcasecmp(t->name + 7, tagstr))
	    return t->val;
    }
    return -1;
}
