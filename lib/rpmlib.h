#ifndef H_RPMLIB
#define	H_RPMLIB

/* This is the *only* module users of rpmlib should need to include */

/* and it shouldn't need these :-( */

#include "rpmio.h"
#include "dbindex.h"
#include "header.h"

#ifdef __cplusplus
extern "C" {
#endif

int rpmReadPackageInfo(FD_t fd, Header * signatures, Header * hdr);
int rpmReadPackageHeader(FD_t fd, Header * hdr, int * isSource, int * major,
			 int * minor);
   /* 0 = success */
   /* 1 = bad magic */
   /* 2 = error */

extern const struct headerTagTableEntry rpmTagTable[];
extern const int rpmTagTableSize;

/* this chains to headerDefaultFormats[] */
extern const struct headerSprintfExtension rpmHeaderFormats[];

/* these tags are for both the database and packages */
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
#define	RPMTAG_FILENAMES		1027
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
#define	RPMTAG_ROOT			1038
#define	RPMTAG_FILEUSERNAME		1039
#define	RPMTAG_FILEGROUPNAME		1040
#define	RPMTAG_EXCLUDE			1041 /* internal - depricated */
#define	RPMTAG_EXCLUSIVE		1042 /* internal - depricated */
#define	RPMTAG_ICON			1043
#define	RPMTAG_SOURCERPM		1044
#define	RPMTAG_FILEVERIFYFLAGS		1045
#define	RPMTAG_ARCHIVESIZE		1046
#define	RPMTAG_PROVIDES                 1047
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
#define	RPMTAG_OBSOLETES		1090
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
#define	RPMTAG_CAPABILITY		1105
#define	RPMTAG_SOURCEPACKAGE		1106 /* internal */
#define	RPMTAG_ORIGFILENAMES		1107
#define	RPMTAG_BUILDREQUIRES		1108 /* internal */

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

#define	RPMINSTALL_TEST			(1 << 2)
#define	RPMINSTALL_UPGRADETOOLD		(1 << 4)
#define	RPMINSTALL_NODOCS		(1 << 5)
#define	RPMINSTALL_NOSCRIPTS		(1 << 6)
#define	RPMINSTALL_ALLFILES		(1 << 9)
#define	RPMINSTALL_JUSTDB		(1 << 10)
#define	RPMINSTALL_KEEPOBSOLETE		(1 << 11)
#define	RPMINSTALL_NOTRIGGERS		(1 << 13)

#define	RPMUNINSTALL_TEST		(1 << 0)
#define	RPMUNINSTALL_NOSCRIPTS		(1 << 1)
#define	RPMUNINSTALL_JUSTDB		(1 << 2)
#define	RPMUNINSTALL_NOTRIGGERS		(1 << 3)

#define RPMVERIFY_NONE		0
#define RPMVERIFY_MD5		(1 << 0)
#define RPMVERIFY_FILESIZE	(1 << 1)
#define RPMVERIFY_LINKTO	(1 << 2)
#define RPMVERIFY_USER		(1 << 3)
#define RPMVERIFY_GROUP		(1 << 4)
#define RPMVERIFY_MTIME		(1 << 5)
#define RPMVERIFY_MODE		(1 << 6)
#define RPMVERIFY_RDEV		(1 << 7)
#define RPMVERIFY_READLINKFAIL	(1 << 28)
#define RPMVERIFY_READFAIL	(1 << 29)
#define RPMVERIFY_LSTATFAIL	(1 << 30)

#define RPMVERIFY_ALL		~(RPMVERIFY_NONE)

#define	RPMSENSE_ANY		0
#define	RPMSENSE_SERIAL		(1 << 0)
#define	RPMSENSE_LESS		(1 << 1)
#define	RPMSENSE_GREATER	(1 << 2)
#define	RPMSENSE_EQUAL		(1 << 3)
#define	RPMSENSE_PROVIDES	(1 << 4) /* only used internally by builds */
#define	RPMSENSE_CONFLICTS	(1 << 5) /* only used internally by builds */
#define	RPMSENSE_PREREQ		(1 << 6)
#define	RPMSENSE_OBSOLETES	(1 << 7) /* only used internally by builds */
#define	RPMSENSE_SENSEMASK	15       /* Mask to get senses, ie serial, */
                                          /* less, greater, equal.          */

#define	RPMSENSE_TRIGGERIN	(1 << 16)
#define	RPMSENSE_TRIGGERUN	(1 << 17)
#define	RPMSENSE_TRIGGERPOSTUN	(1 << 18)
#define	RPMSENSE_TRIGGER	(RPMSENSE_TRIGGERIN | RPMSENSE_TRIGGERUN | \
                                  RPMSENSE_TRIGGERPOSTUN)

/* Stuff for maintaining "variables" like SOURCEDIR, BUILDDIR, etc */

#define	RPMVAR_SOURCEDIR		0
#define	RPMVAR_BUILDDIR			1
/* #define RPMVAR_DOCDIR		2 -- No longer used */
#define	RPMVAR_OPTFLAGS			3
#define	RPMVAR_TOPDIR			4
#define	RPMVAR_SPECDIR			5
#define	RPMVAR_ROOT			6
#define	RPMVAR_RPMDIR			7
#define	RPMVAR_SRPMDIR			8
/* #define RPMVAR_ARCHSENSITIVE 	9  -- No longer used */
#define	RPMVAR_REQUIREDISTRIBUTION	10
/* #define RPMVAR_REQUIREGROUP		11 -- No longer used */
#define	RPMVAR_REQUIREVENDOR		12
#define	RPMVAR_DISTRIBUTION		13
#define	RPMVAR_VENDOR			14
#define	RPMVAR_MESSAGELEVEL		15
#define	RPMVAR_REQUIREICON		16
#define	RPMVAR_TIMECHECK		17
#define	RPMVAR_SIGTYPE			18
#define	RPMVAR_PGP_PATH			19
#define	RPMVAR_PGP_NAME			20
/* #define RPMVAR_PGP_SECRING		21 -- No longer used */
/* #define RPMVAR_PGP_PUBRING		22 -- No longer used */
#define	RPMVAR_EXCLUDEDOCS		23
/* #define RPMVAR_BUILDARCH		24 -- No longer used */
/* #define RPMVAR_BUILDOS		25 -- No longer used */
#define	RPMVAR_BUILDROOT		26
#define	RPMVAR_DBPATH			27
#define	RPMVAR_PACKAGER			28
#define	RPMVAR_FTPPROXY			29
#define	RPMVAR_TMPPATH			30
/* #define RPMVAR_CPIOBIN		31 -- No longer used */
#define	RPMVAR_FTPPORT			32
#define	RPMVAR_NETSHAREDPATH		33
#define	RPMVAR_DEFAULTDOCDIR		34
#define	RPMVAR_FIXPERMS			35
#define	RPMVAR_GZIPBIN			36
#define	RPMVAR_RPMFILENAME		37
#define	RPMVAR_PROVIDES			38
#define	RPMVAR_BUILDSHELL		39
#define	RPMVAR_INSTCHANGELOG		40
#define	RPMVAR_BZIP2BIN			41
#define	RPMVAR_LANGPATT			42
#define	RPMVAR_INCLUDE			43
/* #define RPMVAR_ARCH			44 -- reserved */
/* #define RPMVAR_OS			45 -- reserved */
/* #define RPMVAR_BUILDPLATFORM		46 -- reserved */
/* #define RPMVAR_BUILDARCH		47 -- reserved */
/* #define RPMVAR_BUILDOS		48 -- reserved */
#define	RPMVAR_MACROFILES		49
#define	RPMVAR_GPG_PATH			51
#define	RPMVAR_GPG_NAME			52

#define	RPMVAR_NUM			53     /* number of RPMVAR entries */

#define	xfree(_p)	free((void *)_p)
const char *rpmGetPath(const char *path, ...);
char * rpmGetVar(int var);
int rpmGetBooleanVar(int var);
void rpmSetVar(int var, const char *val);

/** rpmrc.c **/

#define	RPM_MACHTABLE_INSTARCH		0
#define	RPM_MACHTABLE_INSTOS		1
#define	RPM_MACHTABLE_BUILDARCH		2
#define	RPM_MACHTABLE_BUILDOS  		3
#define	RPM_MACHTABLE_COUNT		4	/* number of arch/os tables */

int rpmReadConfigFiles(const char * file, const char * target);
int rpmReadRC(const char * file);
void rpmGetArchInfo(/*@out@*/char ** name, /*@out@*/int * num);
void rpmGetOsInfo(/*@out@*/char ** name, /*@out@*/int * num);
int rpmMachineScore(int type, char * name);
int rpmShowRC(FILE *f);
void rpmSetTables(int archTable, int osTable);  /* only used by build code */
/* if either are NULL, they are set to the default value (munged uname())
   pushed through a translation table (if appropriate) */
void rpmSetMachine(const char * arch, const char * os);
void rpmGetMachine(/*@out@*/char **arch, /*@out@*/char **os);

/** **/

typedef /*@abstract@*/ struct rpmdb_s * rpmdb;

typedef enum rpmCallbackType_e 
	{ RPMCALLBACK_INST_PROGRESS, RPMCALLBACK_INST_START,
	  RPMCALLBACK_INST_OPEN_FILE, RPMCALLBACK_INST_CLOSE_FILE 
	} rpmCallbackType;
typedef void * (*rpmCallbackFunction)(const Header h, 
				      const rpmCallbackType what, 
				      const unsigned long amount, 
				      const unsigned long total,
				      const void * pkgKey, void * data);

int rpmdbOpen (const char * root, rpmdb * dbp, int mode, int perms);
    /* 0 on error */
int rpmdbInit(const char * root, int perms);
    /* nonzero on error */
void rpmdbClose (rpmdb db);

int rpmdbFirstRecNum(rpmdb db);
int rpmdbNextRecNum(rpmdb db, unsigned int lastOffset);
    /* 0 at end, -1 on error */

Header rpmdbGetRecord(rpmdb db, unsigned int offset);
int rpmdbFindByFile(rpmdb db, const char * filespec, dbiIndexSet * matches);
int rpmdbFindByGroup(rpmdb db, const char * group, dbiIndexSet * matches);
int rpmdbFindPackage(rpmdb db, const char * name, dbiIndexSet * matches);
int rpmdbFindByProvides(rpmdb db, const char * provides, dbiIndexSet * matches);
int rpmdbFindByRequiredBy(rpmdb db, const char * requires, dbiIndexSet * matches);
int rpmdbFindByConflicts(rpmdb db, const char * conflicts, dbiIndexSet * matches);
int rpmdbFindByTriggeredBy(rpmdb db, const char * package, dbiIndexSet * matches);

/* these are just convenience functions */
int rpmdbFindByLabel(rpmdb db, const char * label, dbiIndexSet * matches);
int rpmdbFindByHeader(rpmdb db, Header h, dbiIndexSet * matches);

/* we pass these around as an array with a sentinel */
typedef struct rpmRelocation_s {
    char * oldPath;	/* NULL here evals to RPMTAG_DEFAULTPREFIX, this */
			/* odd behavior is only for backwards compatibility */
    char * newPath;	/* NULL means to omit the file completely! */
} rpmRelocation;

int rpmInstallSourcePackage(const char * root, FD_t fd, const char ** specFile,
			    rpmCallbackFunction notify, void * notifyData,
			    char ** cookie);
int rpmVersionCompare(Header first, Header second);
int rpmdbRebuild(const char * root);

int rpmVerifyFile(const char * root, Header h, int filenum, int * result,
		  int omitMask);
int rpmVerifyScript(const char * root, Header h, FD_t err);

/* Transaction sets are inherently unordered! RPM may reorder transaction
   sets to reduce errors. In general, installs/upgrades are done before
   strict removals, and prerequisite ordering is done on installs/upgrades. */
typedef struct rpmTransactionSet_s * rpmTransactionSet;

struct rpmDependencyConflict {
    char * byName, * byVersion, * byRelease;
    Header byHeader;
    /* these needs fields are misnamed -- they are used for the package
       which isn't needed as well */
    char * needsName, * needsVersion;
    int needsFlags;
    const void * suggestedPackage;			/* NULL if none */
    enum { RPMDEP_SENSE_REQUIRES, RPMDEP_SENSE_CONFLICTS } sense;
} ;

/* db may be NULL, but don't do things which require the database! */
/*@only@*/ rpmTransactionSet rpmtransCreateSet(rpmdb db, const char * rootdir);

/* if fd is NULL, the callback specified in rpmtransCreateSet() is used to
   open and close the file descriptor. If Header is NULL, the fd is always
   used, otherwise fd is only needed (and only opened) for actual package 
   installation */
int rpmtransAddPackage(rpmTransactionSet rpmdep, Header h, FD_t fd,
			const void * key, int update, rpmRelocation * relocs);
void rpmtransAvailablePackage(rpmTransactionSet rpmdep, Header h, void * key);
void rpmtransRemovePackage(rpmTransactionSet rpmdep, int dboffset);
void rpmtransFree(/*@only@*/ rpmTransactionSet rpmdep);

/* this checks for dependency satisfaction, but *not* ordering */
int rpmdepCheck(rpmTransactionSet rpmdep,
		struct rpmDependencyConflict ** conflicts, int * numConflicts);
/* Orders items, returns error on circle, finals keys[] is NULL. No dependency
   check is done, use rpmdepCheck() for that. If dependencies are not
   satisfied a "best-try" ordering is returned. */
int rpmdepOrder(rpmTransactionSet order, void *** keysListPtr);
void rpmdepFreeConflicts(struct rpmDependencyConflict * conflicts, int
			 numConflicts);

#define RPMTRANS_FLAG_TEST		(1 << 0)
#define RPMTRANS_FLAG_BUILD_PROBS	(1 << 1)

typedef enum rpmProblemType_e { RPMPROB_BADARCH, 
				RPMPROB_BADOS,
				RPMPROB_PKG_INSTALLED,
				RPMPROB_BADRELOCATE,
				RPMPROB_REQUIRES,
				RPMPROB_CONFLICT,
				RPMPROB_NEW_FILE_CONFLICT,
				RPMPROB_FILE_CONFLICT,
 			      } rpmProblemType;

typedef struct rpmProblem_s {
    Header h, altH;
    const void * key;
    rpmProblemType type;
    int ignoreProblem;
    char * str1;
} rpmProblem;

typedef struct rpmProblemSet_s {
    int numProblems;
    int numProblemsAlloced;
    rpmProblem * probs;
} * rpmProblemSet;

char * rpmProblemString(rpmProblem prob);
void rpmProblemSetFree(rpmProblemSet probs);
void rpmProblemSetFilter(rpmProblemSet ps, int flags);
int rpmRunTransactions(rpmTransactionSet ts, rpmCallbackFunction notify,
		       void * notifyData, rpmProblemSet okProbs,
		       rpmProblemSet * newProbs, int flags);

#define RPMPROB_FILTER_IGNOREOS		(1 << 0)
#define RPMPROB_FILTER_IGNOREARCH	(1 << 1)
#define RPMPROB_FILTER_REPLACEPKG	(1 << 2)
#define RPMPROB_FILTER_FORCERELOCATE	(1 << 3)
#define RPMPROB_FILTER_REPLACENEWFILES	(1 << 4)
#define RPMPROB_FILTER_REPLACEOLDFILES	(1 << 5)

/** messages.c **/

#define	RPMMESS_DEBUG      1
#define	RPMMESS_VERBOSE    2
#define	RPMMESS_NORMAL     3
#define	RPMMESS_WARNING    4
#define	RPMMESS_ERROR      5
#define	RPMMESS_FATALERROR 6

#define	RPMMESS_QUIET (RPMMESS_NORMAL + 1)

void rpmIncreaseVerbosity(void);
void rpmSetVerbosity(int level);
int rpmGetVerbosity(void);
int rpmIsVerbose(void);
int rpmIsDebug(void);
void rpmMessage(int level, char * format, ...);

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

/** rpmerr.c **/

typedef void (*rpmErrorCallBackType)(void);

void rpmError(int code, char * format, ...);
int rpmErrorCode(void);
char *rpmErrorCodeString(void);
char *rpmErrorString(void);
rpmErrorCallBackType rpmErrorSetCallback(rpmErrorCallBackType);

#define	RPMERR_GDBMOPEN		-2      /* gdbm open failed */
#define	RPMERR_GDBMREAD		-3	/* gdbm read failed */
#define	RPMERR_GDBMWRITE	-4	/* gdbm write failed */
#define	RPMERR_INTERNAL		-5	/* internal RPM error */
#define	RPMERR_DBCORRUPT	-6	/* rpm database is corrupt */
#define	RPMERR_OLDDBCORRUPT	-7	/* old style rpm database is corrupt */
#define	RPMERR_OLDDBMISSING	-8	/* old style rpm database is missing */
#define	RPMERR_NOCREATEDB	-9	/* cannot create new database */
#define	RPMERR_DBOPEN		-10     /* database open failed */
#define	RPMERR_DBGETINDEX	-11     /* database get from index failed */
#define	RPMERR_DBPUTINDEX	-12     /* database get from index failed */
#define	RPMERR_NEWPACKAGE	-13     /* package is too new to handle */
#define	RPMERR_BADMAGIC		-14	/* bad magic for an RPM */
#define	RPMERR_RENAME		-15	/* rename(2) failed */
#define	RPMERR_UNLINK		-16	/* unlink(2) failed */
#define	RPMERR_RMDIR		-17	/* rmdir(2) failed */
#define	RPMERR_PKGINSTALLED	-18	/* package already installed */
#define	RPMERR_CHOWN		-19	/* chown() call failed */
#define	RPMERR_NOUSER		-20	/* user does not exist */
#define	RPMERR_NOGROUP		-21	/* group does not exist */
#define	RPMERR_MKDIR		-22	/* mkdir() call failed */
#define	RPMERR_FILECONFLICT     -23     /* file being installed exists */
#define	RPMERR_RPMRC		-24     /* bad line in rpmrc */
#define	RPMERR_NOSPEC		-25     /* .spec file is missing */
#define	RPMERR_NOTSRPM		-26     /* a source rpm was expected */
#define	RPMERR_FLOCK		-27     /* locking the database failed */
#define	RPMERR_OLDPACKAGE	-28	/* trying upgrading to old version */
/*#define	RPMERR_BADARCH  -29        bad architecture or arch mismatch */
#define	RPMERR_CREATE		-30	/* failed to create a file */
#define	RPMERR_NOSPACE		-31	/* out of disk space */
#define	RPMERR_NORELOCATE	-32	/* tried to do improper relocatation */
/*#define	RPMERR_BADOS    -33        bad architecture or arch mismatch */
#define	RPMMESS_BACKUP          -34     /* backup made during [un]install */
#define	RPMERR_MTAB		-35	/* failed to read mount table */
#define	RPMERR_STAT		-36	/* failed to stat something */
#define	RPMERR_BADDEV		-37	/* file on device not listed in mtab */
#define	RPMMESS_ALTNAME         -38     /* file written as .rpmnew */
#define	RPMMESS_PREREQLOOP      -39     /* loop in prerequisites */
#define	RPMERR_BADRELOCATE      -40     /* bad relocation was specified */
#define	RPMERR_OLDDB      	-41     /* old format database */

/* spec.c build.c pack.c */
#define	RPMERR_UNMATCHEDIF      -107    /* unclosed %ifarch or %ifos */
#define	RPMERR_BADARG           -109
#define	RPMERR_SCRIPT           -110    /* errors related to script exec */
#define	RPMERR_READERROR        -111
#define	RPMERR_UNKNOWNOS        -112
#define	RPMERR_UNKNOWNARCH      -113
#define	RPMERR_EXEC             -114
#define	RPMERR_FORK             -115
#define	RPMERR_CPIO             -116
#define	RPMERR_GZIP             -117
#define	RPMERR_BADSPEC          -118
#define	RPMERR_LDD              -119    /* couldn't understand ldd output */
#define	RPMERR_BADFILENAME	-120

#define	RPMERR_BADSIGTYPE       -200    /* Unknown signature type */
#define	RPMERR_SIGGEN           -201    /* Error generating signature */

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

/**************************************************/
/*                                                */
/* verifySignature() results                      */
/*                                                */
/**************************************************/

/* verifySignature() results */
#define	RPMSIG_OK        0
#define	RPMSIG_UNKNOWN   1
#define	RPMSIG_BAD       2
#define	RPMSIG_NOKEY     3  /* Do not have the key to check this signature */

void rpmFreeSignature(Header h);

int rpmVerifySignature(const char *file, int_32 sigTag, void *sig, int count,
		       char *result);

int rpmGetFilesystemList(const char *** listptr, int * num);
int rpmGetFilesystemUsage(const char ** filelist, int_32 * fssizes, int numFiles,
			  uint_32 ** usagesPtr, int flags);

enum rpmQuerySources { QUERY_PACKAGE = 0, QUERY_PATH, QUERY_ALL, QUERY_RPM, 
		       QUERY_GROUP, QUERY_WHATPROVIDES, QUERY_WHATREQUIRES,
		       QUERY_DBOFFSET, QUERY_TRIGGEREDBY, QUERY_SPECFILE };

#define QUERY_FOR_LIST		(1 << 1)
#define QUERY_FOR_STATE		(1 << 2)
#define QUERY_FOR_DOCS		(1 << 3)
#define QUERY_FOR_CONFIG	(1 << 4)
#define QUERY_FOR_DUMPFILES     (1 << 8)

extern struct poptOption rpmQuerySourcePoptTable[];
extern struct poptOption rpmQueryPoptTable[];

struct rpmQueryArguments {
    int flags;
    enum rpmQuerySources source;
    int sourceCount;		/* > 1 is an error */
    char * queryFormat;
    int verbose;
};

int rpmQuery(const char * prefix, enum rpmQuerySources source, int queryFlags, 
	     const char * arg, const char * queryFormat);
void rpmDisplayQueryTags(FILE * f);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMLIB */
