#include "system.h"

#ifdef __aix__
#define COMMENTCHAR '*'
#else
#define COMMENTCHAR '#'
#endif

#if HAVE_STRUCT_MNTTAB 
our_mntent * getmntent(FILE *filep) {
    static struct mnttab entry;
    static our_mntent item;

    if (!fread(&entry, sizeof(entry), 1, filep)) return NULL;
    item.our_mntdir = entry.mt_filsys;

    return &item;
}
#else 
our_mntent *getmntent(FILE *filep) {
    static our_mntent item = { NULL };
    char buf[1024], * start;
    char * chptr;

    if (item.our_mntdir) {
	free(item.our_mntdir);
    }
    
    while (fgets(buf, sizeof(buf) - 1, filep)) {
	/* chop off \n */
	buf[strlen(buf) - 1] = '\0';

	chptr = buf;
	while (isspace(*chptr)) chptr++;

	if (*chptr == COMMENTCHAR) continue;

#	if __aix__
	    /* aix uses a screwed up file format */
	    if (*chptr == '/') {
		start = chptr;
		while (*chptr != ':') chptr++;
		*chptr = '\0';
		item.mnt_dir = strdup(start);
		return &item;
	    }
#	else 
	    while (!isspace(*chptr) && (*chptr)) chptr++;
	    if (!*chptr) return NULL;

	    while (isspace(*chptr) && (*chptr)) chptr++;
	    if (!*chptr) return NULL;
	    start = chptr;
	
	    while (!isspace(*chptr) && (*chptr)) chptr++;
	    *chptr = '\0';

	    item.our_mntdir = strdup(start);
	    return &item;
#	endif
    }

    return NULL;
}
#endif

