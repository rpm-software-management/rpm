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
    RPMRC_OK		= 0,	/*!< Generic success code */
    RPMRC_NOTFOUND	= 1,	/*!< Generic not found code. */
    RPMRC_FAIL		= 2,	/*!< Generic failure code. */
    RPMRC_NOTTRUSTED	= 3,	/*!< Signature is OK, but key is not trusted. */
    RPMRC_NOKEY		= 4	/*!< Public key is unavailable. */
} rpmRC;

/*@-redecl@*/
/*@checked@*/
extern struct MacroContext_s * rpmGlobalMacroContext;

/*@checked@*/
extern struct MacroContext_s * rpmCLIMacroContext;

/*@unchecked@*/ /*@observer@*/
extern const char * RPMVERSION;

/*@unchecked@*/ /*@observer@*/
extern const char * rpmNAME;

/*@unchecked@*/ /*@observer@*/
extern const char * rpmEVR;

/*@unchecked@*/
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
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmts_s * rpmts;

/** \ingroup rpmbuild
 */
typedef struct Spec_s * Spec;

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

/** \ingroup rpmtrans
 * Dependency tag sets from a header, so that a header can be discarded early.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmds_s * rpmds;

/** \ingroup rpmtrans
 * File info tag sets from a header, so that a header can be discarded early.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmfi_s * rpmfi;

/** \ingroup rpmtrans
 * An element of a transaction set, i.e. a TR_ADDED or TR_REMOVED package.
 */
typedef /*@abstract@*/ struct rpmte_s * rpmte;

/** \ingroup rpmdb
 * Database of headers and tag value indices.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmdb_s * rpmdb;

/** \ingroup rpmdb
 * Database iterator.
 */
typedef /*@abstract@*/ struct _rpmdbMatchIterator * rpmdbMatchIterator;

/** \ingroup header
 * Return name, version, release strings from header.
 * @param h		header
 * @retval *np		name pointer (or NULL)
 * @retval *vp		version pointer (or NULL)
 * @retval *rp		release pointer (or NULL)
 * @return		0 always
 */
int headerNVR(Header h,
		/*@null@*/ /*@out@*/ const char ** np,
		/*@null@*/ /*@out@*/ const char ** vp,
		/*@null@*/ /*@out@*/ const char ** rp)
	/*@modifies *np, *vp, *rp @*/;

/** \ingroup header
 * Return name, epoch, version, release, arch strings from header.
 * @param h		header
 * @retval *np		name pointer (or NULL)
 * @retval *ep		epoch pointer (or NULL)
 * @retval *vp		version pointer (or NULL)
 * @retval *rp		release pointer (or NULL)
 * @retval *ap		arch pointer (or NULL)
 * @return		0 always
 */
int headerNEVRA(Header h,
		/*@null@*/ /*@out@*/ const char ** np,
		/*@null@*/ /*@out@*/ /*@unused@*/ const char ** ep,
		/*@null@*/ /*@out@*/ const char ** vp,
		/*@null@*/ /*@out@*/ const char ** rp,
		/*@null@*/ /*@out@*/ const char ** ap)
	/*@modifies *np, *vp, *rp, *ap @*/;

/** \ingroup header
 * Translate and merge legacy signature tags into header.
 * @todo Remove headerSort() through headerInitIterator() modifies sig.
 * @param h		header
 * @param sigh		signature header
 */
void headerMergeLegacySigs(Header h, const Header sigh)
	/*@modifies h, sigh @*/;

/** \ingroup header
 * Regenerate signature header.
 * @todo Remove headerSort() through headerInitIterator() modifies h.
 * @param h		header
 * @param noArchiveSize	don't copy archive size tag (pre rpm-4.1)
 * @return		regenerated signature header
 */
Header headerRegenSigHeader(const Header h, int noArchiveSize)
	/*@modifies h @*/;

/** \ingroup header
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
    RPMTAG_SIGLEMD5_1		= RPMTAG_SIG_BASE+2,	/*!< internal - obsolete */
    RPMTAG_SIGPGP		= RPMTAG_SIG_BASE+3,
    RPMTAG_SIGLEMD5_2		= RPMTAG_SIG_BASE+4,	/*!< internal - obsolete */
    RPMTAG_SIGMD5	        = RPMTAG_SIG_BASE+5,
#define	RPMTAG_PKGID	RPMTAG_SIGMD5
    RPMTAG_SIGGPG	        = RPMTAG_SIG_BASE+6,
    RPMTAG_SIGPGP5	        = RPMTAG_SIG_BASE+7,	/*!< internal - obsolete */

    RPMTAG_BADSHA1_1		= RPMTAG_SIG_BASE+8,	/*!< internal - obsolete */
    RPMTAG_BADSHA1_2		= RPMTAG_SIG_BASE+9,	/*!< internal - obsolete */
    RPMTAG_PUBKEYS		= RPMTAG_SIG_BASE+10,
    RPMTAG_DSAHEADER		= RPMTAG_SIG_BASE+11,
    RPMTAG_RSAHEADER		= RPMTAG_SIG_BASE+12,
    RPMTAG_SHA1HEADER		= RPMTAG_SIG_BASE+13,
#define	RPMTAG_HDRID	RPMTAG_SHA1HEADER

    RPMTAG_NAME  		= 1000,
#define	RPMTAG_N	RPMTAG_NAME
    RPMTAG_VERSION		= 1001,
#define	RPMTAG_V	RPMTAG_VERSION
    RPMTAG_RELEASE		= 1002,
#define	RPMTAG_R	RPMTAG_RELEASE
    RPMTAG_EPOCH   		= 1003,
#define	RPMTAG_E	RPMTAG_EPOCH
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
    RPMTAG_SOURCEPACKAGE	= 1106, /*!< src.rpm header marker */
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
    RPMTAG_INSTALLCOLOR		= 1127, /*!< transaction color when installed */
    RPMTAG_INSTALLTID		= 1128,
    RPMTAG_REMOVETID		= 1129,
/*@-enummemuse@*/
    RPMTAG_SHA1RHN		= 1130, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_RHNPLATFORM		= 1131,
    RPMTAG_PLATFORM		= 1132,
    RPMTAG_PATCHESNAME		= 1133, /*!< placeholder (SuSE) */
    RPMTAG_PATCHESFLAGS		= 1134, /*!< placeholder (SuSE) */
    RPMTAG_PATCHESVERSION	= 1135, /*!< placeholder (SuSE) */
    RPMTAG_CACHECTIME		= 1136,
    RPMTAG_CACHEPKGPATH		= 1137,
    RPMTAG_CACHEPKGSIZE		= 1138,
    RPMTAG_CACHEPKGMTIME	= 1139,
    RPMTAG_FILECOLORS		= 1140,
    RPMTAG_FILECLASS		= 1141,
    RPMTAG_CLASSDICT		= 1142,
    RPMTAG_FILEDEPENDSX		= 1143,
    RPMTAG_FILEDEPENDSN		= 1144,
    RPMTAG_DEPENDSDICT		= 1145,
    RPMTAG_SOURCEPKGID		= 1146,
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
    RPMFILE_STATE_NETSHARED	= 3,
    RPMFILE_STATE_WRONGCOLOR	= 4
} rpmfileState;
#define	RPMFILE_STATE_MISSING	-1	/* XXX used for unavailable data */

/**
 * File Attributes.
 */
typedef	enum rpmfileAttrs_e {
/*@-enummemuse@*/
    RPMFILE_NONE	= 0,
/*@=enummemuse@*/
    RPMFILE_CONFIG	= (1 <<  0),	/*!< from %%config */
    RPMFILE_DOC		= (1 <<  1),	/*!< from %%doc */
    RPMFILE_ICON	= (1 <<  2),	/*!< from %%donotuse. */
    RPMFILE_MISSINGOK	= (1 <<  3),	/*!< from %%config(missingok) */
    RPMFILE_NOREPLACE	= (1 <<  4),	/*!< from %%config(noreplace) */
    RPMFILE_SPECFILE	= (1 <<  5),	/*!< @todo (unnecessary) marks 1st file in srpm. */
    RPMFILE_GHOST	= (1 <<  6),	/*!< from %%ghost */
    RPMFILE_LICENSE	= (1 <<  7),	/*!< from %%license */
    RPMFILE_README	= (1 <<  8),	/*!< from %%readme */
    RPMFILE_EXCLUDE	= (1 <<  9),	/*!< from %%exclude */
    RPMFILE_UNPATCHED	= (1 << 10),	/*!< placeholder (SuSE) */
    RPMFILE_PUBKEY	= (1 << 11)	/*!< from %%pubkey */
} rpmfileAttrs;

#define	RPMFILE_ALL	~(RPMFILE_NONE)

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
	/* (1 << 19) unused. */
    RPMSENSE_SCRIPT_PREP = (1 << 20),	/*!< %prep build dependency. */
    RPMSENSE_SCRIPT_BUILD = (1 << 21),	/*!< %build build dependency. */
    RPMSENSE_SCRIPT_INSTALL = (1 << 22),/*!< %install build dependency. */
    RPMSENSE_SCRIPT_CLEAN = (1 << 23),	/*!< %clean build dependency. */
    RPMSENSE_RPMLIB	= ((1 << 24) | RPMSENSE_PREREQ), /*!< rpmlib(feature) dependency. */
/*@-enummemuse@*/
    RPMSENSE_TRIGGERPREIN = (1 << 25),	/*!< @todo Implement %triggerprein. */
/*@=enummemuse@*/
    RPMSENSE_KEYRING	= (1 << 26),
    RPMSENSE_PATCHES	= (1 << 27),
    RPMSENSE_CONFIG	= (1 << 28)
} rpmsenseFlags;

#define	RPMSENSE_SENSEMASK	15	 /* Mask to get senses, ie serial, */
                                         /* less, greater, equal.          */

#define	RPMSENSE_TRIGGER	\
	(RPMSENSE_TRIGGERIN | RPMSENSE_TRIGGERUN | RPMSENSE_TRIGGERPOSTUN)

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
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies *fp, rpmGlobalMacroContext, fileSystem, internalState  @*/;

/** \ingroup rpmrc
 * @deprecated Use addMacro to set _target_* macros.
 * @todo Eliminate from API.
 # @note Only used by build code.
 * @param archTable
 * @param osTable
 */
void rpmSetTables(int archTable, int osTable)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;

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
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;

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
/** \name RPMTS */
/*@{*/
/**
 * Prototype for headerFreeData() vector.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef /*@null@*/
    void * (*HFD_t) (/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/;

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
			/*@null@*/ /*@out@*/ rpmTagType * type,
			/*@null@*/ /*@out@*/ void ** p,
			/*@null@*/ /*@out@*/ int_32 * c)
	/*@modifies *type, *p, *c @*/;

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
			const void * p, int_32 c)
	/*@modifies h @*/;

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
    FA_SKIPCOLOR	/*!< ... untouched, state "wrong color". */
} fileAction;

#define XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED || (_a) == FA_SKIPCOLOR)

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
    SOCK	= 12	/*!< socket */
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
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmpsm_s * rpmpsm;

/**
 * Perform simple sanity and range checks on header tag(s).
 * @param il		no. of tags in header
 * @param dl		no. of bytes in header data.
 * @param pev		1st element in tag array, big-endian
 * @param iv		failing (or last) tag element, host-endian
 * @param negate	negative offset expected?
 * @return		-1 on success, otherwise failing tag element index
 */
int headerVerifyInfo(int il, int dl, const void * pev, void * iv, int negate)
	/*@modifies *iv @*/;

/** 
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
		/*@out@*/ /*@null@*/ const char ** msg)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, *msg, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/** 
 * Return checked and loaded header.
 * @param ts		transaction set
 * @param fd		file handle
 * @retval hdrp		address of header (or NULL)
 * @retval *msg		verification error message (or NULL)
 * @return		RPMRC_OK on success
 */
rpmRC rpmReadHeader(rpmts ts, FD_t fd, /*@out@*/ Header *hdrp,
		/*@out@*/ /*@null@*/ const char ** msg)
        /*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
        /*@modifies ts, *hdrp, *msg, rpmGlobalMacroContext,
                fileSystem, internalState @*/;

/**
 * Return package header from file handle, verifying digests/signatures.
 * @param ts		transaction set
 * @param fd		file handle
 * @param fn		file name
 * @retval hdrp		address of header (or NULL)
 * @return		0 on success
 */
int rpmReadPackageFile(rpmts ts, FD_t fd,
		const char * fn, /*@null@*/ /*@out@*/ Header * hdrp)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, fd, *hdrp, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/**
 * Install source package.
 * @param ts		transaction set
 * @param fd		file handle
 * @retval specFilePtr	address of spec file name (or NULL)
 * @retval cookie	address of cookie pointer (or NULL)
 * @return		rpmRC return code
 */
rpmRC rpmInstallSourcePackage(rpmts ts, FD_t fd,
			/*@null@*/ /*@out@*/ const char ** specFilePtr,
			/*@null@*/ /*@out@*/ const char ** cookie)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, fd, *specFilePtr, *cookie, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/** \ingroup rpmtrans
 * Bit(s) to control rpmtsRun() operation.
 */
typedef enum rpmtransFlags_e {
    RPMTRANS_FLAG_NONE		= 0,
    RPMTRANS_FLAG_TEST		= (1 <<  0),	/*!< from --test */
    RPMTRANS_FLAG_BUILD_PROBS	= (1 <<  1),	/*!< don't process payload */
    RPMTRANS_FLAG_NOSCRIPTS	= (1 <<  2),	/*!< from --noscripts */
    RPMTRANS_FLAG_JUSTDB	= (1 <<  3),	/*!< from --justdb */
    RPMTRANS_FLAG_NOTRIGGERS	= (1 <<  4),	/*!< from --notriggers */
    RPMTRANS_FLAG_NODOCS	= (1 <<  5),	/*!< from --excludedocs */
    RPMTRANS_FLAG_ALLFILES	= (1 <<  6),	/*!< from --allfiles */
/*@-enummemuse@*/
    RPMTRANS_FLAG_KEEPOBSOLETE	= (1 <<  7),	/*!< @todo Document. */
/*@=enummemuse@*/
	/* (1 << 8) unused. */
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

    RPMTRANS_FLAG_NOTRIGGERPREIN= (1 << 16),	/*!< from --notriggerprein */
    RPMTRANS_FLAG_NOPRE		= (1 << 17),	/*!< from --nopre */
    RPMTRANS_FLAG_NOPOST	= (1 << 18),	/*!< from --nopost */
    RPMTRANS_FLAG_NOTRIGGERIN	= (1 << 19),	/*!< from --notriggerin */
    RPMTRANS_FLAG_NOTRIGGERUN	= (1 << 20),	/*!< from --notriggerun */
    RPMTRANS_FLAG_NOPREUN	= (1 << 21),	/*!< from --nopreun */
    RPMTRANS_FLAG_NOPOSTUN	= (1 << 22),	/*!< from --nopostun */
    RPMTRANS_FLAG_NOTRIGGERPOSTUN = (1 << 23),	/*!< from --notriggerpostun */
/*@-enummemuse@*/
    RPMTRANS_FLAG_NOPAYLOAD	= (1 << 24),
/*@=enummemuse@*/
    RPMTRANS_FLAG_APPLYONLY	= (1 << 25),

    RPMTRANS_FLAG_ANACONDA	= (1 << 26),
    RPMTRANS_FLAG_NOMD5		= (1 << 27),	/*!< from --nomd5 */
    RPMTRANS_FLAG_NOSUGGEST	= (1 << 28),	/*!< from --nosuggest */
    RPMTRANS_FLAG_ADDINDEPS	= (1 << 29),	/*!< from --aid */
    RPMTRANS_FLAG_NOCONFIGS	= (1 << 30)	/*!< from --noconfigs */
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
 * @retval provNames	address of array of rpmlib internal provide names
 * @retval provFlags	address of array of rpmlib internal provide flags
 * @retval provVersions	address of array of rpmlib internal provide versions
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
int rpmCheckRpmlibProvides(const rpmds key)
	/*@*/;

/** \ingroup rpmcli
 * Display current rpmlib feature provides.
 * @param fp		output file handle
 */
void rpmShowRpmlibProvides(FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

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
 * @param tagstr	name of tag
 * @return		tag value
 */
int tagValue(const char * tagstr)
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
    unsigned char major;
    unsigned char minor;
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
/*@-incondefs@*/
int rpmGetFilesystemList( /*@null@*/ /*@out@*/ const char *** listptr,
		/*@null@*/ /*@out@*/ int * num)
	/*@globals fileSystem, internalState @*/
	/*@modifies *listptr, *num, fileSystem, internalState @*/
	/*@requires maxSet(listptr) >= 0 /\ maxSet(num) >= 0 @*/
	/*@ensures maxRead(num) == 0 @*/;
/*@=incondefs@*/

/**
 * Determine per-file system usage for a list of files.
 * @param fileList		array of absolute file names
 * @param fssizes		array of file sizes
 * @param numFiles		number of files in list
 * @retval usagesPtr		address of per-file system usage array (or NULL)
 * @param flags			(unused)
 * @return			0 on success, 1 on error
 */
/*@-incondefs@*/
int rpmGetFilesystemUsage(const char ** fileList, int_32 * fssizes,
		int numFiles, /*@null@*/ /*@out@*/ uint_32 ** usagesPtr,
		int flags)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *usagesPtr, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@requires maxSet(fileList) >= 0 /\ maxSet(fssizes) == 0
		/\ maxSet(usagesPtr) >= 0 @*/
	/*@ensures maxRead(usagesPtr) == 0 @*/;
/*@=incondefs@*/

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
		/*@out@*/ char * result)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, *result, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

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
