#ifndef H_RPMFI
#define H_RPMFI

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmfi.h
 * Structure(s) used for file info tag sets.
 */

#include <rpmlib.h>	/* for rpmfi */

#ifdef __cplusplus
extern "C" {
#endif

extern int _rpmfi_debug;

/** \ingroup rpmfi
 * File types.
 * These are the file types used internally by rpm. The file
 * type is determined by applying stat(2) macros like S_ISDIR to
 * the file mode tag from a header. The values are arbitrary,
 * but are identical to the linux stat(2) file types.
 */
typedef enum rpmFileTypes_e {
    PIPE	=  1,	/*!< pipe/fifo */
    CDEV	=  2,	/*!< character device */
    XDIR	=  4,	/*!< directory */
    BDEV	=  6,	/*!< block device */
    REG		=  8,	/*!< regular file */
    LINK	= 10,	/*!< hard link */
    SOCK	= 12	/*!< socket */
} rpmFileTypes;

/**
 * File States (when installed).
 */
typedef enum rpmfileState_e {
    RPMFILE_STATE_NORMAL 	= 0,
    RPMFILE_STATE_REPLACED 	= 1,
    RPMFILE_STATE_NOTINSTALLED	= 2,
    RPMFILE_STATE_NETSHARED	= 3,
    RPMFILE_STATE_WRONGCOLOR	= 4
} rpmfileState;
#define	RPMFILE_STATE_MISSING	-1	/* XXX used for unavailable data */

#if defined(_RPMFI_INTERNAL)
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

    rpmTag tagN;		/*!< Header tag. */
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
    int32_t ncdict;		/*!< No. of class entries. */
    const uint32_t * fcdictx;	/*!< File class dictionary index (header) */

    const uint32_t * ddict;	/*!< File depends dictionary (header) */
    int32_t nddict;		/*!< No. of depends entries. */
    const uint32_t * fddictx;	/*!< File depends dictionary start (header) */
    const uint32_t * fddictn;	/*!< File depends dictionary count (header) */

/*?null?*/
    const uint32_t * vflags;	/*!< File verify flag(s) (from header) */

    int32_t dc;			/*!< No. of directories. */
    int32_t fc;			/*!< No. of files. */

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

#endif	/* _RPMFI_INTERNAL */

/** \ingroup rpmfi
 * Unreference a file info set instance.
 * @param fi		file info set
 * @param msg
 * @return		NULL always
 */
rpmfi rpmfiUnlink (rpmfi fi,
		const char * msg);

/** @todo Remove debugging entry from the ABI.
 * @param fi		file info set
 * @param msg
 * @param fn
 * @param ln
 * @return		NULL always
 */
rpmfi XrpmfiUnlink (rpmfi fi,
		const char * msg, const char * fn, unsigned ln);
#define	rpmfiUnlink(_fi, _msg) XrpmfiUnlink(_fi, _msg, __FILE__, __LINE__)

/** \ingroup rpmfi
 * Reference a file info set instance.
 * @param fi		file info set
 * @param msg
 * @return		new file info set reference
 */
rpmfi rpmfiLink (rpmfi fi, const char * msg);

/** @todo Remove debugging entry from the ABI.
 * @param fi		file info set
 * @param msg
 * @param fn
 * @param ln
 * @return		NULL always
 */
rpmfi XrpmfiLink (rpmfi fi, const char * msg,
		const char * fn, unsigned ln);
#define	rpmfiLink(_fi, _msg)	XrpmfiLink(_fi, _msg, __FILE__, __LINE__)

/** \ingroup rpmfi
 * Return file count from file info set.
 * @param fi		file info set
 * @return		current file count
 */
int rpmfiFC(rpmfi fi);

/** \ingroup rpmfi
 * Return current file index from file info set.
 * @param fi		file info set
 * @return		current file index
 */
int rpmfiFX(rpmfi fi);

/** \ingroup rpmfi
 * Set current file index in file info set.
 * @param fi		file info set
 * @param fx		new file index
 * @return		current file index
 */
int rpmfiSetFX(rpmfi fi, int fx);

/** \ingroup rpmfi
 * Return directory count from file info set.
 * @param fi		file info set
 * @return		current directory count
 */
int rpmfiDC(rpmfi fi);

/** \ingroup rpmfi
 * Return current directory index from file info set.
 * @param fi		file info set
 * @return		current directory index
 */
int rpmfiDX(rpmfi fi);

/** \ingroup rpmfi
 * Set current directory index in file info set.
 * @param fi		file info set
 * @param dx		new directory index
 * @return		current directory index
 */
int rpmfiSetDX(rpmfi fi, int dx);

/** \ingroup rpmfi
 * Return current base name from file info set.
 * @param fi		file info set
 * @return		current base name, NULL on invalid
 */
extern const char * rpmfiBN(rpmfi fi);

/** \ingroup rpmfi
 * Return current directory name from file info set.
 * @param fi		file info set
 * @return		current directory, NULL on invalid
 */
extern const char * rpmfiDN(rpmfi fi);

/** \ingroup rpmfi
 * Return current file name from file info set.
 * @param fi		file info set
 * @return		current file name
 */
extern const char * rpmfiFN(rpmfi fi);

/** \ingroup rpmfi
 * Return current file flags from file info set.
 * @param fi		file info set
 * @return		current file flags, 0 on invalid
 */
uint32_t rpmfiFFlags(rpmfi fi);

/** \ingroup rpmfi
 * Return current file verify flags from file info set.
 * @param fi		file info set
 * @return		current file verify flags, 0 on invalid
 */
uint32_t rpmfiVFlags(rpmfi fi);

/** \ingroup rpmfi
 * Return current file mode from file info set.
 * @param fi		file info set
 * @return		current file mode, 0 on invalid
 */
int16_t rpmfiFMode(rpmfi fi);

/** \ingroup rpmfi
 * Return current file state from file info set.
 * @param fi		file info set
 * @return		current file state, 0 on invalid
 */
rpmfileState rpmfiFState(rpmfi fi);

/** \ingroup rpmfi
 * Return current file (binary) md5 digest from file info set.
 * @param fi		file info set
 * @return		current file md5 digest, NULL on invalid
 */
extern const unsigned char * rpmfiMD5(rpmfi fi);

/** \ingroup rpmfi
 * Return current file linkto (i.e. symlink(2) target) from file info set.
 * @param fi		file info set
 * @return		current file linkto, NULL on invalid
 */
extern const char * rpmfiFLink(rpmfi fi);

/** \ingroup rpmfi
 * Return current file size from file info set.
 * @param fi		file info set
 * @return		current file size, 0 on invalid
 */
int32_t rpmfiFSize(rpmfi fi);

/** \ingroup rpmfi
 * Return current file rdev from file info set.
 * @param fi		file info set
 * @return		current file rdev, 0 on invalid
 */
int16_t rpmfiFRdev(rpmfi fi);

/** \ingroup rpmfi
 * Return current file inode from file info set.
 * @param fi		file info set
 * @return		current file inode, 0 on invalid
 */
int32_t rpmfiFInode(rpmfi fi);

/** \ingroup rpmfi
 * Return union of all file color bits from file info set.
 * @param fi		file info set
 * @return		current color
 */
uint32_t rpmfiColor(rpmfi fi);

/** \ingroup rpmfi
 * Return current file color bits from file info set.
 * @param fi		file info set
 * @return		current file color
 */
uint32_t rpmfiFColor(rpmfi fi);

/** \ingroup rpmfi
 * Return current file class from file info set.
 * @param fi		file info set
 * @return		current file class, 0 on invalid
 */
extern const char * rpmfiFClass(rpmfi fi);

/** \ingroup rpmfi
 * Return current file security context from file info set.
 * @param fi		file info set
 * @return		current file context, 0 on invalid
 */
extern const char * rpmfiFContext(rpmfi fi);

/** \ingroup rpmfi
 * Return current file depends dictionary from file info set.
 * @param fi		file info set
 * @retval *fddictp	file depends dictionary array (or NULL)
 * @return		no. of file depends entries, 0 on invalid
 */
int32_t rpmfiFDepends(rpmfi fi,
		const uint32_t ** fddictp);

/** \ingroup rpmfi
 * Return (calculated) current file nlink count from file info set.
 * @param fi		file info set
 * @return		current file nlink count, 0 on invalid
 */
int32_t rpmfiFNlink(rpmfi fi);

/** \ingroup rpmfi
 * Return current file modify time from file info set.
 * @param fi		file info set
 * @return		current file modify time, 0 on invalid
 */
int32_t rpmfiFMtime(rpmfi fi);

/** \ingroup rpmfi
 * Return current file owner from file info set.
 * @param fi		file info set
 * @return		current file owner, NULL on invalid
 */
extern const char * rpmfiFUser(rpmfi fi);

/** \ingroup rpmfi
 * Return current file group from file info set.
 * @param fi		file info set
 * @return		current file group, NULL on invalid
 */
extern const char * rpmfiFGroup(rpmfi fi);

/** \ingroup rpmfi
 * Return next file iterator index.
 * @param fi		file info set
 * @return		file iterator index, -1 on termination
 */
int rpmfiNext(rpmfi fi);

/** \ingroup rpmfi
 * Initialize file iterator index.
 * @param fi		file info set
 * @param fx		file iterator index
 * @return		file info set
 */
rpmfi rpmfiInit(rpmfi fi, int fx);

/** \ingroup rpmfi
 * Return next directory iterator index.
 * @param fi		file info set
 * @return		directory iterator index, -1 on termination
 */
int rpmfiNextD(rpmfi fi);

/** \ingroup rpmfi
 * Initialize directory iterator index.
 * @param fi		file info set
 * @param dx		directory iterator index
 * @return		file info set, NULL if dx is out of range
 */
rpmfi rpmfiInitD(rpmfi fi, int dx);

/** \ingroup rpmfi
 * Destroy a file info set.
 * @param fi		file info set
 * @return		NULL always
 */
rpmfi rpmfiFree(rpmfi fi);

/** \ingroup rpmfi
 * Create and load a file info set.
 * @deprecated Only scareMem = 0 will be permitted.
 * @param ts		transaction set (NULL skips path relocation)
 * @param h		header
 * @param tagN		RPMTAG_BASENAMES
 * @param scareMem	Use pointers to refcounted header memory?
 * @return		new file info set
 */
rpmfi rpmfiNew(const rpmts ts, Header h, rpmTag tagN, int scareMem);

/** \ingroup rpmfi
 * Retrieve file classes from header.
 *
 * This function is used to retrieve file classes from the header.
 * 
 * @param h		header
 * @retval *fclassp	array of file classes
 * @retval *fcp		number of files
 */
void rpmfiBuildFClasses(Header h,
		const char *** fclassp, int * fcp);


/** \ingroup rpmfi
 * Retrieve per-file dependencies from header.
 *
 * This function is used to retrieve per-file dependencies from the header.
 * 
 * @param h		header
 * @param tagN		RPMTAG_PROVIDENAME | RPMTAG_REQUIRENAME
 * @retval *fdepsp	array of file dependencies
 * @retval *fcp		number of files
 */
void rpmfiBuildFDeps(Header h, rpmTag tagN,
		const char *** fdepsp, int * fcp);

/** \ingroup rpmfi
 * Retrieve file names from header.
 *
 * The representation of file names in package headers changed in rpm-4.0.
 * Originally, file names were stored as an array of absolute paths.
 * In rpm-4.0, file names are stored as separate arrays of dirname's and
 * basename's, * with a dirname index to associate the correct dirname
 * with each basname.
 *
 * This function is used to retrieve file names independent of how the
 * file names are represented in the package header.
 * 
 * @param h		header
 * @param tagN		RPMTAG_BASENAMES | PMTAG_ORIGBASENAMES
 * @retval *fnp		array of file names
 * @retval *fcp		number of files
 */
void rpmfiBuildFNames(Header h, rpmTag tagN,
		const char *** fnp, int * fcp);

/** \ingroup rpmfi
 * Return file type from mode_t.
 * @param mode		file mode bits (from header)
 * @return		file type
 */
rpmFileTypes rpmfiWhatis(uint16_t mode);

/** \ingroup rpmfi
 * Return file info comparison.
 * @param afi		1st file info
 * @param bfi		2nd file info
 * @return		0 if identical
 */
int rpmfiCompare(const rpmfi afi, const rpmfi bfi);

/** \ingroup rpmfi
 * Return file disposition.
 * @param ofi		old file info
 * @param nfi		new file info
 * @param skipMissing	OK to skip missing files?
 * @return		file dispostion
 */
rpmFileAction rpmfiDecideFate(const rpmfi ofi, rpmfi nfi, int skipMissing);

/** \ingroup rpmfi
 * Return whether file is conflicting config
 * @param fi		file info
 * @return		1 if config file and file on disk conflicts
 */
int rpmfiConfigConflict(const rpmfi fi);

/** \ingroup rpmfi
 * Return formatted string representation of package disposition.
 * @param fi		file info set
 * @return		formatted string
 */
const char * rpmfiTypeString(rpmfi fi);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDS */
