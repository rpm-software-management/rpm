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
 * file content is padded to start on an @a align-aligned absolute offset in the
 * underlying file (base is the file offset at which the payload/cpio stream
 * begins). Has no effect on stripped headers.
 * @param cpio		cpio archive (opened for writing)
 * @param align		content alignment in bytes (0 disables)
 * @param base		absolute file offset of the payload start
 */
RPM_GNUC_INTERNAL
void rpmcpioSetWriteAlign(rpmcpio_t cpio, size_t align, off_t base);

/**
 * Set the content alignment on a read archive so the alignment padding before
 * aligned file content is skipped. Read from RPMTAG_PAYLOADALIGNMENT.
 * @param cpio		cpio archive (opened for reading)
 * @param align		content alignment in bytes (0 = none)
 */
RPM_GNUC_INTERNAL
void rpmcpioSetReadAlign(rpmcpio_t cpio, size_t align);

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
int rpmcpioStrippedHeaderWrite(rpmcpio_t cpio, int fx, int isreg, off_t fsize);

RPM_GNUC_INTERNAL
ssize_t rpmcpioWrite(rpmcpio_t cpio, const void * buf, size_t size);

/**
 * Read cpio header. Iff fx is returned as -1 a cpio header was read
 * and the file name is found in path. Otherwise a stripped header was read
 * and the fx is the number of the file in the header/rpmfi. In this case
 * rpmcpioSetExpectedFileSize() needs to be called with the file size of the
 * payload content - with may be zero for hard links, directory or other
 * special files.
 * @param[out] fsm		file path and stat info
 * @param[out] path		path of the file
 * @param[out] fx		number in the header of the file read
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
int rpmcpioSetExpectedFileSize(rpmcpio_t cpio, int isreg, off_t fsize);

RPM_GNUC_INTERNAL
ssize_t rpmcpioRead(rpmcpio_t cpio, void * buf, size_t size);

#endif	/* H_CPIO */
