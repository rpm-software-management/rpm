#ifndef H_RPMLIB
#define	H_RPMLIB

/* This is the *only* module users of rpmlib should need to include */

/* and it shouldn't need these :-( */

#include "rpmio.h"
#include "rpmmessages.h"
#include "rpmerr.h"
#include "header.h"
#include "popt.h"

typedef /*@abstract@*/ struct _dbiIndexSet * dbiIndexSet;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
int rpmReadPackageInfo(FD_t fd, /*@out@*/ Header * signatures,
	/*@out@*/ Header * hdr);

/**
 */
int rpmReadPackageHeader(FD_t fd, /*@out@*/ Header * hdr,
	/*@out@*/ int * isSource, /*@out@*/ int * major, /*@out@*/ int * minor);

/**
 */
int headerNVR(Header h, /*@out@*/ const char **np, /*@out@*/ const char **vp,
	/*@out@*/ const char **rp);

/**
 */
void	rpmBuildFileList(Header h, /*@out@*/ const char *** fileListPtr, 
			/*@out@*/ int * fileCountPtr);

/*
 * XXX This is a "dressed" entry to headerGetEntry to do:
 *	1) DIRNAME/BASENAME/DIRINDICES -> FILENAMES tag conversions.
 *	2) i18n lookaside (if enabled).
 */
int rpmHeaderGetEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
        /*@out@*/ void **p, /*@out@*/int_32 *c);

   /* 0 = success */
   /* 1 = bad magic */
   /* 2 = error */

extern const struct headerTagTableEntry rpmTagTable[];
extern const int rpmTagTableSize;

/* this chains to headerDefaultFormats[] */
extern const struct headerSprintfExtension rpmHeaderFormats[];

/* these pseudo-tags are used in the dbi interface */
#define	RPMDBI_PACKAGES		0
#define	RPMDBI_DEPENDS		1
#define	RPMDBI_LABEL		2	/* XXX remove rpmdbFindByLabel from API */
#define	RPMDBI_ADDED		3
#define	RPMDBI_REMOVED		4
#define	RPMDBI_AVAILABLE	5


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
#define	RPMTAG_PROVIDES	RPMTAG_PROVIDENAME	/* backward comaptibility */
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
#define	RPMTAG_CAPABILITY		1105 /* unused */
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

/* Stuff for maintaining "variables" like SOURCEDIR, BUILDDIR, etc */

/* #define	RPMVAR_SOURCEDIR		0 -- No longer used */
/* #define	RPMVAR_BUILDDIR			1 -- No longer used */
/* #define	RPMVAR_DOCDIR			2 -- No longer used */
#define	RPMVAR_OPTFLAGS			3
/* #define	RPMVAR_TOPDIR			4 -- No longer used */
/* #define	RPMVAR_SPECDIR			5 -- No longer used */
/* #define	RPMVAR_ROOT			6 -- No longer used */
/* #define	RPMVAR_RPMDIR			7 -- No longer used */
/* #define	RPMVAR_SRPMDIR			8 -- No longer used */
/* #define	RPMVAR_ARCHSENSITIVE 		9  -- No longer used */
/* #define	RPMVAR_REQUIREDISTRIBUTION	10 -- No longer used */
/* #define	RPMVAR_REQUIREGROUP			11 -- No longer used */
/* #define	RPMVAR_REQUIREVENDOR		12 -- No longer used */
/* #define	RPMVAR_DISTRIBUTION		13 -- No longer used */
/* #define	RPMVAR_VENDOR			14 -- No longer used */
/* #define	RPMVAR_MESSAGELEVEL		15 -- No longer used */
/* #define	RPMVAR_REQUIREICON		16 -- No longer used */
/* #define	RPMVAR_TIMECHECK		17 -- No longer used */
/* #define	RPMVAR_SIGTYPE			18 -- No longer used */
/* #define	RPMVAR_PGP_PATH			19 -- No longer used */
/* #define	RPMVAR_PGP_NAME			20 -- No longer used */
/* #define	RPMVAR_PGP_SECRING		21 -- No longer used */
/* #define	RPMVAR_PGP_PUBRING		22 -- No longer used */
/* #define	RPMVAR_EXCLUDEDOCS		23 -- No longer used */
/* #define	RPMVAR_BUILDARCH		24 -- No longer used */
/* #define	RPMVAR_BUILDOS			25 -- No longer used */
/* #define	RPMVAR_BUILDROOT		26 */
/* #define	RPMVAR_DBPATH			27 -- No longer used */
/* #define	RPMVAR_PACKAGER			28 -- No longer used */
/* #define	RPMVAR_FTPPROXY			29 -- No longer used */
/* #define	RPMVAR_TMPPATH			30 -- No longer used */
/* #define	RPMVAR_CPIOBIN			31 -- No longer used */
/* #define	RPMVAR_FTPPORT			32 -- No longer used */
/* #define	RPMVAR_NETSHAREDPATH		33 -- No longer used */
/* #define	RPMVAR_DEFAULTDOCDIR		34 -- No longer used */
/* #define	RPMVAR_FIXPERMS			35 -- No longer used */
/* #define	RPMVAR_GZIPBIN			36 -- No longer used */
/* #define	RPMVAR_RPMFILENAME		37 -- No longer used */
#define	RPMVAR_PROVIDES			38
/* #define	RPMVAR_BUILDSHELL		39 -- No longer used */
/* #define	RPMVAR_INSTCHANGELOG		40 -- No longer used */
/* #define	RPMVAR_BZIP2BIN			41 -- No longer used */
/* #define	RPMVAR_LANGPATT			42 -- No longer used */
#define	RPMVAR_INCLUDE			43
/* #define	RPMVAR_ARCH			44 -- No longer used */
/* #define	RPMVAR_OS			45 -- No longer used */
/* #define	RPMVAR_BUILDPLATFORM		46 -- No longer used */
/* #define	RPMVAR_BUILDARCH		47 -- No longer used */
/* #define	RPMVAR_BUILDOS			48 -- No longer used */
#define	RPMVAR_MACROFILES		49
/* #define	RPMVAR_GPG_PATH			51 -- No longer used */
/* #define	RPMVAR_GPG_NAME			52 -- No longer used */
/* #define	RPMVAR_HTTPPROXY		53 -- No longer used */
/* #define	RPMVAR_HTTPPORT			54 -- No longer used */

#define	RPMVAR_NUM			55	/* number of RPMVAR entries */

#define	xfree(_p)	free((void *)_p)

const char * rpmGetVar(int var);
void rpmSetVar(int var, const char *val);

/** rpmrc.c **/

#define	RPM_MACHTABLE_INSTARCH		0
#define	RPM_MACHTABLE_INSTOS		1
#define	RPM_MACHTABLE_BUILDARCH		2
#define	RPM_MACHTABLE_BUILDOS  		3
#define	RPM_MACHTABLE_COUNT		4	/* number of arch/os tables */

int rpmReadConfigFiles(const char * file, const char * target);
int rpmReadRC(const char * file);
void rpmGetArchInfo( /*@out@*/ const char ** name, /*@out@*/ int * num);
void rpmGetOsInfo( /*@out@*/ const char ** name, /*@out@*/ int * num);
int rpmMachineScore(int type, const char * name);
int rpmShowRC(FILE *f);
void rpmSetTables(int archTable, int osTable);  /* only used by build code */
/* if either are NULL, they are set to the default value (munged uname())
   pushed through a translation table (if appropriate) */
void rpmSetMachine(const char * arch, const char * os);
void rpmGetMachine( /*@out@*/ const char **arch, /*@out@*/ const char **os);
void rpmFreeRpmrc(void);

/* ==================================================================== */
/** **/
typedef /*@abstract@*/ struct rpmdb_s * rpmdb;

/**
 * @param dbp	address of rpm database
 */
int rpmdbOpen (const char * root, /*@out@*/ rpmdb * dbp, int mode, int perms);
    /* 0 on error */

/**
 */
int rpmdbInit(const char * root, int perms);
    /* nonzero on error */

/**
 * Close all database indices and free rpmdb.
 * @param rpmdb		rpm database
 * @return		0 always
 */
int rpmdbClose ( /*@only@*/ rpmdb rpmdb);

/**
 * Sync all database indices.
 * @param rpmdb		rpm database
 * @return		0 always
 */
int rpmdbSync (rpmdb rpmdb);

/**
 * @param rpmdb		rpm database
 */
int rpmdbOpenAll (rpmdb rpmdb);

/**
 * Return number of instances of package in rpm database.
 * @param db		rpm database
 * @param name		rpm package name
 * @return		number of instances
 */
int rpmdbCountPackages(rpmdb db, const char *name);

/**
 */
typedef /*@abstract@*/ struct _rpmdbMatchIterator * rpmdbMatchIterator;

/**
 * Destroy rpm database iterator.
 * @param mi		rpm database iterator
 */
void rpmdbFreeIterator( /*@only@*/ rpmdbMatchIterator mi);

/**
 * Return rpm database used by iterator.
 * @param mi		rpm database iterator
 * @return		rpm database handle
 */
rpmdb rpmdbGetIteratorRpmDB(rpmdbMatchIterator mi);

/**
 * Return join key for current position of rpm database iterator.
 * @param mi		rpm database iterator
 * @return		current join key
 */
unsigned int rpmdbGetIteratorOffset(rpmdbMatchIterator mi);

/**
 * Return number of elements in rpm database iterator.
 * @param mi		rpm database iterator
 * @return		number of elements
 */
int rpmdbGetIteratorCount(rpmdbMatchIterator mi);

/**
 * Append items to set of package instances to iterate.
 * @param mi		rpm database iterator
 * @param hdrNums	array of package instances
 * @param nHdrNums	number of elements in array
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbAppendIterator(rpmdbMatchIterator mi, int * hdrNums, int nHdrNums);

/**
 * Remove items from set of package instances to iterate.
 * @param mi		rpm database iterator
 * @param hdrNums	array of package instances
 * @param nHdrNums	number of elements in array
 * @param sorted	is the array sorted? (array will be sorted on return)
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbPruneIterator(rpmdbMatchIterator mi, int * hdrNums,
	int nHdrNums, int sorted);

/**
 * Modify iterator to filter out headers that do not match version.
 *  TODO: replace with a more general mechanism.
 * @param mi		rpm database iterator
 * @param version	version to check for
 */
void rpmdbSetIteratorVersion(rpmdbMatchIterator mi, /*@kept@*/ const char * version);

/**
 * Modify iterator to filter out headers that do not match release.
 *  TODO: replace with a more general mechanism.
 * @param mi		rpm database iterator
 * @param release	release to check for
 */
void rpmdbSetIteratorRelease(rpmdbMatchIterator mi, /*@kept@*/ const char * release);

/**
 * Modify iterator to mark header for lazy write.
 *  TODO: replace with a more general mechanism.
 * @param mi		rpm database iterator
 * @param modified	new value of modified
 * @return		previous value
 */
int rpmdbSetIteratorModified(rpmdbMatchIterator mi, int modified);

/**
 * Return next package header from iteration.
 * @param mi		rpm database iterator
 * @return		NULL on end of iteration.
 */
Header rpmdbNextIterator(rpmdbMatchIterator mi);
Header XrpmdbNextIterator(rpmdbMatchIterator mi, const char * f, unsigned int l);
#define	rpmdbNextIterator(_a) \
	XrpmdbNextIterator(_a, __FILE__, __LINE__)

/**
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

/**
 * Remove package header from rpm database and indices.
 * @param rpmdb		rpm database
 * @param offset	location in Packages dbi
 * @return		0 on success
 */
int rpmdbRemove(rpmdb db, unsigned int offset);

/**
 * Add package header to rpm database and indices.
 * @param rpmdb		rpm database
 * @param rpmtag	rpm tag
 * @return		0 on success
 */
int rpmdbAdd(rpmdb rpmdb, Header dbentry);

/**
 */
int rpmdbRebuild(const char * root);

/* ==================================================================== */
/* we pass these around as an array with a sentinel */
typedef struct rpmRelocation_s {
    const char * oldPath;	/* NULL here evals to RPMTAG_DEFAULTPREFIX, */
				/* XXX for backwards compatibility */
    const char * newPath;	/* NULL means to omit the file completely! */
} rpmRelocation;


/**
 */
int rpmInstallSourcePackage(const char * root, FD_t fd, const char ** specFile,
			    rpmCallbackFunction notify, void * notifyData,
			    char ** cookie);

/**
 */
int rpmVersionCompare(Header first, Header second);


/**
 */
int rpmVerifyFile(const char * root, Header h, int filenum,
	/*@out@*/ int * result, int omitMask);

/**
 */
int rpmVerifyScript(const char * root, Header h, FD_t err);

/* Transaction sets are inherently unordered! RPM may reorder transaction
   sets to reduce errors. In general, installs/upgrades are done before
   strict removals, and prerequisite ordering is done on installs/upgrades. */
typedef /*@abstract@*/ struct rpmTransactionSet_s * rpmTransactionSet;

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

/* db may be NULL, but don't do things which require the database! */
/**
 * @param db		rpm database
 */
/*@only@*/ rpmTransactionSet rpmtransCreateSet( /*@only@*/ rpmdb db,
	const char * rootdir);

/* if fd is NULL, the callback specified in rpmtransCreateSet() is used to
   open and close the file descriptor. If Header is NULL, the fd is always
   used, otherwise fd is only needed (and only opened) for actual package 
   installation

   returns 0 on success, 1 on I/O error, 2 if the package needs capabilities
   which are not implemented */

/**
 */
int rpmtransAddPackage(rpmTransactionSet rpmdep, Header h, FD_t fd,
		/*@owned@*/ const void * key, int update,
		rpmRelocation * relocs);

/**
 */
void rpmtransAvailablePackage(rpmTransactionSet rpmdep, Header h,
		/*@owned@*/ const void * key);

/**
 */
void rpmtransRemovePackage(rpmTransactionSet rpmdep, int dboffset);

/**
 */
void rpmtransFree( /*@only@*/ rpmTransactionSet rpmdep);

/**
 */
void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd);

/* this checks for dependency satisfaction, but *not* ordering */

/**
 */
int rpmdepCheck(rpmTransactionSet rpmdep,
	/*@exposed@*/ /*@out@*/ struct rpmDependencyConflict ** conflicts,
	/*@exposed@*/ /*@out@*/ int * numConflicts);

/* Orders items, returns error on circle, finals keys[] is NULL. No dependency
   check is done, use rpmdepCheck() for that. If dependencies are not
   satisfied a "best-try" ordering is returned. */

/**
 */
int rpmdepOrder(rpmTransactionSet order);

/**
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

typedef enum rpmProblemType_e { RPMPROB_BADARCH, 
				RPMPROB_BADOS,
				RPMPROB_PKG_INSTALLED,
				RPMPROB_BADRELOCATE,
				RPMPROB_REQUIRES,
				RPMPROB_CONFLICT,
				RPMPROB_NEW_FILE_CONFLICT,
				RPMPROB_FILE_CONFLICT,
				RPMPROB_OLDPACKAGE,
				RPMPROB_DISKSPACE
 			      } rpmProblemType;

typedef /*@abstract@*/ struct rpmProblem_s {
    Header h, altH;
/*@dependent@*/ const void * key;
    rpmProblemType type;
    int ignoreProblem;
/*@only@*/ const char * str1;
    unsigned long ulong1;
} rpmProblem;

typedef /*@abstract@*/ struct rpmProblemSet_s {
    int numProblems;
    int numProblemsAlloced;
    rpmProblem * probs;
} * rpmProblemSet;

void printDepFlags(FILE *fp, const char *version, int flags);
void printDepProblems(FILE *fp, struct rpmDependencyConflict *conflicts,
	int numConflicts);

/*@only@*/ const char * rpmProblemString(rpmProblem prob);
void rpmProblemPrint(FILE *fp, rpmProblem prob);
void rpmProblemSetPrint(FILE *fp, rpmProblemSet probs);

void rpmProblemSetFree( /*@only@*/ rpmProblemSet probs);

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

/** rpmlead.c **/

#define	RPMLEAD_BINARY 0
#define	RPMLEAD_SOURCE 1

#define	RPMLEAD_MAGIC0 0xed
#define	RPMLEAD_MAGIC1 0xab
#define	RPMLEAD_MAGIC2 0xee
#define	RPMLEAD_MAGIC3 0xdb

/* The lead needs to be 8 byte aligned */

#define	RPMLEAD_SIZE 96

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

/** signature.c **/

/**************************************************/
/*                                                */
/* Signature Tags                                 */
/*                                                */
/* These go in the sig Header to specify          */
/* individual signature types.                    */
/*                                                */
/**************************************************/

#define	RPMSIGTAG_SIZE         	        1000
/* the md5 sum was broken *twice* on big endian machines */
#define	RPMSIGTAG_LEMD5_1		1001
#define	RPMSIGTAG_PGP          	        1002
#define	RPMSIGTAG_LEMD5_2		1003
#define	RPMSIGTAG_MD5		        1004
#define	RPMSIGTAG_GPG		        1005
#define	RPMSIGTAG_PGP5		        1006	/* XXX legacy use only */

/* Signature tags by Public Key Algorithm (RFC 2440) */
/* N.B.: These tags are tenative, the values may change */
#define	RPMTAG_PK_BASE			2048
#define	RPMTAG_PK_RSA_ES		RPMTAG_PK_BASE+1
#define	RPMTAG_PK_RSA_E			RPMTAG_PK_BASE+2
#define	RPMTAG_PK_RSA_S			RPMTAG_PK_BASE+3
#define	RPMTAG_PK_ELGAMAL_E		RPMTAG_PK_BASE+16
#define	RPMTAG_PK_DSA			RPMTAG_PK_BASE+17
#define	RPMTAG_PK_ELLIPTIC		RPMTAG_PK_BASE+18
#define	RPMTAG_PK_ECDSA			RPMTAG_PK_BASE+19
#define	RPMTAG_PK_ELGAMAL_ES		RPMTAG_PK_BASE+20
#define	RPMTAG_PK_DH			RPMTAG_PK_BASE+21

#define	RPMTAG_HASH_BASE		2048+64
#define	RPMTAG_HASH_MD5			RPMTAG_HASH_BASE+1
#define	RPMTAG_HASH_SHA1		RPMTAG_HASH_BASE+2
#define	RPMTAG_HASH_RIPEMD160		RPMTAG_HASH_BASE+3
#define	RPMTAG_HASH_MD2			RPMTAG_HASH_BASE+5
#define	RPMTAG_HASH_TIGER192		RPMTAG_HASH_BASE+6
#define	RPMTAG_HASH_HAVAL_5_160		RPMTAG_HASH_BASE+7

/**************************************************/
/*                                                */
/* verifySignature() results                      */
/*                                                */
/**************************************************/

/* verifySignature() results */
#define	RPMSIG_OK		0
#define	RPMSIG_UNKNOWN		1
#define	RPMSIG_BAD		2
#define	RPMSIG_NOKEY		3  /* Do not have the key to check this signature */
#define	RPMSIG_NOTTRUSTED	4  /* We have the key but it is not trusted */

void rpmFreeSignature(Header h);

int rpmVerifySignature(const char *file, int_32 sigTag, void *sig, int count,
		       char *result);

void freeFilesystems(void);
int rpmGetFilesystemList( /*@out@*/ const char *** listptr, /*@out@*/int * num);
int rpmGetFilesystemUsage(const char ** filelist, int_32 * fssizes,
	int numFiles, /*@out@*/ uint_32 ** usagesPtr, int flags);

/* ==================================================================== */
/* --- build mode options */

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
typedef	struct rpmBuildArguments BTA_t;

extern struct rpmBuildArguments         rpmBTArgs;

extern struct poptOption		rpmBuildPoptTable[];

/* ==================================================================== */
/* --- query/verify mode options */

/* XXX SPECFILE is not verify sources */
enum rpmQVSources { RPMQV_PACKAGE = 0, RPMQV_PATH, RPMQV_ALL, RPMQV_RPM, 
		       RPMQV_GROUP, RPMQV_WHATPROVIDES, RPMQV_WHATREQUIRES,
		       RPMQV_TRIGGEREDBY, RPMQV_DBOFFSET, RPMQV_SPECFILE };

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
typedef	struct rpmQVArguments QVA_t;

extern struct rpmQVArguments		rpmQVArgs;

extern struct poptOption		rpmQVSourcePoptTable[];

/**
 * @param qva		parsed query/verify options
 * @param db		rpm database
 * @param h		header to use for query/verify
 */
typedef	int (*QVF_t) (QVA_t *qva, rpmdb db, Header h);

/**
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
 * @param tag		tag value
 * @return		name of tag
 */
/*@observer@*/ const char *const tagName(int tag);

/**
 * @param targstr	name of tag
 * @return		tag value
 */
int tagValue(const char *tagstr);

extern int specedit;
extern struct poptOption rpmQueryPoptTable[];

/**
 * @param f	file handle to use for display
 */
void rpmDisplayQueryTags(FILE * f);

/**
 * @param qva		parsed query/verify options
 * @param source	type of source to query/verify
 * @param arg		name of source to query/verify
 * @param db		rpm database
 * @param showPackage	query/verify routine
 */
int rpmQueryVerify(QVA_t *qva, enum rpmQVSources source, const char * arg,
	rpmdb db, QVF_t showPackage);

/**
 * @param qva		parsed query/verify options
 * @param db		rpm database (unused for queries)
 * @param h		header to use for query
 */
int showQueryPackage(QVA_t *qva, rpmdb db, Header h);

/**
 * @param qva		parsed query/verify options
 * @param source	type of source to query
 * @param arg		name of source to query
 */
int rpmQuery(QVA_t *qva, enum rpmQVSources source, const char * arg);

#define	VERIFY_FILES		(1 <<  9)
#define	VERIFY_DEPS		(1 << 10)
#define	VERIFY_SCRIPT		(1 << 11)
#define	VERIFY_MD5		(1 << 12)

extern struct poptOption rpmVerifyPoptTable[];

/**
 * @param qva		parsed query/verify options
 * @param db		rpm database
 * @param h		header to use for verify
 */
int showVerifyPackage(QVA_t *qva, /*@only@*/ rpmdb db, Header h);

/**
 * @param qva		parsed query/verify options
 * @param source	type of source to verify
 * @param arg		name of source to verify
 */
int rpmVerify(QVA_t *qva, enum rpmQVSources source, const char *arg);

/* ==================================================================== */
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


/**
 */
int rpmInstall(const char * rootdir, const char ** argv, int installFlags, 
	      int interfaceFlags, int probFilter, rpmRelocation * relocations);

/**
 */
int rpmInstallSource(const char * prefix, const char * arg, const char ** specFile,
		    char ** cookie);

/**
 */
int rpmErase(const char * rootdir, const char ** argv, int uninstallFlags, 
		 int interfaceFlags);

/* ==================================================================== */
/* --- checksig/resign */

#define	CHECKSIG_PGP (1 << 0)
#define	CHECKSIG_MD5 (1 << 1)
#define	CHECKSIG_GPG (1 << 2)

/**
 */
int rpmCheckSig(int flags, const char **argv);

/**
 */
int rpmReSign(int add, char *passPhrase, const char **argv);

#define	ADD_SIGNATURE 1
#define	NEW_SIGNATURE 0

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMLIB */
