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
	/*@out@*/ Header * hdr) /*@modifies *signatures, *hdr @*/;

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
	/*@out@*/ int * minor) /*@modifies *hdr, *isSource, *major, *minor @*/;

/**
 * Return name, version, release strings from header.
 * @param h		header
 * @retval np		address of name pointer (or NULL)
 * @retval vp		address of version pointer (or NULL)
 * @retval rp		address of release pointer (or NULL)
 * @return		0 always
 */
int headerNVR(Header h, /*@out@*/ const char **np, /*@out@*/ const char **vp,
	/*@out@*/ const char **rp) /*@modifies *np, *vp, *rp @*/;

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

/* these pseudo-tags are used by the dbi iterator interface */
#define	RPMDBI_PACKAGES		0
#define	RPMDBI_DEPENDS		1
#define	RPMDBI_LABEL		2	/* XXX remove rpmdbFindByLabel from API */
#define	RPMDBI_ADDED		3
#define	RPMDBI_REMOVED		4
#define	RPMDBI_AVAILABLE	5

/* Retrofit (and uniqify) signature tags for use by tagName() and rpmQuery. */

/* XXX underscore prevents tagTable generation */
#define	RPMTAG_SIG_BASE			256
#define	RPMTAG_SIGSIZE         	        RPMTAG_SIG_BASE+1
/* the md5 sum was broken *twice* on big endian machines */
#define	RPMTAG_SIGLEMD5_1		RPMTAG_SIG_BASE+2
#define	RPMTAG_SIGPGP          	        RPMTAG_SIG_BASE+3
#define	RPMTAG_SIGLEMD5_2		RPMTAG_SIG_BASE+4
#define	RPMTAG_SIGMD5		        RPMTAG_SIG_BASE+5
#define	RPMTAG_SIGGPG		        RPMTAG_SIG_BASE+6
#define	RPMTAG_SIGPGP5		        RPMTAG_SIG_BASE+7	/* internal */

/* these tags are found in package headers */
/* none of these can be 0 !!                         */

#define	RPMTAG_NAME  			1000
#define	RPMTAG_VERSION			1001
#define	RPMTAG_RELEASE			1002
#define	RPMTAG_EPOCH	   		1003
#define	RPMTAG_SERIAL		RPMTAG_EPOCH	/* backward comaptibility */
#define	RPMTAG_SUMMARY			1004
#define	RPMTAG_DESCRIPTION		1005
#define	RPMTAG_BUILDTIME		1006
#define	RPMTAG_BUILDHOST		1007
#define	RPMTAG_INSTALLTIME		1008
#define	RPMTAG_SIZE			1009
#define	RPMTAG_DISTRIBUTION		1010
#define	RPMTAG_VENDOR			1011
#define	RPMTAG_GIF			1012
#define	RPMTAG_XPM			1013
#define	RPMTAG_LICENSE			1014
#define	RPMTAG_COPYRIGHT	RPMTAG_LICENSE	/* backward comaptibility */
#define	RPMTAG_PACKAGER			1015
#define	RPMTAG_GROUP			1016
#define	RPMTAG_CHANGELOG		1017 /* internal */
#define	RPMTAG_SOURCE			1018
#define	RPMTAG_PATCH			1019
#define	RPMTAG_URL			1020
#define	RPMTAG_OS			1021
#define	RPMTAG_ARCH			1022
#define	RPMTAG_PREIN			1023
#define	RPMTAG_POSTIN			1024
#define	RPMTAG_PREUN			1025
#define	RPMTAG_POSTUN			1026
#define	RPMTAG_OLDFILENAMES		1027 /* obsolete */
#define	RPMTAG_FILESIZES		1028
#define	RPMTAG_FILESTATES		1029
#define	RPMTAG_FILEMODES		1030
#define	RPMTAG_FILEUIDS			1031 /* internal */
#define	RPMTAG_FILEGIDS			1032 /* internal */
#define	RPMTAG_FILERDEVS		1033
#define	RPMTAG_FILEMTIMES		1034
#define	RPMTAG_FILEMD5S			1035
#define	RPMTAG_FILELINKTOS		1036
#define	RPMTAG_FILEFLAGS		1037
#define	RPMTAG_ROOT			1038 /* obsolete */
#define	RPMTAG_FILEUSERNAME		1039
#define	RPMTAG_FILEGROUPNAME		1040
#define	RPMTAG_EXCLUDE			1041 /* internal - depricated */
#define	RPMTAG_EXCLUSIVE		1042 /* internal - depricated */
#define	RPMTAG_ICON			1043
#define	RPMTAG_SOURCERPM		1044
#define	RPMTAG_FILEVERIFYFLAGS		1045
#define	RPMTAG_ARCHIVESIZE		1046
#define	RPMTAG_PROVIDENAME		1047
#define	RPMTAG_PROVIDES RPMTAG_PROVIDENAME	/* backward comaptibility */
#define	RPMTAG_REQUIREFLAGS		1048
#define	RPMTAG_REQUIRENAME		1049
#define	RPMTAG_REQUIREVERSION		1050
#define	RPMTAG_NOSOURCE			1051 /* internal */
#define	RPMTAG_NOPATCH			1052 /* internal */
#define	RPMTAG_CONFLICTFLAGS		1053
#define	RPMTAG_CONFLICTNAME		1054
#define	RPMTAG_CONFLICTVERSION		1055
#define	RPMTAG_DEFAULTPREFIX		1056 /* internal - deprecated */
#define	RPMTAG_BUILDROOT		1057
#define	RPMTAG_INSTALLPREFIX		1058 /* internal - deprecated */
#define	RPMTAG_EXCLUDEARCH		1059
#define	RPMTAG_EXCLUDEOS		1060
#define	RPMTAG_EXCLUSIVEARCH		1061
#define	RPMTAG_EXCLUSIVEOS		1062
#define	RPMTAG_AUTOREQPROV		1063 /* internal */
#define	RPMTAG_RPMVERSION		1064
#define	RPMTAG_TRIGGERSCRIPTS		1065
#define	RPMTAG_TRIGGERNAME		1066
#define	RPMTAG_TRIGGERVERSION		1067
#define	RPMTAG_TRIGGERFLAGS		1068
#define	RPMTAG_TRIGGERINDEX		1069
#define	RPMTAG_VERIFYSCRIPT		1079
#define	RPMTAG_CHANGELOGTIME		1080
#define	RPMTAG_CHANGELOGNAME		1081
#define	RPMTAG_CHANGELOGTEXT		1082
#define	RPMTAG_BROKENMD5		1083 /* internal */
#define	RPMTAG_PREREQ			1084 /* internal */
#define	RPMTAG_PREINPROG		1085
#define	RPMTAG_POSTINPROG		1086
#define	RPMTAG_PREUNPROG		1087
#define	RPMTAG_POSTUNPROG		1088
#define	RPMTAG_BUILDARCHS		1089
#define	RPMTAG_OBSOLETENAME		1090
#define	RPMTAG_OBSOLETES RPMTAG_OBSOLETENAME	/* backward comaptibility */
#define	RPMTAG_VERIFYSCRIPTPROG		1091
#define	RPMTAG_TRIGGERSCRIPTPROG	1092
#define	RPMTAG_DOCDIR			1093 /* internal */
#define	RPMTAG_COOKIE			1094
#define	RPMTAG_FILEDEVICES		1095
#define	RPMTAG_FILEINODES		1096
#define	RPMTAG_FILELANGS		1097
#define	RPMTAG_PREFIXES			1098
#define	RPMTAG_INSTPREFIXES		1099
#define	RPMTAG_TRIGGERIN		1100 /* internal */
#define	RPMTAG_TRIGGERUN		1101 /* internal */
#define	RPMTAG_TRIGGERPOSTUN		1102 /* internal */
#define	RPMTAG_AUTOREQ			1103 /* internal */
#define	RPMTAG_AUTOPROV			1104 /* internal */
#define	RPMTAG_CAPABILITY		1105 /* unused internal */
#define	RPMTAG_SOURCEPACKAGE		1106 /* internal */
#define	RPMTAG_OLDORIGFILENAMES		1107 /* obsolete */
#define	RPMTAG_BUILDPREREQ		1108 /* internal */
#define	RPMTAG_BUILDREQUIRES		1109 /* internal */
#define	RPMTAG_BUILDCONFLICTS		1110 /* internal */
#define	RPMTAG_BUILDMACROS		1111
#define	RPMTAG_PROVIDEFLAGS		1112
#define	RPMTAG_PROVIDEVERSION		1113
#define	RPMTAG_OBSOLETEFLAGS		1114
#define	RPMTAG_OBSOLETEVERSION		1115
#define	RPMTAG_DIRINDEXES		1116
#define	RPMTAG_BASENAMES		1117
#define	RPMTAG_DIRNAMES			1118
#define	RPMTAG_ORIGDIRINDEXES		1119 /* internal */
#define	RPMTAG_ORIGBASENAMES		1120 /* internal */
#define	RPMTAG_ORIGDIRNAMES		1121 /* internal */
#define	RPMTAG_OPTFLAGS			1122
#define	RPMTAG_DISTURL			1123
#define	RPMTAG_PAYLOADFORMAT		1124
#define	RPMTAG_PAYLOADCOMPRESSOR	1125
#define	RPMTAG_PAYLOADFLAGS		1126
#define	RPMTAG_MULTILIBS		1127

#define	RPMTAG_FIRSTFREE_TAG		1128 /* internal */
#define	RPMTAG_EXTERNAL_TAG		1000000

#define	RPMFILE_STATE_NORMAL 		0
#define	RPMFILE_STATE_REPLACED 		1
#define	RPMFILE_STATE_NOTINSTALLED	2
#define	RPMFILE_STATE_NETSHARED		3

/* these can be ORed together */
#define	RPMFILE_CONFIG			(1 << 0)
#define	RPMFILE_DOC			(1 << 1)
#define	RPMFILE_DONOTUSE		(1 << 2)
#define	RPMFILE_MISSINGOK		(1 << 3)
#define	RPMFILE_NOREPLACE		(1 << 4)
#define	RPMFILE_SPECFILE		(1 << 5)
#define	RPMFILE_GHOST			(1 << 6)
#define	RPMFILE_LICENSE			(1 << 7)
#define	RPMFILE_README			(1 << 8)
#define	RPMFILE_MULTILIB_SHIFT		9
#define	RPMFILE_MULTILIB(N)		((N) << RPMFILE_MULTILIB_SHIFT)
#define	RPMFILE_MULTILIB_MASK		RPMFILE_MULTILIB(7)

/* XXX Check file flags for multilib marker. */
#define	isFileMULTILIB(_fflags)		((_fflags) & RPMFILE_MULTILIB_MASK)

#define	RPMVERIFY_NONE		0
#define	RPMVERIFY_MD5		(1 << 0)
#define	RPMVERIFY_FILESIZE	(1 << 1)
#define	RPMVERIFY_LINKTO	(1 << 2)
#define	RPMVERIFY_USER		(1 << 3)
#define	RPMVERIFY_GROUP		(1 << 4)
#define	RPMVERIFY_MTIME		(1 << 5)
#define	RPMVERIFY_MODE		(1 << 6)
#define	RPMVERIFY_RDEV		(1 << 7)
#define	RPMVERIFY_READLINKFAIL	(1 << 28)
#define	RPMVERIFY_READFAIL	(1 << 29)
#define	RPMVERIFY_LSTATFAIL	(1 << 30)

#define	RPMVERIFY_ALL		~(RPMVERIFY_NONE)

#define	RPMSENSE_ANY		0
#define	RPMSENSE_SERIAL		(1 << 0) /* eliminated, backward compatibilty */
#define	RPMSENSE_LESS		(1 << 1)
#define	RPMSENSE_GREATER	(1 << 2)
#define	RPMSENSE_EQUAL		(1 << 3)
#define	RPMSENSE_PROVIDES	(1 << 4) /* only used internally by builds */
#define	RPMSENSE_CONFLICTS	(1 << 5) /* only used internally by builds */
#define	RPMSENSE_PREREQ		(1 << 6)
#define	RPMSENSE_OBSOLETES	(1 << 7) /* only used internally by builds */
#define	RPMSENSE_SENSEMASK	15	 /* Mask to get senses, ie serial, */
                                         /* less, greater, equal.          */

#define	RPMSENSE_TRIGGERIN	(1 << 16)
#define	RPMSENSE_TRIGGERUN	(1 << 17)
#define	RPMSENSE_TRIGGERPOSTUN	(1 << 18)
#define	RPMSENSE_TRIGGER	(RPMSENSE_TRIGGERIN | RPMSENSE_TRIGGERUN | \
                                  RPMSENSE_TRIGGERPOSTUN)

#define	RPMSENSE_MULTILIB	(1 << 19)

#define	isDependsMULTILIB(_dflags)	((_dflags) & RPMSENSE_MULTILIB)

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
 * Return value of rpmrc variable.
 * @deprecated Use rpmExpand() with appropriate macro expression.
 * @todo Eliminate.
 */
const char * rpmGetVar(int var);

/** \ingroup rpmrc
 * Set value of rpmrc variable.
 * @deprecated Use rpmDefineMacro() to change appropriate macro instead.
 * @todo Eliminate.
 */
void rpmSetVar(int var, const char *val);

/** \ingroup rpmrc
 * Build and install arch/os table identifiers.
 * @todo Eliminate.
 */
enum rpm_machtable_e {
	RPM_MACHTABLE_INSTARCH		= 0,
	RPM_MACHTABLE_INSTOS		= 1,
	RPM_MACHTABLE_BUILDARCH		= 2,
	RPM_MACHTABLE_BUILDOS  		= 3
};
#define	RPM_MACHTABLE_COUNT		4	/* number of arch/os tables */

/** \ingroup rpmrc
 * Read rpmrc (and macro) configuration file(s) for a target.
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
 * An arch score measures the nearness of an arch name to the currently
 * running (or defined) platform arch. For example, the score of "i586"
 * on an i686 platform is (usually) 1. The arch score is used to select
 * one of several otherwise identical packages based on the arch/os hints
 * in the header of the intended platform.
 * @todo Rewrite to use RE's against config.guess target platform output.
 *
 * @param type		any of the RPM_MACHTABLE_* constants
 * @param name		name
 * @return		arch score
 */
int rpmMachineScore(int type, const char * name);

/** \ingroup rpmrc
 * Display current rpmrc (and macro) configuration.
 * @param f		output file handle
 * @return		0 always
 */
int rpmShowRC(FILE *f);

/** \ingroup rpmrc
 * @todo Eliminate, use _target_* macros.
 * @param archTable
 * @param osTable
 */
void rpmSetTables(int archTable, int osTable);  /* only used by build code */

/** \ingroup rpmrc
 * Set current arch/os names.
 * NULL as argument is set to the default value (munged uname())
 * pushed through a translation table (if appropriate).
 * @todo Eliminate, use _target_* macros.
 *
 * @param arch		arch name (or NULL)
 * @param os		os name (or NULL)
 */
void rpmSetMachine(const char * arch, const char * os);

/** \ingroup rpmrc
 * Return current arch/os names.
 * @todo Eliminate, use _target_* macros.
 * @retval arch		address of arch name (or NULL)
 * @retval os		address of os name (or NULL)
 */
void rpmGetMachine( /*@out@*/ const char **arch, /*@out@*/ const char **os);

/** \ingroup rpmrc
 * Destroy rpmrc arch/os compatibility tables.
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
 * @todo replace with a more general mechanism using RE's on tag content.
 * @param mi		rpm database iterator
 * @param version	version to check for
 */
void rpmdbSetIteratorVersion(rpmdbMatchIterator mi, /*@kept@*/ const char * version);

/** \ingroup rpmdb
 * Modify iterator to filter out headers that do not match release.
 * @todo replace with a more general mechanism using RE's on tag content.
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
			/*@kept@*/ rpmdb rpmdb, int rpmtag,
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

/** */
typedef enum rpmProblemType_e {
	RPMPROB_BADARCH, 
	RPMPROB_BADOS,
	RPMPROB_PKG_INSTALLED,
	RPMPROB_BADRELOCATE,
	RPMPROB_REQUIRES,
	RPMPROB_CONFLICT,
	RPMPROB_NEW_FILE_CONFLICT,
	RPMPROB_FILE_CONFLICT,
	RPMPROB_OLDPACKAGE,
	RPMPROB_DISKSPACE,
	RPMPROB_BADPRETRANS
 } rpmProblemType;

/** */
typedef /*@abstract@*/ struct rpmProblem_s {
    Header h, altH;
/*@dependent@*/ const void * key;
    rpmProblemType type;
    int ignoreProblem;
/*@only@*/ const char * str1;
    unsigned long ulong1;
} rpmProblem;

/** */
typedef /*@abstract@*/ struct rpmProblemSet_s {
    int numProblems;
    int numProblemsAlloced;
    rpmProblem * probs;
} * rpmProblemSet;

/**
 */
void printDepFlags(FILE *fp, const char *version, int flags)
	/*@modifies *fp @*/;

/**
 */
struct rpmDependencyConflict {
    char * byName, * byVersion, * byRelease;
    Header byHeader;
    /* these needs fields are misnamed -- they are used for the package
       which isn't needed as well */
    char * needsName, * needsVersion;
    int needsFlags;
    /*@observer@*/ /*@null@*/ const void * suggestedPackage; /* NULL if none */
    enum { RPMDEP_SENSE_REQUIRES, RPMDEP_SENSE_CONFLICTS } sense;
} ;

/**
 */
void printDepProblems(FILE *fp, struct rpmDependencyConflict *conflicts,
	int numConflicts)	/*@modifies *fp @*/;

/**
 */
/*@only@*/ const char * rpmProblemString(rpmProblem prob)	/*@*/;

/**
 */
void rpmProblemPrint(FILE *fp, rpmProblem prob)	/*@modifies *fp @*/;

/**
 */
void rpmProblemSetPrint(FILE *fp, rpmProblemSet probs)	/*@modifies *fp @*/;

/**
 */
void rpmProblemSetFree( /*@only@*/ rpmProblemSet probs);

/*@}*/
/* ==================================================================== */
/** \name RPMTS */
/*@{*/
/* we pass these around as an array with a sentinel */
typedef struct rpmRelocation_s {
				/* XXX for backwards compatibility */
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
int rpmInstallSourcePackage(const char * root, FD_t fd, const char ** specFile,
			    rpmCallbackFunction notify, void * notifyData,
			    char ** cookie);

/**
 * Compare headers to determine which header is "newer".
 * @param first		1st header
 * @param second	2nd header
 * @return		result of comparison
 */
int rpmVersionCompare(Header first, Header second);

/* Transaction sets are inherently unordered! RPM may reorder transaction
   sets to reduce errors. In general, installs/upgrades are done before
   strict removals, and prerequisite ordering is done on installs/upgrades. */
typedef /*@abstract@*/ struct rpmTransactionSet_s * rpmTransactionSet;

/** \ingroup rpmtrans
 * Create an empty transaction set.
 * @param rpmdb		rpm database (may be NULL if database is not accessed)
 * @param rootdir	path to top of install tree
 * @return		rpm transaction set
 */
/*@only@*/ rpmTransactionSet rpmtransCreateSet( /*@only@*/ rpmdb rpmdb,
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
 * @param ts		rpm transaction set
 * @param fd		file handle
 */
void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd);

/* this checks for dependency satisfaction, but *not* ordering */
/** \ingroup rpmtrans
 * @param rpmdep	rpm transaction set
 */
int rpmdepCheck(rpmTransactionSet rpmdep,
	/*@exposed@*/ /*@out@*/ struct rpmDependencyConflict ** conflicts,
	/*@exposed@*/ /*@out@*/ int * numConflicts);

/* Orders items, returns error on circle, finals keys[] is NULL. No dependency
   check is done, use rpmdepCheck() for that. If dependencies are not
   satisfied a "best-try" ordering is returned. */

/** \ingroup rpmtrans
 * @param order		rpm transaction set
 */
int rpmdepOrder(rpmTransactionSet order);

/** \ingroup rpmtrans
 * Destroy dependency conflicts.
 * @param conflicts	dependency conflicts
 * @param numConflicts	no. of dependency conflicts
 */
void rpmdepFreeConflicts( /*@only@*/ struct rpmDependencyConflict * conflicts,
	int numConflicts);

#define	RPMTRANS_FLAG_TEST		(1 << 0)
#define	RPMTRANS_FLAG_BUILD_PROBS	(1 << 1)
#define	RPMTRANS_FLAG_NOSCRIPTS		(1 << 2)
#define	RPMTRANS_FLAG_JUSTDB		(1 << 3)
#define	RPMTRANS_FLAG_NOTRIGGERS	(1 << 4)
#define	RPMTRANS_FLAG_NODOCS		(1 << 5)
#define	RPMTRANS_FLAG_ALLFILES		(1 << 6)
#define	RPMTRANS_FLAG_KEEPOBSOLETE	(1 << 7)
#define	RPMTRANS_FLAG_MULTILIB		(1 << 8)

/** \ingroup rpmdep
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

/** \ingroup rpmdep
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

/** \ingroup rpmtrans
 * @param ts		rpm transaction set
 */
int rpmRunTransactions(rpmTransactionSet ts, rpmCallbackFunction notify,
		       void * notifyData, rpmProblemSet okProbs,
		       /*@out@*/ rpmProblemSet * newProbs, int flags,
			int ignoreSet);

#define	RPMPROB_FILTER_IGNOREOS		(1 << 0)
#define	RPMPROB_FILTER_IGNOREARCH	(1 << 1)
#define	RPMPROB_FILTER_REPLACEPKG	(1 << 2)
#define	RPMPROB_FILTER_FORCERELOCATE	(1 << 3)
#define	RPMPROB_FILTER_REPLACENEWFILES	(1 << 4)
#define	RPMPROB_FILTER_REPLACEOLDFILES	(1 << 5)
#define	RPMPROB_FILTER_OLDPACKAGE	(1 << 6)
#define	RPMPROB_FILTER_DISKSPACE	(1 << 7)

/*@}*/

/** rpmlead.c **/

#define	RPMLEAD_BINARY 0
#define	RPMLEAD_SOURCE 1

#define	RPMLEAD_MAGIC0 0xed
#define	RPMLEAD_MAGIC1 0xab
#define	RPMLEAD_MAGIC2 0xee
#define	RPMLEAD_MAGIC3 0xdb

/* The lead needs to be 8 byte aligned */

#define	RPMLEAD_SIZE 96

/** \ingroup lead
 */
struct rpmlead {
    unsigned char magic[4];
    unsigned char major, minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;
    char reserved[16];      /* pads to 96 bytes -- 8 byte aligned! */
} ;

/** \ingroup lead
 */
struct oldrpmlead {		/* for version 1 packages */
    unsigned char magic[4];
    unsigned char major, minor;
    short type;
    short archnum;
    char name[66];
    unsigned int specOffset;
    unsigned int specLength;
    unsigned int archiveOffset;
} ;

/**
 */
void freeFilesystems(void);

/**
 */
int rpmGetFilesystemList( /*@out@*/ const char *** listptr, /*@out@*/int * num);

/**
 */
int rpmGetFilesystemUsage(const char ** filelist, int_32 * fssizes,
	int numFiles, /*@out@*/ uint_32 ** usagesPtr, int flags);

/* ==================================================================== */
/** \name RPMBT */
/*@{*/
/* --- build mode options */

/** \ingroup rpmcli
 */
struct rpmBuildArguments {
    int buildAmount;
    const char *buildRootOverride;
    char *targets;
    int useCatalog;
    int noLang;
    int noBuild;
    int shortCircuit;
    char buildMode;
    char buildChar;
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
 * @param root		path to top of install tree
 * @param h		header
 */
int rpmVerifyFile(const char * root, Header h, int filenum,
	/*@out@*/ int * result, int omitMask);

/** \ingroup rpmcli
 * @param root		path to top of install tree
 * @param h		header
 * @param err		file handle
 */
int rpmVerifyScript(const char * root, Header h, FD_t err);

/* --- query/verify mode options */

/* XXX SPECFILE is not verify sources */
/** \ingroup rpmcli
 */
enum rpmQVSources { RPMQV_PACKAGE = 0, RPMQV_PATH, RPMQV_ALL, RPMQV_RPM, 
		       RPMQV_GROUP, RPMQV_WHATPROVIDES, RPMQV_WHATREQUIRES,
		       RPMQV_TRIGGEREDBY, RPMQV_DBOFFSET, RPMQV_SPECFILE };

/** \ingroup rpmcli
 */
struct rpmQVArguments {
    enum rpmQVSources qva_source;
    int 	qva_sourceCount;	/* > 1 is an error */
    int		qva_flags;
    int		qva_verbose;
    const char *qva_queryFormat;
    const char *qva_prefix;
    char	qva_mode;
    char	qva_char;
};
/** \ingroup rpmcli
 */
typedef	struct rpmQVArguments QVA_t;

/** \ingroup rpmcli
 */
extern struct rpmQVArguments		rpmQVArgs;

/** \ingroup rpmcli
 */
extern struct poptOption		rpmQVSourcePoptTable[];

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param db		rpm database
 * @param h		header to use for query/verify
 */
typedef	int (*QVF_t) (QVA_t *qva, rpmdb db, Header h);

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param mi		rpm database iterator
 * @param showPackage	query/verify routine
 */
int showMatches(QVA_t *qva, /*@only@*/ /*@null@*/ rpmdbMatchIterator mi,
	QVF_t showPackage);

#define	QUERY_FOR_LIST		(1 << 1)
#define	QUERY_FOR_STATE		(1 << 2)
#define	QUERY_FOR_DOCS		(1 << 3)
#define	QUERY_FOR_CONFIG	(1 << 4)
#define	QUERY_FOR_DUMPFILES     (1 << 8)

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

/** \ingroup rpmcli
 */
extern int specedit;

/** \ingroup rpmcli
 */
extern struct poptOption rpmQueryPoptTable[];

/** \ingroup rpmcli
 * @param f	file handle to use for display
 */
void rpmDisplayQueryTags(FILE * f);

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param source	type of source to query/verify
 * @param arg		name of source to query/verify
 * @param db		rpm database
 * @param showPackage	query/verify routine
 */
int rpmQueryVerify(QVA_t *qva, enum rpmQVSources source, const char * arg,
	rpmdb db, QVF_t showPackage);

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param db		rpm database (unused for queries)
 * @param h		header to use for query
 */
int showQueryPackage(QVA_t *qva, rpmdb db, Header h);

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param source	type of source to query
 * @param arg		name of source to query
 */
int rpmQuery(QVA_t *qva, enum rpmQVSources source, const char * arg);

#define	VERIFY_FILES		(1 <<  9)
#define	VERIFY_DEPS		(1 << 10)
#define	VERIFY_SCRIPT		(1 << 11)
#define	VERIFY_MD5		(1 << 12)

/** \ingroup rpmcli
 */
extern struct poptOption rpmVerifyPoptTable[];

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param db		rpm database
 * @param h		header to use for verify
 */
int showVerifyPackage(QVA_t *qva, /*@only@*/ rpmdb db, Header h);

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param source	type of source to verify
 * @param arg		name of source to verify
 */
int rpmVerify(QVA_t *qva, enum rpmQVSources source, const char *arg);

/*@}*/
/* ==================================================================== */
/** \name RPMEIU */
/*@{*/
/* --- install/upgrade/erase modes */

#define	INSTALL_PERCENT		(1 << 0)
#define	INSTALL_HASH		(1 << 1)
#define	INSTALL_NODEPS		(1 << 2)
#define	INSTALL_NOORDER		(1 << 3)
#define	INSTALL_LABEL		(1 << 4)  /* set if we're being verbose */
#define	INSTALL_UPGRADE		(1 << 5)
#define	INSTALL_FRESHEN		(1 << 6)

#define	UNINSTALL_NODEPS	(1 << 0)
#define	UNINSTALL_ALLMATCHES	(1 << 1)

/** \ingroup rpmcli
 * @param rootdir	path to top of install tree
 * @param argv		array of package file names (NULL terminated)
 */
int rpmInstall(const char * rootdir, const char ** argv, int installFlags, 
	      int interfaceFlags, int probFilter, rpmRelocation * relocations);

/** \ingroup rpmcli
 */
int rpmInstallSource(const char * prefix, const char * arg, const char ** specFile,
		    char ** cookie);

/** \ingroup rpmcli
 * @param rootdir	path to top of install tree
 * @param argv		array of package file names (NULL terminated)
 */
int rpmErase(const char * rootdir, const char ** argv, int uninstallFlags, 
		 int interfaceFlags);

/*@}*/
/* ==================================================================== */
/** \name RPMK */
/*@{*/

/**************************************************/
/*                                                */
/* Signature Tags                                 */
/*                                                */
/* These go in the sig Header to specify          */
/* individual signature types.                    */
/*                                                */
/**************************************************/

/** \ingroup signature
 * Tags found in signature header from package.
 */
enum rpmtagSignature {
	RPMSIGTAG_SIZE		= 1000,
/* the md5 sum was broken *twice* on big endian machines */
	RPMSIGTAG_LEMD5_1	= 1001,
	RPMSIGTAG_PGP		= 1002,
	RPMSIGTAG_LEMD5_2	= 1003,
	RPMSIGTAG_MD5		= 1004,
	RPMSIGTAG_GPG		= 1005,
	RPMSIGTAG_PGP5		= 1006,	/* XXX legacy use only */

/* Signature tags by Public Key Algorithm (RFC 2440) */
/* N.B.: These tags are tenative, the values may change */
	RPMTAG_PK_BASE		= 512,
	RPMTAG_PK_RSA_ES	= RPMTAG_PK_BASE+1,
	RPMTAG_PK_RSA_E		= RPMTAG_PK_BASE+2,
	RPMTAG_PK_RSA_S		= RPMTAG_PK_BASE+3,
	RPMTAG_PK_ELGAMAL_E	= RPMTAG_PK_BASE+16,
	RPMTAG_PK_DSA		= RPMTAG_PK_BASE+17,
	RPMTAG_PK_ELLIPTIC	= RPMTAG_PK_BASE+18,
	RPMTAG_PK_ECDSA		= RPMTAG_PK_BASE+19,
	RPMTAG_PK_ELGAMAL_ES	= RPMTAG_PK_BASE+20,
	RPMTAG_PK_DH		= RPMTAG_PK_BASE+21,

	RPMTAG_HASH_BASE	= 512+64,
	RPMTAG_HASH_MD5		= RPMTAG_HASH_BASE+1,
	RPMTAG_HASH_SHA1	= RPMTAG_HASH_BASE+2,
	RPMTAG_HASH_RIPEMD160	= RPMTAG_HASH_BASE+3,
	RPMTAG_HASH_MD2		= RPMTAG_HASH_BASE+5,
	RPMTAG_HASH_TIGER192	= RPMTAG_HASH_BASE+6,
	RPMTAG_HASH_HAVAL_5_160	= RPMTAG_HASH_BASE+7
};

/**
 *  Return codes from verifySignature().
 */
enum rpmVerifySignatureReturn {
	RPMSIG_OK	= 0,	/*!< Signature is OK. */
	RPMSIG_UNKNOWN	= 1,	/*!< Signature is unknown. */
	RPMSIG_BAD	= 2,	/*!< Signature does not verify. */
	RPMSIG_NOKEY	= 3,	/*!< Key is unavailable. */
	RPMSIG_NOTTRUSTED = 4	/*!< Signature is OK, but key is not trusted. */
};

/** \ingroup signature
 * Verify a signature from a package.
 * @param file		file name of header+payload
 * @param sigTag	type of signature
 * @param sig		signature itself
 * @param count		no. of bytes in signature
 * @param result	detailed text result of signature verification
 * @return		result of signature verification
 */
enum rpmVerifySignatureReturn rpmVerifySignature(const char *file,
		int_32 sigTag, void *sig, int count, char *result);

/** \ingroup signature
 * Destroy signature header from package.
 */
void rpmFreeSignature(Header h);

/* --- checksig/resign */

#define	CHECKSIG_PGP (1 << 0)
#define	CHECKSIG_MD5 (1 << 1)
#define	CHECKSIG_GPG (1 << 2)

/** \ingroup rpmcli
 * @param flags
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success
 */
int rpmCheckSig(int flags, const char ** argv);

/** \ingroup rpmcli
 * Type of signature operation to perform.
 */
enum rpmKtype {
	RPMK_NEW_SIGNATURE = 0,		/*!< Discard previous signature. */
	RPMK_ADD_SIGNATURE		/*!< Add element to signature. */
};

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param add		type of signature operation
 * @param passPhrase
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success
 */
int rpmReSign(enum rpmKtype add, char *passPhrase, const char ** argv);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMLIB */
