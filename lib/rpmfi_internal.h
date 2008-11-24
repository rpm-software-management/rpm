#ifndef _RPMFI_INTERNAL_H
#define _RPMFI_INTERNAL_H

#include <rpm/header.h>
#include <rpm/rpmfi.h>
#include "lib/fsm.h"		/* for FSM_t */

/**
 */
typedef struct sharedFileInfo_s *		sharedFileInfo;

/**
 */
struct sharedFileInfo_s {
    int pkgFileNum;
    int otherPkg;
    int otherFileNum;
};

/* 
 * This limits maximum unique strings (user + group names) from packages to 
 * 65535, should be plenty but easy to bump if ever needed.
 */
typedef uint16_t scidx_t;
typedef struct strcache_s *strcache;

/**
 * A package filename set.
 */
struct rpmfi_s {
    int i;			/*!< Current file index. */
    int j;			/*!< Current directory index. */

    const char * Type;		/*!< Tag name. */

    rpmTag tagN;		/*!< Header tag. */
    Header h;			/*!< Header for file info set (or NULL) */

/*?null?*/
    const char ** bnl;		/*!< Base name(s) (from header) */
/*?null?*/
    const char ** dnl;		/*!< Directory name(s) (from header) */

    strcache flinkcache;	/*!< File link cache */
    scidx_t * flinks;		/*!< Index to file link(s) cache */
    scidx_t * flangs;		/*!< Index to file lang(s) cache */

          uint32_t * dil;	/*!< Directory indice(s) (from header) */
/*?null?*/
    const rpm_flag_t * fflags;	/*!< File flag(s) (from header) */
/*?null?*/
    const rpm_off_t * fsizes;	/*!< File size(s) (from header) */
/*?null?*/
    const rpm_time_t * fmtimes;	/*!< File modification time(s) (from header) */
/*?null?*/
          rpm_mode_t * fmodes;	/*!< File mode(s) (from header) */
/*?null?*/
    const rpm_rdev_t * frdevs;	/*!< File rdev(s) (from header) */
/*?null?*/
    const rpm_ino_t * finodes;	/*!< File inodes(s) (from header) */

    scidx_t *fuser;		/*!< Index to file owner(s) cache */
    scidx_t *fgroup;		/*!< Index to file group(s) cache */

    char * fstates;		/*!< File state(s) (from header) */

    const rpm_color_t * fcolors;/*!< File color bits (header) */
    const char ** fcaps;	/*! File capabilities (header) */

    const char ** cdict;	/*!< File class dictionary (header) */
    rpm_count_t ncdict;		/*!< No. of class entries. */
    const uint32_t * fcdictx;	/*!< File class dictionary index (header) */

    const uint32_t * ddict;	/*!< File depends dictionary (header) */
    rpm_count_t nddict;		/*!< No. of depends entries. */
    const uint32_t * fddictx;	/*!< File depends dictionary start (header) */
    const uint32_t * fddictn;	/*!< File depends dictionary count (header) */

/*?null?*/
    const rpm_flag_t * vflags;	/*!< File verify flag(s) (from header) */

    rpm_count_t dc;		/*!< No. of directories. */
    rpm_count_t fc;		/*!< No. of files. */

/*=============================*/
    rpmte te;

    rpmfiFlags fiflags;		/*!< file info set control flags */
    headerGetFlags scareFlags;	/*!< headerGet flags wrt scareMem */
/*-----------------------------*/
    rpmFileAction * actions;	/*!< File disposition(s). */
    struct fingerPrint_s * fps;	/*!< File fingerprint(s). */

    pgpHashAlgo digestalgo;	/*!< File checksum algorithm */
    unsigned char * digests;	/*!< File checksums in binary. */

    char * fn;			/*!< File name buffer. */

    size_t striplen;
    rpm_loff_t archiveSize;
    char ** apath;
    FSM_t fsm;			/*!< File state machine data. */
    sharedFileInfo replaced;	/*!< (TR_ADDED) */
    int numReplaced;
    int allocatedReplaced;
    rpm_off_t * replacedSizes;	/*!< (TR_ADDED) */
    int magic;
#define	RPMFIMAGIC	0x09697923
/*=============================*/

int nrefs;		/*!< Reference count. */
};

RPM_GNUC_INTERNAL
rpmfi rpmfiUpdateState(rpmfi fi, rpmts ts, rpmte p);

RPM_GNUC_INTERNAL
void rpmfiSetFState(rpmfi fi, int ix, rpmfileState state);

RPM_GNUC_INTERNAL
void rpmfiSetFReplacedSize(rpmfi fi, rpm_loff_t newsize);

RPM_GNUC_INTERNAL
rpm_loff_t rpmfiFReplacedSize(rpmfi fi);

RPM_GNUC_INTERNAL
void rpmfiAddReplaced(rpmfi fi, int pkgFileNum, int otherPkg, int otherFileNum);

RPM_GNUC_INTERNAL
sharedFileInfo rpmfiGetReplaced(rpmfi fi);

RPM_GNUC_INTERNAL
sharedFileInfo rpmfiNextReplaced(rpmfi fi , sharedFileInfo replaced);

/* XXX can't be internal as build code needs this */
FSM_t rpmfiFSM(rpmfi fi);
#endif	/* _RPMFI_INTERNAL_H */

