#ifndef H_ROLLBACK
#define H_ROLLBACK

/** \file lib/rollback.h
 */

#include "depends.h"
#include "install.h"

/**
 */
typedef /*@abstract@*/ struct cpioHeader * FSM_t;

#include "cpio.h"

/**
 */
#define	FSM_INTERNAL	0x8000
#define	FSM_QUIET	0x4000
#define	_fi(_a)		((_a) | FSM_INTERNAL)
#define	_fq(_a)		((_a) | FSM_QUIET)
typedef enum fileStage_e {
    FSM_UNKNOWN =   0,
    FSM_INIT	=  _fq(1),
    FSM_PRE	=  _fq(2),
    FSM_PROCESS	=  _fq(3),
    FSM_POST	=  _fq(4),
    FSM_UNDO	=  5,
    FSM_COMMIT	=  6,

    FSM_INSTALL =  7,
    FSM_ERASE	=  8,
    FSM_BUILD	=  9,

    FSM_CREATE	=  _fi(17),
    FSM_MAP	=  _fi(18),
    FSM_MKDIRS	=  _fi(19),
    FSM_RMDIRS	=  _fi(20),
    FSM_MKLINKS	=  _fi(21),
    FSM_NOTIFY	=  _fi(22),
    FSM_DESTROY	=  _fi(23),
    FSM_VERIFY	=  _fi(24),
    FSM_FINALIZE=  _fi(25),

    FSM_UNLINK	=  _fi(33),
    FSM_RENAME	=  _fi(34),
    FSM_MKDIR	=  _fi(35),
    FSM_RMDIR	=  _fi(36),
    FSM_CHOWN	=  _fi(37),
    FSM_LCHOWN	=  _fi(38),
    FSM_CHMOD	=  _fi(39),
    FSM_UTIME	=  _fi(40),
    FSM_SYMLINK	=  _fi(41),
    FSM_LINK	=  _fi(42),
    FSM_MKFIFO	=  _fi(43),
    FSM_MKNOD	=  _fi(44),
    FSM_LSTAT	=  _fi(45),
    FSM_STAT	=  _fi(46),
    FSM_CHROOT	=  _fi(47),

    FSM_NEXT	=  _fi(65),
    FSM_EAT	=  _fi(66),
    FSM_POS	=  _fi(67),
    FSM_PAD	=  _fi(68),
} fileStage;
#undef	_fi

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
    int_32 epoch;
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
/*@owned@*/ FSM_t fsm;

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
 */
/*@only@*/ /*@null@*/ FSM_t newFSM(void);

/**
 */
/*@null@*/ FSM_t freeFSM(/*@only@*/ /*@null@*/ FSM_t fsm);

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
 * Perform package install/remove actions for s single file.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param i		file index
 * @param a		file stage
 * @return		0 on success, 1 on failure
 */
int pkgAction(const rpmTransactionSet ts, TFI_t fi, int i, fileStage a);

/**
 * Perform package pre-install and remove actions.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param a		file stage
 * @return		0 on success, otherwise no. of failures
 */
int pkgActions(const rpmTransactionSet ts, TFI_t fi, fileStage a);

/**
 * @return		0 on success
 */
int fsmSetup(FSM_t fsm, const rpmTransactionSet ts, const TFI_t fi, FD_t cfd,
	unsigned int * archiveSize, const char ** failedFile);

/**
 * @return		0 on success
 */
int fsmTeardown(FSM_t fsm);

/**
 */
/*@dependent@*/ rpmTransactionSet fsmGetTs(FSM_t fsm);

/**
 */
/*@dependent@*/ TFI_t fsmGetFi(FSM_t fsm);

/**
 */
int fsmGetIndex(FSM_t fsm);

/**
 */
fileAction fsmAction(FSM_t fsm,
	/*@out@*/ const char ** osuffix, /*@out@*/ const char ** suffix);

/**
 * Archive extraction state machine.
 * @param fsm		file state machine data
 * @param stage		next stage
 * @return		0 on success
 */
int fsmStage(FSM_t fsm, fileStage stage);

#ifdef __cplusplus
}
#endif

#endif	/* H_ROLLBACK */
