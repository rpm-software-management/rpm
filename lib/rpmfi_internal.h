#ifndef _RPMFI_INTERNAL_H
#define _RPMFI_INTERNAL_H

#include <rpm/header.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmstrpool.h>
#include "lib/fprint.h"
#include "lib/cpio.h"

#define	RPMFIMAGIC	0x09697923

typedef enum rpmFileIter_e {
    RPMFI_ITER_FWD	= 0,
    RPMFI_ITER_BACK	= 1,
    RPMFI_ITER_READ_ARCHIVE	= 2,
    RPMFI_ITER_WRITE_ARCHIVE	= 3,
} rpmFileIter;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmfi
 * Return file info set string pool handle
 * @param fi		file info
 * @return		string pool handle (weak reference)
 */
RPM_GNUC_INTERNAL
rpmstrPool rpmfilesPool(rpmfiles fi);

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
int rpmfilesDI(rpmfiles fi, int dx);

RPM_GNUC_INTERNAL
rpmsid rpmfilesBNId(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmsid rpmfilesDNId(rpmfiles fi, int jx);

RPM_GNUC_INTERNAL
const char * rpmfilesBN(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfilesDN(rpmfiles fi, int jx);

RPM_GNUC_INTERNAL
char * rpmfilesFN(rpmfiles fi, int ix);

/** \ingroup rpmfi
 * Return current original base name pool id from file info set.
 * @param fi		file info set
 * @return		current base name id, 0 on invalid
 */
RPM_GNUC_INTERNAL
rpmsid rpmfiOBNId(rpmfi fi);

/** \ingroup rpmfi
 * Return current original directory name pool id from file info set.
 * @param fi		file info set
 * @return		current base name id, 0 on invalid
 */
RPM_GNUC_INTERNAL
rpmsid rpmfiODNId(rpmfi fi);

RPM_GNUC_INTERNAL
int rpmfilesODI(rpmfiles fi, int dx);

RPM_GNUC_INTERNAL
rpmsid rpmfilesOBNId(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmsid rpmfilesODNId(rpmfiles fi, int jx);

RPM_GNUC_INTERNAL
const char * rpmfilesOBN(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfilesODN(rpmfiles fi, int jx);

RPM_GNUC_INTERNAL
char * rpmfilesOFN(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmVerifyAttrs rpmfilesVFlags(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmfileState rpmfilesFState(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfilesFLink(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_loff_t rpmfilesFSize(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_color_t rpmfilesFColor(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfilesFClass(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
uint32_t rpmfilesFDepends(rpmfiles fi, int ix, const uint32_t ** fddictp);

RPM_GNUC_INTERNAL
uint32_t rpmfilesFNlink(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
uint32_t rpmfilesFLinks(rpmfiles fi, int ix, const int ** files);

RPM_GNUC_INTERNAL
const char * rpmfilesFLangs(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpmfileAttrs rpmfilesFFlags(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_mode_t rpmfilesFMode(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const unsigned char * rpmfilesFDigest(rpmfiles fi, int ix, int *algo, size_t *len);

RPM_GNUC_INTERNAL
rpm_rdev_t rpmfilesFRdev(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_ino_t rpmfilesFInode(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
rpm_time_t rpmfilesFMtime(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfilesFUser(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfilesFGroup(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfilesFCaps(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
struct fingerPrint_s *rpmfilesFps(rpmfiles fi);

RPM_GNUC_INTERNAL
rpmFileAction rpmfilesDecideFate(rpmfiles ofi, int oix,
				   rpmfiles nfi, int nix,
                                   int skipMissing);

RPM_GNUC_INTERNAL
int rpmfilesCompare(rpmfiles afi, int aix, rpmfiles bfi, int bix);

RPM_GNUC_INTERNAL
int rpmfilesConfigConflict(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
void rpmfilesSetFReplacedSize(rpmfiles fi, int ix, rpm_loff_t newsize);

RPM_GNUC_INTERNAL
rpm_loff_t rpmfilesFReplacedSize(rpmfiles fi, int ix);

RPM_GNUC_INTERNAL
void rpmfilesFpLookup(rpmfiles fi, fingerPrintCache fpc);

rpmfiles rpmfiFiles(rpmfi fi);

rpmfiles rpmfilesNew(rpmstrPool pool, Header h, rpmTagVal tagN, rpmfiFlags flags);

rpmfiles rpmfilesLink(rpmfiles fi);

rpmfiles rpmfilesFree(rpmfiles fi);

rpm_count_t rpmfilesFC(rpmfiles fi);

rpm_count_t rpmfilesDC(rpmfiles fi);

int rpmfilesFindFN(rpmfiles files, const char * fn);

int rpmfilesFindOFN(rpmfiles files, const char * fn);

rpmfi rpmfilesIter(rpmfiles files, int itype);

int rpmfilesDigestAlgo(rpmfiles fi);

rpm_color_t rpmfilesColor(rpmfiles files);

/** \ingroup payload
 * Get new file iterator for writing the archive content.
 * The returned rpmfi will only visit the files needing some content.
 * You need to provide the content using rpmfiArchiveWrite() or
 * rpmfiArchiveWriteFile(). Make sure to close the rpmfi with
 * rpmfiArchiveClose() to get the trailer written.
 * rpmfiSetFX() is not supported for this type of iterator.
 * @param fd		file
 * @param fi            file info
 * @return		new rpmfi
 */
rpmfi rpmfiNewArchiveWriter(FD_t fd, rpmfiles files);

/** \ingroup payload
 * Get new file iterator for looping over the archive content.
 * Returned rpmfi visites files in the order they are read from the payload.
 * Content of the regular files can be retrieved with rpmfiArchiveRead() or
 * rpmfiArchiveReadToFile() when they are visited with rpmfiNext().
 * rpmfiSetFX() is not supported for this type of iterator.
 * @param fd		file
 * @param fi            file info
 * @return		new rpmfi
 */
rpmfi rpmfiNewArchiveReader(FD_t fd, rpmfiles files);

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
 * @param nodigest	omit checksum check if 1
 * @return		> 0 on error
 */
int rpmfiArchiveReadToFile(rpmfi fi, FD_t fd, int nodigest);

#ifdef __cplusplus
}
#endif

#endif	/* _RPMFI_INTERNAL_H */

