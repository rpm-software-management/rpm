#ifndef H_RPMLIB
#define	H_RPMLIB

/** \file lib/rpmlib.h
 * \ingroup rpmcli rpmrc rpmdep rpmtrans rpmdb lead signature header payload dbi
 *
 */

/* This is the *only* module users of rpmlib should need to include */

/* and it shouldn't need these :-( */

#include "rpmio.h"
#include "rpmmessages.h"
#include "rpmerr.h"
#include "header.h"
#include "popt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return package signatures and header from file handle.
 * @param fd		file handle
 * @retval signatures	address of signatures pointer (or NULL)
 * @retval hdr		address of header pointer (or NULL)
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int rpmReadPackageInfo(FD_t fd, /*@out@*/ Header * signatures,
	/*@out@*/ Header * hdr) /*@modifies fd, *signatures, *hdr @*/;

/**
 * Return package header and lead info from file handle.
 * @param fd		file handle
 * @retval hdr		address of header (or NULL)
 * @retval isSource
 * @retval major
 * @retval minor
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int rpmReadPackageHeader(FD_t fd, /*@out@*/ Header * hdr,
	/*@out@*/ int * isSource, /*@out@*/ int * major,
	/*@out@*/ int * minor)
		/*@modifies fd, *hdr, *isSource, *major, *minor @*/;

/** \ingroup header
 * Return name, version, release strings from header.
 * @param h		header
 * @retval np		address of name pointer (or NULL)
 * @retval vp		address of version pointer (or NULL)
 * @retval rp		address of release pointer (or NULL)
 * @return		0 always
 */
int headerNVR(Header h, /*@out@*/ const char **np, /*@out@*/ const char **vp,
	/*@out@*/ const char **rp) /*@modifies *np, *vp, *rp @*/;

/** \ingroup header
 * Translate and merge legacy signature tags into header.
 * @param h		header
 * @param sig		signature header
 */
void headerMergeLegacySigs(Header h, const Header sig)
	/*@modifies h @*/;

/**
 * Retrieve file names from header.
 * The representation of file names in package headers changed in rpm-4.0.
 * Originally, file names were stored as an array of paths. In rpm-4.0,
 * file names are stored as separate arrays of dirname's and basename's,
 * with a dirname index to associate the correct dirname with each basname.
 * This function is used to retrieve file names independent of how the
 * file names are represented in the package header.
 * 
 * @param h		header
 * @retval fileListPtr	address of array of file names
 * @retval fileCountPtr	address of number of files
 */
void rpmBuildFileList(Header h, /*@out@*/ const char *** fileListPtr, 
	/*@out@*/ int * fileCountPtr)
		/*@modifies *fileListPtr, *fileCountPtr @*/;

/**
 * Retrieve tag info from header.
 * This is a "dressed" entry to headerGetEntry to do:
 *	1) DIRNAME/BASENAME/DIRINDICES -> FILENAMES tag conversions.
 *	2) i18n lookaside (if enabled).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int rpmHeaderGetEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
        /*@out@*/ void **p, /*@out@*/ int_32 *c)
		/*@modifies *type, *p, *c @*/;

/**
 * Retrieve tag info from header.
 * Yet Another "dressed" entry to headerGetEntry in order to unify
 * signature/header tag retrieval.
 * @deprecated Signature tags are now duplicated into header when installed.
 * @todo Eliminate from API.
 * @param leadp		rpm lead
 * @param h		header
 * @param sigs		signatures
 * @param tag		tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int rpmPackageGetEntry(void *leadp, Header sigs, Header h,
        int_32 tag, int_32 *type, void **p, int_32 *c)
		/*@modifies *type, *p, *c @*/;

/**
 * Automatically generated table of tag name/value pairs.
 */
extern const struct headerTagTableEntry rpmTagTable[];

/**
 * Number of entries in rpmTagTable.
 */
extern const int rpmTagTableSize;

/**
 * Table of query format extensions.
 * @note Chains to headerDefaultFormats[].
 */
extern const struct headerSprintfExtension rpmHeaderFormats[];

/**
 * Pseudo-tags used by the rpmdb iterator API.
 */
#define	RPMDBI_PACKAGES		0	/*!< Installed package headers. */
#define	RPMDBI_DEPENDS		1	/*!< Dependency resolution cache. */
#define	RPMDBI_LABEL		2	/*!< Fingerprint search marker. */
#define	RPMDBI_ADDED		3	/*!< Added package headers. */
#define	RPMDBI_REMOVED		4	/*!< Removed package headers. */
#define	RPMDBI_AVAILABLE	5	/*!< Available package headers. */

/**
 * Tags identify data in package headers.
 * @note tags should not have value 0!
 */
typedef enum rpmTag_e {

    RPMTAG_HEADERIMAGE		= HEADER_IMAGE,		/*!< Current image. */
    RPMTAG_HEADERSIGNATURES	= HEADER_SIGNATURES,	/*!< Signatures. */
    RPMTAG_HEADERIMMUTABLE	= HEADER_IMMUTABLE,	/*!< Original image. */
    RPMTAG_HEADERREGIONS	= HEADER_REGIONS,	/*!< Regions. */

    RPMTAG_HEADERI18NTABLE	= HEADER_I18NTABLE, /*!< I18N string locales. */

/* Retrofit (and uniqify) signature tags for use by tagName() and rpmQuery. */
/* the md5 sum was broken *twice* on big endian machines */
/* XXX 2nd underscore prevents tagTable generation */
    RPMTAG_SIG_BASE		= HEADER_SIGBASE,
    RPMTAG_SIGSIZE		= RPMTAG_SIG_BASE+1,
    RPMTAG_SIGLEMD5_1		= RPMTAG_SIG_BASE+2,
    RPMTAG_SIGPGP		= RPMTAG_SIG_BASE+3,
    RPMTAG_SIGLEMD5_2		= RPMTAG_SIG_BASE+4,
    RPMTAG_SIGMD5	        = RPMTAG_SIG_BASE+5,
    RPMTAG_SIGGPG	        = RPMTAG_SIG_BASE+6,
    RPMTAG_SIGPGP5	        = RPMTAG_SIG_BASE+7,	/*!< internal */

    RPMTAG_NAME  		= 1000,
    RPMTAG_VERSION		= 1001,
    RPMTAG_RELEASE		= 1002,
    RPMTAG_EPOCH   		= 1003,
#define	RPMTAG_SERIAL	RPMTAG_EPOCH	/* backward comaptibility */
    RPMTAG_SUMMARY		= 1004,
    RPMTAG_DESCRIPTION		= 1005,
    RPMTAG_BUILDTIME		= 1006,
    RPMTAG_BUILDHOST		= 1007,
    RPMTAG_INSTALLTIME		= 1008,
    RPMTAG_SIZE			= 1009,
    RPMTAG_DISTRIBUTION		= 1010,
    RPMTAG_VENDOR		= 1011,
    RPMTAG_GIF			= 1012,
    RPMTAG_XPM			= 1013,
    RPMTAG_LICENSE		= 1014,
#define	RPMTAG_COPYRIGHT RPMTAG_LICENSE	/* backward comaptibility */
    RPMTAG_PACKAGER		= 1015,
    RPMTAG_GROUP		= 1016,
    RPMTAG_CHANGELOG		= 1017, /*!< internal */
    RPMTAG_SOURCE		= 1018,
    RPMTAG_PATCH		= 1019,
    RPMTAG_URL			= 1020,
    RPMTAG_OS			= 1021,
    RPMTAG_ARCH			= 1022,
    RPMTAG_PREIN		= 1023,
    RPMTAG_POSTIN		= 1024,
    RPMTAG_PREUN		= 1025,
    RPMTAG_POSTUN		= 1026,
    RPMTAG_OLDFILENAMES		= 1027, /* obsolete */
    RPMTAG_FILESIZES		= 1028,
    RPMTAG_FILESTATES		= 1029,
    RPMTAG_FILEMODES		= 1030,
    RPMTAG_FILEUIDS		= 1031, /*!< internal */
    RPMTAG_FILEGIDS		= 1032, /*!< internal */
    RPMTAG_FILERDEVS		= 1033,
    RPMTAG_FILEMTIMES		= 1034,
    RPMTAG_FILEMD5S		= 1035,
    RPMTAG_FILELINKTOS		= 1036,
    RPMTAG_FILEFLAGS		= 1037,
    RPMTAG_ROOT			= 1038, /*!< obsolete */
    RPMTAG_FILEUSERNAME		= 1039,
    RPMTAG_FILEGROUPNAME	= 1040,
    RPMTAG_EXCLUDE		= 1041, /*!< internal - deprecated */
    RPMTAG_EXCLUSIVE		= 1042, /*!< internal - deprecated */
    RPMTAG_ICON			= 1043,
    RPMTAG_SOURCERPM		= 1044,
    RPMTAG_FILEVERIFYFLAGS	= 1045,
    RPMTAG_ARCHIVESIZE		= 1046,
    RPMTAG_PROVIDENAME		= 1047,
#define	RPMTAG_PROVIDES RPMTAG_PROVIDENAME	/* backward comaptibility */
    RPMTAG_REQUIREFLAGS		= 1048,
    RPMTAG_REQUIRENAME		= 1049,
    RPMTAG_REQUIREVERSION	= 1050,
    RPMTAG_NOSOURCE		= 1051, /*!< internal */
    RPMTAG_NOPATCH		= 1052, /*!< internal */
    RPMTAG_CONFLICTFLAGS	= 1053,
    RPMTAG_CONFLICTNAME		= 1054,
    RPMTAG_CONFLICTVERSION	= 1055,
    RPMTAG_DEFAULTPREFIX	= 1056, /*!< internal - deprecated */
    RPMTAG_BUILDROOT		= 1057,
    RPMTAG_INSTALLPREFIX	= 1058, /*!< internal - deprecated */
    RPMTAG_EXCLUDEARCH		= 1059,
    RPMTAG_EXCLUDEOS		= 1060,
    RPMTAG_EXCLUSIVEARCH	= 1061,
    RPMTAG_EXCLUSIVEOS		= 1062,
    RPMTAG_AUTOREQPROV		= 1063, /*!< internal */
    RPMTAG_RPMVERSION		= 1064,
    RPMTAG_TRIGGERSCRIPTS	= 1065,
    RPMTAG_TRIGGERNAME		= 1066,
    RPMTAG_TRIGGERVERSION	= 1067,
    RPMTAG_TRIGGERFLAGS		= 1068,
    RPMTAG_TRIGGERINDEX		= 1069,
    RPMTAG_VERIFYSCRIPT		= 1079,
    RPMTAG_CHANGELOGTIME	= 1080,
    RPMTAG_CHANGELOGNAME	= 1081,
    RPMTAG_CHANGELOGTEXT	= 1082,
    RPMTAG_BROKENMD5		= 1083, /*!< internal */
    RPMTAG_PREREQ		= 1084, /*!< internal */
    RPMTAG_PREINPROG		= 1085,
    RPMTAG_POSTINPROG		= 1086,
    RPMTAG_PREUNPROG		= 1087,
    RPMTAG_POSTUNPROG		= 1088,
    RPMTAG_BUILDARCHS		= 1089,
    RPMTAG_OBSOLETENAME		= 1090,
#define	RPMTAG_OBSOLETES RPMTAG_OBSOLETENAME	/* backward comaptibility */
    RPMTAG_VERIFYSCRIPTPROG	= 1091,
    RPMTAG_TRIGGERSCRIPTPROG	= 1092,
    RPMTAG_DOCDIR		= 1093, /*!< internal */
    RPMTAG_COOKIE		= 1094,
    RPMTAG_FILEDEVICES		= 1095,
    RPMTAG_FILEINODES		= 1096,
    RPMTAG_FILELANGS		= 1097,
    RPMTAG_PREFIXES		= 1098,
    RPMTAG_INSTPREFIXES		= 1099,
    RPMTAG_TRIGGERIN		= 1100, /*!< internal */
    RPMTAG_TRIGGERUN		= 1101, /*!< internal */
    RPMTAG_TRIGGERPOSTUN	= 1102, /*!< internal */
    RPMTAG_AUTOREQ		= 1103, /*!< internal */
    RPMTAG_AUTOPROV		= 1104, /*!< internal */
    RPMTAG_CAPABILITY		= 1105, /*!< internal obsolete */
    RPMTAG_SOURCEPACKAGE	= 1106, /*!< internal */
    RPMTAG_OLDORIGFILENAMES	= 1107, /*!< obsolete */
    RPMTAG_BUILDPREREQ		= 1108, /*!< internal */
    RPMTAG_BUILDREQUIRES	= 1109, /*!< internal */
    RPMTAG_BUILDCONFLICTS	= 1110, /*!< internal */
    RPMTAG_BUILDMACROS		= 1111,
    RPMTAG_PROVIDEFLAGS		= 1112,
    RPMTAG_PROVIDEVERSION	= 1113,
    RPMTAG_OBSOLETEFLAGS	= 1114,
    RPMTAG_OBSOLETEVERSION	= 1115,
    RPMTAG_DIRINDEXES		= 1116,
    RPMTAG_BASENAMES		= 1117,
    RPMTAG_DIRNAMES		= 1118,
    RPMTAG_ORIGDIRINDEXES	= 1119, /*!< internal */
    RPMTAG_ORIGBASENAMES	= 1120, /*!< internal */
    RPMTAG_ORIGDIRNAMES		= 1121, /*!< internal */
    RPMTAG_OPTFLAGS		= 1122,
    RPMTAG_DISTURL		= 1123,
    RPMTAG_PAYLOADFORMAT	= 1124,
    RPMTAG_PAYLOADCOMPRESSOR	= 1125,
    RPMTAG_PAYLOADFLAGS		= 1126,
    RPMTAG_MULTILIBS		= 1127,
    RPMTAG_FIRSTFREE_TAG	/*!< internal */
} rpmTag;

#define	RPMTAG_EXTERNAL_TAG		1000000

/**
 * File States (when installed).
 */
typedef enum rpmfileStates_e {
    RPMFILE_STATE_NORMAL 	= 0,
    RPMFILE_STATE_REPLACED 	= 1,
    RPMFILE_STATE_NOTINSTALLED	= 2,
    RPMFILE_STATE_NETSHARED	= 3
} rpmfileStates;

/**
 * File Attributes.
 */
typedef	enum rpmfileAttrs_e {
    RPMFILE_CONFIG	= (1 << 0),	/*!< from %%config */
    RPMFILE_DOC		= (1 << 1),	/*!< from %%doc */
    RPMFILE_DONOTUSE	= (1 << 2),	/*!< @todo (unimplemented) from %donotuse. */
    RPMFILE_MISSINGOK	= (1 << 3),	/*!< from %%config(missingok) */
    RPMFILE_NOREPLACE	= (1 << 4),	/*!< from %%config(noreplace) */
    RPMFILE_SPECFILE	= (1 << 5),	/*!< @todo (unnecessary) marks 1st file in srpm. */
    RPMFILE_GHOST	= (1 << 6),	/*!< from %%ghost */
    RPMFILE_LICENSE	= (1 << 7),	/*!< from %%license */
    RPMFILE_README	= (1 << 8)	/*!< from %%readme */
} rpmfileAttrs;
#define	RPMFILE_MULTILIB_SHIFT		9
#define	RPMFILE_MULTILIB(N)		((N) << RPMFILE_MULTILIB_SHIFT)
#define	RPMFILE_MULTILIB_MASK		RPMFILE_MULTILIB(7)

/* XXX Check file flags for multilib marker. */
#define	isFileMULTILIB(_fflags)		((_fflags) & RPMFILE_MULTILIB_MASK)

/**
 * Dependency Attributes.
 */
typedef	enum rpmsenseFlags_e {
    RPMSENSE_ANY	= 0,
    RPMSENSE_SERIAL	= (1 << 0),	/*!< @todo Legacy. */
    RPMSENSE_LESS	= (1 << 1),
    RPMSENSE_GREATER	= (1 << 2),
    RPMSENSE_EQUAL	= (1 << 3),
    RPMSENSE_PROVIDES	= (1 << 4), /* only used internally by builds */
    RPMSENSE_CONFLICTS	= (1 << 5), /* only used internally by builds */
    RPMSENSE_PREREQ	= (1 << 6),	/*!< @todo Legacy. */
    RPMSENSE_OBSOLETES	= (1 << 7), /* only used internally by builds */
    RPMSENSE_INTERP	= (1 << 8),	/*!< Interpreter used by scriptlet. */
    RPMSENSE_SCRIPT_PRE	= ((1 << 9)|RPMSENSE_PREREQ), /*!< %pre dependency. */
    RPMSENSE_SCRIPT_POST = ((1 << 10)|RPMSENSE_PREREQ), /*!< %post dependency. */
    RPMSENSE_SCRIPT_PREUN = ((1 << 11)|RPMSENSE_PREREQ), /*!< %preun dependency. */
    RPMSENSE_SCRIPT_POSTUN = ((1 << 12)|RPMSENSE_PREREQ), /*!< %postun dependency. */
    RPMSENSE_SCRIPT_VERIFY = (1 << 13),	/*!< %verify dependency. */
    RPMSENSE_FIND_REQUIRES = (1 << 14), /*!< find-requires generated dependency. */
    RPMSENSE_FIND_PROVIDES = (1 << 15), /*!< find-provides generated dependency. */

    RPMSENSE_TRIGGERIN	= (1 << 16),	/*!< %triggerin dependency. */
    RPMSENSE_TRIGGERUN	= (1 << 17),	/*!< %triggerun dependency. */
    RPMSENSE_TRIGGERPOSTUN = (1 << 18),	/*!< %triggerpostun dependency. */
    RPMSENSE_MULTILIB	= (1 << 19),
    RPMSENSE_SCRIPT_PREP = (1 << 20),	/*!< %prep build dependency. */
    RPMSENSE_SCRIPT_BUILD = (1 << 21),	/*!< %build build dependency. */
    RPMSENSE_SCRIPT_INSTALL = (1 << 22),/*!< %install build dependency. */
    RPMSENSE_SCRIPT_CLEAN = (1 << 23),	/*!< %clean build dependency. */
    RPMSENSE_RPMLIB	= ((1 << 24) | RPMSENSE_PREREQ), /*!< rpmlib(feature) dependency. */
    RPMSENSE_TRIGGERPREIN = (1 << 25)	/*!< @todo Implement %triggerprein. */

} rpmsenseFlags;

#define	RPMSENSE_SENSEMASK	15	 /* Mask to get senses, ie serial, */
                                         /* less, greater, equal.          */

#define	RPMSENSE_TRIGGER	\
	(RPMSENSE_TRIGGERIN | RPMSENSE_TRIGGERUN | RPMSENSE_TRIGGERPOSTUN)

#define	isDependsMULTILIB(_dflags)	((_dflags) & RPMSENSE_MULTILIB)

#define	_ALL_REQUIRES_MASK	(\
    RPMSENSE_INTERP | \
    RPMSENSE_SCRIPT_PRE | \
    RPMSENSE_SCRIPT_POST | \
    RPMSENSE_SCRIPT_PREUN | \
    RPMSENSE_SCRIPT_POSTUN | \
    RPMSENSE_SCRIPT_VERIFY | \
    RPMSENSE_FIND_REQUIRES | \
    RPMSENSE_SCRIPT_PREP | \
    RPMSENSE_SCRIPT_BUILD | \
    RPMSENSE_SCRIPT_INSTALL | \
    RPMSENSE_SCRIPT_CLEAN | \
    RPMSENSE_RPMLIB )

#define	_notpre(_x)		((_x) & ~RPMSENSE_PREREQ)
#define	_INSTALL_ONLY_MASK \
    _notpre(RPMSENSE_SCRIPT_PRE|RPMSENSE_SCRIPT_POST|RPMSENSE_RPMLIB)
#define	_ERASE_ONLY_MASK  \
    _notpre(RPMSENSE_SCRIPT_PREUN|RPMSENSE_SCRIPT_POSTUN)

#define	isLegacyPreReq(_x)  (((_x) & _ALL_REQUIRES_MASK) == RPMSENSE_PREREQ)
#define	isInstallPreReq(_x)	((_x) & _INSTALL_ONLY_MASK)
#define	isErasePreReq(_x)	((_x) & _ERASE_ONLY_MASK)

#define	xfree(_p)	free((void *)_p)

/* ==================================================================== */
/** \name RPMRC */
/*@{*/

/* Stuff for maintaining "variables" like SOURCEDIR, BUILDDIR, etc */
#define	RPMVAR_OPTFLAGS			3
#define	RPMVAR_PROVIDES			38
#define	RPMVAR_INCLUDE			43
#define	RPMVAR_MACROFILES		49

#define	RPMVAR_NUM			55	/* number of RPMVAR entries */

/** \ingroup rpmrc
 * Return value of an rpmrc variable.
 * @deprecated Use rpmExpand() with appropriate macro expression.
 * @todo Eliminate from API.
 */
const char * rpmGetVar(int var);

/** \ingroup rpmrc
 * Set value of an rpmrc variable.
 * @deprecated Use rpmDefineMacro() to change appropriate macro instead.
 * @todo Eliminate from API.
 */
void rpmSetVar(int var, const char *val);

/** \ingroup rpmrc
 * List of macro files to read when configuring rpm.
 * This is a colon separated list of files. URI's are permitted as well,
 * identified by the token '://', so file paths must not begin with '//'.
 */
extern const char * macrofiles;

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
int rpmReadConfigFiles(const char * file, const char * target);

/** \ingroup rpmrc
 * Read rpmrc (and macro) configuration file(s).
 * @param file		colon separated files to read (NULL uses default)
 * @return		0 on succes
 */
int rpmReadRC(const char * file);

/** \ingroup rpmrc
 * Return current arch name and/or number.
 * @todo Generalize to extract arch component from target_platform macro.
 * @retval name		address of arch name (or NULL)
 * @retval num		address of arch number (or NULL)
 */
void rpmGetArchInfo( /*@out@*/ const char ** name, /*@out@*/ int * num);

/** \ingroup rpmrc
 * Return current os name and/or number.
 * @todo Generalize to extract os component from target_platform macro.
 * @retval name		address of os name (or NULL)
 * @retval num		address of os number (or NULL)
 */
void rpmGetOsInfo( /*@out@*/ const char ** name, /*@out@*/ int * num);

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
 * @param f		output file handle
 * @return		0 always
 */
int rpmShowRC(FILE *f);

/** \ingroup rpmrc
 * @deprecated Use addMacro to set _target_* macros.
 * @todo Eliminate from API.
 * @param archTable
 * @param osTable
 */
void rpmSetTables(int archTable, int osTable);  /* only used by build code */

/** \ingroup rpmrc
 * Set current arch/os names.
 * NULL as argument is set to the default value (munged uname())
 * pushed through a translation table (if appropriate).
 * @deprecated Use addMacro to set _target_* macros.
 * @todo Eliminate from API.
 *
 * @param arch		arch name (or NULL)
 * @param os		os name (or NULL)
 */
void rpmSetMachine(const char * arch, const char * os);

/** \ingroup rpmrc
 * Return current arch/os names.
 * @deprecated Use rpmExpand on _target_* macros.
 * @todo Eliminate from API.
 *
 * @retval arch		address of arch name (or NULL)
 * @retval os		address of os name (or NULL)
 */
void rpmGetMachine( /*@out@*/ const char **arch, /*@out@*/ const char **os);

/** \ingroup rpmrc
 * Destroy rpmrc arch/os compatibility tables.
 * @todo Eliminate from API.
 */
void rpmFreeRpmrc(void);

/*@}*/
/* ==================================================================== */
/** \name RPMDB */
/*@{*/
/** \ingroup rpmdb
 */
typedef /*@abstract@*/ struct rpmdb_s * rpmdb;

/** \ingroup rpmdb
 */
typedef /*@abstract@*/ struct _dbiIndexSet * dbiIndexSet;

/** \ingroup rpmdb
 * Open rpm database.
 * @param root		path to top of install tree
 * @retval dbp		address of rpm database
 * @param mode		open(2) flags:  O_RDWR or O_RDONLY (O_CREAT also)
 * @param perms		database permissions
 * @return		0 on success
 */
int rpmdbOpen (const char * root, /*@out@*/ rpmdb * dbp, int mode, int perms);

/** \ingroup rpmdb
 * Initialize database.
 * @param root		path to top of install tree
 * @param perms		database permissions
 * @return		0 on success
 */
int rpmdbInit(const char * root, int perms);

/** \ingroup rpmdb
 * Close all database indices and free rpmdb.
 * @param rpmdb		rpm database
 * @return		0 always
 */
int rpmdbClose ( /*@only@*/ rpmdb rpmdb);

/** \ingroup rpmdb
 * Sync all database indices.
 * @param rpmdb		rpm database
 * @return		0 always
 */
int rpmdbSync (rpmdb rpmdb);

/** \ingroup rpmdb
 * Open all database indices.
 * @param rpmdb		rpm database
 * @return		0 always
 */
int rpmdbOpenAll (rpmdb rpmdb);

/** \ingroup rpmdb
 * Return number of instances of package in rpm database.
 * @param db		rpm database
 * @param name		rpm package name
 * @return		number of instances
 */
int rpmdbCountPackages(rpmdb db, const char *name);

/** \ingroup rpmdb
 */
typedef /*@abstract@*/ struct _rpmdbMatchIterator * rpmdbMatchIterator;

/** \ingroup rpmdb
 * Destroy rpm database iterator.
 * @param mi		rpm database iterator
 */
void rpmdbFreeIterator( /*@only@*/ rpmdbMatchIterator mi);

/** \ingroup rpmdb
 * Return rpm database used by iterator.
 * @param mi		rpm database iterator
 * @return		rpm database handle
 */
rpmdb rpmdbGetIteratorRpmDB(rpmdbMatchIterator mi);

/** \ingroup rpmdb
 * Return join key for current position of rpm database iterator.
 * @param mi		rpm database iterator
 * @return		current join key
 */
unsigned int rpmdbGetIteratorOffset(rpmdbMatchIterator mi);

/** \ingroup rpmdb
 * Return number of elements in rpm database iterator.
 * @param mi		rpm database iterator
 * @return		number of elements
 */
int rpmdbGetIteratorCount(rpmdbMatchIterator mi);

/** \ingroup rpmdb
 * Append items to set of package instances to iterate.
 * @param mi		rpm database iterator
 * @param hdrNums	array of package instances
 * @param nHdrNums	number of elements in array
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbAppendIterator(rpmdbMatchIterator mi, int * hdrNums, int nHdrNums);

/** \ingroup rpmdb
 * Remove items from set of package instances to iterate.
 * @param mi		rpm database iterator
 * @param hdrNums	array of package instances
 * @param nHdrNums	number of elements in array
 * @param sorted	is the array sorted? (array will be sorted on return)
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbPruneIterator(rpmdbMatchIterator mi, int * hdrNums,
	int nHdrNums, int sorted);

/** \ingroup rpmdb
 * Modify iterator to filter out headers that do not match version.
 * @todo Replace with a more general mechanism using RE's on tag content.
 * @param mi		rpm database iterator
 * @param version	version to check for
 */
void rpmdbSetIteratorVersion(rpmdbMatchIterator mi, /*@kept@*/ const char * version);

/** \ingroup rpmdb
 * Modify iterator to filter out headers that do not match release.
 * @todo Replace with a more general mechanism using RE's on tag content.
 * @param mi		rpm database iterator
 * @param release	release to check for
 */
void rpmdbSetIteratorRelease(rpmdbMatchIterator mi, /*@kept@*/ const char * release);

/** \ingroup rpmdb
 * Modify iterator to mark header for lazy write.
 * @param mi		rpm database iterator
 * @param modified	new value of modified
 * @return		previous value
 */
int rpmdbSetIteratorModified(rpmdbMatchIterator mi, int modified);

/** \ingroup rpmdb
 * Return next package header from iteration.
 * @param mi		rpm database iterator
 * @return		NULL on end of iteration.
 */
Header rpmdbNextIterator(rpmdbMatchIterator mi);
#define	rpmdbNextIterator(_a) \
	XrpmdbNextIterator(_a, __FILE__, __LINE__)
Header XrpmdbNextIterator(rpmdbMatchIterator mi, const char * f, unsigned int l);

/** \ingroup rpmdb
 * Return database iterator.
 * @param rpmdb		rpm database
 * @param rpmtag	rpm tag
 * @param keyp		key data (NULL for sequential acess)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		NULL on failure
 */
/*@only@*/ /*@null@*/ rpmdbMatchIterator rpmdbInitIterator(
			rpmdb rpmdb, int rpmtag,
			const void * key, size_t keylen);

/** \ingroup rpmdb
 * Remove package header from rpm database and indices.
 * @param rpmdb		rpm database
 * @param offset	location in Packages dbi
 * @return		0 on success
 */
int rpmdbRemove(rpmdb db, unsigned int offset);

/** \ingroup rpmdb
 * Add package header to rpm database and indices.
 * @param rpmdb		rpm database
 * @param rpmtag	rpm tag
 * @return		0 on success
 */
int rpmdbAdd(rpmdb rpmdb, Header dbentry);

/** \ingroup rpmdb
 * Rebuild database indices from package headers.
 * @param root		path to top of install tree
 */
int rpmdbRebuild(const char * root);

/*@}*/
/* ==================================================================== */
/** \name RPMPROBS */
/*@{*/

/**
 * Enumerate transaction set problem types.
 */
typedef enum rpmProblemType_e {
    RPMPROB_BADARCH,	/*!< package ... is for a different architecture */
    RPMPROB_BADOS,	/*!< package ... is for a different operating system */
    RPMPROB_PKG_INSTALLED, /*!< package ... is already installed */
    RPMPROB_BADRELOCATE,/*!< path ... is not relocateable for package ... */
    RPMPROB_REQUIRES,	/*!< @todo Use for dependency errors. */
    RPMPROB_CONFLICT,	/*!< @todo Use for dependency errors. */
    RPMPROB_NEW_FILE_CONFLICT, /*!< file ... conflicts between attemped installs of ... */
    RPMPROB_FILE_CONFLICT,/*!< file ... from install of ... conflicts with file from package ... */
    RPMPROB_OLDPACKAGE,	/*!< package ... (which is newer than ...) is already installed */
    RPMPROB_DISKSPACE,	/*!< installing package ... needs ... on the ...  filesystem */
    RPMPROB_DISKNODES,	/*!< installing package ... needs ... on the ...  filesystem */
    RPMPROB_BADPRETRANS	/*!< (unimplemented) */
 } rpmProblemType;

/**
 */
typedef /*@abstract@*/ struct rpmProblem_s {
/*@only@*/ /*@null@*/ const char * pkgNEVR;
/*@only@*/ /*@null@*/ const char * altNEVR;
/*@dependent@*/ const void * key;
    Header h;
    rpmProblemType type;
    int ignoreProblem;
/*@only@*/ const char * str1;
    unsigned long ulong1;
} * rpmProblem;

/**
 */
typedef /*@abstract@*/ struct rpmProblemSet_s {
    int numProblems;		/*!< Current probs array size. */
    int numProblemsAlloced;	/*!< Allocated probs array size. */
    rpmProblem probs;		/*!< Array of specific problems. */
} * rpmProblemSet;

/**
 */
void printDepFlags(FILE *fp, const char *version, int flags)
	/*@modifies *fp @*/;

/**
 */
struct rpmDependencyConflict {
    char * byName;
    char * byVersion;
    char * byRelease;
    Header byHeader;
    /* these needs fields are misnamed -- they are used for the package
       which isn't needed as well */
    char * needsName;
    char * needsVersion;
    int needsFlags;
/*@observer@*/ /*@null@*/ const void * suggestedPackage; /* NULL if none */
    enum {
	RPMDEP_SENSE_REQUIRES,
	RPMDEP_SENSE_CONFLICTS
    } sense;
} ;

/**
 */
void printDepProblems(FILE *fp, struct rpmDependencyConflict *conflicts,
	int numConflicts)	/*@modifies *fp @*/;

/**
 * Return formatted string representation of problem.
 * @deprecated API: prob used to be passed by value, now passed by reference.
 * @param prob		rpm problem
 * @return		formatted string
 */
/*@only@*/ const char * rpmProblemString(rpmProblem prob) /*@modifies prob @*/;

/**
 * Output formatted string representation of problem to file handle.
 * @deprecated API: prob used to be passed by value, now passed by reference.
 * @param fp		file handle
 * @param prob		rpm problem
 */
void rpmProblemPrint(FILE *fp, rpmProblem prob)	/*@modifies *fp, prob @*/;

/**
 * Print problems to file handle.
 * @param fp		file handle
 * @param probs		problem set
 */
void rpmProblemSetPrint(FILE *fp, rpmProblemSet probs)
		/*@modifies *fp, probs @*/;

/**
 * Destroy problem set.
 * @param probs		problem set
 */
void rpmProblemSetFree( /*@only@*/ rpmProblemSet probs);

/*@}*/
/* ==================================================================== */
/** \name RPMTS */
/*@{*/
/* we pass these around as an array with a sentinel */
typedef struct rpmRelocation_s {
    const char * oldPath;	/*!< NULL here evals to RPMTAG_DEFAULTPREFIX, */
    const char * newPath;	/*!< NULL means to omit the file completely! */
} rpmRelocation;

/**
 * Install source package.
 * @param root		path to top of install tree
 * @param fd		file handle
 * @retval specFile	address of spec file name
 * @param notify	progress callback
 * @param notifyData	progress callback private data
 * @retval cooke	address of cookie pointer
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int rpmInstallSourcePackage(const char * root, FD_t fd,
			/*@out@*/ const char ** specFile,
			rpmCallbackFunction notify, rpmCallbackData notifyData,
			/*@out@*/ char ** cookie);

/**
 * Compare headers to determine which header is "newer".
 * @param first		1st header
 * @param second	2nd header
 * @return		result of comparison
 */
int rpmVersionCompare(Header first, Header second);

/** \ingroup rpmtrans
 * The RPM Transaction Set.
 * Transaction sets are inherently unordered! RPM may reorder transaction
 * sets to reduce errors. In general, installs/upgrades are done before
 * strict removals, and prerequisite ordering is done on installs/upgrades.
 */
typedef /*@abstract@*/ struct rpmTransactionSet_s * rpmTransactionSet;

/** \ingroup rpmtrans
 * Create an empty transaction set.
 * @param rpmdb		rpm database (may be NULL if database is not accessed)
 * @param rootdir	path to top of install tree
 * @return		rpm transaction set
 */
/*@only@*/ rpmTransactionSet rpmtransCreateSet(rpmdb rpmdb,
	const char * rootdir);

/** \ingroup rpmtrans
 * Add package to be installed to unordered transaction set.
 *
 * If fd is NULL, the callback specified in rpmtransCreateSet() is used to
 * open and close the file descriptor. If Header is NULL, the fd is always
 * used, otherwise fd is only needed (and only opened) for actual package 
 * installation.
 *
 * @param rpmdep	rpm transaction set
 * @param h		package header
 * @param fd		package file handle
 * @param key		package private data
 * @param update	is package being upgraded?
 * @param relocs	package file relocations
 * @return		0 on success, 1 on I/O error, 2 needs capabilities
 */
int rpmtransAddPackage(rpmTransactionSet rpmdep, Header h, FD_t fd,
		/*@owned@*/ const void * key, int update,
		rpmRelocation * relocs);

/** \ingroup rpmtrans
 * Add package to universe of possible packages to install in transaction set.
 * @param rpmdep	rpm transaction set
 * @param h		header
 * @param key		package private data
 */
void rpmtransAvailablePackage(rpmTransactionSet rpmdep, Header h,
		/*@owned@*/ const void * key);

/** \ingroup rpmtrans
 * Add package to be removed to unordered transaction set.
 * @param rpmdep	rpm transaction set
 * @param dboffset	rpm database instance
 */
void rpmtransRemovePackage(rpmTransactionSet rpmdep, int dboffset);

/** \ingroup rpmtrans
 * Destroy transaction set.
 * @param rpmdep	rpm transaction set
 */
void rpmtransFree( /*@only@*/ rpmTransactionSet rpmdep);

/** \ingroup rpmtrans
 * Save file handle to be used as stderr when running package scripts.
 * @param ts		rpm transaction set
 * @param fd		file handle
 */
void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd)
	/*@modifies ts, fd @*/;

/** \ingroup rpmtrans
 * Retrieve keys from ordered transaction set.
 * @todo Removed packages have no keys, returned as interleaved NULL pointers.
 * @param ts		rpm transaction set
 * @retval ep		address of returned element array pointer (or NULL)
 * @retval nep		address of no. of returned elements (or NULL)
 * @return		0 always
 */
int rpmtransGetKeys(const rpmTransactionSet ts,
	/*@out@*/ const void *** ep, /*@out@*/ int * nep)
		/*@modifies ep, nep @*/;

/** \ingroup rpmtrans
 * Check that all dependencies can be resolved.
 * @param rpmdep	rpm transaction set
 * @retval conflicts
 * @retval numConflicts
 * @return		0 on success
 */
int rpmdepCheck(rpmTransactionSet rpmdep,
	/*@exposed@*/ /*@out@*/ struct rpmDependencyConflict ** conflicts,
	/*@exposed@*/ /*@out@*/ int * numConflicts);

/** \ingroup rpmtrans
 * Determine package order in a transaction set according to dependencies.
 *
 * Order packages, returning error if circular dependencies cannot be
 * eliminated by removing PreReq's from the loop(s). Only dependencies from
 * added or removed packages are used to determine ordering using a
 * topological sort (Knuth vol. 1, p. 262). Use rpmdepCheck() to verify
 * that all dependencies can be reolved.
 *
 * The order ends up as installed packages followed by removed packages,
 * with packages removed for upgrades immediately following the new package
 * to be installed.
 *
 * The operation would be easier if we could sort the addedPackages array in the
 * transaction set, but we store indexes into the array in various places.
 *
 * @param rpmdep	transaction set
 * @return		0 if packages are successfully ordered, 1 otherwise
 */
int rpmdepOrder(rpmTransactionSet rpmdep)	/*@modifies rpmdep @*/;

/** \ingroup rpmtrans
 * Destroy dependency conflicts storage.
 * @param conflicts	dependency conflicts
 * @param numConflicts	no. of dependency conflicts
 */
void rpmdepFreeConflicts( /*@only@*/ struct rpmDependencyConflict * conflicts,
	int numConflicts);

/** \ingroup rpmtrans
 * Bit(s) to control rpmRunTransaction() operation.
 */
typedef enum rpmtransFlags_e {
    RPMTRANS_FLAG_TEST		= (1 << 0),	/*!< from --test */
    RPMTRANS_FLAG_BUILD_PROBS	= (1 << 1),	/*!< @todo Document. */
    RPMTRANS_FLAG_NOSCRIPTS	= (1 << 2),	/*!< from --noscripts */
    RPMTRANS_FLAG_JUSTDB	= (1 << 3),	/*!< from --justdb */
    RPMTRANS_FLAG_NOTRIGGERS	= (1 << 4),	/*!< from --notriggers */
    RPMTRANS_FLAG_NODOCS	= (1 << 5),	/*!< from --nodocs */
    RPMTRANS_FLAG_ALLFILES	= (1 << 6),	/*!< from --allfiles */
    RPMTRANS_FLAG_KEEPOBSOLETE	= (1 << 7),	/*!< @todo Document. */
    RPMTRANS_FLAG_MULTILIB	= (1 << 8),	/*!< @todo Document. */
} rpmtransFlags;

/** \ingroup rpmtrans
 * Return copy of rpmlib internal provides.
 * @retval		address of array of rpmlib internal provide names
 * @retval		address of array of rpmlib internal provide flags
 * @retval		address of array of rpmlib internal provide versions
 * @return		no. of entries
 */
int rpmGetRpmlibProvides(/*@out@*/ const char *** provNames,
	/*@out@*/ int ** provFlags, /*@out@*/ const char *** provVersions)
		/*@ modifies *provNames, *provFlags, *provVersions @*/;

/** \ingroup rpmtrans
 * Compare two versioned dependency ranges, looking for overlap.
 * @param AName		1st dependncy name string
 * @param AEVR		1st dependency [epoch:]version[-release] string
 * @param AFlags	1st dependency logical range qualifiers
 * @param BName		2nd dependncy name string
 * @param BEVR		2nd dependency [epoch:]version[-release] string
 * @param BFlags	2nd dependency logical range qualifiers
 * @return		1 if dependencies overlap, 0 otherwise
 */
int rpmRangesOverlap(const char *AName, const char *AEVR, int AFlags,
        const char *BName, const char *BEVR, int BFlags)	/*@*/;

/** \ingroup rpmtrans
 * Check dependency against internal rpmlib feature provides.
 * @param keyName	dependency name string
 * @param keyEVR	dependency [epoch:]version[-release] string
 * @param keyFlags	dependency logical range qualifiers
 * @return		1 if dependency overlaps, 0 otherwise
 */
int rpmCheckRpmlibProvides(const char * keyName, const char * keyEVR,
	int keyFlags)	/*@*/;

/** \ingroup rpmcli
 * Display current rpmlib feature provides.
 * @param fp		output file handle
 */
void rpmShowRpmlibProvides(FILE * fp) /*@modifies *fp @*/;

/**
 * @todo Generalize filter mechanism.
 */
typedef enum rpmprobFilterFlags_e {
    RPMPROB_FILTER_NONE		= 0,
    RPMPROB_FILTER_IGNOREOS	= (1 << 0),
    RPMPROB_FILTER_IGNOREARCH	= (1 << 1),
    RPMPROB_FILTER_REPLACEPKG	= (1 << 2),
    RPMPROB_FILTER_FORCERELOCATE= (1 << 3),
    RPMPROB_FILTER_REPLACENEWFILES= (1 << 4),
    RPMPROB_FILTER_REPLACEOLDFILES= (1 << 5),
    RPMPROB_FILTER_OLDPACKAGE	= (1 << 6),
    RPMPROB_FILTER_DISKSPACE	= (1 << 7),
    RPMPROB_FILTER_DISKNODES	= (1 << 8)
} rpmprobFilterFlags;

/** \ingroup rpmtrans
 * Process all packages in transactions set.
 * @param ts		rpm transaction set
 * @param notify	progress callback
 * @param notifyData	progress callback private data
 * @param okProbs	previously known problems (or NULL)
 * @retval newProbs	address to return unfiltered problems (or NULL)
 * @param transFlags	bits to control rpmRunTransactions()
 * @param ignoreSet	bits to filter problem types
 * @return		0 on success, -1 on error, >0 with newProbs set
 */
int rpmRunTransactions(rpmTransactionSet ts,
			rpmCallbackFunction notify,
			/*@owned@*/ rpmCallbackData notifyData,
			rpmProblemSet okProbs,
			/*@out@*/ rpmProblemSet * newProbs,
			rpmtransFlags transFlags,
			rpmprobFilterFlags ignoreSet);

/*@}*/

/**
 * Return name of tag from value.
 * @param tag		tag value
 * @return		name of tag
 */
/*@observer@*/ const char *const tagName(int tag)	/*@*/;

/**
 * Return value of tag from name.
 * @param targstr	name of tag
 * @return		tag value
 */
int tagValue(const char *tagstr)			/*@*/;

#define	RPMLEAD_BINARY 0
#define	RPMLEAD_SOURCE 1

#define	RPMLEAD_MAGIC0 0xed
#define	RPMLEAD_MAGIC1 0xab
#define	RPMLEAD_MAGIC2 0xee
#define	RPMLEAD_MAGIC3 0xdb

#define	RPMLEAD_SIZE 96		/*!< Don't rely on sizeof(struct) */

/** \ingroup lead
 * The lead data structure.
 * The lead needs to be 8 byte aligned.
 * @deprecated The lead (except for signature_type) is legacy.
 * @todo Don't use any information from lead.
 */
struct rpmlead {
    unsigned char magic[4];
    unsigned char major, minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;	/*!< Signature header type (RPMSIG_HEADERSIG) */
    char reserved[16];		/*!< Pad to 96 bytes -- 8 byte aligned! */
} ;

/**
 * Release storage used by file system usage cache.
 */
void freeFilesystems(void);

/**
 * Return (cached) file system mount points.
 * @retval			addess of file system names (or NULL)
 * @retval num			address of number of file systems
 * @return			0 on success, 1 on error
 */
int rpmGetFilesystemList( /*@out@*/ const char *** listptr, /*@out@*/ int * num)
	/*@modifies *listptr, *num @*/;

/**
 * Determine per-file system usage for a list of files.
 * @param filelist		array of absolute file names
 * @param fssizes		array of file sizes
 * @param numFiles		number of files in list
 * @retval usagesPtr		address of per-file system usage array.
 * @param flags			(unused)
 * @return			0 on success, 1 on error
 */
int rpmGetFilesystemUsage(const char ** filelist, int_32 * fssizes,
	int numFiles, /*@out@*/ uint_32 ** usagesPtr, int flags);

/* ==================================================================== */
/** \name RPMBT */
/*@{*/

/** \ingroup rpmcli
 * Describe build command line request.
 */
struct rpmBuildArguments {
    int buildAmount;		/*!< Bit(s) to control operation. */
    const char *buildRootOverride; /*!< from --buildroot */
    char *targets;		/*!< Target platform(s), comma separated. */
    int useCatalog;		/*!< from --usecatalog */
    int noLang;			/*!< from --nolang */
    int noBuild;		/*!< from --nobuild */
    int shortCircuit;		/*!< from --short-circuit */
    char buildMode;		/*!< Build mode (one of "btBC") */
    char buildChar;		/*!< Build stage (one of "abcilps ") */
/*@dependent@*/ const char *rootdir;
};
/** \ingroup rpmcli
 */
typedef	struct rpmBuildArguments BTA_t;

/** \ingroup rpmcli
 */
extern struct rpmBuildArguments         rpmBTArgs;

/** \ingroup rpmcli
 */
extern struct poptOption		rpmBuildPoptTable[];

/*@}*/
/* ==================================================================== */
/** \name RPMQV */
/*@{*/

/** \ingroup rpmcli
 * Bit(s) for rpmVerifyFile() attributes and result.
 */
typedef enum rpmVerifyAttrs_e {
    RPMVERIFY_NONE	= 0,		/*!< */
    RPMVERIFY_MD5	= (1 << 0),	/*!< */
    RPMVERIFY_FILESIZE	= (1 << 1),	/*!< */
    RPMVERIFY_LINKTO	= (1 << 2),	/*!< */
    RPMVERIFY_USER	= (1 << 3),	/*!< */
    RPMVERIFY_GROUP	= (1 << 4),	/*!< */
    RPMVERIFY_MTIME	= (1 << 5),	/*!< */
    RPMVERIFY_MODE	= (1 << 6),	/*!< */
    RPMVERIFY_RDEV	= (1 << 7),	/*!< */
    RPMVERIFY_READLINKFAIL= (1 << 28),	/*!< */
    RPMVERIFY_READFAIL	= (1 << 29),	/*!< */
    RPMVERIFY_LSTATFAIL	= (1 << 30)	/*!< */
} rpmVerifyAttrs;
#define	RPMVERIFY_ALL		~(RPMVERIFY_NONE)

/** \ingroup rpmcli
 * Verify file attributes and MD5 sum.
 * @todo gnorpm and python bindings prevent this from being static.
 * @todo add rpmVerifyAttrs to prototype.
 * @param root		path to top of install tree
 * @param h		header
 * @param filenum	index of file in header file info arrays
 * @retval result	address of failure flags
 * @param omitMask	bit(s) to disable verify checks
 * @return		0 on success (or not installed), 1 on error
 */
int rpmVerifyFile(const char * root, Header h, int filenum,
	/*@out@*/ int * result, int omitMask);

/**
 * Return exit code from running verify script in header.
 * @todo gnorpm/kpackage prevents static, should be using VERIFY_SCRIPT flag.
 * @param rootDir	path to top of install tree
 * @param h		header
 * @param scriptFd	file handle to use for stderr (or NULL)
 * @return		0 on success
 */
int rpmVerifyScript(const char * rootDir, Header h, FD_t scriptFd);

/** \ingroup rpmcli
 * The command line argument will be used to retrieve header(s) ...
 */
typedef enum rpmQVSources_e {
    RPMQV_PACKAGE = 0,	/*!< ... from package name db search. */
    RPMQV_PATH,		/*!< ... from file path db search. */
    RPMQV_ALL,		/*!< ... from each installed package. */
    RPMQV_RPM, 		/*!< ... from reading binary rpm package. */
    RPMQV_GROUP,	/*!< ... from group db search. */
    RPMQV_WHATPROVIDES,	/*!< ... from provides db search. */
    RPMQV_WHATREQUIRES,	/*!< ... from requires db search. */
    RPMQV_TRIGGEREDBY,	/*!< ... from trigger db search. */
    RPMQV_DBOFFSET,	/*!< ... from database header instance. */
    RPMQV_SPECFILE	/*!< ... from spec file parse (query only). */
} rpmQVSources;

/** \ingroup rpmcli
 * Bit(s) to control rpmQuery() operation, stored in qva_flags.
 */
typedef enum rpmQueryFlags_e {
    QUERY_FOR_LIST	= (1 << 1),	/*!< from --list */
    QUERY_FOR_STATE	= (1 << 2),	/*!< from --state */
    QUERY_FOR_DOCS	= (1 << 3),	/*!< from --docfiles */
    QUERY_FOR_CONFIG	= (1 << 4),	/*!< from --configfiles */
    QUERY_FOR_DUMPFILES	= (1 << 8)	/*!< from --dump */
} rpmQueryFlags;

/** \ingroup rpmcli
 * Bit(s) to control rpmVerify() operation, stored in qva_flags.
 */
typedef enum rpmVerifyFlags_e {
    VERIFY_FILES	= (1 <<  9),	/*!< from --nofiles */
    VERIFY_DEPS		= (1 << 10),	/*!< from --nodeps */
    VERIFY_SCRIPT	= (1 << 11),	/*!< from --noscripts */
    VERIFY_MD5		= (1 << 12)	/*!< from --nomd5 */
} rpmVerifyFlags;

/** \ingroup rpmcli
 * Describe query/verify command line request.
 */
typedef struct rpmQVArguments {
    rpmQVSources qva_source;	/*!< Identify CLI arg type. */
    int 	qva_sourceCount;/*!< Exclusive check (>1 is error). */
    int		qva_flags;	/*!< Bit(s) to control operation. */
    int		qva_verbose;	/*!< (unused) */
    const char *qva_queryFormat;/*!< Format for headerSprintf(). */
    const char *qva_prefix;	/*!< Path to top of install tree. */
    char	qva_mode;	/*!< 'q' is query, 'v' is verify mode. */
    char	qva_char;	/*!< (unused) always ' ' */
} QVA_t;

/** \ingroup rpmcli
 */
extern QVA_t rpmQVArgs;

/** \ingroup rpmcli
 */
extern struct poptOption rpmQVSourcePoptTable[];

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param db		rpm database
 * @param h		header to use for query/verify
 */
typedef	int (*QVF_t) (QVA_t *qva, rpmdb db, Header h);

/** \ingroup rpmcli
 * Display query/verify information for each header in iterator.
 * @param qva		parsed query/verify options
 * @param mi		rpm database iterator
 * @param showPackage	query/verify display routine
 * @return		result of last non-zero showPackage() return
 */
int showMatches(QVA_t *qva, /*@only@*/ /*@null@*/ rpmdbMatchIterator mi,
	QVF_t showPackage);

/** \ingroup rpmcli
 */
extern int specedit;

/** \ingroup rpmcli
 */
extern struct poptOption rpmQueryPoptTable[];

/** \ingroup rpmcli
 * Display list of tags that can be used in --queryformat.
 * @param f	file handle to use for display
 */
void rpmDisplayQueryTags(FILE * f);

/** \ingroup rpmcli
 * Common query/verify source interface, called once for each CLI arg.
 * @param qva		parsed query/verify options
 * @param source	type of source to query/verify
 * @param arg		name of source to query/verify
 * @param db		rpm database
 * @param showPackage	query/verify specific display routine
 * @return		showPackage() result, 1 if rpmdbInitIterator() is NULL
 */
int rpmQueryVerify(QVA_t *qva, rpmQVSources source, const char * arg,
	rpmdb db, QVF_t showPackage);

/** \ingroup rpmcli
 * Display results of package query.
 * @todo Devise a meaningful return code.
 * @param qva		parsed query/verify options
 * @param db		rpm database (unused for queries)
 * @param h		header to use for query
 * @return		0 always
 */
int showQueryPackage(QVA_t *qva, rpmdb db, Header h);

/** \ingroup rpmcli
 * Display package information.
 * @param qva		parsed query/verify options
 * @param source	type of source to query
 * @param arg		name of source to query
 * @return		rpmQueryVerify() result, or 1 on rpmdbOpen() failure
 */
int rpmQuery(QVA_t *qva, rpmQVSources source, const char * arg);

/** \ingroup rpmcli
 */
extern struct poptOption rpmVerifyPoptTable[];

/** \ingroup rpmcli
 * Display results of package verify.
 * @param qva		parsed query/verify options
 * @param db		rpm database
 * @param h		header to use for verify
 * @return		result of last non-zero verify return
 */
int showVerifyPackage(QVA_t *qva, /*@only@*/ rpmdb db, Header h);

/** \ingroup rpmcli
 * Verify package install.
 * @param qva		parsed query/verify options
 * @param source	type of source to verify
 * @param arg		name of source to verify
 * @return		rpmQueryVerify() result, or 1 on rpmdbOpen() failure
 */
int rpmVerify(QVA_t *qva, rpmQVSources source, const char *arg);

/*@}*/
/* ==================================================================== */
/** \name RPMEIU */
/*@{*/
/* --- install/upgrade/erase modes */

/** \ingroup rpmcli
 * Bit(s) to control rpmInstall() operation.
 */
typedef enum rpmInstallInterfaceFlags_e {
    INSTALL_PERCENT	= (1 << 0),	/*!< from --percent */
    INSTALL_HASH	= (1 << 1),	/*!< from --hash */
    INSTALL_NODEPS	= (1 << 2),	/*!< from --nodeps */
    INSTALL_NOORDER	= (1 << 3),	/*!< from --noorder */
    INSTALL_LABEL	= (1 << 4),	/*!< from --verbose (notify) */
    INSTALL_UPGRADE	= (1 << 5),	/*!< from --upgrade */
    INSTALL_FRESHEN	= (1 << 6)	/*!< from --freshen */
} rpmInstallInterfaceFlags;

/** \ingroup rpmcli
 * Install/upgrade/freshen binary rpm package.
 * @param rootdir	path to top of install tree
 * @param argv		array of package file names (NULL terminated)
 * @param transFlags	bits to control rpmRunTransactions()
 * @param interfaceFlags bits to control rpmInstall()
 * @param probFilter 	bits to filter problem types
 * @param relocations	package file relocations
 * @return		0 on success
 */
int rpmInstall(const char * rootdir, const char ** argv,
		rpmtransFlags transFlags, 
		rpmInstallInterfaceFlags interfaceFlags,
		rpmprobFilterFlags probFilter,
		rpmRelocation * relocations);

/** \ingroup rpmcli
 * Install source rpm package.
 * @param prefix	path to top of install tree
 * @param arg		source rpm file name
 * @retval specFile	address of (installed) spec file name
 * @retval cookie
 * @return		0 on success
 */
int rpmInstallSource(const char * prefix, const char * arg,
		/*@out@*/ const char ** specFile, /*@out@*/ char ** cookie);

/** \ingroup rpmcli
 * Bit(s) to control rpmErase() operation.
 */
typedef enum rpmEraseInterfaceFlags_e {
    UNINSTALL_NODEPS	= (1 << 0),	/*!< from --nodeps */
    UNINSTALL_ALLMATCHES= (1 << 1)	/*!< from --allmatches */
} rpmEraseInterfaceFlags;

/** \ingroup rpmcli
 * Erase binary rpm package.
 * @param rootdir	path to top of install tree
 * @param argv		array of package file names (NULL terminated)
 * @param transFlags	bits to control rpmRunTransactions()
 * @param interfaceFlags bits to control rpmInstall()
 * @return		0 on success
 */
int rpmErase(const char * rootdir, const char ** argv,
		rpmtransFlags transFlags, 
		rpmEraseInterfaceFlags interfaceFlags);

/*@}*/
/* ==================================================================== */
/** \name RPMK */
/*@{*/

/** \ingroup signature
 * Tags found in signature header from package.
 */
enum rpmtagSignature {
    RPMSIGTAG_SIZE	= 1000,	/*!< Size in bytes. */
/* the md5 sum was broken *twice* on big endian machines */
    RPMSIGTAG_LEMD5_1	= 1001,	/*!< Broken MD5, take 1 */
    RPMSIGTAG_PGP	= 1002,	/*!< PGP 2.6.3 signature. */
    RPMSIGTAG_LEMD5_2	= 1003,	/*!< Broken MD5, take 2 */
    RPMSIGTAG_MD5	= 1004,	/*!< MD5 signature. */
    RPMSIGTAG_GPG	= 1005, /*!< GnuPG signature. */
    RPMSIGTAG_PGP5	= 1006,	/*!< PGP5 signature @deprecated legacy. */

/* Signature tags by Public Key Algorithm (RFC 2440) */
/* N.B.: These tags are tenative, the values may change */
    RPMTAG_PK_BASE	= 512,		/*!< @todo Implement. */
    RPMTAG_PK_RSA_ES	= RPMTAG_PK_BASE+1,	/*!< (unused */
    RPMTAG_PK_RSA_E	= RPMTAG_PK_BASE+2,	/*!< (unused) */
    RPMTAG_PK_RSA_S	= RPMTAG_PK_BASE+3,	/*!< (unused) */
    RPMTAG_PK_ELGAMAL_E	= RPMTAG_PK_BASE+16,	/*!< (unused) */
    RPMTAG_PK_DSA	= RPMTAG_PK_BASE+17,	/*!< (unused) */
    RPMTAG_PK_ELLIPTIC	= RPMTAG_PK_BASE+18,	/*!< (unused) */
    RPMTAG_PK_ECDSA	= RPMTAG_PK_BASE+19,	/*!< (unused) */
    RPMTAG_PK_ELGAMAL_ES= RPMTAG_PK_BASE+20,	/*!< (unused) */
    RPMTAG_PK_DH	= RPMTAG_PK_BASE+21,	/*!< (unused) */

    RPMTAG_HASH_BASE	= 512+64,	/*!< @todo Implement. */
    RPMTAG_HASH_MD5	= RPMTAG_HASH_BASE+1,	/*!< (unused) */
    RPMTAG_HASH_SHA1	= RPMTAG_HASH_BASE+2,	/*!< (unused) */
    RPMTAG_HASH_RIPEMD160= RPMTAG_HASH_BASE+3,	/*!< (unused) */
    RPMTAG_HASH_MD2	= RPMTAG_HASH_BASE+5,	/*!< (unused) */
    RPMTAG_HASH_TIGER192= RPMTAG_HASH_BASE+6,	/*!< (unused) */
    RPMTAG_HASH_HAVAL_5_160= RPMTAG_HASH_BASE+7	/*!< (unused) */
};

/**
 *  Return codes from verifySignature().
 */
typedef enum rpmVerifySignatureReturn_e {
    RPMSIG_OK		= 0,	/*!< Signature is OK. */
    RPMSIG_UNKNOWN	= 1,	/*!< Signature is unknown. */
    RPMSIG_BAD		= 2,	/*!< Signature does not verify. */
    RPMSIG_NOKEY	= 3,	/*!< Key is unavailable. */
    RPMSIG_NOTTRUSTED = 4	/*!< Signature is OK, but key is not trusted. */
} rpmVerifySignatureReturn;

/** \ingroup signature
 * Verify a signature from a package.
 * @param file		file name of header+payload
 * @param sigTag	type of signature
 * @param sig		signature itself
 * @param count		no. of bytes in signature
 * @retval result	detailed text result of signature verification
 * @return		result of signature verification
 */
rpmVerifySignatureReturn rpmVerifySignature(const char *file,
		int_32 sigTag, const void * sig, int count, char *result);

/** \ingroup signature
 * Destroy signature header from package.
 */
void rpmFreeSignature(Header h);

/* --- checksig/resign */

/** \ingroup rpmcli
 * Bit(s) to control rpmCheckSig() operation.
 */
typedef enum rpmCheckSigFlags_e {
    CHECKSIG_NONE	= 0,		/*!< Don't check any signatures. */
    CHECKSIG_PGP	= (1 << 0),	/*!< if not --nopgp */
    CHECKSIG_MD5	= (1 << 1),	/*!< if not --nomd5 */
    CHECKSIG_GPG	= (1 << 2)	/*!< if not --nogpg */
} rpmCheckSigFlags;

/** \ingroup rpmcli
 * Check elements in signature header.
 * @param flags		bit(s) to enable signature checks
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success
 */
int rpmCheckSig(rpmCheckSigFlags flags, const char ** argv);

/** \ingroup rpmcli
 * Bit(s) to control rpmReSign() operation.
 */
typedef enum rpmResignFlags_e {
    RESIGN_NEW_SIGNATURE = 0,	/*!< from --resign */
    RESIGN_ADD_SIGNATURE	/*!< from --addsign */
} rpmResignFlags;

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param add		type of signature operation
 * @param passPhrase
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success
 */
int rpmReSign(rpmResignFlags add, char *passPhrase, const char ** argv);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMLIB */
