#ifndef H_ROLLBACK
#define H_ROLLBACK

/** \file lib/rollback.h
 */

#include "depends.h"
#include "install.h"
#include "cpio.h"

/**
 */
typedef enum fileStage_e {
    FI_CREATE	= 0,
    FI_INIT,
    FI_MAP,
    FI_SKIP,
    FI_PRE,
    FI_PROCESS,
    FI_POST,
    FI_NOTIFY,
    FI_UNDO,
    FI_COMMIT,
    FI_DESTROY,
} fileStage;

/**
 * File disposition(s) during package install/erase transaction.
 */
typedef enum fileAction_e {
    FA_UNKNOWN = 0,	/*!< initial action (default action for source rpm) */
    FA_CREATE,		/*!< ... to be replaced. */
    FA_BACKUP,		/*!< ... renamed with ".rpmorig" extension. */
    FA_SAVE,		/*!< ... renamed with ".rpmsave" extension. */
    FA_SKIP, 		/*!< ... already replaced, don't remove. */
    FA_ALTNAME,		/*!< ... create with ".rpmnew" extension. */
    FA_REMOVE,		/*!< ... to be removed. */
    FA_SKIPNSTATE,	/*!< ... untouched, state "not installed". */
    FA_SKIPNETSHARED,	/*!< ... untouched, state "netshared". */
    FA_SKIPMULTILIB,	/*!< ... untouched. @todo state "multilib" ???. */
    FA_DONE = (1 << 31)
} fileAction;


#define XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED || (_a) == FA_SKIPMULTILIB)

/**
 */
typedef	enum rollbackDir_e {
    ROLLBACK_SAVE	= 1,	/*!< Save files. */
    ROLLBACK_RESTORE	= 2,	/*!< Restore files. */
} rollbackDir;

/**
 * File types.
 * These are the types of files used internally by rpm. The file
 * type is determined by applying stat(2) macros like S_ISDIR to
 * the file mode tag from a header. The values are arbitrary,
 * but are identical to the linux stat(2) file types.
 */
enum fileTypes {
    PIPE	= 1,	/*!< pipe/fifo */
    CDEV	= 2,	/*!< character device */
    XDIR	= 4,	/*!< directory */
    BDEV	= 6,	/*!< block device */
    REG		= 8,	/*!< regular file */
    LINK	= 10,	/*!< hard link */
    SOCK	= 12,	/*!< socket */
};

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
/*@owned@*/ fileAction * actions;	/*!< file disposition */
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

/**
 * Return formatted string representation of package disposition.
 * @param a		package dispostion
 * @return		formatted string
 */
/*@observer@*/ const char *const fiTypeString(TFI_t fi);

/**
 * Return formatted string representation of file stages.
 * @param a		file stage
 * @return		formatted string
 */
/*@observer@*/ const char *const fileStageString(fileStage a);

/**
 * Return formatted string representation of file disposition.
 * @param a		file dispostion
 * @return		formatted string
 */
/*@observer@*/ const char *const fileActionString(fileAction a);

/**
 * Perform package install/remove actions.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @return		0 on success, otherwise no. of failures
 */
int pkgActions(const rpmTransactionSet ts, TFI_t fi);

#ifdef __cplusplus
}
#endif

#endif	/* H_ROLLBACK */
