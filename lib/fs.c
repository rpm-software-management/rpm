#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>

struct fsinfo {
    char * mntPoint;
    dev_t dev;
};

static struct fsinfo * filesystems;
static const char ** fsnames;
static int numFilesystems;

#if HAVE_MNTCTL

/* modeled after sample code from Till Bubeck */

#include <sys/mntctl.h>
#include <sys/vmount.h>

/* 
 * There is NO mntctl prototype in any header file of AIX 3.2.5! 
 * So we have to declare it by ourself...
 */
int mntctl(int command, int size, char *buffer);

static int getFilesystemList(void)
{
    int size;
    void * buf;
    struct vmount * vm;
    struct stat sb;
    int num;
    int fsnameLength;
    int i;

    num = mntctl(MCTL_QUERY, sizeof(size), (char *) &size);
    if (num < 0) {
	rpmError(RPMERR_MTAB, _("mntctl() failed to return fugger size: %s"), 
		 strerror(errno));
	return 1;
    }

    /*
     * Double the needed size, so that even when the user mounts a 
     * filesystem between the previous and the next call to mntctl
     * the buffer still is large enough.
     */
    size *= 2;

    buf = alloca(size);
    num = mntctl(MCTL_QUERY, size, buf);
    if ( num <= 0 ) {
        rpmError(RPMERR_MTAB, "mntctl() failed to return mount points: %s", 
		 strerror(errno));
	return 1;
    }

    numFilesystems = num;

    filesystems = malloc(sizeof(*filesystems) * (numFilesystems + 1));
    fsnames = malloc(sizeof(char *) * (numFilesystems + 1));
    
    for (vm = buf, i = 0; i < num; i++) {
	char *fsn;
	fsnameLength = vm->vmt_data[VMT_STUB].vmt_size;
	fsn = malloc(fsnameLength + 1);
	strncpy(fsn, (char *)vm + vm->vmt_data[VMT_STUB].vmt_off, 
		fsnameLength);

	filesystems[i].mntPoint = fsnames[i] = fsn;
	
	if (stat(filesystems[i].mntPoint, &sb)) {
	    rpmError(RPMERR_STAT, _("failed to stat %s: %s"), fsnames[i],
			strerror(errno));

	    for (i = 0; i < num; i++)
		free(filesystems[i].mntPoint);
	    free(filesystems);
	    free(fsnames);

	    filesystems = NULL;
	}
	
	filesystems[i].dev = sb.st_dev;

	/* goto the next vmount structure: */
	vm = (struct vmount *)((char *)vm + vm->vmt_length);
    }

    filesystems[i].mntPoint = NULL;
    fsnames[i]              = NULL;

    return 0;
}
#else 
static int getFilesystemList(void)
{
    int numAlloced = 10;
    int num = 0;
    struct stat sb;
    int i;
    char * mntdir;
#   if GETMNTENT_ONE || GETMNTENT_TWO
    our_mntent item, * itemptr;
    FILE * mtab;
#   elif HAVE_GETMNTINFO_R
    struct statfs * mounts = NULL;
    int mntCount = 0, bufSize = 0, flags = MNT_NOWAIT;
    int nextMount = 0;
#   endif

#   if GETMNTENT_ONE || GETMNTENT_TWO
	mtab = fopen(MOUNTED, "r");
	if (!mtab) {
	    rpmError(RPMERR_MTAB, _("failed to open %s: %s"), MOUNTED, 
		     strerror(errno));
	    return 1;
	}
#   elif HAVE_GETMNTINFO_R
	getmntinfo_r(&mounts, flags, &mntCount, &bufSize);
#   endif

    filesystems = malloc(sizeof(*filesystems) * (numAlloced + 1));

    while (1) {
#	if GETMNTENT_ONE
	    /* this is Linux */
	    itemptr = getmntent(mtab);
	    if (!itemptr) break;
	    item = *itemptr;
	    mntdir = item.our_mntdir;
#	elif GETMNTENT_TWO
	    /* Solaris, maybe others */
	    if (getmntent(mtab, &item)) break;
	    mntdir = item.our_mntdir;
#	elif HAVE_GETMNTINFO_R
	    if (nextMount == mntCount) break;
	    mntdir = mounts[nextMount++].f_mntonname;
#	endif

	if (stat(mntdir, &sb)) {
	    rpmError(RPMERR_STAT, "failed to stat %s: %s", mntdir,
			strerror(errno));

	    for (i = 0; i < num; i++)
		free(filesystems[i].mntPoint);
	    free(filesystems);

	    filesystems = NULL;
	    return 1;
	}

	if (num == numAlloced) {
	    numAlloced += 10;
	    filesystems = realloc(filesystems, 
				  sizeof(*filesystems) * (numAlloced + 1));
	}

	filesystems[num].dev = sb.st_dev;
	filesystems[num++].mntPoint = strdup(mntdir);
    }

#   if GETMNTENT_ONE || GETMNTENT_TWO
	fclose(mtab);
#   elif HAVE_GETMNTINFO_R
	free(mounts);
#   endif

    filesystems[num].mntPoint = NULL;

    fsnames = malloc(sizeof(*fsnames) * (num + 1));
    for (i = 0; i < num; i++)
	fsnames[i] = filesystems[i].mntPoint;
    fsnames[num] = NULL;

    numFilesystems = num;

    return 0; 
}
#endif

int rpmGetFilesystemList(const char *** listptr, int * num)
{
    if (!fsnames) 
	if (getFilesystemList())
	    return 1;

    if (listptr) *listptr = fsnames;
    if (num) *num = numFilesystems;

    return 0;
}

int rpmGetFilesystemUsage(const char ** fileList, int_32 * fssizes, int numFiles,
			  uint_32 ** usagesPtr, int flags)
{
    int_32 * usages;
    int i, len, j;
    char * buf, * dirName;
    char * chptr;
    int maxLen;
    char * lastDir;
    const char * sourceDir;
    int lastfs = 0;
    int lastDev = -1;		/* I hope nobody uses -1 for a st_dev */
    struct stat sb;

    if (!fsnames) 
	if (getFilesystemList())
	    return 1;

    usages = calloc(numFilesystems, sizeof(usages));

    sourceDir = rpmGetPath("", "/%{_sourcedir}", NULL);

    maxLen = strlen(sourceDir);
    for (i = 0; i < numFiles; i++) {
	len = strlen(fileList[i]);
	if (maxLen < len) maxLen = len;
    }
    
    buf = alloca(maxLen + 1);
    lastDir = alloca(maxLen + 1);
    dirName = alloca(maxLen + 1);
    *lastDir = '\0';

    /* cut off last filename */
    for (i = 0; i < numFiles; i++) {
	if (*fileList[i] == '/') {
	    strcpy(buf, fileList[i]);
	    chptr = buf + strlen(buf) - 1;
	    while (*chptr != '/') chptr--;
	    if (chptr == buf)
		buf[1] = '\0';
	    else
		*chptr-- = '\0';
	} else {
	    /* this should only happen for source packages (gulp) */
	    strcpy(buf,  sourceDir);
	}

	if (strcmp(lastDir, buf)) {
	    strcpy(dirName, buf);
	    chptr = dirName + strlen(dirName) - 1;
	    while (stat(dirName, &sb)) {
		if (errno != ENOENT) {
		    rpmError(RPMERR_STAT, _("failed to stat %s: %s"), buf,
				strerror(errno));
		    free(usages);
		    return 1;
		}

		/* cut off last directory part, because it was not found. */
		while (*chptr != '/') chptr--;

		if (chptr == dirName)
		    dirName[1] = '\0';
		else
		    *chptr-- = '\0';
	    }

	    if (lastDev != sb.st_dev) {
		for (j = 0; j < numFilesystems; j++)
		    if (filesystems[j].dev == sb.st_dev) break;

		if (j == numFilesystems) {
		    rpmError(RPMERR_BADDEV, 
				_("file %s is on an unknown device"), buf);
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

    if (sourceDir) xfree(sourceDir);

    *usagesPtr = usages;

    return 0;
}
