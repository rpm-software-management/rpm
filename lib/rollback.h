#ifndef H_ROLLBACK
#define H_ROLLBACK

/** \file lib/rollback.h
 */

#include "depends.h"
#include "install.h"
#include "cpio.h"

/**
 */
typedef int (*HGE_t) (Header h, int_32 tag, /*@out@*/ int_32 * type,
			/*@out@*/ void ** p, /*@out@*/int_32 * c)
				/*@modifies *type, *p, *c @*/;

/**
 */
struct transactionFileInfo_s {
  /* for all packages */
    enum rpmTransactionType type;
    enum fileActions * actions;	/*!< file disposition */
/*@dependent@*/ struct fingerPrint_s * fps; /*!< file fingerprints */
    HGE_t hge;
    Header h;			/*!< Package header */
    const char * name;
    const char * version;
    const char * release;
    const uint_32 * fflags;	/*!< File flags (from header) */
    const uint_32 * fsizes;	/*!< File sizes (from header) */
    const char ** bnl;		/*!< Base names (from header) */
    const char ** dnl;		/*!< Directory names (from header) */
    int * dil;			/*!< Directory indices (from header) */
    const char ** obnl;		/*!< Original Base names (from header) */
    const char ** odnl;		/*!< Original Directory names (from header) */
    int * odil;			/*!< Original Directory indices (from header) */
    const char ** fmd5s;	/*!< file MD5 sums (from header) */
    const char ** flinks;	/*!< file links (from header) */
/* XXX setuid/setgid bits are turned off if fuser/fgroup doesn't map. */
    uint_16 * fmodes;		/*!< file modes (from header) */
    char * fstates;		/*!< file states (from header) */
    const char ** fuser;	/*!< file owner(s) */
    const char ** fgroup;	/*!< file group(s) */
    const char ** flangs;	/*!< file lang(s) */
    int fc;			/*!< No. of files. */
    int dc;			/*!< No. of directories. */
    int bnlmax;			/*!< Length (in bytes) of longest base name. */
    int dnlmax;			/*!< Length (in bytes) of longest dir name. */
    int mapflags;
    int striplen;
    int scriptArg;
    const char ** apath;
    uid_t uid;
    gid_t gid;
    uid_t * fuids;
    gid_t * fgids;
    int * fmapflags;
    unsigned int archiveSize;
    int magic;
#define	TFIMAGIC	0x09697923
  /* these are for TR_ADDED packages */
    struct availablePackage * ap;
    struct sharedFileInfo * replaced;
    uint_32 * replacedSizes;
  /* for TR_REMOVED packages */
    unsigned int record;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Save/restore files from transaction element by renaming on file system.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param dir		save or restore?
 * @return		0 on success
 */
int dirstashPackage(const rpmTransactionSet ts, const TFI_t fi,
		rollbackDir dir);

/**
 */
void loadFi(Header h, TFI_t fi);

/**
 */
void freeFi(TFI_t fi);

#ifdef __cplusplus
}
#endif

#endif	/* H_ROLLBACK */
