#ifndef H_ROLLBACK
#define H_ROLLBACK

/** \file lib/rollback.h
 */

#include "depends.h"
#include "install.h"

/**
 */
typedef /*@abstract@*/ struct fsm_s * FSM_t;

#include "cpio.h"

/**
 */
#define	FSM_VERBOSE	0x8000
#define	FSM_INTERNAL	0x4000
#define	FSM_SYSCALL	0x2000
#define	FSM_DEAD	0x1000
#define	_fv(_a)		((_a) | FSM_VERBOSE)
#define	_fi(_a)		((_a) | FSM_INTERNAL)
#define	_fs(_a)		((_a) | (FSM_INTERNAL | FSM_SYSCALL))
#define	_fd(_a)		((_a) | (FSM_INTERNAL | FSM_DEAD))
typedef enum fileStage_e {
    FSM_UNKNOWN =   0,
    FSM_INIT	=  _fd(1),
    FSM_PRE	=  _fd(2),
    FSM_PROCESS	=  _fv(3),
    FSM_POST	=  _fd(4),
    FSM_UNDO	=  5,
    FSM_FINI	=  6,

    FSM_INSTALL =  7,
    FSM_ERASE	=  8,
    FSM_BUILD	=  9,

    FSM_CREATE	=  _fi(17),
    FSM_MAP	=  _fi(18),
    FSM_MKDIRS	=  _fi(19),
    FSM_RMDIRS	=  _fi(20),
    FSM_MKLINKS	=  _fi(21),
    FSM_NOTIFY	=  _fd(22),
    FSM_DESTROY	=  _fi(23),
    FSM_VERIFY	=  _fd(24),
    FSM_COMMIT	=  _fd(25),

    FSM_UNLINK	=  _fs(33),
    FSM_RENAME	=  _fs(34),
    FSM_MKDIR	=  _fs(35),
    FSM_RMDIR	=  _fs(36),
    FSM_CHOWN	=  _fs(37),
    FSM_LCHOWN	=  _fs(38),
    FSM_CHMOD	=  _fs(39),
    FSM_UTIME	=  _fs(40),
    FSM_SYMLINK	=  _fs(41),
    FSM_LINK	=  _fs(42),
    FSM_MKFIFO	=  _fs(43),
    FSM_MKNOD	=  _fs(44),
    FSM_LSTAT	=  _fs(45),
    FSM_STAT	=  _fs(46),
    FSM_READLINK=  _fs(47),
    FSM_CHROOT	=  _fs(48),

    FSM_NEXT	=  _fd(65),
    FSM_EAT	=  _fd(66),
    FSM_POS	=  _fd(67),
    FSM_PAD	=  _fd(68),
    FSM_TRAILER	=  _fd(69),
    FSM_HREAD	=  _fd(70),
    FSM_HWRITE	=  _fd(71),
    FSM_DREAD	=  _fs(72),
    FSM_DWRITE	=  _fs(73),

    FSM_ROPEN	=  _fs(129),
    FSM_READ	=  _fs(130),
    FSM_RCLOSE	=  _fs(131),
    FSM_WOPEN	=  _fs(132),
    FSM_WRITE	=  _fs(133),
    FSM_WCLOSE	=  _fs(134),
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
struct transactionFileInfo_s {
  /* for all packages */
    enum rpmTransactionType type;
/*@owned@*/ fileAction * actions;	/*!< File disposition(s) */
/*@owned@*/ struct fingerPrint_s * fps;	/*!< File fingerprint(s) */
    HGE_t hge;			/*!< Vector to headerGetEntry() */
    HFD_t hfd;			/*!< Vector to headerFreeData() */
    Header h;			/*!< Package header */
/*@owned@*/ const char * name;
/*@owned@*/ const char * version;
/*@owned@*/ const char * release;
    int_32 epoch;
    const uint_32 * fflags;	/*!< File flags (from header) */
    const uint_32 * fsizes;	/*!< File sizes (from header) */
/*@owned@*/ const char ** bnl;	/*!< Base names (from header) */
/*@owned@*/ const char ** dnl;	/*!< Directory names (from header) */
    int_32 * dil;		/*!< Directory indices (from header) */
/*@owned@*/ const char ** obnl;	/*!< Original base names (from header) */
/*@owned@*/ const char ** odnl;	/*!< Original directory names (from header) */
    int_32 * odil;		/*!< Original directory indices (from header) */
/*@owned@*/ const char ** fmd5s;/*!< File MD5 sums (from header) */
/*@owned@*/ const char ** flinks;	/*!< File links (from header) */
/* XXX setuid/setgid bits are turned off if fuser/fgroup doesn't map. */
    uint_16 * fmodes;		/*!< File modes (from header) */
/*@owned@*/ char * fstates;	/*!< File states (from header) */
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
    unsigned int archiveSize;
    mode_t dperms;		/*!< Directory perms (0755) if unmapped. */
    mode_t fperms;		/*!< File perms (0644) if unmapped. */
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
 * Create file state machine instance.
 * @return		file state machine data
 */
/*@only@*/ /*@null@*/ FSM_t newFSM(void);

/**
 * Destroy file state machine instance.
 * @param fsm		file state machine data
 * @return		always NULL
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
 * @todo Eliminate.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param i		file index
 * @param a		file stage
 * @return		0 on success, 1 on failure
 */
int pkgAction(const rpmTransactionSet ts, TFI_t fi, int i, fileStage a);

/**
 * Perform package pre-install and remove actions.
 * @todo Eliminate.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param a		file stage
 * @return		0 on success, otherwise no. of failures
 */
int pkgActions(const rpmTransactionSet ts, TFI_t fi, fileStage a);

/**
 * Load external data into file state machine.
 * @param fsm		file state machine data
 * @param goal
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param archiveSize	pointer to archive size
 * @param failedFile	pointer to first file name that failed.
 * @return		0 on success
 */
int fsmSetup(FSM_t fsm, fileStage goal,
	/*@kept@*/ const rpmTransactionSet ts,
	/*@kept@*/ const TFI_t fi,
	FD_t cfd,
	/*@out@*/ unsigned int * archiveSize,
	/*@out@*/ const char ** failedFile)
		/*@modifies fsm, *archiveSize, *failedFile  @*/;

/**
 * Clean file state machine.
 * @param fsm		file state machine data
 * @return		0 on success
 */
int fsmTeardown(FSM_t fsm)
		/*@modifies fsm @*/;

/**
 * Retrieve transaction set from file state machine iterator.
 * @param fsm		file state machine data
 * @return		transaction set
 */
/*@kept@*/ rpmTransactionSet fsmGetTs(const FSM_t fsm)	/*@*/;

/**
 * Retrieve transaction element file info from file state machine iterator.
 * @param fsm		file state machine data
 * @return		transaction element file info
 */
/*@kept@*/ TFI_t fsmGetFi(const FSM_t fsm)	/*@*/;

/**
 * Map next file path and action.
 * @param fsm		file state machine data
 */
int fsmMapPath(FSM_t fsm)
		/*@modifies fsm @*/;

/**
 * Map file stat(2) info.
 * @param fsm		file state machine data
 */
int fsmMapAttrs(FSM_t fsm)
		/*@modifies fsm @*/;

/**
 * File state machine driver.
 * @param fsm		file state machine data
 * @param stage		next stage
 * @return		0 on success
 */
int fsmStage(FSM_t fsm, fileStage stage)
		/*@modifies fsm @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_ROLLBACK */
