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

/** \ingroup rpmtag
 * Automatically generated table of tag name/value pairs.
 */
extern const struct headerTagTableEntry_s * rpmTagTable;

/** \ingroup rpmtag
 * Number of entries in rpmTagTable.
 */
extern const int rpmTagTableSize;

/** \ingroup rpmtag
 */
extern headerTagIndices rpmTags;

/** \ingroup header
 * Table of query format extensions.
 * @note Chains to headerDefaultFormats[].
 */
extern const struct headerSprintfExtension_s rpmHeaderFormats[];

/**
 * Pseudo-tags used by the rpmdb and rpmgi iterator API's.
 */
#define	RPMDBI_PACKAGES		0	/* Installed package headers. */
#define	RPMDBI_DEPENDS		1	/* Dependency resolution cache. */
#define	RPMDBI_LABEL		2	/* Fingerprint search marker. */
#define	RPMDBI_ADDED		3	/* Added package headers. */
#define	RPMDBI_REMOVED		4	/* Removed package headers. */
#define	RPMDBI_AVAILABLE	5	/* Available package headers. */
#define	RPMDBI_HDLIST		6	/* (rpmgi) Header list. */
#define	RPMDBI_ARGLIST		7	/* (rpmgi) Argument list. */
#define	RPMDBI_FTSWALK		8	/* (rpmgi) File tree  walk. */

/** \ingroup rpmtag
 * Tags identify data in package headers.
 * @note tags should not have value 0!
 */
/** @todo: Somehow supply type **/
typedef enum rpmTag_e {

    RPMTAG_HEADERIMAGE		= HEADER_IMAGE,		/*!< Current image. */
    RPMTAG_HEADERSIGNATURES	= HEADER_SIGNATURES,	/*!< Signatures. */
    RPMTAG_HEADERIMMUTABLE	= HEADER_IMMUTABLE,	/*!< Original image. */
    RPMTAG_HEADERREGIONS	= HEADER_REGIONS,	/*!< Regions. */

    RPMTAG_HEADERI18NTABLE	= HEADER_I18NTABLE, /*!< I18N string locales. */

/* Retrofit (and uniqify) signature tags for use by rpmTagGetName() and rpmQuery. */
/* the md5 sum was broken *twice* on big endian machines */
/* XXX 2nd underscore prevents tagTable generation */
    RPMTAG_SIG_BASE		= HEADER_SIGBASE,
    RPMTAG_SIGSIZE		= RPMTAG_SIG_BASE+1,	/* i */
    RPMTAG_SIGLEMD5_1		= RPMTAG_SIG_BASE+2,	/* internal - obsolete */
    RPMTAG_SIGPGP		= RPMTAG_SIG_BASE+3,	/* x */
    RPMTAG_SIGLEMD5_2		= RPMTAG_SIG_BASE+4,	/* x internal - obsolete */
    RPMTAG_SIGMD5	        = RPMTAG_SIG_BASE+5,	/* x */
#define	RPMTAG_PKGID	RPMTAG_SIGMD5			/* x */
    RPMTAG_SIGGPG	        = RPMTAG_SIG_BASE+6,	/* x */
    RPMTAG_SIGPGP5	        = RPMTAG_SIG_BASE+7,	/* internal - obsolete */

    RPMTAG_BADSHA1_1		= RPMTAG_SIG_BASE+8,	/* internal - obsolete */
    RPMTAG_BADSHA1_2		= RPMTAG_SIG_BASE+9,	/* internal - obsolete */
    RPMTAG_PUBKEYS		= RPMTAG_SIG_BASE+10,	/* s[] */
    RPMTAG_DSAHEADER		= RPMTAG_SIG_BASE+11,	/* x */
    RPMTAG_RSAHEADER		= RPMTAG_SIG_BASE+12,	/* x */
    RPMTAG_SHA1HEADER		= RPMTAG_SIG_BASE+13,	/* s */
#define	RPMTAG_HDRID	RPMTAG_SHA1HEADER	/* s */

    RPMTAG_NAME  		= 1000,	/* s */
#define	RPMTAG_N	RPMTAG_NAME	/* s */
    RPMTAG_VERSION		= 1001,	/* s */
#define	RPMTAG_V	RPMTAG_VERSION	/* s */
    RPMTAG_RELEASE		= 1002,	/* s */
#define	RPMTAG_R	RPMTAG_RELEASE	/* s */
    RPMTAG_EPOCH   		= 1003,	/* i */
#define	RPMTAG_E	RPMTAG_EPOCH	/* i */
    RPMTAG_SUMMARY		= 1004,	/* s{} */
    RPMTAG_DESCRIPTION		= 1005,	/* s{} */
    RPMTAG_BUILDTIME		= 1006,	/* i */
    RPMTAG_BUILDHOST		= 1007,	/* s */
    RPMTAG_INSTALLTIME		= 1008,	/* i */
    RPMTAG_SIZE			= 1009,	/* i */
    RPMTAG_DISTRIBUTION		= 1010,	/* s */
    RPMTAG_VENDOR		= 1011,	/* s */
    RPMTAG_GIF			= 1012,	/* x */
    RPMTAG_XPM			= 1013,	/* x */
    RPMTAG_LICENSE		= 1014,	/* s */
    RPMTAG_PACKAGER		= 1015,	/* s */
    RPMTAG_GROUP		= 1016,	/* s{} */
    RPMTAG_CHANGELOG		= 1017, /* s[] internal */
    RPMTAG_SOURCE		= 1018,	/* s[] */
    RPMTAG_PATCH		= 1019,	/* s[] */
    RPMTAG_URL			= 1020,	/* s */
    RPMTAG_OS			= 1021,	/* s legacy used int */
    RPMTAG_ARCH			= 1022,	/* s legacy used int */
    RPMTAG_PREIN		= 1023,	/* s */
    RPMTAG_POSTIN		= 1024,	/* s */
    RPMTAG_PREUN		= 1025,	/* s */
    RPMTAG_POSTUN		= 1026,	/* s */
    RPMTAG_OLDFILENAMES		= 1027, /* s[] obsolete */
    RPMTAG_FILESIZES		= 1028,	/* i[] */
    RPMTAG_FILESTATES		= 1029, /* c[] */
    RPMTAG_FILEMODES		= 1030,	/* h[] */
    RPMTAG_FILEUIDS		= 1031, /* i[] internal */
    RPMTAG_FILEGIDS		= 1032, /* i[] internal */
    RPMTAG_FILERDEVS		= 1033,	/* h[] */
    RPMTAG_FILEMTIMES		= 1034, /* i[] */
    RPMTAG_FILEDIGESTS		= 1035,	/* s[] */
#define RPMTAG_FILEMD5S	RPMTAG_FILEDIGESTS /* s[] */
    RPMTAG_FILELINKTOS		= 1036,	/* s[] */
    RPMTAG_FILEFLAGS		= 1037,	/* i[] */
    RPMTAG_ROOT			= 1038, /* internal - obsolete */
    RPMTAG_FILEUSERNAME		= 1039,	/* s[] */
    RPMTAG_FILEGROUPNAME	= 1040,	/* s[] */
    RPMTAG_EXCLUDE		= 1041, /* internal - obsolete */
    RPMTAG_EXCLUSIVE		= 1042, /* internal - obsolete */
    RPMTAG_ICON			= 1043, /* x */
    RPMTAG_SOURCERPM		= 1044,	/* s */
    RPMTAG_FILEVERIFYFLAGS	= 1045,	/* i[] */
    RPMTAG_ARCHIVESIZE		= 1046,	/* i */
    RPMTAG_PROVIDENAME		= 1047,	/* s[] */
#define	RPMTAG_PROVIDES RPMTAG_PROVIDENAME	/* s[] */
#define	RPMTAG_P	RPMTAG_PROVIDENAME	/* s[] */
    RPMTAG_REQUIREFLAGS		= 1048,	/* i[] */
    RPMTAG_REQUIRENAME		= 1049,	/* s[] */
#define	RPMTAG_REQUIRES RPMTAG_REQUIRENAME	/* s[] */
    RPMTAG_REQUIREVERSION	= 1050,	/* s[] */
    RPMTAG_NOSOURCE		= 1051, /* i internal */
    RPMTAG_NOPATCH		= 1052, /* i internal */
    RPMTAG_CONFLICTFLAGS	= 1053, /* i[] */
    RPMTAG_CONFLICTNAME		= 1054,	/* s[] */
#define	RPMTAG_CONFLICTS RPMTAG_CONFLICTNAME	/* s[] */
#define	RPMTAG_C	RPMTAG_CONFLICTNAME	/* s[] */
    RPMTAG_CONFLICTVERSION	= 1055,	/* s[] */
    RPMTAG_DEFAULTPREFIX	= 1056, /* s internal - deprecated */
    RPMTAG_BUILDROOT		= 1057, /* s internal */
    RPMTAG_INSTALLPREFIX	= 1058, /* s internal - deprecated */
    RPMTAG_EXCLUDEARCH		= 1059, /* s[] */
    RPMTAG_EXCLUDEOS		= 1060, /* s[] */
    RPMTAG_EXCLUSIVEARCH	= 1061, /* s[] */
    RPMTAG_EXCLUSIVEOS		= 1062, /* s[] */
    RPMTAG_AUTOREQPROV		= 1063, /* s internal */
    RPMTAG_RPMVERSION		= 1064,	/* s */
    RPMTAG_TRIGGERSCRIPTS	= 1065,	/* s[] */
    RPMTAG_TRIGGERNAME		= 1066,	/* s[] */
    RPMTAG_TRIGGERVERSION	= 1067,	/* s[] */
    RPMTAG_TRIGGERFLAGS		= 1068,	/* i[] */
    RPMTAG_TRIGGERINDEX		= 1069,	/* i[] */
    RPMTAG_VERIFYSCRIPT		= 1079,	/* s */
    RPMTAG_CHANGELOGTIME	= 1080,	/* i[] */
    RPMTAG_CHANGELOGNAME	= 1081,	/* s[] */
    RPMTAG_CHANGELOGTEXT	= 1082,	/* s[] */
    RPMTAG_BROKENMD5		= 1083, /* internal - obsolete */
    RPMTAG_PREREQ		= 1084, /* internal */
    RPMTAG_PREINPROG		= 1085,	/* s */
    RPMTAG_POSTINPROG		= 1086,	/* s */
    RPMTAG_PREUNPROG		= 1087,	/* s */
    RPMTAG_POSTUNPROG		= 1088,	/* s */
    RPMTAG_BUILDARCHS		= 1089, /* s[] */
    RPMTAG_OBSOLETENAME		= 1090,	/* s[] */
#define	RPMTAG_OBSOLETES RPMTAG_OBSOLETENAME	/* s[] */
#define	RPMTAG_O	RPMTAG_OBSOLETENAME	/* s[] */
    RPMTAG_VERIFYSCRIPTPROG	= 1091,	/* s */
    RPMTAG_TRIGGERSCRIPTPROG	= 1092,	/* s[] */
    RPMTAG_DOCDIR		= 1093, /* internal */
    RPMTAG_COOKIE		= 1094,	/* s */
    RPMTAG_FILEDEVICES		= 1095,	/* i[] */
    RPMTAG_FILEINODES		= 1096,	/* i[] */
    RPMTAG_FILELANGS		= 1097,	/* s[] */
    RPMTAG_PREFIXES		= 1098,	/* s[] */
    RPMTAG_INSTPREFIXES		= 1099,	/* s[] */
    RPMTAG_TRIGGERIN		= 1100, /* internal */
    RPMTAG_TRIGGERUN		= 1101, /* internal */
    RPMTAG_TRIGGERPOSTUN	= 1102, /* internal */
    RPMTAG_AUTOREQ		= 1103, /* internal */
    RPMTAG_AUTOPROV		= 1104, /* internal */
    RPMTAG_CAPABILITY		= 1105, /* i legacy - obsolete */
    RPMTAG_SOURCEPACKAGE	= 1106, /* i legacy - obsolete */
    RPMTAG_OLDORIGFILENAMES	= 1107, /* internal - obsolete */
    RPMTAG_BUILDPREREQ		= 1108, /* internal */
    RPMTAG_BUILDREQUIRES	= 1109, /* internal */
    RPMTAG_BUILDCONFLICTS	= 1110, /* internal */
    RPMTAG_BUILDMACROS		= 1111, /* internal - unused */
    RPMTAG_PROVIDEFLAGS		= 1112,	/* i[] */
    RPMTAG_PROVIDEVERSION	= 1113,	/* s[] */
    RPMTAG_OBSOLETEFLAGS	= 1114,	/* i[] */
    RPMTAG_OBSOLETEVERSION	= 1115,	/* s[] */
    RPMTAG_DIRINDEXES		= 1116,	/* i[] */
    RPMTAG_BASENAMES		= 1117,	/* s[] */
    RPMTAG_DIRNAMES		= 1118,	/* s[] */
    RPMTAG_ORIGDIRINDEXES	= 1119, /* i[] relocation */
    RPMTAG_ORIGBASENAMES	= 1120, /* s[] relocation */
    RPMTAG_ORIGDIRNAMES		= 1121, /* s[] relocation */
    RPMTAG_OPTFLAGS		= 1122,	/* s */
    RPMTAG_DISTURL		= 1123,	/* s */
    RPMTAG_PAYLOADFORMAT	= 1124,	/* s */
    RPMTAG_PAYLOADCOMPRESSOR	= 1125,	/* s */
    RPMTAG_PAYLOADFLAGS		= 1126,	/* s */
    RPMTAG_INSTALLCOLOR		= 1127, /* i transaction color when installed */
    RPMTAG_INSTALLTID		= 1128,	/* i */
    RPMTAG_REMOVETID		= 1129,	/* i */
    RPMTAG_SHA1RHN		= 1130, /* internal - obsolete */
    RPMTAG_RHNPLATFORM		= 1131,	/* s deprecated */
    RPMTAG_PLATFORM		= 1132,	/* s */
    RPMTAG_PATCHESNAME		= 1133, /* s[] deprecated placeholder (SuSE) */
    RPMTAG_PATCHESFLAGS		= 1134, /* i[] deprecated placeholder (SuSE) */
    RPMTAG_PATCHESVERSION	= 1135, /* s[] deprecated placeholder (SuSE) */
    RPMTAG_CACHECTIME		= 1136,	/* i */
    RPMTAG_CACHEPKGPATH		= 1137,	/* s */
    RPMTAG_CACHEPKGSIZE		= 1138,	/* i */
    RPMTAG_CACHEPKGMTIME	= 1139,	/* i */
    RPMTAG_FILECOLORS		= 1140,	/* i[] */
    RPMTAG_FILECLASS		= 1141,	/* i[] */
    RPMTAG_CLASSDICT		= 1142,	/* s[] */
    RPMTAG_FILEDEPENDSX		= 1143,	/* i[] */
    RPMTAG_FILEDEPENDSN		= 1144,	/* i[] */
    RPMTAG_DEPENDSDICT		= 1145,	/* i[] */
    RPMTAG_SOURCEPKGID		= 1146,	/* x */
    RPMTAG_FILECONTEXTS		= 1147,	/* s[] - obsolete */
    RPMTAG_FSCONTEXTS		= 1148,	/* s[] extension */
    RPMTAG_RECONTEXTS		= 1149,	/* s[] extension */
    RPMTAG_POLICIES		= 1150,	/* s[] selinux *.te policy file. */
    RPMTAG_PRETRANS		= 1151,	/* s */
    RPMTAG_POSTTRANS		= 1152,	/* s */
    RPMTAG_PRETRANSPROG		= 1153,	/* s */
    RPMTAG_POSTTRANSPROG	= 1154,	/* s */
    RPMTAG_DISTTAG		= 1155,	/* s */
    RPMTAG_SUGGESTSNAME		= 1156,	/* s[] extension */
#define	RPMTAG_SUGGESTS RPMTAG_SUGGESTSNAME	/* s[] */
    RPMTAG_SUGGESTSVERSION	= 1157,	/* s[] extension */
    RPMTAG_SUGGESTSFLAGS	= 1158,	/* i[] extension */
    RPMTAG_ENHANCESNAME		= 1159,	/* s[] extension placeholder */
#define	RPMTAG_ENHANCES RPMTAG_ENHANCESNAME	/* s[] */
    RPMTAG_ENHANCESVERSION	= 1160,	/* s[] extension placeholder */
    RPMTAG_ENHANCESFLAGS	= 1161,	/* i[] extension placeholder */
    RPMTAG_PRIORITY		= 1162, /* i[] extension placeholder */
    RPMTAG_CVSID		= 1163, /* s */
#define	RPMTAG_SVNID	RPMTAG_CVSID	/* s */
    RPMTAG_BLINKPKGID		= 1164, /* s[] */
    RPMTAG_BLINKHDRID		= 1165, /* s[] */
    RPMTAG_BLINKNEVRA		= 1166, /* s[] */
    RPMTAG_FLINKPKGID		= 1167, /* s[] */
    RPMTAG_FLINKHDRID		= 1168, /* s[] */
    RPMTAG_FLINKNEVRA		= 1169, /* s[] */
    RPMTAG_PACKAGEORIGIN	= 1170, /* s */
    RPMTAG_TRIGGERPREIN		= 1171, /* internal */
    RPMTAG_BUILDSUGGESTS	= 1172, /* internal */
    RPMTAG_BUILDENHANCES	= 1173, /* internal */
    RPMTAG_SCRIPTSTATES		= 1174, /* i[] scriptlet exit codes */
    RPMTAG_SCRIPTMETRICS	= 1175, /* i[] scriptlet execution times */
    RPMTAG_BUILDCPUCLOCK	= 1176, /* i */
    RPMTAG_FILEDIGESTALGOS	= 1177, /* i[] */
    RPMTAG_VARIANTS		= 1178, /* s[] */
    RPMTAG_XMAJOR		= 1179, /* i */
    RPMTAG_XMINOR		= 1180, /* i */
    RPMTAG_REPOTAG		= 1181,	/* s */
    RPMTAG_KEYWORDS		= 1182,	/* s[] */
    RPMTAG_BUILDPLATFORMS	= 1183,	/* s[] */
    RPMTAG_PACKAGECOLOR		= 1184, /* i */
    RPMTAG_PACKAGEPREFCOLOR	= 1185, /* i (unimplemented) */
    RPMTAG_XATTRSDICT		= 1186, /* s[] (unimplemented) */
    RPMTAG_FILEXATTRSX		= 1187, /* i[] (unimplemented) */
    RPMTAG_DEPATTRSDICT		= 1188, /* s[] (unimplemented) */
    RPMTAG_CONFLICTATTRSX	= 1189, /* i[] (unimplemented) */
    RPMTAG_OBSOLETEATTRSX	= 1190, /* i[] (unimplemented) */
    RPMTAG_PROVIDEATTRSX	= 1191, /* i[] (unimplemented) */
    RPMTAG_REQUIREATTRSX	= 1192, /* i[] (unimplemented) */
    RPMTAG_BUILDPROVIDES	= 1193, /* internal */
    RPMTAG_BUILDOBSOLETES	= 1194, /* internal */

    RPMTAG_FIRSTFREE_TAG	/*!< internal */
} rpmTag;

#define	RPMTAG_EXTERNAL_TAG		1000000

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

/** \ingroup rpmds
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
	/* bit 6 used to be RPMSENSE_PREREQ */
#define	RPMSENSE_PREREQ	RPMSENSE_ANY
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
    RPMSENSE_MISSINGOK	= (1 << 19),	/*!< suggests/enhances hint. */
    RPMSENSE_SCRIPT_PREP = (1 << 20),	/*!< %prep build dependency. */
    RPMSENSE_SCRIPT_BUILD = (1 << 21),	/*!< %build build dependency. */
    RPMSENSE_SCRIPT_INSTALL = (1 << 22),/*!< %install build dependency. */
    RPMSENSE_SCRIPT_CLEAN = (1 << 23),	/*!< %clean build dependency. */
    RPMSENSE_RPMLIB = ((1 << 24) | RPMSENSE_PREREQ), /*!< rpmlib(feature) dependency. */
    RPMSENSE_TRIGGERPREIN = (1 << 25),	/*!< @todo Implement %triggerprein. */
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

/**
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
    RPMTRANS_FLAG_KEEPOBSOLETE	= (1 <<  7),	/*!< @todo Document. */
    RPMTRANS_FLAG_NOCONTEXTS	= (1 <<  8),	/*!< from --nocontexts */
    RPMTRANS_FLAG_DIRSTASH	= (1 <<  9),	/*!< from --dirstash */
    RPMTRANS_FLAG_REPACKAGE	= (1 << 10),	/*!< from --repackage */

    RPMTRANS_FLAG_PKGCOMMIT	= (1 << 11),
    RPMTRANS_FLAG_PKGUNDO	= (1 << 12),
    RPMTRANS_FLAG_COMMIT	= (1 << 13),
    RPMTRANS_FLAG_UNDO		= (1 << 14),
    RPMTRANS_FLAG_REVERSE	= (1 << 15),

    RPMTRANS_FLAG_NOTRIGGERPREIN= (1 << 16),	/*!< from --notriggerprein */
    RPMTRANS_FLAG_NOPRE		= (1 << 17),	/*!< from --nopre */
    RPMTRANS_FLAG_NOPOST	= (1 << 18),	/*!< from --nopost */
    RPMTRANS_FLAG_NOTRIGGERIN	= (1 << 19),	/*!< from --notriggerin */
    RPMTRANS_FLAG_NOTRIGGERUN	= (1 << 20),	/*!< from --notriggerun */
    RPMTRANS_FLAG_NOPREUN	= (1 << 21),	/*!< from --nopreun */
    RPMTRANS_FLAG_NOPOSTUN	= (1 << 22),	/*!< from --nopostun */
    RPMTRANS_FLAG_NOTRIGGERPOSTUN = (1 << 23),	/*!< from --notriggerpostun */
    RPMTRANS_FLAG_NOPAYLOAD	= (1 << 24),
    RPMTRANS_FLAG_APPLYONLY	= (1 << 25),

    RPMTRANS_FLAG_NOMD5		= (1 << 27),	/*!< from --nomd5 */
    RPMTRANS_FLAG_NOSUGGEST	= (1 << 28),	/*!< from --nosuggest */
    RPMTRANS_FLAG_ADDINDEPS	= (1 << 29),	/*!< from --aid */
    RPMTRANS_FLAG_NOCONFIGS	= (1 << 30),	/*!< from --noconfigs */
    RPMTRANS_FLAG_DEPLOOPS	= (1 << 31)	/*!< from --deploops */
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


/** \ingroup rpmtag
 * Return tag name from value.
 * @param tag		tag value
 * @return		tag name, "(unknown)" on not found
 */
const char * rpmTagGetName(int tag);

/** \ingroup rpmtag
 * Return tag data type from value.
 * @param tag		tag value
 * @return		tag data type, RPM_NULL_TYPE on not found.
 */
int rpmTagGetType(int tag);

/** \ingroup rpmtag
 * Return tag value from name.
 * @param tagstr	name of tag
 * @return		tag value, -1 on not found
 */
int rpmTagGetValue(const char * tagstr);

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
