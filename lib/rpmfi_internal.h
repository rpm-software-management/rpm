#ifndef _RPMFI_INTERNAL_H
#define _RPMFI_INTERNAL_H

#include <rpm/rpmfi.h>

/**
 */
typedef struct sharedFileInfo_s *		sharedFileInfo;

/**
 */
struct sharedFileInfo_s {
    int pkgFileNum;
    int otherFileNum;
    int otherPkg;
    int isRemoved;
};

/**
 * A package filename set.
 */
struct rpmfi_s {
    int i;			/*!< Current file index. */
    int j;			/*!< Current directory index. */

    const char * Type;		/*!< Tag name. */

    rpm_tag_t tagN;		/*!< Header tag. */
    Header h;			/*!< Header for file info set (or NULL) */

/*?null?*/
    const char ** bnl;		/*!< Base name(s) (from header) */
/*?null?*/
    const char ** dnl;		/*!< Directory name(s) (from header) */

    const char ** fmd5s;	/*!< File MD5 sum(s) (from header) */
    const char ** flinks;	/*!< File link(s) (from header) */
    const char ** flangs;	/*!< File lang(s) (from header) */

          uint32_t * dil;	/*!< Directory indice(s) (from header) */
/*?null?*/
    const uint32_t * fflags;	/*!< File flag(s) (from header) */
/*?null?*/
    const uint32_t * fsizes;	/*!< File size(s) (from header) */
/*?null?*/
    const uint32_t * fmtimes;	/*!< File modification time(s) (from header) */
/*?null?*/
          uint16_t * fmodes;	/*!< File mode(s) (from header) */
/*?null?*/
    const uint16_t * frdevs;	/*!< File rdev(s) (from header) */
/*?null?*/
    const uint32_t * finodes;	/*!< File inodes(s) (from header) */

    const char ** fuser;	/*!< File owner(s) (from header) */
    const char ** fgroup;	/*!< File group(s) (from header) */

    char * fstates;		/*!< File state(s) (from header) */

    const uint32_t * fcolors;	/*!< File color bits (header) */

    const char ** fcontexts;	/*! FIle security contexts. */

    const char ** cdict;	/*!< File class dictionary (header) */
    rpm_count_t ncdict;		/*!< No. of class entries. */
    const uint32_t * fcdictx;	/*!< File class dictionary index (header) */

    const uint32_t * ddict;	/*!< File depends dictionary (header) */
    rpm_count_t nddict;		/*!< No. of depends entries. */
    const uint32_t * fddictx;	/*!< File depends dictionary start (header) */
    const uint32_t * fddictn;	/*!< File depends dictionary count (header) */

/*?null?*/
    const uint32_t * vflags;	/*!< File verify flag(s) (from header) */

    rpm_count_t dc;		/*!< No. of directories. */
    rpm_count_t fc;		/*!< No. of files. */

/*=============================*/
    rpmte te;

    HGE_t hge;			/*!< Vector to headerGetEntry() */
    HAE_t hae;			/*!< Vector to headerAddEntry() */
    HME_t hme;			/*!< Vector to headerModifyEntry() */
    HRE_t hre;			/*!< Vector to headerRemoveEntry() */
    HFD_t hfd;			/*!< Vector to headerFreeData() */
/*-----------------------------*/
    uid_t uid;			/*!< File uid (default). */
    gid_t gid;			/*!< File gid (default). */
    uint32_t flags;		/*!< File flags (default). */
    rpmFileAction action;	/*!< File disposition (default). */
    rpmFileAction * actions;	/*!< File disposition(s). */
    struct fingerPrint_s * fps;	/*!< File fingerprint(s). */
    const char ** obnl;		/*!< Original basename(s) (from header) */
    const char ** odnl;		/*!< Original dirname(s) (from header) */
    int32_t * odil;		/*!< Original dirindex(s) (from header) */

    unsigned char * md5s;	/*!< File md5 sums in binary. */

    const char * pretrans;
    const char * pretransprog;
    const char * posttrans;
    const char * posttransprog;

    char * fn;			/*!< File name buffer. */
    int fnlen;			/*!< FIle name buffer length. */

    int astriplen;
    int striplen;
    unsigned int archivePos;
    unsigned int archiveSize;
    mode_t dperms;		/*!< Directory perms (0755) if not mapped. */
    mode_t fperms;		/*!< File perms (0644) if not mapped. */
    const char ** apath;
    int mapflags;
    int * fmapflags;
    FSM_t fsm;			/*!< File state machine data. */
    int keep_header;		/*!< Keep header? */
    uint32_t color;		/*!< Color bit(s) from file color union. */
    sharedFileInfo replaced;	/*!< (TR_ADDED) */
    uint32_t * replacedSizes;	/*!< (TR_ADDED) */
    unsigned int record;	/*!< (TR_REMOVED) */
    int magic;
#define	RPMFIMAGIC	0x09697923
/*=============================*/

int nrefs;		/*!< Reference count. */
};

#endif	/* _RPMFI_INTERNAL_H */

