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

#define RPMERR_CHECK_ERRNO    -32768

/** \ingroup payload
 */
enum cpioErrorReturns {
	RPMERR_ITER_END		= -1,
	RPMERR_BAD_MAGIC	= -2,
	RPMERR_BAD_HEADER	= -3,
	RPMERR_HDR_SIZE	= -4,
	RPMERR_UNKNOWN_FILETYPE= -5,
	RPMERR_MISSING_FILE	= -6,
	RPMERR_DIGEST_MISMATCH	= -7,
	RPMERR_INTERNAL	= -8,
	RPMERR_UNMAPPED_FILE	= -9,
	RPMERR_ENOENT		= -10,
	RPMERR_ENOTEMPTY	= -11,
	RPMERR_FILE_SIZE	= -12,

	RPMERR_OPEN_FAILED	= -32768,
	RPMERR_CHMOD_FAILED	= -32769,
	RPMERR_CHOWN_FAILED	= -32770,
	RPMERR_WRITE_FAILED	= -32771,
	RPMERR_UTIME_FAILED	= -32772,
	RPMERR_UNLINK_FAILED	= -32773,
	RPMERR_RENAME_FAILED	= -32774,
	RPMERR_SYMLINK_FAILED	= -32775,
	RPMERR_STAT_FAILED	= -32776,
	RPMERR_LSTAT_FAILED	= -32777,
	RPMERR_MKDIR_FAILED	= -32778,
	RPMERR_RMDIR_FAILED	= -32779,
	RPMERR_MKNOD_FAILED	= -32780,
	RPMERR_MKFIFO_FAILED	= -32781,
	RPMERR_LINK_FAILED	= -32782,
	RPMERR_READLINK_FAILED	= -32783,
	RPMERR_READ_FAILED	= -32784,
	RPMERR_COPY_FAILED	= -32785,
	RPMERR_LSETFCON_FAILED	= -32786,
	RPMERR_SETCAP_FAILED	= -32787,
};


typedef struct rpmcpio_s * rpmcpio_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create CPIO file object
 * @param fd		file
 * @param mode		XXX
 * @return CPIO object
 **/
rpmcpio_t rpmcpioOpen(FD_t fd, char mode);

int rpmcpioClose(rpmcpio_t cpio);

off_t rpmcpioTell(rpmcpio_t cpio);

rpmcpio_t rpmcpioFree(rpmcpio_t cpio);

/**
 * Write cpio header.
 * @retval fsm		file path and stat info
 * @param st
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmcpioHeaderWrite(rpmcpio_t cpio, char * path, struct stat * st);
RPM_GNUC_INTERNAL
int rpmcpioStrippedHeaderWrite(rpmcpio_t cpio, int fx, off_t fsize);

ssize_t rpmcpioWrite(rpmcpio_t cpio, const void * buf, size_t size);

/**
 * Read cpio header. Iff fx is returned as -1 a cpio header was read
 * and the file name is found in path. Otherwise a stripped header was read
 * and the fx is the number of the file in the header/rpmfi. In this case
 * rpmcpioSetExpectedFileSize() needs to be called with the file size of the
 * payload content - with may be zero for hard links, directory or other
 * special files.
 * @retval fsm		file path and stat info
 * @retval path		path of the file
 * @retval fx		number in the header of the file read
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmcpioHeaderRead(rpmcpio_t cpio, char ** path, int * fx);

/**
 * Tell the cpio object the expected file size in the payload.
 * The size must be zero for all but the last of hard linked files,
 * directories and special files.
 * This is needed after reading a stripped cpio header! See above.
 */
RPM_GNUC_INTERNAL
void rpmcpioSetExpectedFileSize(rpmcpio_t cpio, off_t fsize);

ssize_t rpmcpioRead(rpmcpio_t cpio, void * buf, size_t size);

/** \ingroup payload
 * Return formatted error message on payload handling failure.
 * @param rc		error code
 * @return		formatted error string (malloced)
 */
/* XXX should be RPM_GNUC_INTERNAL too but build/pack.c uses */
char * rpmcpioStrerror(int rc);

#ifdef __cplusplus
}
#endif

#endif	/* H_CPIO */
