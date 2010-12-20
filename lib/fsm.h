#ifndef H_FSM
#define H_FSM

/** \ingroup payload
 * \file lib/fsm.h
 * File state machine to handle a payload within an rpm package.
 */

#include <rpm/rpmfi.h>

extern int _fsm_debug;

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
    FSM_LSETFCON=  _fs(39),
    FSM_CHOWN	=  _fs(40),
    FSM_LCHOWN	=  _fs(41),
    FSM_CHMOD	=  _fs(42),
    FSM_UTIME	=  _fs(43),
    FSM_SYMLINK	=  _fs(44),
    FSM_LINK	=  _fs(45),
    FSM_MKFIFO	=  _fs(46),
    FSM_MKNOD	=  _fs(47),
    FSM_LSTAT	=  _fs(48),
    FSM_STAT	=  _fs(49),
    FSM_READLINK=  _fs(50),
    FSM_SETCAP	=  _fs(52),

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
    FSM_WCLOSE	=  _fs(134)
} fileStage;
#undef	_fv
#undef	_fi
#undef	_fs
#undef	_fd

/** \ingroup payload
 */
enum cpioMapFlags_e {
    CPIO_MAP_PATH	= (1 << 0),
    CPIO_MAP_MODE	= (1 << 1),
    CPIO_MAP_UID	= (1 << 2),
    CPIO_MAP_GID	= (1 << 3),
    CPIO_FOLLOW_SYMLINKS= (1 << 4), /*!< only for building. */
    CPIO_MAP_ABSOLUTE	= (1 << 5),
    CPIO_MAP_ADDDOT	= (1 << 6),
    CPIO_ALL_HARDLINKS	= (1 << 7), /*!< fail if hardlinks are missing. */
    CPIO_MAP_TYPE	= (1 << 8),  /*!< only for building. */
    CPIO_SBIT_CHECK	= (1 << 9)
};
typedef rpmFlags cpioMapFlags;

typedef struct fsmIterator_s * FSMI_t;
typedef struct fsm_s * FSM_t;

typedef struct hardLink_s * hardLink_t;

/** \ingroup payload
 * File name and stat information.
 */
struct fsm_s {
    char * path;		/*!< Current file name. */
    char * opath;		/*!< Original file name. */
    FD_t cfd;			/*!< Payload file handle. */
    FD_t rfd;			/*!<  read: File handle. */
    char * rdbuf;		/*!<  read: Buffer. */
    char * rdb;			/*!<  read: Buffer allocated. */
    size_t rdsize;		/*!<  read: Buffer allocated size. */
    size_t rdlen;		/*!<  read: Number of bytes requested.*/
    size_t rdnb;		/*!<  read: Number of bytes returned. */
    FD_t wfd;			/*!< write: File handle. */
    char * wrbuf;		/*!< write: Buffer. */
    char * wrb;			/*!< write: Buffer allocated. */
    size_t wrsize;		/*!< write: Buffer allocated size. */
    size_t wrlen;		/*!< write: Number of bytes requested.*/
    size_t wrnb;		/*!< write: Number of bytes returned. */
    FSMI_t iter;		/*!< File iterator. */
    int ix;			/*!< Current file iterator index. */
    hardLink_t links;		/*!< Pending hard linked file(s). */
    hardLink_t li;		/*!< Current hard linked file(s). */
    rpm_loff_t * archiveSize;	/*!< Pointer to archive size. */
    rpm_loff_t archivePos;	/*!< Current archive position. */
    rpm_loff_t cpioPos;
    char ** failedFile;		/*!< First file name that failed. */
    const char * subdir;	/*!< Current file sub-directory. */
    const char * osuffix;	/*!< Old, preserved, file suffix. */
    const char * nsuffix;	/*!< New, created, file suffix. */
    const char * suffix;	/*!< Current file suffix. */
    char sufbuf[64];	/* XXX eliminate */
    short * dnlx;		/*!< Last dirpath verified indexes. */
    char * ldn;			/*!< Last dirpath verified. */
    int ldnlen;			/*!< Last dirpath current length. */
    int ldnalloc;		/*!< Last dirpath allocated length. */
    int postpone;		/*!< Skip remaining stages? */
    int diskchecked;		/*!< Has stat(2) been performed? */
    int exists;			/*!< Does current file exist on disk? */
    int mkdirsdone;		/*!< Have "orphan" dirs been created? */
    int rc;			/*!< External file stage return code. */
    cpioMapFlags mapFlags;	/*!< Bit(s) to control mapping. */
    const char * dirName;	/*!< File directory name. */
    const char * baseName;	/*!< File base name. */
    const unsigned char * digest;	/*!< Binary digest (NULL disables). */
    security_context_t fcontext;/*!< File security context (NULL disables). */
    void *fcaps;		/*!< File capabilities */
    pgpHashAlgo digestalgo;	/*!< File digest algorithm */
    
    unsigned fflags;		/*!< File flags. */
    rpmFileAction action;	/*!< File disposition. */
    fileStage goal;		/*!< Package state machine goal. */
    fileStage stage;		/*!< External file stage. */
    fileStage nstage;		/*!< Next file stage. */
    struct stat sb;		/*!< Current file stat(2) info. */
    struct stat osb;		/*!< Original file stat(2) info. */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create file state machine instance.
 * @param mapflags	CPIO map flags to use
 * @return		file state machine
 */
RPM_GNUC_INTERNAL
FSM_t newFSM(cpioMapFlags mapflags);

/**
 * Destroy file state machine instance.
 * @param fsm		file state machine
 * @return		always NULL
 */
RPM_GNUC_INTERNAL
FSM_t freeFSM(FSM_t fsm);

/**
 * Load external data into file state machine.
 * @param fsm		file state machine
 * @param goal
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param cfd
 * @retval archiveSize	pointer to archive size
 * @retval failedFile	pointer to first file name that failed (malloced)
 * @return		0 on success
 */
int fsmSetup(FSM_t fsm, fileStage goal,
		rpmts ts,
		rpmte te,
		rpmfi fi,
		FD_t cfd,
		rpm_loff_t * archiveSize,
		char ** failedFile);

/**
 * Clean file state machine.
 * @param fsm		file state machine
 * @return		0 on success
 */
int fsmTeardown(FSM_t fsm);

/**
 * File state machine driver.
 * @param fsm		file state machine
 * @param nstage		next stage
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int fsmNext(FSM_t fsm, fileStage nstage);

#ifdef __cplusplus
}
#endif

#endif	/* H_FSM */
