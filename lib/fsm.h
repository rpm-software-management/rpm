#ifndef H_FSM
#define H_FSM

/** \file lib/fsm.h
 */

#include <rpmlib.h>
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

    FSM_PKGINSTALL	= _fd(7),
    FSM_PKGERASE	= _fd(8),
    FSM_PKGBUILD	= _fd(9),
    FSM_PKGCOMMIT	= _fd(10),
    FSM_PKGUNDO		= _fd(11),

    FSM_CREATE	=  _fd(17),
    FSM_MAP	=  _fd(18),
    FSM_MKDIRS	=  _fi(19),
    FSM_RMDIRS	=  _fi(20),
    FSM_MKLINKS	=  _fi(21),
    FSM_NOTIFY	=  _fd(22),
    FSM_DESTROY	=  _fd(23),
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
#undef	_fv
#undef	_fi
#undef	_fs
#undef	_fd

/** \ingroup payload
 * Keeps track of the set of all hard links to a file in an archive.
 */
struct hardLink {
/*@owned@*/ struct hardLink * next;
/*@owned@*/ const char ** nsuffix;
/*@owned@*/ int * filex;
    dev_t dev;
    ino_t inode;
    int nlink;
    int linksLeft;
    int linkIndex;
    int createdPath;
};

/** \ingroup payload
 * Iterator across package file info, forward on install, backward on erase.
 */
struct fsmIterator_s {
/*@kept@*/ rpmTransactionSet ts;	/*!< transaction set. */
/*@kept@*/ TFI_t fi;			/*!< transaction element file info. */
    int reverse;			/*!< reversed traversal? */
    int isave;				/*!< last returned iterator index. */
    int i;				/*!< iterator index. */
};

/** \ingroup payload
 * File name and stat information.
 */
struct fsm_s {
/*@owned@*/ const char * path;		/*!< Current file name. */
/*@owned@*/ const char * opath;		/*!< Original file name. */
    FD_t cfd;				/*!< Payload file handle. */
    FD_t rfd;				/*!<  read: File handle. */
/*@dependent@*/ char * rdbuf;		/*!<  read: Buffer. */
/*@owned@*/ char * rdb;			/*!<  read: Buffer allocated. */
    size_t rdsize;			/*!<  read: Buffer allocated size. */
    size_t rdlen;			/*!<  read: Number of bytes requested. */
    size_t rdnb;			/*!<  read: Number of bytes returned. */
    FD_t wfd;				/*!< write: File handle. */
/*@dependent@*/ char * wrbuf;		/*!< write: Buffer. */
/*@owned@*/ char * wrb;			/*!< write: Buffer allocated. */
    size_t wrsize;			/*!< write: Buffer allocated size. */
    size_t wrlen;			/*!< write: Number of bytes requested. */
    size_t wrnb;			/*!< write: Number of bytes returned. */
/*@only@*/ FSMI_t iter;			/*!< File iterator. */
    int ix;				/*!< Current file iterator index. */
/*@only@*/ struct hardLink * links;	/*!< Pending hard linked file(s). */
/*@only@*/ struct hardLink * li;	/*!< Current hard linked file(s). */
/*@kept@*/ unsigned int * archiveSize;	/*!< Pointer to archive size. */
/*@kept@*/ const char ** failedFile;	/*!< First file name that failed. */
/*@shared@*/ const char * subdir;	/*!< Current file sub-directory. */
    char subbuf[64];	/* XXX eliminate */
/*@observer@*/ const char * osuffix;	/*!< Old, preserved, file suffix. */
/*@observer@*/ const char * nsuffix;	/*!< New, created, file suffix. */
/*@shared@*/ const char * suffix;	/*!< Current file suffix. */
    char sufbuf[64];	/* XXX eliminate */
/*@only@*/ short * dnlx;		/*!< Last dirpath verified indexes. */
/*@only@*/ char * ldn;			/*!< Last dirpath verified. */
    int ldnlen;				/*!< Last dirpath current length. */
    int ldnalloc;			/*!< Last dirpath allocated length. */
    int postpone;			/*!< Skip remaining stages? */
    int diskchecked;			/*!< Has stat(2) been performed? */
    int exists;				/*!< Does current file exist on disk? */
    int mkdirsdone;			/*!< Have "orphan" dirs been created? */
    int astriplen;			/*!< Length of buildroot prefix. */
    int rc;				/*!< External file stage return code. */
    int commit;				/*!< Commit synchronously? */
    cpioMapFlags mapFlags;		/*!< Bit(s) to control mapping. */
/*@shared@*/ const char * dirName;	/*!< File directory name. */
/*@shared@*/ const char * baseName;	/*!< File base name. */
/*@shared@*/ const char * fmd5sum;	/*!< File MD5 sum (NULL disables). */
    unsigned fflags;			/*!< File flags. */
    fileAction action;			/*!< File disposition. */
    fileStage goal;			/*!< Package state machine goal. */
    fileStage stage;			/*!< External file stage. */
    struct stat sb;			/*!< Current file stat(2) info. */
    struct stat osb;			/*!< Original file stat(2) info. */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return formatted string representation of file stages.
 * @param a		file stage
 * @return		formatted string
 */
/*@observer@*/ const char *const fileStageString(fileStage a);

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

#endif	/* H_FSM */
