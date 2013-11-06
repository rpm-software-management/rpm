#ifndef _RPMFI_INTERNAL_H
#define _RPMFI_INTERNAL_H

#include <rpm/header.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmstrpool.h>
#include "lib/fprint.h"
#include "lib/cpio.h"

#define	RPMFIMAGIC	0x09697923

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmfi
 * Return file info set string pool handle
 * @param fi		file info
 * @return		string pool handle (weak reference)
 */
RPM_GNUC_INTERNAL
rpmstrPool rpmfiPool(rpmfi fi);

/** \ingroup rpmfi
 * Return current base name pool id from file info set.
 * @param fi		file info set
 * @return		current base name id, 0 on invalid
 */
RPM_GNUC_INTERNAL
rpmsid rpmfiBNId(rpmfi fi);

/** \ingroup rpmfi
 * Return current directory name pool id from file info set.
 * @param fi		file info set
 * @return		current base name id, 0 on invalid
 */
RPM_GNUC_INTERNAL
rpmsid rpmfiDNId(rpmfi fi);

RPM_GNUC_INTERNAL
int rpmfiDIIndex(rpmfiles fi, int dx);

RPM_GNUC_INTERNAL
rpmsid rpmfiBNIdIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmsid rpmfiDNIdIndex(rpmfiles fi, int jx);

RPM_GNUC_INTERNAL
const char * rpmfiBNIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiDNIndex(rpmfiles fi, int jx);

RPM_GNUC_INTERNAL
char * rpmfiFNIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmVerifyAttrs rpmfiVFlagsIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmfileState rpmfiFStateIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFLinkIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_loff_t rpmfiFSizeIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_color_t rpmfiFColorIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFClassIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
uint32_t rpmfiFDependsIndex(rpmfiles fi, int ix, const uint32_t ** fddictp);

RPM_GNUC_INTERNAL
uint32_t rpmfiFNlinkIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
uint32_t rpmfiFLinksIndex(rpmfiles fi, int ix, const int ** files);

RPM_GNUC_INTERNAL
const char * rpmfiFLangsIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmfileAttrs rpmfiFFlagsIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_mode_t rpmfiFModeIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const unsigned char * rpmfiFDigestIndex(rpmfiles fi, int ix, int *algo, size_t *len);

RPM_GNUC_INTERNAL
rpm_rdev_t rpmfiFRdevIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_ino_t rpmfiFInodeIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_time_t rpmfiFMtimeIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFUserIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFGroupIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFCapsIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
struct fingerPrint_s *rpmfiFps(rpmfiles fi);

RPM_GNUC_INTERNAL
rpmFileAction rpmfiDecideFateIndex(rpmfiles ofi, int oix,
				   rpmfiles nfi, int nix,
                                   int skipMissing);

RPM_GNUC_INTERNAL
int rpmfiCompareIndex(rpmfiles afi, int aix, rpmfiles bfi, int bix);

RPM_GNUC_INTERNAL
int rpmfiConfigConflictIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
void rpmfiSetFReplacedSizeIndex(rpmfiles fi, int ix, rpm_loff_t newsize);

RPM_GNUC_INTERNAL
rpm_loff_t rpmfiFReplacedSizeIndex(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
void rpmfiFpLookup(rpmfiles fi, fingerPrintCache fpc);

rpmfiles rpmfiFiles(rpmfi fi);

rpm_count_t rpmfilesFC(rpmfiles fi);

rpm_count_t rpmfilesDC(rpmfiles fi);

rpmfi rpmfilesIter(rpmfiles files, int flags);

int rpmfilesDigestAlgo(rpmfiles fi);

/* Temporary ugly kludge to eliminate direct struct rpmfi access... */
void rpmfiSetApath(rpmfi fi, char **apath);

/** \ingroup payload
 * Add payload archive to the file info
 * @param fi		file info
 * @param fd		file
 * @param mode		O_RDONLY or O_WRONLY
 * @return		> 0 on error
 */
int rpmfiAttachArchive(rpmfi fi, FD_t fd, char mode);

/** \ingroup payload
 * Close payload archive
 * @param fi		file info
 * @return		> 0 on error
 */
int rpmfiArchiveClose(rpmfi fi);

/** \ingroup payload
 * Return current position in payload archive
 * @param fi		file info
 * @return		position
 */
rpm_loff_t rpmfiArchiveTell(rpmfi fi);

/** \ingroup payload
 * Write archive header for current file
 * @param fi		file info
 * @return		> 0 on error
 */
int rpmfiArchiveWriteHeader(rpmfi fi);

/** \ingroup payload
 * Write content into current file in archive
 * @param fi		file info
 * @param buf		pointer to content
 * @prama size		number of bytes to write
 * @return		bytes actually written
 */
size_t rpmfiArchiveWrite(rpmfi fi, const void * buf, size_t size);

/** \ingroup payload
 * Write content from given file into current file in archive
 * @param fi		file info
 * @param fd		file descriptor of file to read
 * @return		> 0 on error
 */
int rpmfiArchiveWriteFile(rpmfi fi, FD_t fd);

/** \ingroup payload
 * Read next archive header from archive and move fi to the
 * file found in the archive. Sets rpmfiFX!
 * @param fi		file info
 * @return		> 0 on error
 */
int rpmfiArchiveNext(rpmfi fi);


/** \ingroup payload
 * Read content from current file in archive
 * @param fi		file info
 * @param buf		pointer to buffer
 * @prama size		number of bytes to read
 * @return		bytes actually read
 */
size_t rpmfiArchiveRead(rpmfi fi, void * buf, size_t size);

/** \ingroup payload
 * Write content from current file in archive to a file
 * @param fi		file info
 * @param fd		file descriptor of file to write to
 * @param nodigest	ommit checksum check if > 0
 * @return		> 0 on error
 */
int rpmfiArchiveReadToFile(rpmfi fi, FD_t fd, char nodigest);

#ifdef __cplusplus
}
#endif

#endif	/* _RPMFI_INTERNAL_H */

