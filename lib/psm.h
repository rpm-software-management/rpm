#ifndef H_PSM
#define H_PSM

/** \file lib/psm.h
 */

#include <rpmlib.h>
#include "depends.h"
#include "scriptlet.h"
#include "fsm.h"

/**
 */
typedef	enum rollbackDir_e {
    ROLLBACK_SAVE	= 1,	/*!< Save files. */
    ROLLBACK_RESTORE	= 2,	/*!< Restore files. */
} rollbackDir;

/**
 */
struct sharedFile {
    int mainFileNumber;
    int secRecOffset;
    int secFileNumber;
} ;

/**
 */
struct sharedFileInfo {
    int pkgFileNum;
    int otherFileNum;
    int otherPkg;
    int isRemoved;
};

/**
 */
struct transactionFileInfo_s {
  /* for all packages */
    enum rpmTransactionType type;
    fileAction action;		/*!< File disposition default. */
/*@owned@*/ fileAction * actions;	/*!< File disposition(s) */
/*@owned@*/ struct fingerPrint_s * fps;	/*!< File fingerprint(s) */
    HGE_t hge;			/*!< Vector to headerGetEntry() */
    HFD_t hfd;			/*!< Vector to headerFreeData() */
    Header h;			/*!< Package header */
/*@owned@*/ const char * name;
/*@owned@*/ const char * version;
/*@owned@*/ const char * release;
    int_32 epoch;
    uint_32 flags;		/*!< File flag default. */
    const uint_32 * fflags;	/*!< File flag(s) (from header) */
    const uint_32 * fsizes;	/*!< File size(s) (from header) */
    const uint_32 * fmtimes;	/*!< File modification time(s) (from header) */
/*@owned@*/ const char ** bnl;	/*!< Base name(s) (from header) */
/*@owned@*/ const char ** dnl;	/*!< Directory name(s) (from header) */
    int_32 * dil;		/*!< Directory indice(s) (from header) */
/*@owned@*/ const char ** obnl;	/*!< Original base name(s) (from header) */
/*@owned@*/ const char ** odnl;	/*!< Original directory name(s) (from header) */
    int_32 * odil;	/*!< Original directory indice(s) (from header) */
/*@owned@*/ const char ** fmd5s;/*!< File MD5 sum(s) (from header) */
/*@owned@*/ const char ** flinks;	/*!< File link(s) (from header) */
/* XXX setuid/setgid bits are turned off if fuser/fgroup doesn't map. */
    uint_16 * fmodes;		/*!< File mode(s) (from header) */
/*@owned@*/ char * fstates;	/*!< File state(s) (from header) */
/*@owned@*/ const char ** fuser;	/*!< File owner(s) */
/*@owned@*/ const char ** fgroup;	/*!< File group(s) */
/*@owned@*/ const char ** flangs;	/*!< File lang(s) */
    int fc;			/*!< No. of files. */
    int dc;			/*!< No. of directories. */
    int bnlmax;			/*!< Length (in bytes) of longest base name. */
    int dnlmax;			/*!< Length (in bytes) of longest dir name. */
    int astriplen;
    int striplen;
    int scriptArg;
    int chrootDone;
    unsigned int archiveSize;
    mode_t dperms;		/*!< Directory perms (0755) if not mapped. */
    mode_t fperms;		/*!< File perms (0644) if not mapped. */
/*@owned@*/ const char ** apath;
    int mapflags;
/*@owned@*/ int * fmapflags;
    uid_t uid;
/*@owned@*/ /*@null@*/ uid_t * fuids;	/*!< File uid(s) */
    gid_t gid;
/*@owned@*/ /*@null@*/ gid_t * fgids;	/*!< File gid(s) */
    int magic;
#define	TFIMAGIC	0x09697923
/*@owned@*/ FSM_t fsm;		/*!< File state machine data. */

  /* these are for TR_ADDED packages */
/*@dependent@*/ struct availablePackage * ap;
/*@owned@*/ struct sharedFileInfo * replaced;
/*@owned@*/ uint_32 * replacedSizes;
  /* for TR_REMOVED packages */
    unsigned int record;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load data from header into transaction file element info.
 * @param h		header
 * @param fi		transaction element file info
 */
void loadFi(Header h, TFI_t fi)
	/*@modifies h, fi @*/;

/**
 * Destroy transaction element file info.
 * @param fi		transaction element file info
 */
void freeFi(TFI_t fi)
	/*@modifies fi @*/;

/**
 * Return formatted string representation of package disposition.
 * @param a		package dispostion
 * @return		formatted string
 */
/*@observer@*/ const char *const fiTypeString(TFI_t fi);

/**
 * Return formatted string representation of file disposition.
 * @param a		file dispostion
 * @return		formatted string
 */
/*@observer@*/ const char *const fileActionString(fileAction a);

/**
 * Install binary package (from transaction set).
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int installBinaryPackage(const rpmTransactionSet ts, TFI_t fi);

/**
 * Erase binary package (from transaction set).
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @return		0 on success
 */
int removeBinaryPackage(const rpmTransactionSet ts, TFI_t fi);

/**
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @return		0 on success
 */
int repackage(const rpmTransactionSet ts, TFI_t fi);

#ifdef __cplusplus
}
#endif

#endif	/* H_ROLLBACK */
