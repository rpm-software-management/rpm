#ifndef H_CPIO
#define H_CPIO

/** \file lib/cpio.h
 *  Structures used to handle cpio payloads within rpm packages.
 *
 *  Warning: Don't think that rpm's cpio implementation behaves just like
 *  standard cpio.
 *  The implementation is pretty close, but it has some behaviors which are
 *  more to RPM's liking. I tried to document the differing behavior in cpio.c,
 *  but I may have missed some.
 *
 */

#include <zlib.h>
#include <sys/types.h>

#include <rpmio.h>

/**
 * Note:  CPIO_CHECK_ERRNO bit is set only if errno is valid. These have to
 * be positive numbers or this setting the high bit stuff is a bad idea.
 */
#define CPIOERR_CHECK_ERRNO	0x00008000

#define CPIOERR_BAD_MAGIC	(2			)
#define CPIOERR_BAD_HEADER	(3			)
#define CPIOERR_OPEN_FAILED	(4    | CPIOERR_CHECK_ERRNO)
#define CPIOERR_CHMOD_FAILED	(5    | CPIOERR_CHECK_ERRNO)
#define CPIOERR_CHOWN_FAILED	(6    | CPIOERR_CHECK_ERRNO)
#define CPIOERR_WRITE_FAILED	(7    | CPIOERR_CHECK_ERRNO)
#define CPIOERR_UTIME_FAILED	(8    | CPIOERR_CHECK_ERRNO)
#define CPIOERR_UNLINK_FAILED	(9    | CPIOERR_CHECK_ERRNO)

#define CPIOERR_SYMLINK_FAILED	(11   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_STAT_FAILED	(12   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_MKDIR_FAILED	(13   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_MKNOD_FAILED	(14   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_MKFIFO_FAILED	(15   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_LINK_FAILED	(16   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_READLINK_FAILED	(17   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_READ_FAILED	(18   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_COPY_FAILED	(19   | CPIOERR_CHECK_ERRNO)
#define CPIOERR_HDR_SIZE	(20			)
#define CPIOERR_UNKNOWN_FILETYPE (21			)
#define CPIOERR_MISSING_HARDLINK (22			)
#define CPIOERR_INTERNAL	(23			)

#define CPIO_MAP_PATH		(1 << 0)
#define CPIO_MAP_MODE		(1 << 1)
#define CPIO_MAP_UID		(1 << 2)
#define CPIO_MAP_GID		(1 << 3)
#define CPIO_FOLLOW_SYMLINKS	(1 << 4)  /* only for building */

/** The structure used to define a cpio payload file. */
struct cpioFileMapping {
/*@{*/
    /*@owned@*/ const char * archivePath; /*!< Path to store in cpio archive. */
    /*@owned@*/ const char * fsPath;	/*!< Location of payload file. */
    mode_t finalMode;		/*!< Mode of payload file (from header). */
    uid_t finalUid;		/*!< Uid of payload file (from header). */
    gid_t finalGid;		/*!< Gid of payload file (from header). */
    int mapFlags;
/*@}*/
};

/** The structure passed as first argument during a cpio progress callback.
 *
 * Note: When building the cpio payload, only "file" is filled in.
 */
struct cpioCallbackInfo {
/*@{*/
    /*@dependent@*/ const char * file;	/*!< File name being installed. */
    long fileSize;			/*!< Total file size. */
    long fileComplete;			/*!< Amount of file unpacked. */
    long bytesProcessed;		/*!< No. bytes in archive read. */
/*@}*/
};

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cpioCallback) (struct cpioCallbackInfo * filespec, void * data);

/**
 * The RPM internal equivalent of the command line "cpio -i".
 * If no mappings are passed, this installs everything! If one is passed
 * it should be sorted according to cpioFileMapCmp() and only files included
 * in the map are installed. Files are installed relative to the current
 * directory unless a mapping is given which specifies an absolute
 * directory. The mode mapping is only used for the permission bits, not
 * for the file type. The owner/group mappings are ignored for the nonroot
 * user. If *failedFile is non-NULL on return, it should be free()d.
 */
int cpioInstallArchive(FD_t cfd, struct cpioFileMapping * mappings,
		       int numMappings, cpioCallback cb, void * cbData,
		       /*@out@*/const char ** failedFile);
/**
 * The RPM internal equivalent of the command line "cpio -o".
 */
int cpioBuildArchive(FD_t cfd, struct cpioFileMapping * mappings,
		     int numMappings, cpioCallback cb, void * cbData,
		     unsigned int * archiveSize, /*@out@*/const char ** failedFile);

/** This is designed to be qsort/bsearch compatible */
int cpioFileMapCmp(const void * a, const void * b);

/** */
/*@observer@*/ const char *cpioStrerror(int rc);

#ifdef __cplusplus
}
#endif

#endif	/* H_CPIO */
