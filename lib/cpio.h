#ifndef H_CPIO
#define H_CPIO

/** \ingroup payload
 * \file lib/cpio.h
 *  Structures used to handle cpio payloads within rpm packages.
 *
 *  @warning Rpm's cpio implementation may be different than standard cpio.
 *  The implementation is pretty close, but it has some behaviors which are
 *  more to RPM's liking. I tried to document the differing behavior in cpio.c,
 *  but I may have missed some (ewt).
 *
 */

#include <zlib.h>
#include <sys/types.h>

#include <rpmio_internal.h>

/** \ingroup payload
 * @note CPIO_CHECK_ERRNO bit is set only if errno is valid.
 */
#define CPIOERR_CHECK_ERRNO	0x00008000

/** \ingroup payload
 */
enum cpioErrorReturns {
	CPIOERR_BAD_MAGIC	= (2			),
	CPIOERR_BAD_HEADER	= (3			),
	CPIOERR_OPEN_FAILED	= (4    | CPIOERR_CHECK_ERRNO),
	CPIOERR_CHMOD_FAILED	= (5    | CPIOERR_CHECK_ERRNO),
	CPIOERR_CHOWN_FAILED	= (6    | CPIOERR_CHECK_ERRNO),
	CPIOERR_WRITE_FAILED	= (7    | CPIOERR_CHECK_ERRNO),
	CPIOERR_UTIME_FAILED	= (8    | CPIOERR_CHECK_ERRNO),
	CPIOERR_UNLINK_FAILED	= (9    | CPIOERR_CHECK_ERRNO),

	CPIOERR_SYMLINK_FAILED	= (11   | CPIOERR_CHECK_ERRNO),
	CPIOERR_STAT_FAILED	= (12   | CPIOERR_CHECK_ERRNO),
	CPIOERR_MKDIR_FAILED	= (13   | CPIOERR_CHECK_ERRNO),
	CPIOERR_MKNOD_FAILED	= (14   | CPIOERR_CHECK_ERRNO),
	CPIOERR_MKFIFO_FAILED	= (15   | CPIOERR_CHECK_ERRNO),
	CPIOERR_LINK_FAILED	= (16   | CPIOERR_CHECK_ERRNO),
	CPIOERR_READLINK_FAILED	= (17   | CPIOERR_CHECK_ERRNO),
	CPIOERR_READ_FAILED	= (18   | CPIOERR_CHECK_ERRNO),
	CPIOERR_COPY_FAILED	= (19   | CPIOERR_CHECK_ERRNO),
	CPIOERR_HDR_SIZE	= (20			),
	CPIOERR_UNKNOWN_FILETYPE= (21			),
	CPIOERR_MISSING_HARDLINK= (22			),
	CPIOERR_MD5SUM_MISMATCH	= (23			),
	CPIOERR_INTERNAL	= (24			)
};

/** \ingroup payload
 */
typedef enum cpioMapFlags_e {
    CPIO_MAP_PATH		= (1 << 0),
    CPIO_MAP_MODE		= (1 << 1),
    CPIO_MAP_UID		= (1 << 2),
    CPIO_MAP_GID		= (1 << 3),
    CPIO_FOLLOW_SYMLINKS	= (1 << 4),  /* only for building */
    CPIO_MULTILIB		= (1 << 31) /* internal, only for building */
} cpioMapFlags;

/** \ingroup payload
 * Defines a single file to be included in a cpio payload.
 */
struct cpioFileMapping {
/*@dependent@*/ const char * archivePath; /*!< Path to store in cpio archive. */
/*@dependent@*/ const char * dirName;	/*!< Payload file directory. */
/*@dependent@*/ const char * baseName;	/*!< Payload file base name. */
/*@dependent@*/ const char * md5sum;	/*!< File MD5 sum (NULL disables). */
    mode_t finalMode;		/*!< Mode of payload file (from header). */
    uid_t finalUid;		/*!< Uid of payload file (from header). */
    gid_t finalGid;		/*!< Gid of payload file (from header). */
    cpioMapFlags mapFlags;
};

/** \ingroup payload
 * The first argument passed in a cpio progress callback.
 *
 * @note When building the cpio payload, only "file" is filled in.
 */
struct cpioCallbackInfo {
/*@dependent@*/ const char * file;	/*!< File name being installed. */
    long fileSize;			/*!< Total file size. */
    long fileComplete;			/*!< Amount of file unpacked. */
    long bytesProcessed;		/*!< No. bytes in archive read. */
};

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup payload
 */
typedef void (*cpioCallback) (struct cpioCallbackInfo * filespec, void * data);

/** \ingroup payload
 * The RPM internal equivalent of the command line "cpio -i".
 *
 * If no mappings are passed, this installs everything! If one is passed
 * it should be sorted according to cpioFileMapCmp() and only files included
 * in the map are installed. Files are installed relative to the current
 * directory unless a mapping is given which specifies an absolute
 * directory. The mode mapping is only used for the permission bits, not
 * for the file type. The owner/group mappings are ignored for the nonroot
 * user.
 *
 * @param cfd		file handle
 * @param mappings	archive info for extraction
 * @param numMappings	number of archive elements
 * @param cb		progress callback
 * @param cbData	progress callback data
 * @retval failedFile	file name (malloc'ed) that caused failure (if any)
 * @return		0 on success
 */
int cpioInstallArchive(FD_t cfd, const void * mappings,
		       int numMappings, cpioCallback cb, void * cbData,
		       /*@out@*/const char ** failedFile)
	/*@modifies fileSystem, cfd, *failedFile @*/;

/** \ingroup payload
 * The RPM internal equivalent of the command line "cpio -o".
 *
 * @param cfd		file handle
 * @param mappings	archive info for building
 * @param numMappings	number of archive elements
 * @param cb		progress callback
 * @param cbData	progress callback data
 * @retval failedFile	file name (malloc'ed) that caused failure (if any)
 * @return		0 on success
 */
int cpioBuildArchive(FD_t cfd, const void * mappings,
		     int numMappings, cpioCallback cb, void * cbData,
		     unsigned int * archiveSize, /*@out@*/const char ** failedFile)
	/*@modifies fileSystem, cfd, *archiveSize, *failedFile @*/;

#ifdef	DYING
/** \ingroup payload
 * Compare two cpio file map entries (qsort/bsearch).
 * This is designed to be qsort/bsearch compatible.
 * @param a		1st map
 * @param b		2nd map
 * @return		result of comparison
 */
int cpioFileMapCmp(const void * a, const void * b)	/*@*/;
#endif

/** \ingroup payload
 * Return formatted error message on payload handling failure.
 * @param		error code
 * @return		formatted error string
 */
/*@observer@*/ const char *const cpioStrerror(int rc)		/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_CPIO */
