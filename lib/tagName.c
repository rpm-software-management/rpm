#include "system.h"

#include <rpmlib.h>

const char *tagName(int tag)
{
    int i;
    static char nameBuf[128];	/* XXX yuk */
    char *s;

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
