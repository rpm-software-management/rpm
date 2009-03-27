#ifndef _RPMFI_INTERNAL_H
#define _RPMFI_INTERNAL_H

#include <rpm/header.h>
#include <rpm/rpmfi.h>
#include "lib/fsm.h"		/* for FSM_t */
#include "lib/fprint.h"

/* 
 * This limits maximum unique strings (user + group names) from packages to 
 * 65535, should be plenty but easy to bump if ever needed.
 */
typedef uint16_t scidx_t;
typedef struct strcache_s *strcache;

#define	RPMFIMAGIC	0x09697923

/**
 * A package filename set.
 */
struct rpmfi_s {
    int i;			/*!< Current file index. */
    int j;			/*!< Current directory index. */

    Header h;			/*!< Header for file info set (or NULL) */

    const char ** bnl;		/*!< Base name(s) (from header) */
    const char ** dnl;		/*!< Directory name(s) (from header) */

    strcache flinkcache;	/*!< File link cache */
    scidx_t * flinks;		/*!< Index to file link(s) cache */
    scidx_t * flangs;		/*!< Index to file lang(s) cache */

    uint32_t * dil;		/*!< Directory indice(s) (from header) */
    const rpm_flag_t * fflags;	/*!< File flag(s) (from header) */
    const rpm_off_t * fsizes;	/*!< File size(s) (from header) */
    const rpm_time_t * fmtimes;	/*!< File modification time(s) (from header) */
    rpm_mode_t * fmodes;	/*!< File mode(s) (from header) */
    const rpm_rdev_t * frdevs;	/*!< File rdev(s) (from header) */
    const rpm_ino_t * finodes;	/*!< File inodes(s) (from header) */

    scidx_t *fuser;		/*!< Index to file owner(s) cache */
    scidx_t *fgroup;		/*!< Index to file group(s) cache */

    char * fstates;		/*!< File state(s) (from header) */

    const rpm_color_t * fcolors;/*!< File color bits (header) */
    strcache fcapcache;		/*!< File capabilities cache */
    scidx_t * fcaps;		/*!< Index to file cap(s) cache */

    const char ** cdict;	/*!< File class dictionary (header) */
    rpm_count_t ncdict;		/*!< No. of class entries. */
    const uint32_t * fcdictx;	/*!< File class dictionary index (header) */

    const uint32_t * ddict;	/*!< File depends dictionary (header) */
    rpm_count_t nddict;		/*!< No. of depends entries. */
    const uint32_t * fddictx;	/*!< File depends dictionary start (header) */
    const uint32_t * fddictn;	/*!< File depends dictionary count (header) */
    const rpm_flag_t * vflags;	/*!< File verify flag(s) (from header) */

    rpm_count_t dc;		/*!< No. of directories. */
    rpm_count_t fc;		/*!< No. of files. */

    rpmfiFlags fiflags;		/*!< file info set control flags */
    headerGetFlags scareFlags;	/*!< headerGet flags wrt scareMem */

    struct fingerPrint_s * fps;	/*!< File fingerprint(s). */

    pgpHashAlgo digestalgo;	/*!< File digest algorithm */
    unsigned char * digests;	/*!< File digests in binary. */

    char * fn;			/*!< File name buffer. */

    size_t striplen;
    rpm_loff_t archiveSize;
    char ** apath;
    FSM_t fsm;			/*!< File state machine data. */
    rpm_off_t * replacedSizes;	/*!< (TR_ADDED) */
    int magic;
    int nrefs;		/*!< Reference count. */
};

RPM_GNUC_INTERNAL
int rpmfiDIIndex(rpmfi fi, int dx);

RPM_GNUC_INTERNAL
const char * rpmfiBNIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const char * rpmfiDNIndex(rpmfi fi, int jx);

RPM_GNUC_INTERNAL
const char * rpmfiFNIndex(rpmfi fi, int ix);

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
const char * rpmfiFLangsIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpmfileAttrs rpmfiFFlagsIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
rpm_mode_t rpmfiFModeIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
const unsigned char * rpmfiFDigestIndex(rpmfi fi, int ix, pgpHashAlgo *algo, size_t *len);

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
struct fingerPrint_s *rpmfiFpsIndex(rpmfi fi, int ix);

RPM_GNUC_INTERNAL
void rpmfiSetFReplacedSize(rpmfi fi, rpm_loff_t newsize);

RPM_GNUC_INTERNAL
rpm_loff_t rpmfiFReplacedSize(rpmfi fi);

RPM_GNUC_INTERNAL
void rpmfiFpLookup(rpmfi fi, fingerPrintCache fpc);

/* XXX can't be internal as build code needs this */
FSM_t rpmfiFSM(rpmfi fi);
#endif	/* _RPMFI_INTERNAL_H */

