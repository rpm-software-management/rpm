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

/**
 * Enable content alignment on a write archive. When align is non-zero, regular
 * file content is padded to start on an @a align-aligned offset. This is
 * relative to compressed streams and absolute for a plain backing file.
 * @param cpio		cpio archive (opened for writing)
 * @param align		content alignment in bytes (0 disables)
 */
RPM_GNUC_INTERNAL
void rpmcpioSetWriteAlign(rpmcpio_t cpio, size_t align);

/**
 * Set the content alignment on a read archive so the alignment padding before
 * aligned file content is skipped. Read from RPMTAG_PAYLOADALIGNMENT.
 * @param cpio		cpio archive (opened for reading)
 * @param align		content alignment in bytes (0 = none)
 */
RPM_GNUC_INTERNAL
void rpmcpioSetReadAlign(rpmcpio_t cpio, size_t align);

/** Returned by range-copy wrappers when raw descriptor copying is unsuitable
 * and the caller should copy through the FD_t stream instead. */
#define RPMCPIO_COPY_FALLBACK 1

/** Optional notification after bytes have been copied by rpmcpioCopyRange(). */
typedef void (*rpmcpioCopyNotify)(void *data, off_t copied);

/**
 * Copy a source file's content into the archive at the current content
 * position with a clone or copy_file_range(2). Usable only when the archive is
 * a plain (uncompressed) file; falls back otherwise. Must be called with the
 * archive positioned at the start of the file content (i.e. right after
 * rpmfiArchiveWriteHeader).
 * @param cpio		cpio archive (opened for writing)
 * @param src_fd	source file descriptor (regular file, read from current offset)
 * @param size		file size in bytes
 * @return		0 on success, RPMCPIO_COPY_FALLBACK if an accelerated
 *			copy cannot be used, < 0 (RPMERR_*) on hard error
 */
RPM_GNUC_INTERNAL
int rpmcpioWriteFile(rpmcpio_t cpio, int src_fd, off_t size);

/**
 * Copy a byte range from one file to another. First attempt to clone the whole
 * range with FICLONERANGE. If cloning is unavailable, copy in bounded
 * copy_file_range(2) chunks so callers can report progress, with positional
 * userspace I/O as the fallback.
 * @param notify	optional cumulative progress notification
 * @param data		opaque data passed to @a notify
 * @return		0 on success, < 0 (RPMERR_*) on hard error
 */
RPM_GNUC_INTERNAL
int rpmcpioCopyRange(int dst_fd, int src_fd,
		     off_t src_off, off_t dst_off, off_t len,
		     rpmcpioCopyNotify notify, void *data);

RPM_GNUC_INTERNAL
rpmcpio_t rpmcpioFree(rpmcpio_t cpio);

/**
 * Write cpio header.
 * @param cpio		cpio archive
 * @param path		file name
 * @param st		stat struct with meta data
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmcpioHeaderWrite(rpmcpio_t cpio, char * path, struct stat * st);
RPM_GNUC_INTERNAL
int rpmcpioStrippedHeaderWrite(rpmcpio_t cpio, int fx, mode_t fmode,
			       off_t fsize);

RPM_GNUC_INTERNAL
ssize_t rpmcpioWrite(rpmcpio_t cpio, const void * buf, size_t size);

/**
 * Read cpio header. Iff fx is returned as -1 a cpio header was read
 * and the file name is found in path. Otherwise a stripped header was read
 * and the fx is the number of the file in the header/rpmfi. In this case
 * rpmcpioSetExpectedFileSize() needs to be called with the file mode and size
 * of the payload content, which may be zero for hard links, directories or
 * other special files.
 * @param[out] fsm		file path and stat info
 * @param[out] path		path of the file
 * @param[out] fx		number in the header of the file read
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmcpioHeaderRead(rpmcpio_t cpio, char ** path, int * fx);

/**
 * Tell the cpio object the expected file mode and size in the payload.
 * The size must be zero for all but the last of hard linked files,
 * directories and special files.
 * This is needed after reading a stripped cpio header! See above.
 */
RPM_GNUC_INTERNAL
int rpmcpioSetExpectedFileSize(rpmcpio_t cpio, mode_t fmode, off_t fsize);

RPM_GNUC_INTERNAL
ssize_t rpmcpioRead(rpmcpio_t cpio, void * buf, size_t size);

#endif	/* H_CPIO */
