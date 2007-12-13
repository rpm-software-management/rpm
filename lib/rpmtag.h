#ifndef _RPMTAG_H
#define _RPMTAG_H

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
#define RPMTAG_NOT_FOUND		-1


/** \ingroup rpmtag
 * Return tag name from value.
 * @param tag		tag value
 * @return		tag name, "(unknown)" on not found
 */
const char * rpmTagGetName(rpm_tag_t tag);

/** \ingroup rpmtag
 * Return tag data type from value.
 * @param tag		tag value
 * @return		tag data type, RPM_NULL_TYPE on not found.
 */
int rpmTagGetType(rpm_tag_t tag);

/** \ingroup rpmtag
 * Return tag value from name.
 * @param tagstr	name of tag
 * @return		tag value, -1 on not found
 */
rpm_tag_t rpmTagGetValue(const char * tagstr);

#endif /* _RPMTAG_H */
