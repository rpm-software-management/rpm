#ifndef H_RPMLIB
#define	H_RPMLIB

/** \ingroup rpmcli rpmrc rpmdep rpmtrans rpmdb lead signature header payload dbi
 * \file lib/rpmlib.h
 *
 */

/* This is the *only* module users of rpmlib should need to include */

/* and it shouldn't need these :-( */

#include "rpmio.h"
#include "rpmmessages.h"
#include "rpmerr.h"
#include "header.h"
#include "popt.h"

/**
 * Package read return codes.
 */
typedef	enum rpmRC_e {
    RPMRC_OK		= 0,
    RPMRC_BADMAGIC	= 1,
    RPMRC_FAIL		= 2,
    RPMRC_BADSIZE	= 3,
    RPMRC_SHORTREAD	= 4,
} rpmRC;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ const void * p)
	/*@modifies p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

/**
 * Return package signatures and header from file handle.
 * @deprecated Signature tags are appended to header in rpm-4.0.2.
 * @todo Eliminate.
 * @param fd		file handle
 * @retval sigp		address of signature header (or NULL)
 * @retval hdrp		address of header (or NULL)
 * @return		rpmRC return code
 */
rpmRC rpmReadPackageInfo(FD_t fd, /*@null@*/ /*@out@*/ Header * sigp,
		/*@null@*/ /*@out@*/ Header * hdrp)
	/*@modifies fd, *sigp, *hdrp, fileSystem @*/;

/**
 * Return package header and lead info from file handle.
 * @param fd		file handle
 * @retval hdrp		address of header (or NULL)
 * @retval isSource	address to return lead source flag (or NULL)
 * @retval major	address to return lead major (or NULL)
 * @retval minor	address to return lead minor (or NULL)
 * @return		rpmRC return code
 */
rpmRC rpmReadPackageHeader(FD_t fd, /*@null@*/ /*@out@*/ Header * hdrp,
		/*@null@*/ /*@out@*/ int * isSource,
		/*@null@*/ /*@out@*/ int * major,
		/*@null@*/ /*@out@*/ int * minor)
	/*@modifies fd, *hdrp, *isSource, *major, *minor, fileSystem @*/;

/** \ingroup header
 * Return name, version, release strings from header.
 * @param h		header
 * @retval np		address of name pointer (or NULL)
 * @retval vp		address of version pointer (or NULL)
 * @retval rp		address of release pointer (or NULL)
 * @return		0 always
 */
int headerNVR(Header h,
		/*@null@*/ /*@out@*/ const char ** np,
		/*@null@*/ /*@out@*/ const char ** vp,
		/*@null@*/ /*@out@*/ const char ** rp)
	/*@modifies *np, *vp, *rp @*/;

/** \ingroup header
 * Translate and merge legacy signature tags into header.
 * @param h		header
 * @param sig		signature header
 */
void headerMergeLegacySigs(Header h, const Header sig)
	/*@modifies h @*/;

/** \ingroup header
 * Regenerate signature header.
 * @param h		header
 * @return		regenerated signature header
 */
Header headerRegenSigHeader(const Header h)
	/*@*/;

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
/*@unused@*/
int rpmPackageGetEntry(void *leadp, Header sigs, Header h,
		int_32 tag, int_32 *type, void **p, int_32 *c)
	/*@modifies *type, *p, *c @*/;

/**
 * Automatically generated table of tag name/value pairs.
 */
/*@-redecl@*/
extern const struct headerTagTableEntry rpmTagTable[];
/*@=redecl@*/

/**
 * Number of entries in rpmTagTable.
 */
/*@-redecl@*/
extern const int rpmTagTableSize;
/*@=redecl@*/

/**
 * Table of query format extensions.
 * @note Chains to headerDefaultFormats[].
 */
/*@-redecl@*/
extern const struct headerSprintfExtension rpmHeaderFormats[];
/*@=redecl@*/

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
/*@-enummemuse@*/
    RPMTAG_HEADERREGIONS	= HEADER_REGIONS,	/*!< Regions. */

    RPMTAG_HEADERI18NTABLE	= HEADER_I18NTABLE, /*!< I18N string locales. */
/*@=enummemuse@*/

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

    RPMTAG_SHA1HEADER		= RPMTAG_SIG_BASE+8,

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
/*@-enummemuse@*/
    RPMTAG_CHANGELOG		= 1017, /*!< internal */
/*@=enummemuse@*/
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
/*@-enummemuse@*/
    RPMTAG_ROOT			= 1038, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_FILEUSERNAME		= 1039,
    RPMTAG_FILEGROUPNAME	= 1040,
/*@-enummemuse@*/
    RPMTAG_EXCLUDE		= 1041, /*!< internal - obsolete */
    RPMTAG_EXCLUSIVE		= 1042, /*!< internal - obsolete */
/*@=enummemuse@*/
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
    RPMTAG_BUILDROOT		= 1057, /*!< internal */
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
/*@-enummemuse@*/
    RPMTAG_CAPABILITY		= 1105, /*!< internal obsolete */
/*@=enummemuse@*/
    RPMTAG_SOURCEPACKAGE	= 1106, /*!< internal */
/*@-enummemuse@*/
    RPMTAG_OLDORIGFILENAMES	= 1107, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_BUILDPREREQ		= 1108, /*!< internal */
    RPMTAG_BUILDREQUIRES	= 1109, /*!< internal */
    RPMTAG_BUILDCONFLICTS	= 1110, /*!< internal */
/*@-enummemuse@*/
    RPMTAG_BUILDMACROS		= 1111, /*!< internal */
/*@=enummemuse@*/
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
    RPMTAG_INSTALLTID		= 1128,
    RPMTAG_REMOVETID		= 1129,
/*@-enummemuse@*/
    RPMTAG_FIRSTFREE_TAG	/*!< internal */
/*@=enummemuse@*/
} rpmTag;

#define	RPMTAG_EXTERNAL_TAG		1000000

/**
 * File States (when installed).
 */
typedef enum rpmfileState_e {
    RPMFILE_STATE_NORMAL 	= 0,
    RPMFILE_STATE_REPLACED 	= 1,
    RPMFILE_STATE_NOTINSTALLED	= 2,
    RPMFILE_STATE_NETSHARED	= 3
} rpmfileState;

/**
 * File Attributes.
 */
typedef	enum rpmfileAttrs_e {
/*@-enummemuse@*/
    RPMFILE_NONE	= 0,
/*@=enummemuse@*/
    RPMFILE_CONFIG	= (1 << 0),	/*!< from %%config */
    RPMFILE_DOC		= (1 << 1),	/*!< from %%doc */
/*@-enummemuse@*/
    RPMFILE_DONOTUSE	= (1 << 2),	/*!< @todo (unimplemented) from %donotuse. */
/*@=enummemuse@*/
    RPMFILE_MISSINGOK	= (1 << 3),	/*!< from %%config(missingok) */
    RPMFILE_NOREPLACE	= (1 << 4),	/*!< from %%config(noreplace) */
    RPMFILE_SPECFILE	= (1 << 5),	/*!< @todo (unnecessary) marks 1st file in srpm. */
    RPMFILE_GHOST	= (1 << 6),	/*!< from %%ghost */
    RPMFILE_LICENSE	= (1 << 7),	/*!< from %%license */
    RPMFILE_README	= (1 << 8),	/*!< from %%readme */
    RPMFILE_EXCLUDE	= (1 << 9)	/*!< from %%exclude */
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
/*@-enummemuse@*/
    RPMSENSE_SERIAL	= (1 << 0),	/*!< @todo Legacy. */
/*@=enummemuse@*/
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
/*@-enummemuse@*/
    RPMSENSE_TRIGGERPREIN = (1 << 25)	/*!< @todo Implement %triggerprein. */
/*@=enummemuse@*/

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
/*@-redecl@*/
/*@observer@*/ /*@null@*/ extern const char * rpmGetVar(int var)
	/*@*/;
/*@=redecl@*/

/** \ingroup rpmrc
 * Set value of an rpmrc variable.
 * @deprecated Use rpmDefineMacro() to change appropriate macro instead.
 * @todo Eliminate from API.
 */
void rpmSetVar(int var, const char * val)
	/*@modifies internalState @*/;

/** \ingroup rpmrc
 * List of macro files to read when configuring rpm.
 * This is a colon separated list of files. URI's are permitted as well,
 * identified by the token '://', so file paths must not begin with '//'.
 */
/*@-redecl@*/
/*@observer@*/ extern const char * macrofiles;
/*@=redecl@*/

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
int rpmReadConfigFiles(/*@null@*/ const char * file,
		/*@null@*/ const char * target)
	/*@modifies fileSystem @*/;

/** \ingroup rpmrc
 * Read rpmrc (and macro) configuration file(s).
 * @param rcfiles	colon separated files to read (NULL uses default)
 * @return		0 on succes
 */
int rpmReadRC(/*@null@*/ const char * rcfiles)
	/*@modifies fileSystem @*/;

/** \ingroup rpmrc
 * Return current arch name and/or number.
 * @todo Generalize to extract arch component from target_platform macro.
 * @retval name		address of arch name (or NULL)
 * @retval num		address of arch number (or NULL)
 */
void rpmGetArchInfo( /*@null@*/ /*@out@*/ const char ** name,
		/*@null@*/ /*@out@*/ int * num)
	/*@modifies *name, *num @*/;

/** \ingroup rpmrc
 * Return current os name and/or number.
 * @todo Generalize to extract os component from target_platform macro.
 * @retval name		address of os name (or NULL)
 * @retval num		address of os number (or NULL)
 */
void rpmGetOsInfo( /*@null@*/ /*@out@*/ const char ** name,
		/*@null@*/ /*@out@*/ int * num)
	/*@modifies *name, *num @*/;

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
int rpmMachineScore(int type, const char * name)
	/*@*/;

/** \ingroup rpmrc
 * Display current rpmrc (and macro) configuration.
 * @param fp		output file handle
 * @return		0 always
 */
int rpmShowRC(FILE * fp)
	/*@modifies *fp, fileSystem @*/;

/** \ingroup rpmrc
 * @deprecated Use addMacro to set _target_* macros.
 * @todo Eliminate from API.
 # @note Only used by build code.
 * @param archTable
 * @param osTable
 */
void rpmSetTables(int archTable, int osTable)
	/*@modifies internalState @*/;

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
void rpmSetMachine(/*@null@*/ const char * arch, /*@null@*/ const char * os)
	/*@modifies internalState @*/;

/** \ingroup rpmrc
 * Return current arch/os names.
 * @deprecated Use rpmExpand on _target_* macros.
 * @todo Eliminate from API.
 *
 * @retval arch		address of arch name (or NULL)
 * @retval os		address of os name (or NULL)
 */
/*@unused@*/
void rpmGetMachine( /*@null@*/ /*@out@*/ const char **arch,
		/*@null@*/ /*@out@*/ const char **os)
	/*@modifies *arch, *os @*/;

/** \ingroup rpmrc
 * Destroy rpmrc arch/os compatibility tables.
 * @todo Eliminate from API.
 */
void rpmFreeRpmrc(void)
	/*@modifies internalState @*/;

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
int rpmdbOpen (/*@null@*/ const char * root, /*@null@*/ /*@out@*/ rpmdb * dbp,
		int mode, int perms)
	/*@modifies *dbp, fileSystem @*/;

/** \ingroup rpmdb
 * Initialize database.
 * @param root		path to top of install tree
 * @param perms		database permissions
 * @return		0 on success
 */
int rpmdbInit(/*@null@*/ const char * root, int perms)
	/*@modifies fileSystem @*/;

/** \ingroup rpmdb
 * Verify database components.
 * @param root		path to top of install tree
 * @return		0 on success
 */
int rpmdbVerify(/*@null@*/ const char * root)
	/*@modifies fileSystem @*/;

/** \ingroup rpmdb
 * Close all database indices and free rpmdb.
 * @param rpmdb		rpm database
 * @return		0 on success
 */
int rpmdbClose (/*@only@*/ /*@null@*/ rpmdb rpmdb)
	/*@modifies fileSystem @*/;

/** \ingroup rpmdb
 * Sync all database indices.
 * @param rpmdb		rpm database
 * @return		0 on success
 */
int rpmdbSync (/*@null@*/ rpmdb rpmdb)
	/*@modifies fileSystem @*/;

/** \ingroup rpmdb
 * Open all database indices.
 * @param db		rpm database
 * @return		0 on success
 */
int rpmdbOpenAll (/*@null@*/ rpmdb db)
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmdb
 * Return number of instances of package in rpm database.
 * @param db		rpm database
 * @param name		rpm package name
 * @return		number of instances
 */
int rpmdbCountPackages(rpmdb db, const char * name)
	/*@modifies db @*/;

/** \ingroup rpmdb
 */
typedef /*@abstract@*/ struct _rpmdbMatchIterator * rpmdbMatchIterator;

/** \ingroup rpmdb
 * Destroy rpm database iterator.
 * @param mi		rpm database iterator
 * @return		NULL always
 */
/*@null@*/ rpmdbMatchIterator rpmdbFreeIterator(
		/*@only@*//*@null@*/rpmdbMatchIterator mi)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Return rpm database used by iterator.
 * @param mi		rpm database iterator
 * @return		rpm database handle
 */
/*@kept@*/ /*@null@*/ rpmdb rpmdbGetIteratorRpmDB(
		/*@null@*/ rpmdbMatchIterator mi)
	/*@*/;

/** \ingroup rpmdb
 * Return join key for current position of rpm database iterator.
 * @param mi		rpm database iterator
 * @return		current join key
 */
unsigned int rpmdbGetIteratorOffset(/*@null@*/ rpmdbMatchIterator mi)
	/*@*/;

/** \ingroup rpmdb
 * Return number of elements in rpm database iterator.
 * @param mi		rpm database iterator
 * @return		number of elements
 */
int rpmdbGetIteratorCount(/*@null@*/ rpmdbMatchIterator mi)
	/*@*/;

/** \ingroup rpmdb
 * Append items to set of package instances to iterate.
 * @param mi		rpm database iterator
 * @param hdrNums	array of package instances
 * @param nHdrNums	number of elements in array
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbAppendIterator(/*@null@*/ rpmdbMatchIterator mi,
		/*@null@*/ const int * hdrNums, int nHdrNums)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Remove items from set of package instances to iterate.
 * @note Sorted hdrNums are always passed in rpmlib.
 * @param mi		rpm database iterator
 * @param hdrNums	array of package instances
 * @param nHdrNums	number of elements in array
 * @param sorted	is the array sorted? (array will be sorted on return)
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbPruneIterator(/*@null@*/ rpmdbMatchIterator mi,
		/*@null@*/ int * hdrNums, int nHdrNums, int sorted)
	/*@modifies mi, hdrNums @*/;

/**
 * Tag value pattern match mode.
 */
typedef enum rpmMireMode_e {
    RPMMIRE_DEFAULT	= 0,	/*!< regex with \., .* and ^...$ */
    RPMMIRE_STRCMP	= 1,	/*!< strcmp on strings */
    RPMMIRE_REGEX	= 2,	/*!< regex patterns */
    RPMMIRE_GLOB	= 3	/*!< glob patterns */
} rpmMireMode;

/** \ingroup rpmdb
 * Add pattern to iterator selector.
 * @param mi		rpm database iterator
 * @param tag		rpm tag
 * @param mode		type of pattern match
 * @param pattern	pattern to match
 * @return		0 on success
 */
int rpmdbSetIteratorRE(/*@null@*/ rpmdbMatchIterator mi, rpmTag tag,
		rpmMireMode mode, /*@null@*/ const char * pattern)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Modify iterator to filter out headers that do not match version.
 * @deprecated Use
 *	rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_DEFAULT, version)
 * instead.
 * @todo Eliminate from API.
 * @param mi		rpm database iterator
 * @param version	version to match (can be a regex pattern)
 * @return		0 on success
 */
/*@unused@*/
int rpmdbSetIteratorVersion(/*@null@*/ rpmdbMatchIterator mi,
		/*@null@*/ const char * version)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Modify iterator to filter out headers that do not match release.
 * @deprecated Use
 *	rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_DEFAULT, release)
 * instead.
 * @todo Eliminate from API.
 * @param mi		rpm database iterator
 * @param release	release to match (can be a regex pattern)
 * @return		0 on success
 */
/*@unused@*/
int rpmdbSetIteratorRelease(/*@null@*/ rpmdbMatchIterator mi,
		/*@null@*/ const char * release)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Prepare iterator for lazy writes.
 * @note Must be called before rpmdbNextIterator() in CDB model database.
 * @param mi		rpm database iterator
 * @param rewrite	new value of rewrite
 * @return		previous value
 */
int rpmdbSetIteratorRewrite(/*@null@*/ rpmdbMatchIterator mi, int rewrite)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Modify iterator to mark header for lazy write.
 * @param mi		rpm database iterator
 * @param modified	new value of modified
 * @return		previous value
 */
int rpmdbSetIteratorModified(/*@null@*/ rpmdbMatchIterator mi, int modified)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Return next package header from iteration.
 * @param mi		rpm database iterator
 * @return		NULL on end of iteration.
 */
/*@null@*/ Header rpmdbNextIterator(/*@null@*/ rpmdbMatchIterator mi)
	/*@modifies mi @*/;

/** @todo Remove debugging entry from the ABI. */
/*@unused@*/
/*@null@*/ Header XrpmdbNextIterator(rpmdbMatchIterator mi,
		const char * f, unsigned int l)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Return database iterator.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		NULL on failure
 */
/*@only@*/ /*@null@*/ rpmdbMatchIterator rpmdbInitIterator(
			/*@kept@*/ /*@null@*/ rpmdb db, int rpmtag,
			/*@null@*/ const void * key, size_t keylen)
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmdb
 * Add package header to rpm database and indices.
 * @param db		rpm database
 * @param iid		install transaction id (or -1 to skip)
 * @param h		header
 * @return		0 on success
 */
int rpmdbAdd(rpmdb db, int iid, Header h)
	/*@modifies db, h, fileSystem @*/;

/** \ingroup rpmdb
 * Remove package header from rpm database and indices.
 * @param db		rpm database
 * @param rid		remove transaction id (or -1 to skip)
 * @param offset	location in Packages dbi
 * @return		0 on success
 */
int rpmdbRemove(rpmdb db, int rid, unsigned int offset)
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmdb
 * Rebuild database indices from package headers.
 * @param root		path to top of install tree
 */
int rpmdbRebuild(/*@null@*/ const char * root)
	/*@modifies fileSystem @*/;

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
/*@kept@*/ /*@null@*/ const void * key;
/*@null@*/ Header h;
    rpmProblemType type;
    int ignoreProblem;
/*@only@*/ /*@null@*/ const char * str1;
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
	/*@modifies *fp, fileSystem @*/;

/**
 * Dependency problems found by rpmdepCheck().
 * @todo Rename, but rpmfind prevents "struct rpmDependencyConflict_s".
 */
typedef /*@abstract@*/ struct rpmDependencyConflict {
    const char * byName;	/*!< package name */
    const char * byVersion;	/*!< package version */
    const char * byRelease;	/*!< package release */
    Header byHeader;		/*!< header with dependency problems */
    /*
     * These needs fields are misnamed -- they are used for the package
     * which isn't needed as well.
     */
    const char * needsName;	/*!< dependency name */
    const char * needsVersion;	/*!< dependency epoch:version-release */
    int needsFlags;		/*!< dependency flags */
/*@owned@*/ /*@null@*/ const void ** suggestedPackages; /* terminated by NULL */
    enum {
	RPMDEP_SENSE_REQUIRES,		/*!< requirement not satisfied. */
	RPMDEP_SENSE_CONFLICTS		/*!< conflict was found. */
    } sense;
} * rpmDependencyConflict;

/**
 * Print results of rpmdepCheck() dependency check.
 * @param fp		output file
 * @param conflicts	dependency problems
 * @param numConflicts	no. of dependency problems
 */
void printDepProblems(FILE * fp, const rpmDependencyConflict conflicts,
			int numConflicts)
	/*@modifies *fp, fileSystem @*/;

/**
 * Return formatted string representation of problem.
 * @deprecated API: prob used to be passed by value, now passed by reference.
 * @param prob		rpm problem
 * @return		formatted string
 */
/*@-redecl@*/	/* LCL: is confused. */
/*@only@*/ extern const char * rpmProblemString(const rpmProblem prob)
	/*@*/;
/*@=redecl@*/

/**
 * Output formatted string representation of problem to file handle.
 * @deprecated API: prob used to be passed by value, now passed by reference.
 * @param fp		file handle
 * @param prob		rpm problem
 */
void rpmProblemPrint(FILE *fp, rpmProblem prob)
	/*@modifies prob, *fp, fileSystem @*/;

/**
 * Print problems to file handle.
 * @param fp		file handle
 * @param probs		problem set
 */
void rpmProblemSetPrint(FILE *fp, rpmProblemSet probs)
	/*@modifies probs, *fp, fileSystem @*/;

/**
 * Destroy problem set.
 * @param probs		problem set
 */
void rpmProblemSetFree( /*@only@*/ rpmProblemSet probs)
	/*@modifies probs @*/;

/*@}*/
/* ==================================================================== */
/** \name RPMTS */
/*@{*/
/**
 * Prototype for headerFreeData() vector.
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef /*@null@*/
    void * (*HFD_t) (/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/;

/**
 * Prototype for headerGetEntry() vector.
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
			/*@null@*/ /*@out@*/ rpmTagType * type,
			/*@null@*/ /*@out@*/ void ** p,
			/*@null@*/ /*@out@*/ int_32 * c)
	/*@modifies *type, *p, *c @*/;

/**
 * Prototype for headerAddEntry() vector.
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
			const void * p, int_32 c)
	/*@modifies h @*/;

/**
 * Prototype for headerModifyEntry() vector.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef int (*HME_t) (Header h, rpmTag tag, rpmTagType type,
			const void * p, int_32 c)
	/*@modifies h @*/;

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
typedef int (*HRE_t) (Header h, int_32 tag)
	/*@modifies h @*/;

/**
 * We pass these around as an array with a sentinel.
 */
typedef struct rpmRelocation_s {
/*@only@*/ /*@null@*/ const char * oldPath;
				/*!< NULL here evals to RPMTAG_DEFAULTPREFIX, */
/*@only@*/ /*@null@*/ const char * newPath;
				/*!< NULL means to omit the file completely! */
} rpmRelocation;

/**
 * Install source package.
 * @param rootDir	path to top of install tree (or NULL)
 * @param fd		file handle
 * @retval specFilePtr	address of spec file name (or NULL)
 * @param notify	progress callback
 * @param notifyData	progress callback private data
 * @retval cooke	address of cookie pointer (or NULL)
 * @return		rpmRC return code
 */
rpmRC rpmInstallSourcePackage(/*@null@*/ const char * rootDir, FD_t fd,
			/*@null@*/ /*@out@*/ const char ** specFilePtr,
			/*@null@*/ rpmCallbackFunction notify,
			/*@null@*/ rpmCallbackData notifyData,
			/*@null@*/ /*@out@*/ char ** cookie)
	/*@modifies fd, *specFilePtr, *cookie @*/;

/**
 * Compare headers to determine which header is "newer".
 * @param first		1st header
 * @param second	2nd header
 * @return		result of comparison
 */
int rpmVersionCompare(Header first, Header second)
	/*@*/;

/**
 * File disposition(s) during package install/erase transaction.
 */
typedef enum fileAction_e {
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
    FA_SKIPMULTILIB,	/*!< ... untouched. @todo state "multilib" ???. */
} fileAction;

#define XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED || (_a) == FA_SKIPMULTILIB)

/**
 * File types.
 * These are the types of files used internally by rpm. The file
 * type is determined by applying stat(2) macros like S_ISDIR to
 * the file mode tag from a header. The values are arbitrary,
 * but are identical to the linux stat(2) file types.
 */
typedef enum fileTypes_e {
    PIPE	=  1,	/*!< pipe/fifo */
    CDEV	=  2,	/*!< character device */
    XDIR	=  4,	/*!< directory */
    BDEV	=  6,	/*!< block device */
    REG		=  8,	/*!< regular file */
    LINK	= 10,	/*!< hard link */
    SOCK	= 12,	/*!< socket */
} fileTypes;

/** \ingroup payload
 * Iterator across package file info, forward on install, backward on erase.
 */
typedef /*@abstract@*/ struct fsmIterator_s * FSMI_t;

/** \ingroup payload
 * File state machine data.
 */
typedef /*@abstract@*/ struct fsm_s * FSM_t;

/** \ingroup rpmtrans
 * Package state machine data.
 */
typedef /*@abstract@*/ struct psm_s * PSM_t;

/** \ingroup rpmtrans
 */
typedef /*@abstract@*/ struct transactionFileInfo_s * TFI_t;

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
 * @return		transaction set
 */
/*@only@*/ rpmTransactionSet rpmtransCreateSet(
		/*@null@*/ /*@kept@*/ rpmdb rpmdb,
		/*@null@*/ const char * rootDir)
	/*@*/;

/** \ingroup rpmtrans
 * Add package to be installed to unordered transaction set.
 *
 * If fd is NULL, the callback specified in rpmtransCreateSet() is used to
 * open and close the file descriptor. If Header is NULL, the fd is always
 * used, otherwise fd is only needed (and only opened) for actual package 
 * installation.
 *
 * @param ts		transaction set
 * @param h		package header
 * @param fd		package file handle
 * @param key		package private data
 * @param upgrade	is package being upgraded?
 * @param relocs	package file relocations
 * @return		0 on success, 1 on I/O error, 2 needs capabilities
 */
int rpmtransAddPackage(rpmTransactionSet ts, Header h, /*@null@*/ FD_t fd,
		/*@null@*/ /*@owned@*/ const void * key, int upgrade,
		/*@null@*/ rpmRelocation * relocs)
	/*@modifies fd, h, ts @*/;

/** \ingroup rpmtrans
 * Add package to universe of possible packages to install in transaction set.
 * @param ts		transaction set
 * @param h		header
 * @param key		package private data
 */
/*@unused@*/
void rpmtransAvailablePackage(rpmTransactionSet ts, Header h,
		/*@null@*/ /*@owned@*/ const void * key)
	/*@modifies h, ts @*/;

/** \ingroup rpmtrans
 * Add package to be removed to unordered transaction set.
 * @param ts		transaction set
 * @param dboffset	rpm database instance
 */
void rpmtransRemovePackage(rpmTransactionSet ts, int dboffset)
	/*@modifies ts @*/;

/** \ingroup rpmtrans
 * Destroy transaction set.
 * @param ts		transaction set
 */
void rpmtransFree( /*@only@*/ rpmTransactionSet ts)
	/*@modifies ts @*/;

/** \ingroup rpmtrans
 * Save file handle to be used as stderr when running package scripts.
 * @param ts		transaction set
 * @param fd		file handle
 */
/*@unused@*/
void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd)
	/*@modifies ts, fd @*/;

/** \ingroup rpmtrans
 * Retrieve keys from ordered transaction set.
 * @todo Removed packages have no keys, returned as interleaved NULL pointers.
 * @param ts		transaction set
 * @retval ep		address of returned element array pointer (or NULL)
 * @retval nep		address of no. of returned elements (or NULL)
 * @return		0 always
 */
/*@unused@*/
int rpmtransGetKeys(const rpmTransactionSet ts,
		/*@null@*/ /*@out@*/ const void *** ep,
		/*@null@*/ /*@out@*/ int * nep)
	/*@modifies ep, nep @*/;

/** \ingroup rpmtrans
 * Check that all dependencies can be resolved.
 * @param ts		transaction set
 * @retval conflicts	dependency problems
 * @retval numConflicts	no. of dependency problems
 * @return		0 on success
 */
int rpmdepCheck(rpmTransactionSet ts,
		/*@exposed@*/ /*@out@*/ rpmDependencyConflict * conflicts,
		/*@exposed@*/ /*@out@*/ int * numConflicts)
	/*@modifies ts, *conflicts, *numConflicts, fileSystem @*/;

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
 * @param ts		transaction set
 * @return		0 if packages are successfully ordered, 1 otherwise
 */
int rpmdepOrder(rpmTransactionSet ts)
	/*@modifies ts, fileSystem @*/;

/** \ingroup rpmtrans
 * Destroy dependency conflicts storage.
 * @param conflicts	dependency problems
 * @param numConflicts	no. of dependency problems
 * @retrun		NULL always
 */
/*@null@*/ rpmDependencyConflict rpmdepFreeConflicts(
		/*@only@*/ /*@null@*/ rpmDependencyConflict conflicts,
		int numConflicts)
	/*@modifies conflicts @*/;

/** \ingroup rpmtrans
 * Bit(s) to control rpmRunTransaction() operation.
 */
typedef enum rpmtransFlags_e {
    RPMTRANS_FLAG_NONE		= 0,
    RPMTRANS_FLAG_TEST		= (1 <<  0),	/*!< from --test */
    RPMTRANS_FLAG_BUILD_PROBS	= (1 <<  1),	/*!< @todo Document. */
    RPMTRANS_FLAG_NOSCRIPTS	= (1 <<  2),	/*!< from --noscripts */
    RPMTRANS_FLAG_JUSTDB	= (1 <<  3),	/*!< from --justdb */
    RPMTRANS_FLAG_NOTRIGGERS	= (1 <<  4),	/*!< from --notriggers */
    RPMTRANS_FLAG_NODOCS	= (1 <<  5),	/*!< from --excludedocs */
    RPMTRANS_FLAG_ALLFILES	= (1 <<  6),	/*!< from --allfiles */
/*@-enummemuse@*/
    RPMTRANS_FLAG_KEEPOBSOLETE	= (1 <<  7),	/*!< @todo Document. */
/*@=enummemuse@*/
    RPMTRANS_FLAG_MULTILIB	= (1 <<  8),	/*!< @todo Document. */
    RPMTRANS_FLAG_DIRSTASH	= (1 <<  9),	/*!< from --dirstash */
    RPMTRANS_FLAG_REPACKAGE	= (1 << 10),	/*!< from --repackage */

    RPMTRANS_FLAG_PKGCOMMIT	= (1 << 11),
/*@-enummemuse@*/
    RPMTRANS_FLAG_PKGUNDO	= (1 << 12),
/*@=enummemuse@*/
    RPMTRANS_FLAG_COMMIT	= (1 << 13),
/*@-enummemuse@*/
    RPMTRANS_FLAG_UNDO		= (1 << 14),
/*@=enummemuse@*/
    RPMTRANS_FLAG_REVERSE	= (1 << 15),

    RPMTRANS_FLAG_NOTRIGGERPREIN= (1 << 16),
    RPMTRANS_FLAG_NOPRE		= (1 << 17),
    RPMTRANS_FLAG_NOPOST	= (1 << 18),
    RPMTRANS_FLAG_NOTRIGGERIN	= (1 << 19),
    RPMTRANS_FLAG_NOTRIGGERUN	= (1 << 20),
    RPMTRANS_FLAG_NOPREUN	= (1 << 21),
    RPMTRANS_FLAG_NOPOSTUN	= (1 << 22),
    RPMTRANS_FLAG_NOTRIGGERPOSTUN = (1 << 23),
/*@-enummemuse@*/
    RPMTRANS_FLAG_NOPAYLOAD	= (1 << 24),
/*@=enummemuse@*/
    RPMTRANS_FLAG_APPLYONLY	= (1 << 25),

    RPMTRANS_FLAG_CHAINSAW	= (1 << 26),
} rpmtransFlags;

#define	_noTransScripts		\
  ( RPMTRANS_FLAG_NOPRE |	\
    RPMTRANS_FLAG_NOPOST |	\
    RPMTRANS_FLAG_NOPREUN |	\
    RPMTRANS_FLAG_NOPOSTUN	\
  )

#define	_noTransTriggers	\
  ( RPMTRANS_FLAG_NOTRIGGERPREIN | \
    RPMTRANS_FLAG_NOTRIGGERIN |	\
    RPMTRANS_FLAG_NOTRIGGERUN |	\
    RPMTRANS_FLAG_NOTRIGGERPOSTUN \
  )

/** \ingroup rpmtrans
 * Return copy of rpmlib internal provides.
 * @retval		address of array of rpmlib internal provide names
 * @retval		address of array of rpmlib internal provide flags
 * @retval		address of array of rpmlib internal provide versions
 * @return		no. of entries
 */
/*@unused@*/
int rpmGetRpmlibProvides(/*@null@*/ /*@out@*/ const char *** provNames,
			/*@null@*/ /*@out@*/ int ** provFlags,
			/*@null@*/ /*@out@*/ const char *** provVersions)
	/*@ modifies *provNames, *provFlags, *provVersions @*/;

/** \ingroup rpmtrans
 * Segmented string compare for version and/or release.
 *
 * @param a		1st string
 * @param b		2nd string
 * @return		+1 if a is "newer", 0 if equal, -1 if b is "newer"
 */
int rpmvercmp(const char * a, const char * b)
	/*@*/;

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
int rpmRangesOverlap(const char * AName, const char * AEVR, int AFlags,
			const char * BName, const char * BEVR, int BFlags)
	/*@*/;

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
void rpmShowRpmlibProvides(FILE * fp)
	/*@modifies *fp, fileSystem @*/;

/**
 * @todo Generalize filter mechanism.
 */
typedef enum rpmprobFilterFlags_e {
    RPMPROB_FILTER_NONE		= 0,
    RPMPROB_FILTER_IGNOREOS	= (1 << 0),	/*!< from --ignoreos */
    RPMPROB_FILTER_IGNOREARCH	= (1 << 1),	/*!< from --ignorearch */
    RPMPROB_FILTER_REPLACEPKG	= (1 << 2),	/*!< from --replacepkgs */
    RPMPROB_FILTER_FORCERELOCATE= (1 << 3),	/*!< from --badreloc */
    RPMPROB_FILTER_REPLACENEWFILES= (1 << 4),	/*!< from --replacefiles */
    RPMPROB_FILTER_REPLACEOLDFILES= (1 << 5),	/*!< from --replacefiles */
    RPMPROB_FILTER_OLDPACKAGE	= (1 << 6),	/*!< from --oldpackage */
    RPMPROB_FILTER_DISKSPACE	= (1 << 7),	/*!< from --ignoresize */
    RPMPROB_FILTER_DISKNODES	= (1 << 8)	/*!< from --ignoresize */
} rpmprobFilterFlags;

/** \ingroup rpmtrans
 * Process all packages in transaction set.
 * @param ts		transaction set
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
			rpmprobFilterFlags ignoreSet)
	/*@modifies ts, *newProbs, fileSystem @*/;

/*@}*/

/**
 * Return name of tag from value.
 * @param tag		tag value
 * @return		name of tag
 */
/*@-redecl@*/
/*@observer@*/ extern const char *const tagName(int tag)
	/*@*/;
/*@=redecl@*/

/**
 * Return value of tag from name.
 * @param targstr	name of tag
 * @return		tag value
 */
int tagValue(const char *tagstr)
	/*@*/;

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
/*@unused@*/ char reserved[16];	/*!< Pad to 96 bytes -- 8 byte aligned! */
} ;

/**
 * Release storage used by file system usage cache.
 */
void freeFilesystems(void)
	/*@modifies internalState@*/;

/**
 * Return (cached) file system mount points.
 * @retval listptr		addess of file system names (or NULL)
 * @retval num			address of number of file systems (or NULL)
 * @return			0 on success, 1 on error
 */
int rpmGetFilesystemList( /*@null@*/ /*@out@*/ const char *** listptr,
		/*@null@*/ /*@out@*/ int * num)
	/*@modifies *listptr, *num @*/;

/**
 * Determine per-file system usage for a list of files.
 * @param fileList		array of absolute file names
 * @param fssizes		array of file sizes
 * @param numFiles		number of files in list
 * @retval usagesPtr		address of per-file system usage array (or NULL)
 * @param flags			(unused)
 * @return			0 on success, 1 on error
 */
int rpmGetFilesystemUsage(const char ** fileList, int_32 * fssizes,
		int numFiles, /*@null@*/ /*@out@*/ uint_32 ** usagesPtr,
		int flags)
	/*@modifies *usagesPtr @*/;

/* ==================================================================== */
/** \name RPMQV */
/*@{*/

/** \ingroup rpmcli
 */
typedef struct rpmQVArguments_s * QVA_t;

/** \ingroup rpmcli
 * The command line argument will be used to retrieve header(s) ...
 * @todo Move to rpmcli.h
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
 * Bit(s) for rpmVerifyFile() attributes and result.
 * @todo Move to rpmcli.h.
 */
typedef enum rpmVerifyAttrs_e {
    RPMVERIFY_NONE	= 0,		/*!< */
    RPMVERIFY_MD5	= (1 << 0),	/*!< from %verify(md5) */
    RPMVERIFY_FILESIZE	= (1 << 1),	/*!< from %verify(size) */
    RPMVERIFY_LINKTO	= (1 << 2),	/*!< from %verify(link) */
    RPMVERIFY_USER	= (1 << 3),	/*!< from %verify(user) */
    RPMVERIFY_GROUP	= (1 << 4),	/*!< from %verify(group) */
    RPMVERIFY_MTIME	= (1 << 5),	/*!< from %verify(mtime) */
    RPMVERIFY_MODE	= (1 << 6),	/*!< from %verify(mode) */
    RPMVERIFY_RDEV	= (1 << 7),	/*!< from %verify(rdev) */
	/* bits 8-15 unused, reserved for rpmVerifyAttrs */
	/* bits 16-19 used in rpmVerifyFlags */
	/* bits 20-22 unused */
	/* bits 23-27 used in rpmQueryFlags */
    RPMVERIFY_READLINKFAIL= (1 << 28),	/*!< */
    RPMVERIFY_READFAIL	= (1 << 29),	/*!< */
    RPMVERIFY_LSTATFAIL	= (1 << 30)	/*!< */
	/* bit 31 unused */
} rpmVerifyAttrs;
#define	RPMVERIFY_ALL		~(RPMVERIFY_NONE)

/** \ingroup rpmcli
 * Verify file attributes (including MD5 sum).
 * @todo gnorpm and python bindings prevent this from being static.
 * @param root		path to top of install tree
 * @param h		header
 * @param filenum	index of file in header file info arrays
 * @retval result	address of bit(s) returned to indicate failure
 * @param omitMask	bit(s) to disable verify checks
 * @return		0 on success (or not installed), 1 on error
 */
int rpmVerifyFile(const char * root, Header h, int filenum,
		/*@out@*/ rpmVerifyAttrs * result, rpmVerifyAttrs omitMask)
	/*@modifies h, *result, fileSystem @*/;

/**
 * Return exit code from running verify script from header.
 * @todo gnorpm/kpackage prevents static, should be using VERIFY_SCRIPT flag.
 * @param rootDir	path to top of install tree
 * @param h		header
 * @param scriptFd	file handle to use for stderr (or NULL)
 * @return		0 on success
 */
int rpmVerifyScript(const char * rootDir, Header h, /*@null@*/ FD_t scriptFd)
	/*@modifies h, scriptFd, fileSystem @*/;

/*@}*/
/* ==================================================================== */
/** \name RPMEIU */
/*@{*/
/* --- install/upgrade/erase modes */

/** \ingroup rpmcli
 * Bit(s) to control rpmInstall() operation.
 * @todo Move to rpmcli.h
 */
typedef enum rpmInstallInterfaceFlags_e {
    INSTALL_NONE	= 0,
    INSTALL_PERCENT	= (1 << 0),	/*!< from --percent */
    INSTALL_HASH	= (1 << 1),	/*!< from --hash */
    INSTALL_NODEPS	= (1 << 2),	/*!< from --nodeps */
    INSTALL_NOORDER	= (1 << 3),	/*!< from --noorder */
    INSTALL_LABEL	= (1 << 4),	/*!< from --verbose (notify) */
    INSTALL_UPGRADE	= (1 << 5),	/*!< from --upgrade */
    INSTALL_FRESHEN	= (1 << 6),	/*!< from --freshen */
    INSTALL_NOERASE	= (1 << 7),	/*!< from --install */
/*@-enummemuse@*/
    INSTALL_ERASE	= (1 << 8)	/*!< from --erase */
/*@=enummemuse@*/
} rpmInstallInterfaceFlags;

/** \ingroup rpmcli
 * Bit(s) to control rpmErase() operation.
 */
typedef enum rpmEraseInterfaceFlags_e {
    UNINSTALL_NONE	= 0,
    UNINSTALL_NODEPS	= (1 << 0),	/*!< from --nodeps */
    UNINSTALL_ALLMATCHES= (1 << 1)	/*!< from --allmatches */
} rpmEraseInterfaceFlags;

/*@}*/
/* ==================================================================== */
/** \name RPMK */
/*@{*/

/** \ingroup signature
 * Tags found in signature header from package.
 */
/*@-enummemuse@*/
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
/*@=enummemuse@*/

/**
 *  Return codes from verifySignature().
 */
typedef enum rpmVerifySignatureReturn_e {
    RPMSIG_OK		= 0,	/*!< Signature is OK. */
    RPMSIG_UNKNOWN	= 1,	/*!< Signature is unknown. */
    RPMSIG_BAD		= 2,	/*!< Signature does not verify. */
    RPMSIG_NOKEY	= 3,	/*!< Key is unavailable. */
    RPMSIG_NOTTRUSTED	= 4	/*!< Signature is OK, but key is not trusted. */
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
		int_32 sigTag, const void * sig, int count, char * result)
	/*@modifies *result, fileSystem @*/;

/** \ingroup signature
 * Destroy signature header from package.
 * @param h		signature header
 * @return		NULL always
 */
/*@null@*/ Header rpmFreeSignature(/*@null@*/ /*@killref@*/ Header h)
	/*@modifies h @*/;

/*@}*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMLIB */
