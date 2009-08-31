/**
 * \file lib/fs.c
 */

#include "system.h"

#include <rpm/rpmlib.h>		/* rpmGetFilesystem*() prototypes */
#include <rpm/rpmfileutil.h>	/* for rpmGetPath */
#include <rpm/rpmlog.h>

#include "debug.h"


struct fsinfo {
    char * mntPoint;		/*!< path to mount point. */
    dev_t dev;			/*!< devno for mount point. */
    int rdonly;			/*!< is mount point read only? */
};

static struct fsinfo * filesystems = NULL;
static const char ** fsnames = NULL;
static int numFilesystems = 0;

void rpmFreeFilesystems(void)
{
    int i;

    if (filesystems)
    for (i = 0; i < numFilesystems; i++)
	filesystems[i].mntPoint = _free(filesystems[i].mntPoint);

    filesystems = _free(filesystems);
    fsnames = _free(fsnames);
    numFilesystems = 0;
}

#if HAVE_MNTCTL

/* modeled after sample code from Till Bubeck */

#include <sys/mntctl.h>
#include <sys/vmount.h>

/* 
 * There is NO mntctl prototype in any header file of AIX 3.2.5! 
 * So we have to declare it by ourself...
 */
int mntctl(int command, int size, char *buffer);

/**
 * Get information for mounted file systems.
 * @todo determine rdonly for non-linux file systems.
 * @return		0 on success, 1 on error
 */
static int getFilesystemList(void)
{
    int size;
    void * buf = NULL;
    struct vmount * vm;
    struct stat sb;
    int rdonly = 0;
    int num;
    int fsnameLength;
    int i;
    int rc = 1; /* assume failure */

    num = mntctl(MCTL_QUERY, sizeof(size), (char *) &size);
    if (num < 0) {
	rpmlog(RPMLOG_ERR, _("mntctl() failed to return size: %s\n"), 
		 strerror(errno));
	goto exit;
    }

    /*
     * Double the needed size, so that even when the user mounts a 
     * filesystem between the previous and the next call to mntctl
     * the buffer still is large enough.
     */
    size *= 2;

    buf = xmalloc(size);
    num = mntctl(MCTL_QUERY, size, buf);
    if ( num <= 0 ) {
        rpmlog(RPMLOG_ERR, _("mntctl() failed to return mount points: %s\n"), 
		 strerror(errno));
	goto exit;
    }

    numFilesystems = num;

    filesystems = xcalloc((numFilesystems + 1), sizeof(*filesystems));
    fsnames = xcalloc((numFilesystems + 1), sizeof(char *));
    
    for (vm = buf, i = 0; i < num; i++) {
	char *fsn;
	fsnameLength = vm->vmt_data[VMT_STUB].vmt_size;
	fsn = xmalloc(fsnameLength + 1);
	strncpy(fsn, (char *)vm + vm->vmt_data[VMT_STUB].vmt_off, 
		fsnameLength);
	fsn[fsnameLength] = '\0';

	filesystems[i].mntPoint = fsnames[i] = fsn;
	
	if (stat(filesystems[i].mntPoint, &sb)) {
	    switch (errno) {
	    case EACCES: /* fuse mount */
	    case ESTALE: 
		continue;
	    default:
	    	rpmlog(RPMLOG_ERR, _("failed to stat %s: %s\n"), fsnames[i],
			strerror(errno));

	    	rpmFreeFilesystems();
	    	goto exit;
	    }
	}
	
	filesystems[i].dev = sb.st_dev;
	filesystems[i].rdonly = rdonly;

	/* goto the next vmount structure: */
	vm = (struct vmount *)((char *)vm + vm->vmt_length);
    }

    filesystems[i].mntPoint = NULL;
    fsnames[i]              = NULL;
    rc = 0;

exit:
    free(buf);

    return rc;
}

#else	/* HAVE_MNTCTL */

/**
 * Get information for mounted file systems.
 * @todo determine rdonly for non-linux file systems.
 * @return		0 on success, 1 on error
 */
static int getFilesystemList(void)
{
    int numAlloced = 10;
    struct stat sb;
    int i;
    const char * mntdir;
    int rdonly = 0;

#   if GETMNTENT_ONE || GETMNTENT_TWO
    our_mntent item;
    FILE * mtab;

	mtab = fopen(MOUNTED, "r");
	if (!mtab) {
	    rpmlog(RPMLOG_ERR, _("failed to open %s: %s\n"), MOUNTED, 
		     strerror(errno));
	    return 1;
	}
#   elif HAVE_GETMNTINFO_R
    /* This is OSF */
    struct statfs * mounts = NULL;
    int mntCount = 0, bufSize = 0, flags = MNT_NOWAIT;
    int nextMount = 0;

	getmntinfo_r(&mounts, flags, &mntCount, &bufSize);
#   elif HAVE_GETMNTINFO
    /* This is Mac OS X */
    struct statfs * mounts = NULL;
    int mntCount = 0, flags = MNT_NOWAIT;
    int nextMount = 0;

	/* XXX 0 on error, errno set */
	mntCount = getmntinfo(&mounts, flags);
#   endif

    filesystems = xcalloc((numAlloced + 1), sizeof(*filesystems));	/* XXX memory leak */

    numFilesystems = 0;
    while (1) {
#	if GETMNTENT_ONE
	    /* this is Linux */
	    our_mntent * itemptr = getmntent(mtab);
	    if (!itemptr) break;
	    item = *itemptr;	/* structure assignment */
	    mntdir = item.our_mntdir;
#if defined(MNTOPT_RO)
	    if (hasmntopt(itemptr, MNTOPT_RO) != NULL)
		rdonly = 1;
#endif
#	elif GETMNTENT_TWO
	    /* Solaris, maybe others */
	    if (getmntent(mtab, &item)) break;
	    mntdir = item.our_mntdir;
#	elif HAVE_GETMNTINFO_R
	    /* This is OSF */
	    if (nextMount == mntCount) break;
	    mntdir = mounts[nextMount++].f_mntonname;
#	elif HAVE_GETMNTINFO
	    /* This is Mac OS X */
	    if (nextMount == mntCount) break;
	    mntdir = mounts[nextMount++].f_mntonname;
#	endif

	if (stat(mntdir, &sb)) {
	    switch (errno) {
	    case ESTALE:
	    case EACCES:
		continue;
	    default:
	        rpmlog(RPMLOG_ERR, _("failed to stat %s: %s\n"), mntdir,
			strerror(errno));
	        rpmFreeFilesystems();
	        return 1;
	    }
	}

	if ((numFilesystems + 2) == numAlloced) {
	    numAlloced += 10;
	    filesystems = xrealloc(filesystems, 
				  sizeof(*filesystems) * (numAlloced + 1));
	}

	filesystems[numFilesystems].dev = sb.st_dev;
	filesystems[numFilesystems].mntPoint = xstrdup(mntdir);
	filesystems[numFilesystems].rdonly = rdonly;
#if 0
	rpmlog(RPMLOG_DEBUG, "%5d 0x%04x %s %s\n",
		numFilesystems,
		(unsigned) filesystems[numFilesystems].dev,
		(filesystems[numFilesystems].rdonly ? "ro" : "rw"),
		filesystems[numFilesystems].mntPoint);
#endif
	numFilesystems++;
    }

#   if GETMNTENT_ONE || GETMNTENT_TWO
	(void) fclose(mtab);
#   elif HAVE_GETMNTINFO_R
	mounts = _free(mounts);
#   endif

    filesystems[numFilesystems].dev = 0;
    filesystems[numFilesystems].mntPoint = NULL;
    filesystems[numFilesystems].rdonly = 0;

    fsnames = xcalloc((numFilesystems + 1), sizeof(*fsnames));
    for (i = 0; i < numFilesystems; i++)
	fsnames[i] = filesystems[i].mntPoint;
    fsnames[numFilesystems] = NULL;

/* FIX: fsnames[] may be NULL */
    return 0; 
}
#endif	/* HAVE_MNTCTL */

int rpmGetFilesystemList(const char *** listptr, unsigned int * num)
{
    if (!fsnames) 
	if (getFilesystemList())
	    return 1;

    if (listptr) *listptr = fsnames;
    if (num) *num = numFilesystems;

    return 0;
}

int rpmGetFilesystemUsage(const char ** fileList, rpm_loff_t * fssizes, 
			  unsigned int numFiles,
			  rpm_loff_t ** usagesPtr, int flags)
{
    rpm_loff_t * usages;
    int i, len, j;
    char * buf, * dirName;
    char * chptr;
    int maxLen;
    char * lastDir;
    char * sourceDir;
    int lastfs = 0;
    int lastDev = -1;		/* I hope nobody uses -1 for a st_dev */
    struct stat sb;
    int rc = 1;

    if (!fsnames) 
	if (getFilesystemList())
	    return 1;

    usages = xcalloc(numFilesystems, sizeof(usages));

    sourceDir = rpmGetPath("%{_sourcedir}", NULL);

    maxLen = strlen(sourceDir);
    for (i = 0; i < numFiles; i++) {
	len = strlen(fileList[i]);
	if (maxLen < len) maxLen = len;
    }
    
    buf = xmalloc(maxLen + 1);
    lastDir = xmalloc(maxLen + 1);
    dirName = xmalloc(maxLen + 1);
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

	if (!rstreq(lastDir, buf)) {
	    strcpy(dirName, buf);
	    chptr = dirName + strlen(dirName) - 1;
	    while (stat(dirName, &sb)) {
		if (errno != ENOENT) {
		    rpmlog(RPMLOG_ERR, _("failed to stat %s: %s\n"), buf,
				strerror(errno));
		    goto exit;
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
		    if (filesystems && filesystems[j].dev == sb.st_dev)
			break;

		if (j == numFilesystems) {
		    rpmlog(RPMLOG_ERR, 
				_("file %s is on an unknown device\n"), buf);
		    goto exit;
		}

		lastfs = j;
		lastDev = sb.st_dev;
	    }
	}

	strcpy(lastDir, buf);
	usages[lastfs] += fssizes[i];
    }
    rc = 0;

exit:
    free(sourceDir);
    free(buf);
    free(lastDir);
    free(dirName);

    if (usagesPtr)
	*usagesPtr = usages;
    else
	free(usages);

    return rc;
}
