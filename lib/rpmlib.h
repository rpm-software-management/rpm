#ifndef H_RPMLIB
#define	H_RPMLIB

/** \ingroup rpmcli rpmrc rpmdep rpmtrans rpmdb lead signature header payload dbi
 * \file lib/rpmlib.h
 */

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

/*@-redecl@*/
/*@checked@*/
extern struct MacroContext_s * rpmGlobalMacroContext;

/*@checked@*/
extern struct MacroContext_s * rpmCLIMacroContext;

/*@observer@*/ /*@checked@*/
extern const char * RPMVERSION;

/*@observer@*/ /*@checked@*/
extern const char * rpmNAME;

/*@observer@*/ /*@checked@*/
extern const char * rpmEVR;

/*@checked@*/
extern int rpmFLAGS;
/*@=redecl@*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/
void * _free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p)
	/*@modifies p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

/** \ingroup rpmtrans
 * The RPM Transaction Set.
 * Transaction sets are inherently unordered! RPM may reorder transaction
 * sets to reduce errors. In general, installs/upgrades are done before
 * strict removals, and prerequisite ordering is done on installs/upgrades.
 */
typedef /*@abstract@*/ /*@refcounted@*/
struct rpmTransactionSet_s * rpmTransactionSet;

/** \ingroup rpmtrans
 * An added/available package retrieval key.
 */
typedef /*@abstract@*/ void * alKey;
#define	RPMAL_NOMATCH	((alKey)-1L)

/** \ingroup rpmtrans
 * An added/available package retrieval index.
 */
/*@-mutrep@*/
typedef /*@abstract@*/ int alNum;
/*@=mutrep@*/

/**
 * Dependency tag sets from a header, so that a header can be discarded early.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmDepSet_s * rpmDepSet;

/**
 * File info tag sets from a header, so that a header can be discarded early.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmFNSet_s * rpmFNSet;

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
/*@unused@*/
int rpmHeaderGetEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
		/*@out@*/ void **p, /*@out@*/ int_32 *c)
	/*@modifies *type, *p, *c @*/;

/**
 * Automatically generated table of tag name/value pairs.
 */
/*@-redecl@*/
/*@observer@*/ /*@unchecked@*/
extern const struct headerTagTableEntry_s * rpmTagTable;
/*@=redecl@*/

/**
 * Number of entries in rpmTagTable.
 */
/*@-redecl@*/
/*@unchecked@*/
extern const int rpmTagTableSize;
/*@=redecl@*/

/**
 * Table of query format extensions.
 * @note Chains to headerDefaultFormats[].
 */
/*@-redecl@*/
/*@unchecked@*/
extern const struct headerSprintfExtension_s rpmHeaderFormats[];
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
    RPMTAG_SIGLEMD5_1		= RPMTAG_SIG_BASE+2,	/*!< internal */
    RPMTAG_SIGPGP		= RPMTAG_SIG_BASE+3,
    RPMTAG_SIGLEMD5_2		= RPMTAG_SIG_BASE+4,	/*!< internal */
    RPMTAG_SIGMD5	        = RPMTAG_SIG_BASE+5,
    RPMTAG_SIGGPG	        = RPMTAG_SIG_BASE+6,
    RPMTAG_SIGPGP5	        = RPMTAG_SIG_BASE+7,	/*!< internal */

/*@-enummemuse@*/
    RPMTAG_BADSHA1HEADER	= RPMTAG_SIG_BASE+8,	/*!< internal */
/*@=enummemuse@*/
    RPMTAG_SHA1HEADER		= RPMTAG_SIG_BASE+9,
    RPMTAG_PUBKEYS		= RPMTAG_SIG_BASE+10,

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
/*@-enummemuse@*/
    RPMTAG_BROKENMD5		= 1083, /*!< internal - obsolete */
/*@=enummemuse@*/
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
    RPMTAG_CAPABILITY		= 1105, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_SOURCEPACKAGE	= 1106, /*!< internal */
/*@-enummemuse@*/
    RPMTAG_OLDORIGFILENAMES	= 1107, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_BUILDPREREQ		= 1108, /*!< internal */
    RPMTAG_BUILDREQUIRES	= 1109, /*!< internal */
    RPMTAG_BUILDCONFLICTS	= 1110, /*!< internal */
/*@-enummemuse@*/
    RPMTAG_BUILDMACROS		= 1111, /*!< internal - unused */
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
    RPMTAG_SHA1RHN		= 1130, /*!< internal */
    RPMTAG_RHNPLATFORM		= 1131,
    RPMTAG_PLATFORM		= 1132,
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

#define	RPMFILE_ALL	~(RPMFILE_NONE)

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
    RPMSENSE_TRIGGERPREIN = (1 << 25),	/*!< @todo Implement %triggerprein. */
/*@=enummemuse@*/
    RPMSENSE_KEYRING	= (1 << 26)
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
    RPMSENSE_RPMLIB | \
    RPMSENSE_KEYRING )

#define	_notpre(_x)		((_x) & ~RPMSENSE_PREREQ)
#define	_INSTALL_ONLY_MASK \
    _notpre(RPMSENSE_SCRIPT_PRE|RPMSENSE_SCRIPT_POST|RPMSENSE_RPMLIB|RPMSENSE_KEYRING)
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
	/*@globals internalState@*/
	/*@modifies internalState @*/;

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
	/*@globals rpmGlobalMacroContext, rpmCLIMacroContext,
		fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, rpmCLIMacroContext,
		fileSystem, internalState @*/;

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
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *fp, rpmGlobalMacroContext,
		fileSystem, internalState  @*/;

/** \ingroup rpmrc
 * @deprecated Use addMacro to set _target_* macros.
 * @todo Eliminate from API.
 # @note Only used by build code.
 * @param archTable
 * @param osTable
 */
void rpmSetTables(int archTable, int osTable)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

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
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

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
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/*@}*/
/* ==================================================================== */
/** \name RPMDB */
/*@{*/

/** \ingroup rpmdb
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmdb_s * rpmdb;

/** \ingroup rpmdb
 */
typedef /*@abstract@*/ struct _dbiIndexSet * dbiIndexSet;

/** \ingroup rpmdb
 * Tags for which rpmdb indices will be built.
 */
/*@unchecked@*/
/*@only@*/ /*@null@*/ extern int * dbiTags;
/*@unchecked@*/
extern int dbiTagsMax;

/** \ingroup rpmdb
 * Unreference a database instance.
 * @param db		rpm database
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmdb rpmdbUnlink (/*@killref@*/ /*@only@*/ rpmdb db, const char * msg)
	/*@modifies db @*/;

/** @todo Remove debugging entry from the ABI. */
/*@null@*/
rpmdb XrpmdbUnlink (/*@killref@*/ /*@only@*/ rpmdb db, const char * msg,
		const char * fn, unsigned ln)
	/*@modifies db @*/;
#define	rpmdbUnlink(_db, _msg)	XrpmdbUnlink(_db, _msg, __FILE__, __LINE__)

/** \ingroup rpmdb
 * Reference a database instance.
 * @param db		rpm database
 * @return		new rpm database reference
 */
/*@unused@*/
rpmdb rpmdbLink (rpmdb db, const char * msg)
	/*@modifies db @*/;

/** @todo Remove debugging entry from the ABI. */
rpmdb XrpmdbLink (rpmdb db, const char * msg,
		const char * fn, unsigned ln)
        /*@modifies db @*/;
#define	rpmdbLink(_db, _msg)	XrpmdbLink(_db, _msg, __FILE__, __LINE__)

/** \ingroup rpmdb
 * Open rpm database.
 * @param prefix	path to top of install tree
 * @retval dbp		address of rpm database
 * @param mode		open(2) flags:  O_RDWR or O_RDONLY (O_CREAT also)
 * @param perms		database permissions
 * @return		0 on success
 */
int rpmdbOpen (/*@null@*/ const char * prefix, /*@null@*/ /*@out@*/ rpmdb * dbp,
		int mode, int perms)
	/*@globals fileSystem @*/
	/*@modifies *dbp, fileSystem @*/;

/** \ingroup rpmdb
 * Initialize database.
 * @param prefix	path to top of install tree
 * @param perms		database permissions
 * @return		0 on success
 */
int rpmdbInit(/*@null@*/ const char * prefix, int perms)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/** \ingroup rpmdb
 * Verify database components.
 * @param prefix	path to top of install tree
 * @return		0 on success
 */
int rpmdbVerify(/*@null@*/ const char * prefix)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/** \ingroup rpmdb
 * Close all database indices and free rpmdb.
 * @param db		rpm database
 * @return		0 on success
 */
int rpmdbClose (/*@killref@*/ /*@only@*/ /*@null@*/ rpmdb db)
	/*@globals fileSystem @*/
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmdb
 * Sync all database indices.
 * @param db		rpm database
 * @return		0 on success
 */
int rpmdbSync (/*@null@*/ rpmdb db)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/** \ingroup rpmdb
 * Open all database indices.
 * @param db		rpm database
 * @return		0 on success
 */
int rpmdbOpenAll (/*@null@*/ rpmdb db)
	/*@modifies db @*/;

/** \ingroup rpmdb
 * Return number of instances of package in rpm database.
 * @param db		rpm database
 * @param name		rpm package name
 * @return		number of instances
 */
int rpmdbCountPackages(/*@null@*/ rpmdb db, const char * name)
	/*@globals fileSystem @*/
	/*@modifies db, fileSystem @*/;

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
	/*@globals fileSystem @*/
	/*@modifies mi, fileSystem @*/;

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
	/*@globals fileSystem @*/
	/*@modifies mi, fileSystem @*/;

/** @todo Remove debugging entry from the ABI. */
/*@unused@*/
/*@null@*/ Header XrpmdbNextIterator(rpmdbMatchIterator mi,
		const char * f, unsigned int l)
	/*@globals fileSystem @*/
	/*@modifies mi, fileSystem @*/;

/** \ingroup rpmdb
 * Return database iterator.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		NULL on failure
 */
/*@only@*/ /*@null@*/ rpmdbMatchIterator rpmdbInitIterator(
			/*@null@*/ rpmdb db, int rpmtag,
			/*@null@*/ const void * keyp, size_t keylen)
	/*@globals fileSystem @*/
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmdb
 * Add package header to rpm database and indices.
 * @param db		rpm database
 * @param iid		install transaction id (iid = 0 or -1 to skip)
 * @param h		header
 * @return		0 on success
 */
int rpmdbAdd(/*@null@*/ rpmdb db, int iid, Header h)
	/*@globals fileSystem @*/
	/*@modifies db, h, fileSystem @*/;

/** \ingroup rpmdb
 * Remove package header from rpm database and indices.
 * @param db		rpm database
 * @param rid		remove transaction id (rid = 0 or -1 to skip)
 * @param hdrNum	package instance number in database
 * @return		0 on success
 */
int rpmdbRemove(/*@null@*/ rpmdb db, /*@unused@*/ int rid, unsigned int hdrNum)
	/*@globals fileSystem @*/
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmdb
 * Rebuild database indices from package headers.
 * @param prefix	path to top of install tree
 * @return		0 on success
 */
int rpmdbRebuild(/*@null@*/ const char * prefix)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/*@}*/
/* ==================================================================== */
/** \name RPMPROBS */
/*@{*/

/**
 * Raw data for an element of a problem set.
 */
typedef /*@abstract@*/ struct rpmProblem_s * rpmProblem;

/**
 * Transaction problems found by rpmRunTransactions().
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmProblemSet_s * rpmProblemSet;

/**
 * Enumerate transaction set problem types.
 */
typedef enum rpmProblemType_e {
    RPMPROB_BADARCH,	/*!< package ... is for a different architecture */
    RPMPROB_BADOS,	/*!< package ... is for a different operating system */
    RPMPROB_PKG_INSTALLED, /*!< package ... is already installed */
    RPMPROB_BADRELOCATE,/*!< path ... is not relocateable for package ... */
    RPMPROB_REQUIRES,	/*!< package ... has unsatisfied Requires: ... */
    RPMPROB_CONFLICT,	/*!< package ... has unsatisfied Conflicts: ... */
    RPMPROB_NEW_FILE_CONFLICT, /*!< file ... conflicts between attemped installs of ... */
    RPMPROB_FILE_CONFLICT,/*!< file ... from install of ... conflicts with file from package ... */
    RPMPROB_OLDPACKAGE,	/*!< package ... (which is newer than ...) is already installed */
    RPMPROB_DISKSPACE,	/*!< installing package ... needs ... on the ... filesystem */
    RPMPROB_DISKNODES,	/*!< installing package ... needs ... on the ... filesystem */
    RPMPROB_BADPRETRANS	/*!< (unimplemented) */
 } rpmProblemType;

/**
 */
struct rpmProblem_s {
/*@only@*/ /*@null@*/ char * pkgNEVR;
/*@only@*/ /*@null@*/ char * altNEVR;
/*@dependent@*/ /*@null@*/ fnpyKey key;
    rpmProblemType type;
    int ignoreProblem;
/*@only@*/ /*@null@*/ char * str1;
    unsigned long ulong1;
};

/**
 */
struct rpmProblemSet_s {
    int numProblems;		/*!< Current probs array size. */
    int numProblemsAlloced;	/*!< Allocated probs array size. */
    rpmProblem probs;		/*!< Array of specific problems. */
/*@refs@*/ int nrefs;		/*!< Reference count. */
};

/**
 */
void printDepFlags(FILE *fp, const char *version, int flags)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

/**
 * Print a problem array.
 * @param fp		output file
 * @param probs		dependency problems
 * @param numProblems	no. of dependency problems
 */
void printDepProblems(FILE * fp, rpmProblem probs, int numProblems)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

/**
 * Return formatted string representation of a problem.
 * @param prob		rpm problem
 * @return		formatted string (malloc'd)
 */
/*@-redecl@*/	/* LCL: is confused. */
/*@only@*/ extern const char * rpmProblemString(const rpmProblem prob)
	/*@*/;
/*@=redecl@*/

/**
 * Unreference a problem set instance.
 * @param ps		problem set
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmProblemSet rpmpsUnlink (/*@killref@*/ /*@only@*/ rpmProblemSet ps,
		const char * msg)
	/*@modifies ps @*/;

/** @todo Remove debugging entry from the ABI. */
/*@null@*/
rpmProblemSet XrpmpsUnlink (/*@killref@*/ /*@only@*/ rpmProblemSet ps,
		const char * msg, const char * fn, unsigned ln)
	/*@modifies ps @*/;
#define	rpmpsUnlink(_ps, _msg)	XrpmpsUnlink(_ps, _msg, __FILE__, __LINE__)

/**
 * Reference a problem set instance.
 * @param ps		transaction set
 * @return		new transaction set reference
 */
/*@unused@*/
rpmProblemSet rpmpsLink (rpmProblemSet ps, const char * msg)
	/*@modifies ps @*/;

/** @todo Remove debugging entry from the ABI. */
rpmProblemSet XrpmpsLink (rpmProblemSet ps,
		const char * msg, const char * fn, unsigned ln)
        /*@modifies ps @*/;
#define	rpmpsLink(_ps, _msg)	XrpmpsLink(_ps, _msg, __FILE__, __LINE__)

/**
 * Create a problem set.
 */
rpmProblemSet rpmProblemSetCreate(void)
	/*@*/;

/**
 * Destroy a problem array.
 * @param ps		problem set
 * @return		NULL always
 */
/*@null@*/
rpmProblemSet rpmProblemSetFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmProblemSet ps)
	/*@modifies ps @*/;

/**
 * Output formatted string representation of a problem to file handle.
 * @deprecated API: prob used to be passed by value, now passed by reference.
 * @param fp		file handle
 * @param prob		rpm problem
 */
void rpmProblemPrint(FILE *fp, rpmProblem prob)
	/*@globals fileSystem @*/
	/*@modifies prob, *fp, fileSystem @*/;

/**
 * Print problems to file handle.
 * @param fp		file handle
 * @param probs		problem set
 */
void rpmProblemSetPrint(FILE *fp, rpmProblemSet ps)
	/*@globals fileSystem @*/
	/*@modifies ps, *fp, fileSystem @*/;

/**
 * Append a problem to set.
 */
void rpmProblemSetAppend(/*@null@*/ rpmProblemSet ps, rpmProblemType type,
		/*@null@*/ const char * pkgNEVR,
		/*@exposed@*/ /*@null@*/ fnpyKey key,
		const char * dn, const char * bn,
		/*@null@*/ const char * altNEVR,
		unsigned long ulong1)
	/*@modifies ps @*/;

/**
 * Filter a problem set.
 *
 * As the problem sets are generated in an order solely dependent
 * on the ordering of the packages in the transaction, and that
 * ordering can't be changed, the problem sets must be parallel to
 * one another. Additionally, the filter set must be a subset of the
 * target set, given the operations available on transaction set.
 * This is good, as it lets us perform this trim in linear time, rather
 * then logarithmic or quadratic.
 *
 * @param ps		problem set
 * @param filter	problem filter (or NULL)
 * @return		0 no problems, 1 if problems remain
 */
int rpmProblemSetTrim(/*@null@*/ rpmProblemSet ps,
		/*@null@*/ rpmProblemSet filter)
	/*@modifies ps @*/;

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
/*@only@*/ /*@null@*/
    const char * oldPath;	/*!< NULL here evals to RPMTAG_DEFAULTPREFIX, */
/*@only@*/ /*@null@*/
    const char * newPath;	/*!< NULL means to omit the file completely! */
} rpmRelocation;

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
 * These are the file types used internally by rpm. The file
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
typedef /*@abstract@*/ /*@refcounted@*/ struct transactionFileInfo_s * TFI_t;

/**
 * Return package header from file handle.
 * @param ts		transaction set
 * @param fd		file handle
 * @param fn		file name
 * @retval hdrp		address of header (or NULL)
 * @return		0 on success
 */
int rpmReadPackageFile(rpmTransactionSet ts, FD_t fd,
		const char * fn, /*@null@*/ /*@out@*/ Header * hdrp)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, fd, *hdrp, fileSystem, internalState @*/;

/**
 * Install source package.
 * @param ts		transaction set
 * @param fd		file handle
 * @retval specFilePtr	address of spec file name (or NULL)
 * @param notify	progress callback
 * @param notifyData	progress callback private data
 * @retval cooke	address of cookie pointer (or NULL)
 * @return		rpmRC return code
 */
rpmRC rpmInstallSourcePackage(rpmTransactionSet ts, FD_t fd,
			/*@null@*/ /*@out@*/ const char ** specFilePtr,
			/*@null@*/ rpmCallbackFunction notify,
			/*@null@*/ rpmCallbackData notifyData,
			/*@null@*/ /*@out@*/ const char ** cookie)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies ts, fd, *specFilePtr, *cookie, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/** \ingroup rpmtrans
 * Unreference a transaction element file info instance.
 * @param fi		transaction element file info
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
TFI_t rpmfiUnlink (/*@killref@*/ /*@only@*/ TFI_t fi, const char * msg)
	/*@modifies fi @*/;

/** @todo Remove debugging entry from the ABI. */
/*@null@*/
TFI_t XrpmfiUnlink (/*@killref@*/ /*@only@*/ TFI_t fi, const char * msg,
		const char * fn, unsigned ln)
	/*@modifies fi @*/;
#define	rpmfiUnlink(_fi, _msg)	XrpmfiUnlink(_fi, _msg, __FILE__, __LINE__)

/** \ingroup rpmtrans
 * Reference a transaction element file info instance.
 * @param fi		transaction element file info
 * @return		new transaction element file info reference
 */
/*@unused@*/
TFI_t rpmfiLink (TFI_t fi, const char * msg)
	/*@modifies fi @*/;

/** @todo Remove debugging entry from the ABI. */
TFI_t XrpmfiLink (TFI_t fi, const char * msg,
		const char * fn, unsigned ln)
        /*@modifies fi @*/;
#define	rpmfiLink(_fi, _msg)	XrpmfiLink(_fi, _msg, __FILE__, __LINE__)

/** \ingroup rpmtrans
 * Unreference a transaction instance.
 * @param ts		transaction set
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmTransactionSet rpmtsUnlink (/*@killref@*/ /*@only@*/ rpmTransactionSet ts,
		const char * msg)
	/*@modifies ts @*/;

/** @todo Remove debugging entry from the ABI. */
/*@null@*/
rpmTransactionSet XrpmtsUnlink (/*@killref@*/ /*@only@*/ rpmTransactionSet ts,
		const char * msg, const char * fn, unsigned ln)
	/*@modifies ts @*/;
#define	rpmtsUnlink(_ts, _msg)	XrpmtsUnlink(_ts, _msg, __FILE__, __LINE__)

/** \ingroup rpmtrans
 * Reference a transaction set instance.
 * @param ts		transaction set
 * @return		new transaction set reference
 */
/*@unused@*/
rpmTransactionSet rpmtsLink (rpmTransactionSet ts, const char * msg)
	/*@modifies ts @*/;

/** @todo Remove debugging entry from the ABI. */
rpmTransactionSet XrpmtsLink (rpmTransactionSet ts,
		const char * msg, const char * fn, unsigned ln)
        /*@modifies ts @*/;
#define	rpmtsLink(_ts, _msg)	XrpmtsLink(_ts, _msg, __FILE__, __LINE__)

/** \ingroup rpmtrans
 * Close the database used by the transaction.
 * @param ts		transaction set
 * @return		0 on success
 */
int rpmtsCloseDB(rpmTransactionSet ts)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/;

/** \ingroup rpmtrans
 * Open the database used by the transaction.
 * @param ts		transaction set
 * @param dbmode	O_RDONLY or O_RDWR
 * @return		0 on success
 */
int rpmtsOpenDB(rpmTransactionSet ts, int dbmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, fileSystem, internalState @*/;

/** \ingroup rpmtrans
 * Return transaction database iterator.
 * @param ts		transaction set
 * @param rpmtag	rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		NULL on failure
 */
/*@only@*/ /*@null@*/
rpmdbMatchIterator rpmtsInitIterator(const rpmTransactionSet ts, int rpmtag,
			/*@null@*/ const void * keyp, size_t keylen)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/** \ingroup rpmtrans
 * Create an empty transaction set.
 * @param db		rpm database (may be NULL if database is not accessed)
 * @param rootdir	path to top of install tree
 * @return		transaction set
 */
/*@only@*/ rpmTransactionSet rpmtransCreateSet(
		/*@null@*/ rpmdb db,
		/*@null@*/ const char * rootDir)
	/*@modifies db @*/;

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
		/*@null@*/ /*@owned@*/ const fnpyKey key, int upgrade,
		/*@null@*/ rpmRelocation * relocs)
	/*@globals fileSystem, internalState @*/
	/*@modifies fd, h, ts, fileSystem, internalState @*/;

/** \ingroup rpmtrans
 * Add package to universe of possible packages to install in transaction set.
 * @warning The key parameter is non-functional.
 * @param ts		transaction set
 * @param h		header
 * @param key		package private data
 */
/*@unused@*/
void rpmtransAvailablePackage(rpmTransactionSet ts, Header h,
		/*@null@*/ /*@owned@*/ fnpyKey key)
	/*@modifies h, ts @*/;

/** \ingroup rpmtrans
 * Add package to be removed to unordered transaction set.
 * @param ts		transaction set
 * @param dboffset	rpm database instance
 * @return		0 on success
 */
int rpmtransRemovePackage(rpmTransactionSet ts, int dboffset)
	/*@modifies ts @*/;

/** \ingroup rpmtrans
 * Re-create an empty transaction set.
 * @param ts		transaction set
 */
void rpmtransClean(rpmTransactionSet ts)
	/*@modifies ts @*/;

/** \ingroup rpmtrans
 * Destroy transaction set, closing the database as well.
 * @param ts		transaction set
 * @return		NULL always
 */
/*@null@*/ rpmTransactionSet
rpmtransFree(/*@killref@*/ /*@only@*//*@null@*/ rpmTransactionSet ts)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/;

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
		/*@null@*/ /*@out@*/ fnpyKey ** ep,
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
		/*@exposed@*/ /*@out@*/ rpmProblem * conflicts,
		/*@exposed@*/ /*@out@*/ int * numConflicts)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, *conflicts, *numConflicts,
		fileSystem, internalState @*/;

/** \ingroup rpmtrans
 * Determine package order in a transaction set according to dependencies.
 *
 * Order packages, returning error if circular dependencies cannot be
 * eliminated by removing PreReq's from the loop(s). Only dependencies from
 * added or removed packages are used to determine ordering using a
 * topological sort (Knuth vol. 1, p. 262). Use rpmdepCheck() to verify
 * that all dependencies can be resolved.
 *
 * The final order ends up as installed packages followed by removed packages,
 * with packages removed for upgrades immediately following the new package
 * to be installed.
 *
 * The operation would be easier if we could sort the addedPackages array in the
 * transaction set, but we store indexes into the array in various places.
 *
 * @param ts		transaction set
 * @return		no. of (added) packages that could not be ordered
 */
int rpmdepOrder(rpmTransactionSet ts)
	/*@globals fileSystem, internalState@*/
	/*@modifies ts, fileSystem, internalState @*/;

/** \ingroup rpmtrans
 * Destroy reported problems.
 * @param probs		dependency problems
 * @param numProblems	no. of dependency problems
 * @retrun		NULL always
 */
/*@null@*/
rpmProblem rpmdepFreeConflicts(/*@only@*/ /*@null@*/ rpmProblem probs,
		int numProblems)
	/*@modifies probs @*/;

/** \ingroup rpmtrans
 * Bit(s) to control rpmRunTransactions() operation.
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
	/*@modifies *provNames, *provFlags, *provVersions @*/;

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
 * Check dependency against internal rpmlib feature provides.
 * @param key		dependency
 * @return		1 if dependency overlaps, 0 otherwise
 */
int rpmCheckRpmlibProvides(const rpmDepSet key)
	/*@*/;

/** \ingroup rpmcli
 * Display current rpmlib feature provides.
 * @param fp		output file handle
 */
void rpmShowRpmlibProvides(FILE * fp)
	/*@globals fileSystem @*/
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
	/*@observer@*/	rpmCallbackFunction notify,
	/*@observer@*/	rpmCallbackData notifyData,
			rpmProblemSet okProbs,
			/*@out@*/ rpmProblemSet * newProbs,
			rpmtransFlags transFlags,
			rpmprobFilterFlags ignoreSet)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState@*/
	/*@modifies ts, *newProbs, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

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
	/*@globals internalState@*/
	/*@modifies internalState@*/;

/**
 * Return (cached) file system mount points.
 * @retval listptr		addess of file system names (or NULL)
 * @retval num			address of number of file systems (or NULL)
 * @return			0 on success, 1 on error
 */
int rpmGetFilesystemList( /*@null@*/ /*@out@*/ const char *** listptr,
		/*@null@*/ /*@out@*/ int * num)
	/*@globals fileSystem, internalState @*/
	/*@modifies *listptr, *num, fileSystem, internalState @*/;

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
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *usagesPtr, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

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
    INSTALL_INSTALL	= (1 << 7),	/*!< from --install */
    INSTALL_ERASE	= (1 << 8)	/*!< from --erase */
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
enum rpmtagSignature {
    RPMSIGTAG_SIZE	= 1000,	/*!< Header+Payload size in bytes. */
/* the md5 sum was broken *twice* on big endian machines */
    RPMSIGTAG_LEMD5_1	= 1001,	/*!< Broken MD5, take 1 */
    RPMSIGTAG_PGP	= 1002,	/*!< PGP 2.6.3 signature. */
    RPMSIGTAG_LEMD5_2	= 1003,	/*!< Broken MD5, take 2 */
    RPMSIGTAG_MD5	= 1004,	/*!< MD5 signature. */
    RPMSIGTAG_GPG	= 1005, /*!< GnuPG signature. */
    RPMSIGTAG_PGP5	= 1006,	/*!< PGP5 signature @deprecated legacy. */
};

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
 *
 * This needs the following variables from the transaction set:
 *	- ts->sigtag	type of signature
 *	- ts->sig	signature itself (from signature header)
 *	- ts->siglen	no. of bytes in signature
 *	- ts->dig	signature parameters (malloc'd workspace)
 *
 * @param ts		transaction set
 * @retval result	detailed text result of signature verification
 * @return		result of signature verification
 */
rpmVerifySignatureReturn rpmVerifySignature(const rpmTransactionSet ts,
		/*@out@*/ char * result)
	/*@modifies ts, *result @*/;

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
