#include "miscfn.h"

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "header.h"
#include "intl.h"
#include "rpmlib.h"

struct fsinfo {
    char * mntPoint;
    dev_t dev;
};

static struct fsinfo * filesystems;
static char ** fsnames;
static int numFilesystems;

static int getFilesystemList(void);

#if HAVE_MNTCTL

/* modeled after sample code from Till Bubeck */

#include <sys/mntctl.h>
#include <sys/vmount.h>

static int getFilesystemList(void) {
    int size;
    void * buf;
    struct vmount * vm;
    int num;
    int fsnameLength;
    int i;

    num = mntctl(MCTL_QUERY, sizeof(size), (char *) &size);
    if (num < 0) {
	rpmError(RPMERR_MTAB, _("mntctl() failed to return fugger size: %s"), 
		 strerror(errno));
	return 1;
    }

    buf = alloca(size);
    num = mntctl(MCTL_QUERY, size, buf);

    numFilesystems = num;

    filesystems = malloc(sizeof(*filesystems) * numFilesystems);
    fsnames = malloc(sizeof(*filesystems) * numFilesystems);
    
    for (vm = buf, i = 0; i < num; i++) {
	fsnameLength = vm->vmt_data[VMT_STUB].vmt_size;
	fsnames[i] = malloc(fsnameLength + 1);
	strncpy(fsnames[i],(char *)vm + vm->vmt_data[VMT_STUB].vmt_off, 
		fsname_len);

	filesystems[i].mntPoint = fsnames[i];

	if (stat(fsnames[i], &sb)) {
	    rpmError(RPMERR_STAT, "failed to stat %s: %s", fsnames[i],
			strerror(errno));

	    for (i = 0; i < num; i++)
		free(filesystems[i].mntPoint);
	    free(filesystems);
	    free(fsnameS);

	    filesystems = NULL;
	}
	
	filesystems[num].dev = sb.st_dev;

	/* goto the next vmount structure: */
	vm = (struct vmount *)((char *)vm + vm->vmt_length);
    }

    return 0;
}
#else 
static int getFilesystemList(void) {
    our_mntent item, * itemptr;
    FILE * mtab;
    int numAlloced = 10;
    int num = 0;
    struct stat sb;
    int i;

    mtab = fopen(MOUNTED, "r");
    if (!mtab) {
	rpmError(RPMERR_MTAB, _("failed to open %s: %s"), MOUNTED, 
		 strerror(errno));
	return 1;
    }

    filesystems = malloc(sizeof(*filesystems) * (numAlloced + 1));

    while (1) {
	#if GETMNTENT_ONE
	    /* this is Linux */
	    itemptr = getmntent(mtab);
	    if (!itemptr) break;
	    item = *itemptr;
	#elif GETMNTENT_TWO
	    /* Solaris, maybe others */
	    if (getmntent(mtab, &item)) break;
	#endif

	if (stat(item.our_mntdir, &sb)) {
	    rpmError(RPMERR_STAT, "failed to stat %s: %s", item.our_mntdir,
			strerror(errno));

	    for (i = 0; i < num; i++)
		free(filesystems[i].mntPoint);
	    free(filesystems);

	    filesystems = NULL;
	}

	if (num == numAlloced) {
	    numAlloced += 10;
	    filesystems = realloc(filesystems, 
				  sizeof(*filesystems) * (numAlloced + 1));
	}

	filesystems[num].dev = sb.st_dev;
	filesystems[num++].mntPoint = strdup(item.our_mntdir);
    }

    fclose(mtab);
    filesystems[num].mntPoint = NULL;

    fsnames = malloc(sizeof(*fsnames) * (num + 1));
    for (i = 0; i < num; i++)
	fsnames[i] = filesystems[i].mntPoint;
    fsnames[num] = NULL;

    numFilesystems = num;

    return 0; 
}
#endif

int rpmGetFilesystemList(char *** listptr) {
    if (!fsnames) 
	if (getFilesystemList())
	    return 1;

    *listptr = fsnames;
    return 0;
}

int rpmGetFilesystemUsage(char ** fileList, int_32 * fssizes, int numFiles,
			  uint_32 ** usagesPtr, int flags) {
    int_32 * usages;
    int i, len, j;
    char * buf, * dirName;
    char * chptr;
    int maxLen;
    char * lastDir;
    int lastfs = 0;
    int lastDev = -1;		/* I hope nobody uses -1 for a st_dev */
    struct stat sb;

    if (!fsnames) 
	if (getFilesystemList())
	    return 1;

    usages = calloc(numFilesystems, sizeof(usages));

    maxLen = 0;
    for (i = 0; i < numFiles; i++) {
	len = strlen(fileList[i]);
	if (maxLen < len) maxLen = len;
    }
    
    buf = alloca(maxLen + 1);
    lastDir = alloca(maxLen + 1);
    dirName = alloca(maxLen + 1);
    *lastDir = '\0';

    for (i = 0; i < numFiles; i++) {
	strcpy(buf, fileList[i]);
	chptr = buf + strlen(buf) - 1;
	while (*chptr != '/') chptr--;
	if (chptr == buf)
	    buf[1] = '\0';
	else
	    *chptr-- = '\0';

	if (strcmp(lastDir, buf)) {
	    strcpy(dirName, buf);
	    chptr = dirName + strlen(dirName) - 1;
	    while (stat(dirName, &sb)) {
		if (errno != ENOENT) {
		    rpmError(RPMERR_STAT, "failed to stat %s: %s", buf,
				strerror(errno));
		    free(usages);
		    return 1;
		}

		while (*chptr != '/') chptr--;
		*chptr-- = '\0';
	    }

	    if (lastDev != sb.st_dev) {
		for (j = 0; j < numFilesystems; j++)
		    if (filesystems[j].dev == sb.st_dev) break;

		if (j == numFilesystems) {
		    rpmError(RPMERR_BADDEV, 
				"file %s is on an unknown device", buf);
		    free(usages);
		    return 1;
		}

		lastfs = j;
		lastDev = sb.st_dev;
	    }
	}

	strcpy(lastDir, buf);
	usages[lastfs] += fssizes[i];
    }

    *usagesPtr = usages;

    return 0;
}
