#ifndef H_RPMLIB
#define	H_RPMLIB

/** \ingroup rpmcli rpmrc rpmdep rpmtrans rpmdb lead signature header payload dbi
 * \file lib/rpmlib.h
 *
 * In Memoriam: Steve Taylor <staylor@redhat.com> was here, now he's not.
 *
 */

#include <rpmio.h>
#include <header.h>
#include <rpmtag.h>
#include <popt.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Package read return codes.
 */
typedef	enum rpmRC_e {
    RPMRC_OK		= 0,	/*!< Generic success code */
    RPMRC_NOTFOUND	= 1,	/*!< Generic not found code. */
    RPMRC_FAIL		= 2,	/*!< Generic failure code. */
    RPMRC_NOTTRUSTED	= 3,	/*!< Signature is OK, but key is not trusted. */
    RPMRC_NOKEY		= 4	/*!< Public key is unavailable. */
} rpmRC;

extern struct rpmMacroContext_s * rpmGlobalMacroContext;

extern struct rpmMacroContext_s * rpmCLIMacroContext;

extern const char * RPMVERSION;

extern const char * rpmNAME;

extern const char * rpmEVR;

extern int rpmFLAGS;

/** \ingroup rpmtrans
 * The RPM Transaction Set.
 * Transaction sets are inherently unordered! RPM may reorder transaction
 * sets to reduce errors. In general, installs/upgrades are done before
 * strict removals, and prerequisite ordering is done on installs/upgrades.
 */
typedef struct rpmts_s * rpmts;

/** \ingroup rpmbuild
 */
typedef struct rpmSpec_s * rpmSpec;

/** \ingroup rpmds
 * Dependency tag sets from a header, so that a header can be discarded early.
 */
typedef struct rpmds_s * rpmds;

/** \ingroup rpmfi
 * File info tag sets from a header, so that a header can be discarded early.
 */
typedef struct rpmfi_s * rpmfi;

/** \ingroup rpmte
 * An element of a transaction set, i.e. a TR_ADDED or TR_REMOVED package.
 */
typedef struct rpmte_s * rpmte;

/** \ingroup rpmdb
 * Database of headers and tag value indices.
 */
typedef struct rpmdb_s * rpmdb;

/** \ingroup rpmdb
 * Database iterator.
 */
typedef struct _rpmdbMatchIterator * rpmdbMatchIterator;

/** \ingroup rpmgi
 * Generalized iterator.
 */
typedef struct rpmgi_s * rpmgi;

/** \ingroup header
 * Translate and merge legacy signature tags into header.
 * @todo Remove headerSort() through headerInitIterator() modifies sig.
 * @param h		header
 * @param sigh		signature header
 */
void headerMergeLegacySigs(Header h, const Header sigh);

/** \ingroup header
 * Regenerate signature header.
 * @todo Remove headerSort() through headerInitIterator() modifies h.
 * @param h		header
 * @param noArchiveSize	don't copy archive size tag (pre rpm-4.1)
 * @return		regenerated signature header
 */
Header headerRegenSigHeader(const Header h, int noArchiveSize);

/* ==================================================================== */
/** \name RPMRC */

/** \ingroup rpmrc
 * Build and install arch/os table identifiers.
 * @todo Eliminate from API.
 */
enum rpm_machtable_e {
    RPM_MACHTABLE_INSTARCH	= 0,	/*!< Install platform architecture. */
    RPM_MACHTABLE_INSTOS	= 1,	/*!< Install platform operating system. */
    RPM_MACHTABLE_BUILDARCH	= 2,	/*!< Build platform architecture. */
    RPM_MACHTABLE_BUILDOS	= 3	/*!< Build platform operating system. */
};
#define	RPM_MACHTABLE_COUNT	4	/*!< No. of arch/os tables. */

/** \ingroup rpmrc
 * Read macro configuration file(s) for a target.
 * @param file		colon separated files to read (NULL uses default)
 * @param target	target platform (NULL uses default)
 * @return		0 on success, -1 on error
 */
int rpmReadConfigFiles(const char * file,
		const char * target);

/** \ingroup rpmrc
 * Return current arch name and/or number.
 * @todo Generalize to extract arch component from target_platform macro.
 * @retval name		address of arch name (or NULL)
 * @retval num		address of arch number (or NULL)
 */
void rpmGetArchInfo( const char ** name,
		int * num);

/** \ingroup rpmrc
 * Return current os name and/or number.
 * @todo Generalize to extract os component from target_platform macro.
 * @retval name		address of os name (or NULL)
 * @retval num		address of os number (or NULL)
 */
void rpmGetOsInfo( const char ** name,
		int * num);

/** \ingroup rpmrc
 * Return arch/os score of a name.
 * An arch/os score measures the "nearness" of a name to the currently
 * running (or defined) platform arch/os. For example, the score of arch
 * "i586" on an i686 platform is (usually) 2. The arch/os score is used
 * to select one of several otherwise identical packages using the arch/os
 * tags from the header as hints of the intended platform for the package.
 * @todo Rewrite to use RE's against config.guess target platform output.
 *
 * @param type		any of the RPM_MACHTABLE_* constants
 * @param name		name
 * @return		arch score (0 is no match, lower is preferred)
 */
int rpmMachineScore(int type, const char * name);

/** \ingroup rpmrc
 * Display current rpmrc (and macro) configuration.
 * @param fp		output file handle
 * @return		0 always
 */
int rpmShowRC(FILE * fp);

/** \ingroup rpmrc
 * @deprecated Use addMacro to set _target_* macros.
 * @todo Eliminate from API.
 # @note Only used by build code.
 * @param archTable
 * @param osTable
 */
void rpmSetTables(int archTable, int osTable);

/** \ingroup rpmrc
 * Destroy rpmrc arch/os compatibility tables.
 * @todo Eliminate from API.
 */
void rpmFreeRpmrc(void);

/* ==================================================================== */
/** \name RPMTS */
/**
 * Prototype for headerFreeData() vector.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef
    void * (*HFD_t) (const void * data, rpmTagType type);

/**
 * Prototype for headerGetEntry() vector.
 *
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef int (*HGE_t) (Header h, rpmTag tag,
			rpmTagType * type,
			void ** p,
			int32_t * c);

/**
 * Prototype for headerAddEntry() vector.
 *
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h             header
 * @param tag           tag
 * @param type          tag value data type
 * @param p             pointer to tag value(s)
 * @param c             number of values
 * @return              1 on success, 0 on failure
 */
typedef int (*HAE_t) (Header h, rpmTag tag, rpmTagType type,
			const void * p, int32_t c);

/**
 * Prototype for headerModifyEntry() vector.
 * If there are multiple entries with this tag, the first one gets replaced.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef int (*HME_t) (Header h, rpmTag tag, rpmTagType type,
			const void * p, int32_t c);

/**
 * Prototype for headerRemoveEntry() vector.
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
typedef int (*HRE_t) (Header h, int32_t tag);

/**
 * We pass these around as an array with a sentinel.
 */
typedef struct rpmRelocation_s {
    const char * oldPath;	/*!< NULL here evals to RPMTAG_DEFAULTPREFIX, */
    const char * newPath;	/*!< NULL means to omit the file completely! */
} rpmRelocation;

/**
 * Compare headers to determine which header is "newer".
 * @param first		1st header
 * @param second	2nd header
 * @return		result of comparison
 */
int rpmVersionCompare(Header first, Header second);

/** \ingroup header
 * Perform simple sanity and range checks on header tag(s).
 * @param il		no. of tags in header
 * @param dl		no. of bytes in header data.
 * @param pev		1st element in tag array, big-endian
 * @param iv		failing (or last) tag element, host-endian
 * @param negate	negative offset expected?
 * @return		-1 on success, otherwise failing tag element index
 */
int headerVerifyInfo(int il, int dl, const void * pev, void * iv, int negate);

/** \ingroup header
 * Check for supported payload format in header.
 * @param h		header to check
 * @return		RPMRC_OK if supported, RPMRC_FAIL otherwise
 */
rpmRC headerCheckPayloadFormat(Header h);

/**  \ingroup header
 * Check header consistency, performing headerGetEntry() the hard way.
 *  
 * Sanity checks on the header are performed while looking for a
 * header-only digest or signature to verify the blob. If found,
 * the digest or signature is verified.
 *
 * @param ts		transaction set
 * @param uh		unloaded header blob
 * @param uc		no. of bytes in blob (or 0 to disable)
 * @retval *msg		verification error message (or NULL)
 * @return		RPMRC_OK on success
 */
rpmRC headerCheck(rpmts ts, const void * uh, size_t uc,
		const char ** msg);

/**  \ingroup header
 * Return checked and loaded header.
 * @param ts		transaction set
 * @param fd		file handle
 * @retval hdrp		address of header (or NULL)
 * @retval *msg		verification error message (or NULL)
 * @return		RPMRC_OK on success
 */
rpmRC rpmReadHeader(rpmts ts, FD_t fd, Header *hdrp,
		const char ** msg);

/** \ingroup header
 * Return package header from file handle, verifying digests/signatures.
 * @param ts		transaction set
 * @param fd		file handle
 * @param fn		file name
 * @retval hdrp		address of header (or NULL)
 * @return		RPMRC_OK on success
 */
rpmRC rpmReadPackageFile(rpmts ts, FD_t fd,
		const char * fn, Header * hdrp);

/** \ingroup rpmtrans
 * Install source package.
 * @param ts		transaction set
 * @param fd		file handle
 * @retval specFilePtr	address of spec file name (or NULL)
 * @retval cookie	address of cookie pointer (or NULL)
 * @return		rpmRC return code
 */
rpmRC rpmInstallSourcePackage(rpmts ts, FD_t fd,
			const char ** specFilePtr,
			const char ** cookie);

/** \ingroup rpmtrans
 * Return copy of rpmlib internal provides.
 * @retval provNames	address of array of rpmlib internal provide names
 * @retval provFlags	address of array of rpmlib internal provide flags
 * @retval provVersions	address of array of rpmlib internal provide versions
 * @return		no. of entries
 */
int rpmGetRpmlibProvides(const char *** provNames,
			int ** provFlags,
			const char *** provVersions);

/** \ingroup rpmtrans
 * Segmented string compare for version and/or release.
 *
 * @param a		1st string
 * @param b		2nd string
 * @return		+1 if a is "newer", 0 if equal, -1 if b is "newer"
 */
int rpmvercmp(const char * a, const char * b);

/** \ingroup rpmtrans
 * Check dependency against internal rpmlib feature provides.
 * @param key		dependency
 * @return		1 if dependency overlaps, 0 otherwise
 */
int rpmCheckRpmlibProvides(const rpmds key);

/** \ingroup rpmcli
 * Display current rpmlib feature provides.
 * @param fp		output file handle
 */
void rpmShowRpmlibProvides(FILE * fp);

/**
 * Release storage used by file system usage cache.
 */
void rpmFreeFilesystems(void);

/**
 * Return (cached) file system mount points.
 * @retval listptr		addess of file system names (or NULL)
 * @retval num			address of number of file systems (or NULL)
 * @return			0 on success, 1 on error
 */
int rpmGetFilesystemList( const char *** listptr,
		int * num);

/**
 * Determine per-file system usage for a list of files.
 * @param fileList		array of absolute file names
 * @param fssizes		array of file sizes
 * @param numFiles		number of files in list
 * @retval usagesPtr		address of per-file system usage array (or NULL)
 * @param flags			(unused)
 * @return			0 on success, 1 on error
 */
int rpmGetFilesystemUsage(const char ** fileList, int32_t * fssizes,
		int numFiles, uint32_t ** usagesPtr,
		int flags);

/* ==================================================================== */
/** \name RPMK */

/** \ingroup signature
 * Tags found in signature header from package.
 */
enum rpmtagSignature {
    RPMSIGTAG_SIZE	= 1000,	/*!< internal Header+Payload size in bytes. */
    RPMSIGTAG_LEMD5_1	= 1001,	/*!< internal Broken MD5, take 1 @deprecated legacy. */
    RPMSIGTAG_PGP	= 1002,	/*!< internal PGP 2.6.3 signature. */
    RPMSIGTAG_LEMD5_2	= 1003,	/*!< internal Broken MD5, take 2 @deprecated legacy. */
    RPMSIGTAG_MD5	= 1004,	/*!< internal MD5 signature. */
    RPMSIGTAG_GPG	= 1005, /*!< internal GnuPG signature. */
    RPMSIGTAG_PGP5	= 1006,	/*!< internal PGP5 signature @deprecated legacy. */
    RPMSIGTAG_PAYLOADSIZE = 1007,/*!< internal uncompressed payload size in bytes. */
    RPMSIGTAG_BADSHA1_1	= RPMTAG_BADSHA1_1,	/*!< internal Broken SHA1, take 1. */
    RPMSIGTAG_BADSHA1_2	= RPMTAG_BADSHA1_2,	/*!< internal Broken SHA1, take 2. */
    RPMSIGTAG_SHA1	= RPMTAG_SHA1HEADER,	/*!< internal sha1 header digest. */
    RPMSIGTAG_DSA	= RPMTAG_DSAHEADER,	/*!< internal DSA header signature. */
    RPMSIGTAG_RSA	= RPMTAG_RSAHEADER	/*!< internal RSA header signature. */
};

/** \ingroup signature
 * Verify a signature from a package.
 *
 * This needs the following variables from the transaction set:
 *	- ts->sigtag	type of signature
 *	- ts->sig	signature itself (from signature header)
 *	- ts->siglen	no. of bytes in signature
 *	- ts->dig	signature/pubkey parameters (malloc'd workspace)
 *
 * @param ts		transaction set
 * @retval result	detailed text result of signature verification
 * @return		result of signature verification
 */
rpmRC rpmVerifySignature(const rpmts ts,
		char * result);

/** \ingroup signature
 * Destroy signature header from package.
 * @param h		signature header
 * @return		NULL always
 */
Header rpmFreeSignature(Header h);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMLIB */
