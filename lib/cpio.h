#ifndef H_CPIO
#define H_CPIO

#include <zlib.h>

/* Note the "high" bit is set only if errno is valid */
#define CPIO_CHECK_ERRNO	0x80000000
#define CPIO_READ_FAILED	(-1)
#define CPIO_BAD_MAGIC		(-2			)
#define CPIO_BAD_HEADER		(-3			)
#define CPIO_OPEN_FAILED	(-4   | CPIO_CHECK_ERRNO)
#define CPIO_CHMOD_FAILED	(-5   | CPIO_CHECK_ERRNO)
#define CPIO_CHOWN_FAILED	(-6   | CPIO_CHECK_ERRNO)
#define CPIO_WRITE_FAILED	(-7   | CPIO_CHECK_ERRNO)
#define CPIO_UTIME_FAILED	(-8   | CPIO_CHECK_ERRNO)
#define CPIO_UNLINK_FAILED	(-9   | CPIO_CHECK_ERRNO)
#define CPIO_INTERNAL		(-10			)
#define CPIO_SYMLINK_FAILED	(-11  | CPIO_CHECK_ERRNO)
#define CPIO_STAT_FAILED	(-12  | CPIO_CHECK_ERRNO)
#define CPIO_MKDIR_FAILED	(-13  | CPIO_CHECK_ERRNO)
#define CPIO_MKNOD_FAILED	(-14  | CPIO_CHECK_ERRNO)
#define CPIO_MKFIFO_FAILED	(-15  | CPIO_CHECK_ERRNO)

/* Don't think this behaves just like standard cpio. It's pretty close, but
   it has some behaviors which are more to RPM's liking. I tried to document
   them inline in cpio.c, but I may have missed some. */

#define CPIO_MAP_PATH	(1 << 0)
#define CPIO_MAP_MODE	(1 << 1)
#define CPIO_MAP_UID	(1 << 2)
#define CPIO_MAP_GID	(1 << 3)

struct cpioFileMapping {
    char * archivePath;
    char * finalPath;
    mode_t finalMode;
    uid_t finalUid;
    gid_t finalGid;
    int mapFlags;
};

struct cpioCallbackInfo {
    char * file;
    long fileSize;			/* total file size */
    long fileComplete;			/* amount of file unpacked */
    long bytesProcessed;		/* bytes in archive read */
};

typedef void (*cpioCallback)(struct cpioCallbackInfo * filespec, void * data);

/* If no mappings are passed, this installs everything! If one is passed
   it should be sorted according to cpioFileMapCmp() and only files included
   in the map are installed. Files are installed relative to the current
   directory unless a mapping is given which specifies an absolute 
   directory. The mode mapping is only used for the permission bits, not
   for the file type. The owner/group mappings are ignored for the nonroot
   user. If *failedFile is non-NULL on return, it should be free()d. */
int cpioInstallArchive(gzFile stream, struct cpioFileMapping * mappings, 
		       int numMappings, cpioCallback cb, void * cbData,
		       char ** failedFile);

/* This is designed to be qsort/bsearch compatible */
int cpioFileMapCmp(const void * a, const void * b);

#endif
