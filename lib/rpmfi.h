#ifndef H_RPMFI
#define H_RPMFI

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmfi.h
 * Structure(s) used for file info tag sets.
 */

#include <rpm/rpmtypes.h>
#include <rpm/rpmvf.h>

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

/**
 * File Attributes.
 */
typedef	enum rpmfileAttrs_e {
    RPMFILE_NONE	= 0,
    RPMFILE_CONFIG	= (1 <<  0),	/*!< from %%config */
    RPMFILE_DOC		= (1 <<  1),	/*!< from %%doc */
    RPMFILE_ICON	= (1 <<  2),	/*!< from %%donotuse. */
    RPMFILE_MISSINGOK	= (1 <<  3),	/*!< from %%config(missingok) */
    RPMFILE_NOREPLACE	= (1 <<  4),	/*!< from %%config(noreplace) */
    RPMFILE_SPECFILE	= (1 <<  5),	/*!< @todo (unnecessary) marks 1st file in srpm. */
    RPMFILE_GHOST	= (1 <<  6),	/*!< from %%ghost */
    RPMFILE_LICENSE	= (1 <<  7),	/*!< from %%license */
    RPMFILE_README	= (1 <<  8),	/*!< from %%readme */
    RPMFILE_EXCLUDE	= (1 <<  9),	/*!< from %%exclude, internal */
    RPMFILE_UNPATCHED	= (1 << 10),	/*!< placeholder (SuSE) */
    RPMFILE_PUBKEY	= (1 << 11),	/*!< from %%pubkey */
    RPMFILE_POLICY	= (1 << 12)	/*!< from %%policy */
} rpmfileAttrs;

#define	RPMFILE_ALL	~(RPMFILE_NONE)

/** \ingroup rpmfi
 * File disposition(s) during package install/erase transaction.
 */
typedef enum rpmFileAction_e {
    FA_UNKNOWN = 0,	/*!< initial action for file ... */
    FA_CREATE,		/*!< ... copy in from payload. */
    FA_COPYIN,		/*!< ... copy in from payload. */
    FA_COPYOUT,		/*!< ... copy out to payload. */
    FA_BACKUP,		/*!< ... renamed with ".rpmorig" extension. */
    FA_SAVE,		/*!< ... renamed with ".rpmsave" extension. */
    FA_SKIP, 		/*!< ... already replaced, don't remove. */
    FA_ALTNAME,		/*!< ... create with ".rpmnew" extension. */
    FA_ERASE,		/*!< ... to be removed. */
    FA_SKIPNSTATE,	/*!< ... untouched, state "not installed". */
    FA_SKIPNETSHARED,	/*!< ... untouched, state "netshared". */
    FA_SKIPCOLOR	/*!< ... untouched, state "wrong color". */
} rpmFileAction;

#define XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED || (_a) == FA_SKIPCOLOR)

/**
 * We pass these around as an array with a sentinel.
 */
struct rpmRelocation_s {
    char * oldPath;	/*!< NULL here evals to RPMTAG_DEFAULTPREFIX, */
    char * newPath;	/*!< NULL means to omit the file completely! */
};

/** \ingroup rpmfi
 * Unreference a file info set instance.
 * @param fi		file info set
 * @param msg
 * @return		NULL always
 */
rpmfi rpmfiUnlink (rpmfi fi,
		const char * msg);

/** \ingroup rpmfi
 * Reference a file info set instance.
 * @param fi		file info set
 * @param msg
 * @return		new file info set reference
 */
rpmfi rpmfiLink (rpmfi fi, const char * msg);

/** \ingroup rpmfi
 * Return file count from file info set.
 * @param fi		file info set
 * @return		current file count
 */
rpm_count_t rpmfiFC(rpmfi fi);

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
rpm_count_t rpmfiDC(rpmfi fi);

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
const char * rpmfiBN(rpmfi fi);

/** \ingroup rpmfi
 * Return current directory name from file info set.
 * @param fi		file info set
 * @return		current directory, NULL on invalid
 */
const char * rpmfiDN(rpmfi fi);

/** \ingroup rpmfi
 * Return current file name from file info set.
 * @param fi		file info set
 * @return		current file name
 */
const char * rpmfiFN(rpmfi fi);

/** \ingroup rpmfi
 * Return current file flags from file info set.
 * @param fi		file info set
 * @return		current file flags, 0 on invalid
 */
rpmfileAttrs rpmfiFFlags(rpmfi fi);

/** \ingroup rpmfi
 * Return current file verify flags from file info set.
 * @param fi		file info set
 * @return		current file verify flags, 0 on invalid
 */
rpmVerifyAttrs rpmfiVFlags(rpmfi fi);

/** \ingroup rpmfi
 * Return current file mode from file info set.
 * @param fi		file info set
 * @return		current file mode, 0 on invalid
 */
rpm_mode_t rpmfiFMode(rpmfi fi);

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
const unsigned char * rpmfiMD5(rpmfi fi);

/** \ingroup rpmfi
 * Return current file linkto (i.e. symlink(2) target) from file info set.
 * @param fi		file info set
 * @return		current file linkto, NULL on invalid
 */
const char * rpmfiFLink(rpmfi fi);

/** \ingroup rpmfi
 * Return current file size from file info set.
 * @param fi		file info set
 * @return		current file size, 0 on invalid
 */
rpm_off_t rpmfiFSize(rpmfi fi);

/** \ingroup rpmfi
 * Return current file rdev from file info set.
 * @param fi		file info set
 * @return		current file rdev, 0 on invalid
 */
rpm_rdev_t rpmfiFRdev(rpmfi fi);

/** \ingroup rpmfi
 * Return current file inode from file info set.
 * @param fi		file info set
 * @return		current file inode, 0 on invalid
 */
rpm_ino_t rpmfiFInode(rpmfi fi);

/** \ingroup rpmfi
 * Return union of all file color bits from file info set.
 * @param fi		file info set
 * @return		current color
 */
rpm_color_t rpmfiColor(rpmfi fi);

/** \ingroup rpmfi
 * Return current file color bits from file info set.
 * @param fi		file info set
 * @return		current file color
 */
rpm_color_t rpmfiFColor(rpmfi fi);

/** \ingroup rpmfi
 * Return current file class from file info set.
 * @param fi		file info set
 * @return		current file class, 0 on invalid
 */
const char * rpmfiFClass(rpmfi fi);

/** \ingroup rpmfi
 * Return current file security context from file info set.
 * @param fi		file info set
 * @return		current file context, 0 on invalid
 */
const char * rpmfiFContext(rpmfi fi);

/** \ingroup rpmfi
 * Return current file depends dictionary from file info set.
 * @param fi		file info set
 * @retval *fddictp	file depends dictionary array (or NULL)
 * @return		no. of file depends entries, 0 on invalid
 */
uint32_t rpmfiFDepends(rpmfi fi,
		const uint32_t ** fddictp);

/** \ingroup rpmfi
 * Return (calculated) current file nlink count from file info set.
 * @param fi		file info set
 * @return		current file nlink count, 0 on invalid
 */
uint32_t rpmfiFNlink(rpmfi fi);

/** \ingroup rpmfi
 * Return current file modify time from file info set.
 * @param fi		file info set
 * @return		current file modify time, 0 on invalid
 */
rpm_time_t rpmfiFMtime(rpmfi fi);

/** \ingroup rpmfi
 * Return current file owner from file info set.
 * @param fi		file info set
 * @return		current file owner, NULL on invalid
 */
const char * rpmfiFUser(rpmfi fi);

/** \ingroup rpmfi
 * Return current file group from file info set.
 * @param fi		file info set
 * @return		current file group, NULL on invalid
 */
const char * rpmfiFGroup(rpmfi fi);

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
		const char *** fnp, rpm_count_t * fcp);

/** \ingroup rpmfi
 * Return file type from mode_t.
 * @param mode		file mode bits (from header)
 * @return		file type
 */
rpmFileTypes rpmfiWhatis(rpm_mode_t mode);

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
