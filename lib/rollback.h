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
/*@owned@*/ enum fileActions * actions;	/*!< file disposition */
/*@owned@*/ struct fingerPrint_s * fps;	/*!< file fingerprints */
    HGE_t hge;
    Header h;			/*!< Package header */
/*@owned@*/ const char * name;
/*@owned@*/ const char * version;
/*@owned@*/ const char * release;
    const uint_32 * fflags;	/*!< File flags (from header) */
    const uint_32 * fsizes;	/*!< File sizes (from header) */
/*@owned@*/ const char ** bnl;	/*!< Base names (from header) */
/*@owned@*/ const char ** dnl;	/*!< Directory names (from header) */
    int_32 * dil;			/*!< Directory indices (from header) */
/*@owned@*/ const char ** obnl;	/*!< Original Base names (from header) */
/*@owned@*/ const char ** odnl;	/*!< Original Directory names (from header) */
    int_32 * odil;		/*!< Original Directory indices (from header) */
/*@owned@*/ const char ** fmd5s;/*!< file MD5 sums (from header) */
/*@owned@*/ const char ** flinks;	/*!< file links (from header) */
/* XXX setuid/setgid bits are turned off if fuser/fgroup doesn't map. */
    uint_16 * fmodes;		/*!< file modes (from header) */
/*@owned@*/ char * fstates;	/*!< file states (from header) */
/*@owned@*/ const char ** fuser;	/*!< file owner(s) */
/*@owned@*/ const char ** fgroup;	/*!< file group(s) */
/*@owned@*/ const char ** flangs;	/*!< file lang(s) */
    int fc;			/*!< No. of files. */
    int dc;			/*!< No. of directories. */
    int bnlmax;			/*!< Length (in bytes) of longest base name. */
    int dnlmax;			/*!< Length (in bytes) of longest dir name. */
    int striplen;
    int scriptArg;
    unsigned int archiveSize;
/*@owned@*/ const char ** apath;
    int mapflags;
/*@owned@*/ int * fmapflags;
    uid_t uid;
/*@owned@*/ uid_t * fuids;
    gid_t gid;
/*@owned@*/ gid_t * fgids;
    int magic;
#define	TFIMAGIC	0x09697923
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
 * Save/restore files from transaction element by renaming on file system.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param dir		save or restore?
 * @return		0 on success
 */
int dirstashPackage(const rpmTransactionSet ts, const TFI_t fi,
		rollbackDir dir);

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

#ifdef __cplusplus
}
#endif

#endif	/* H_ROLLBACK */
