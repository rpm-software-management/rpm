#ifndef _RPMFI_INTERNAL_H
#define _RPMFI_INTERNAL_H

#include <rpm/header.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmstrpool.h>
#include "lib/fprint.h"
#include "lib/cpio.h"

#define	RPMFIMAGIC	0x09697923

/**
 * A package filename set.
 */
struct rpmfi_s {
    int i;			/*!< Current file index. */
    int j;			/*!< Current directory index. */

    Header h;			/*!< Header for file info set (or NULL) */
    rpmstrPool pool;		/*!< String pool of this file info set */

    rpmsid * bnid;		/*!< Index to base name(s) (pool) */
    rpmsid * dnid;		/*!< Index to directory name(s) (pool) */

    rpmsid * flinks;		/*!< Index to file link(s) (pool) */

    uint32_t * dil;		/*!< Directory indice(s) (from header) */
    rpm_flag_t * fflags;	/*!< File flag(s) (from header) */
    rpm_off_t * fsizes;		/*!< File size(s) (from header) */
    rpm_loff_t * lfsizes;	/*!< File size(s) (from header) */
    rpm_time_t * fmtimes;	/*!< File modification time(s) (from header) */
    rpm_mode_t * fmodes;	/*!< File mode(s) (from header) */
    rpm_rdev_t * frdevs;	/*!< File rdev(s) (from header) */
    rpm_ino_t * finodes;	/*!< File inodes(s) (from header) */

    rpmsid * fuser;		/*!< Index to file owner(s) (misc pool) */
    rpmsid * fgroup;		/*!< Index to file group(s) (misc pool) */
    rpmsid * flangs;		/*!< Index to file lang(s) (misc pool) */

    char * fstates;		/*!< File state(s) (from header) */

    rpm_color_t * fcolors;	/*!< File color bits (header) */
    char ** fcaps;		/*!< File capability strings (header) */

    char ** cdict;		/*!< File class dictionary (header) */
    rpm_count_t ncdict;		/*!< No. of class entries. */
    uint32_t * fcdictx;		/*!< File class dictionary index (header) */

    uint32_t * ddict;		/*!< File depends dictionary (header) */
    rpm_count_t nddict;		/*!< No. of depends entries. */
    uint32_t * fddictx;		/*!< File depends dictionary start (header) */
    uint32_t * fddictn;		/*!< File depends dictionary count (header) */
    rpm_flag_t * vflags;	/*!< File verify flag(s) (from header) */

    rpm_count_t dc;		/*!< No. of directories. */
    rpm_count_t fc;		/*!< No. of files. */

    rpmfiFlags fiflags;		/*!< file info set control flags */

    struct fingerPrint_s * fps;	/*!< File fingerprint(s). */

    int digestalgo;		/*!< File digest algorithm */
    unsigned char * digests;	/*!< File digests in binary. */

    char * fn;			/*!< File name buffer. */

    char ** apath;
    struct nlinkHash_s * nlinks;/*!< Files connected by hardlinks */
    rpm_off_t * replacedSizes;	/*!< (TR_ADDED) */
    rpm_loff_t * replacedLSizes;/*!< (TR_ADDED) */
    rpmcpio_t archive;		/*!< Archive with payload */
    int magic;
    int nrefs;		/*!< Reference count. */
};

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
int rpmfiDIIndex(rpmfi fi, int dx);

RPM_GNUC_INTERNAL
rpmsid rpmfiBNIdIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpmsid rpmfiDNIdIndex(rpmfi fi, int jx);

RPM_GNUC_INTERNAL
const char * rpmfiBNIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiDNIndex(rpmfi fi, int jx);

RPM_GNUC_INTERNAL
char * rpmfiFNIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpmVerifyAttrs rpmfiVFlagsIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpmfileState rpmfiFStateIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFLinkIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpm_loff_t rpmfiFSizeIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpm_color_t rpmfiFColorIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFClassIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
uint32_t rpmfiFDependsIndex(rpmfi fi, int ix, const uint32_t ** fddictp);

RPM_GNUC_INTERNAL
uint32_t rpmfiFNlinkIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
uint32_t rpmfiFLinksIndex(rpmfi fi, int ix, const int ** files);

RPM_GNUC_INTERNAL
const char * rpmfiFLangsIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpmfileAttrs rpmfiFFlagsIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpm_mode_t rpmfiFModeIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const unsigned char * rpmfiFDigestIndex(rpmfi fi, int ix, int *algo, size_t *len);

RPM_GNUC_INTERNAL
rpm_rdev_t rpmfiFRdevIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpm_ino_t rpmfiFInodeIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpm_time_t rpmfiFMtimeIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFUserIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFGroupIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiFCapsIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
struct fingerPrint_s *rpmfiFps(rpmfi fi);

RPM_GNUC_INTERNAL
rpmFileAction rpmfiDecideFateIndex(rpmfi ofi, int oix, rpmfi nfi, int nix,
                                   int skipMissing);

RPM_GNUC_INTERNAL
int rpmfiCompareIndex(rpmfi afi, int aix, rpmfi bfi, int bix);

RPM_GNUC_INTERNAL
int rpmfiConfigConflictIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
void rpmfiSetFReplacedSizeIndex(rpmfi fi, int ix, rpm_loff_t newsize);

RPM_GNUC_INTERNAL
rpm_loff_t rpmfiFReplacedSizeIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
void rpmfiFpLookup(rpmfi fi, fingerPrintCache fpc);

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
size_t rpmfiArchiveWrite(rpmfi fi, void * buf, size_t size);

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

