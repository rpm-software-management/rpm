#ifndef H_CPIO
#define H_CPIO

/** \ingroup payload
 * \file cpio.h
 *  Structures used to handle cpio payloads within rpm packages.
 *
 *  @warning Rpm's cpio implementation may be different than standard cpio.
 *  The implementation is pretty close, but it has some behaviors which are
 *  more to RPM's liking. I tried to document the differing behavior in cpio.c,
 *  but I may have missed some (ewt).
 *
 */

typedef struct rpmcpio_s * rpmcpio_t;

/**
 * Create CPIO file object
 * @param fd		file
 * @param mode		XXX
 * @return CPIO object
 **/
RPM_GNUC_INTERNAL
rpmcpio_t rpmcpioOpen(FD_t fd, char mode);

RPM_GNUC_INTERNAL
int rpmcpioClose(rpmcpio_t cpio);

RPM_GNUC_INTERNAL
off_t rpmcpioTell(rpmcpio_t cpio);

RPM_GNUC_INTERNAL
rpmcpio_t rpmcpioFree(rpmcpio_t cpio);

/**
 * Write a standard newc CPIO header for one file entry.
 *
 * The caller is expected to write exactly st->st_size bytes of file data
 * via rpmcpioWrite() after this call. The next call to rpmcpioHeaderWrite
 * (or rpmcpioTrailerWrite / rpmcpioClose) will verify that the full amount
 * was written (fileend == offset check) and pad the data before continuing.
 *
 * Note: The CPIO "checksum" is not used.
 * @param cpio		cpio archive
 * @param path		file name
 * @param st		stat struct with meta data
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmcpioHeaderWrite(rpmcpio_t cpio, char * path, struct stat * st);

/**
 * Write a stripped CPIO header for one file entry (RPM v4.12+ large file support).
 *
 * The stripped format (magic "07070X") omits all file metadata from the CPIO
 * header — only the file's index into the RPM header arrays is stored. All
 * metadata (name, mode, uid, gid, mtime, etc.) comes from the RPM header instead.
 * The file size is also obtained from the RPM header (RPMTAG_LONGFILESIZES),
 * and passed here as fsize so that fileend can be set correctly.
 * @param cpio		cpio archive
 * @param fx		file index
 * @param fsize		file size
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmcpioStrippedHeaderWrite(rpmcpio_t cpio, int fx, off_t fsize);

RPM_GNUC_INTERNAL
ssize_t rpmcpioWrite(rpmcpio_t cpio, const void * buf, size_t size);

/**
 * Read the next CPIO header from the archive.
 *
 * Handles both standard newc (070701/070702) and stripped (07070X) headers.
 *
 * On success, if a standard header was read:
 * - sets *path to the entry name (caller must free)
 * - sets *fx to -1
 *
 * Else if a stripped header was read:
 * - sets *fx to the number of the file in the package header/rpmfi
 * - caller must call rpmcpioSetExpectedFileSize() with the size of the file
 *   content (read from the package header) before reading data because the
 *   archive header does not know the size. The size may be zero for hard
 *   links, directory or other special files.
 *
 * If the previous entry's data was not fully consumed by the caller, this
 * function skips the remaining bytes before reading the pad and next header.
 *
 * @param[out] fsm		file path and stat info
 * @param[out] path		path of the file
 * @param[out] fx		number in the header of the file read
 * @return		0 on success, RPMERR_ITER_END for the trailer entry.
 */
RPM_GNUC_INTERNAL
int rpmcpioHeaderRead(rpmcpio_t cpio, char ** path, int * fx);

/**
 * Set the expected file size for the current entry in a stripped CPIO archive.
 *
 * In stripped format, the header does not contain the file size — it must be
 * obtained from the RPM header (RPMTAG_LONGFILESIZES). This function is called
 * (is required to be called) from rpmfiArchiveReadHeader() after reading a
 * stripped header, so that subsequent rpmcpioRead() calls know where the file
 * data ends.
 *
 * The size must be zero for all but the last of hard linked files,
 * directories and special files.
 */
RPM_GNUC_INTERNAL
void rpmcpioSetExpectedFileSize(rpmcpio_t cpio, off_t fsize);

RPM_GNUC_INTERNAL
ssize_t rpmcpioRead(rpmcpio_t cpio, void * buf, size_t size);

#endif	/* H_CPIO */
