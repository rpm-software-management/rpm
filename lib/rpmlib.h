#ifndef H_RPMLIB
#define H_RPMLIB

/* This is the *only* module users of rpmlib should need to include */

#include <db.h>

/* it shouldn't need these :-( */
#include "dbindex.h"
#include "header.h"
#include "messages.h"

struct rpmTagTableEntry {
    char * name;
    int val;
};

int pkgReadHeader(int fd, Header * hdr, int * isSource, int * major,
		  int * minor);
   /* 0 = success */
   /* 1 = bad magic */
   /* 2 = error */

extern const struct rpmTagTableEntry rpmTagTable[];
extern const int rpmTagTableSize;

/* these tags are for both the database and packages */
/* none of these can be 0 !!                         */

#define RPMTAG_NAME  			1000
#define RPMTAG_VERSION			1001
#define RPMTAG_RELEASE			1002
#define RPMTAG_SERIAL   		1003
#define	RPMTAG_SUMMARY			1004
#define RPMTAG_DESCRIPTION		1005
#define RPMTAG_BUILDTIME		1006
#define RPMTAG_BUILDHOST		1007
#define RPMTAG_INSTALLTIME		1008
#define RPMTAG_SIZE			1009
#define RPMTAG_DISTRIBUTION		1010
#define RPMTAG_VENDOR			1011
#define RPMTAG_GIF			1012
#define RPMTAG_XPM			1013
#define RPMTAG_COPYRIGHT                1014
#define RPMTAG_PACKAGER                 1015
#define RPMTAG_GROUP                    1016
#define RPMTAG_CHANGELOG                1017
#define RPMTAG_SOURCE                   1018
#define RPMTAG_PATCH                    1019
#define RPMTAG_URL                      1020
#define RPMTAG_OS                       1021
#define RPMTAG_ARCH                     1022
#define RPMTAG_PREIN                    1023
#define RPMTAG_POSTIN                   1024
#define RPMTAG_PREUN                    1025
#define RPMTAG_POSTUN                   1026
#define RPMTAG_FILENAMES		1027
#define RPMTAG_FILESIZES		1028
#define RPMTAG_FILESTATES		1029
#define RPMTAG_FILEMODES		1030
#define RPMTAG_FILEUIDS			1031
#define RPMTAG_FILEGIDS			1032
#define RPMTAG_FILERDEVS		1033
#define RPMTAG_FILEMTIMES		1034
#define RPMTAG_FILEMD5S			1035
#define RPMTAG_FILELINKTOS		1036
#define RPMTAG_FILEFLAGS		1037
#define RPMTAG_ROOT                     1038
#define RPMTAG_FILEUSERNAME             1039
#define RPMTAG_FILEGROUPNAME            1040
#define RPMTAG_EXCLUDE                  1041 /* not used */
#define RPMTAG_EXCLUSIVE                1042 /* not used */
#define RPMTAG_ICON                     1043
#define RPMTAG_SOURCERPM                1044
#define RPMTAG_FILEVERIFYFLAGS          1045
#define RPMTAG_ARCHIVESIZE              1046
#define RPMTAG_PROVIDES                 1047
#define RPMTAG_REQUIREFLAGS             1048
#define RPMTAG_REQUIRENAME              1049
#define RPMTAG_REQUIREVERSION           1050
#define RPMTAG_NOSOURCE                 1051
#define RPMTAG_NOPATCH                  1052
#define RPMTAG_CONFLICTFLAGS            1053
#define RPMTAG_CONFLICTNAME             1054
#define RPMTAG_CONFLICTVERSION          1055
#define RPMTAG_DEFAULTPREFIX            1056
#define RPMTAG_BUILDROOT                1057
#define RPMTAG_INSTALLPREFIX		1058
#define RPMTAG_EXCLUDEARCH              1059
#define RPMTAG_EXCLUDEOS                1060
#define RPMTAG_EXCLUSIVEARCH            1061
#define RPMTAG_EXCLUSIVEOS              1062
#define RPMTAG_AUTOREQPROV              1063 /* used internally by builds */
#define RPMTAG_RPMVERSION		1064
#define RPMTAG_TRIGGERSCRIPTS           1065
#define RPMTAG_TRIGGERNAME              1066
#define RPMTAG_TRIGGERVERSION           1067
#define RPMTAG_TRIGGERFLAGS             1068
#define RPMTAG_TRIGGERINDEX             1069

#define RPMFILE_STATE_NORMAL 		0
#define RPMFILE_STATE_REPLACED 		1
#define RPMFILE_STATE_NOTINSTALLED	2
#define RPMFILE_STATE_NETSHARED		3

/* these can be ORed together */
#define RPMFILE_CONFIG			1
#define RPMFILE_DOC			2

#define INSTALL_REPLACEPKG	(1 << 0)
#define INSTALL_REPLACEFILES	(1 << 1)
#define INSTALL_TEST		(1 << 2)
#define INSTALL_UPGRADE		(1 << 3)
#define INSTALL_UPGRADETOOLD	(1 << 4)
#define INSTALL_NODOCS		(1 << 5)
#define INSTALL_NOSCRIPTS	(1 << 6)
#define INSTALL_NOARCH		(1 << 7)
#define INSTALL_NOOS		(1 << 8)

#define UNINSTALL_TEST          (1 << 0)
#define UNINSTALL_NOSCRIPTS	(1 << 1)

#define VERIFY_NONE             0
#define VERIFY_MD5              (1 << 0)
#define VERIFY_FILESIZE         (1 << 1)
#define VERIFY_LINKTO           (1 << 2)
#define VERIFY_USER             (1 << 3)
#define VERIFY_GROUP            (1 << 4)
#define VERIFY_MTIME            (1 << 5)
#define VERIFY_MODE             (1 << 6)
#define VERIFY_RDEV             (1 << 7)
#define VERIFY_ALL              ~(VERIFY_NONE)

#define REQUIRE_ANY             0
#define REQUIRE_SERIAL          (1 << 0)
#define REQUIRE_LESS            (1 << 1)
#define REQUIRE_GREATER         (1 << 2)
#define REQUIRE_EQUAL           (1 << 3)
#define REQUIRE_PROVIDES        (1 << 4)   /* only used internally by builds */
#define REQUIRE_CONFLICTS       (1 << 5)   /* only used internally by builds */
#define REQUIRE_SENSEMASK       15         /* Mask to get senses */

#define TRIGGER_ON              (1 << 16)
#define TROGGER_OFF             (1 << 17)

/* Stuff for maintaining "variables" like SOURCEDIR, BUILDDIR, etc */

#define RPMVAR_SOURCEDIR     		0
#define RPMVAR_BUILDDIR      		1
/* #define RPMVAR_DOCDIR        	2 -- No longer used */
#define RPMVAR_OPTFLAGS      		3
#define RPMVAR_TOPDIR        		4
#define RPMVAR_SPECDIR       		5
#define RPMVAR_ROOT          		6
#define RPMVAR_RPMDIR        		7
#define RPMVAR_SRPMDIR       		8
/* #define RPMVAR_ARCHSENSITIVE 	9  -- No longer used */
#define RPMVAR_REQUIREDISTRIBUTION	10
/* #define RPMVAR_REQUIREGROUP		11 -- No longer used */
#define RPMVAR_REQUIREVENDOR		12
#define RPMVAR_DISTRIBUTION		13
#define RPMVAR_VENDOR			14
#define RPMVAR_MESSAGELEVEL		15
#define RPMVAR_REQUIREICON		16
#define RPMVAR_TIMECHECK		17
#define RPMVAR_SIGTYPE                  18
#define RPMVAR_PGP_PATH                 19
#define RPMVAR_PGP_NAME                 20
/* #define RPMVAR_PGP_SECRING           21 -- No longer used */
/* #define RPMVAR_PGP_PUBRING           22 -- No longer used */
#define RPMVAR_EXCLUDEDOCS              23
/* #define RPMVAR_BUILDARCH             24 -- No longer used */
/* #define RPMVAR_BUILDOS               25 -- No longer used */
#define RPMVAR_BUILDROOT                26
#define RPMVAR_DBPATH                   27
#define RPMVAR_PACKAGER                 28
#define RPMVAR_FTPPROXY                 29
#define RPMVAR_TMPPATH                  30
#define RPMVAR_CPIOBIN                  31
#define RPMVAR_FTPPORT			32
#define RPMVAR_NETSHAREDPATH		33
#define RPMVAR_DEFAULTDOCDIR		34
#define RPMVAR_LASTVAR	                35 /* IMPORTANT to keep right! */

char *getVar(int var);
int getBooleanVar(int var);
void setVar(int var, char *val);

/* rpmrc.c */
int rpmReadConfigFiles(char * file, char * arch, char * os, int building);
int getOsNum(void);
int getArchNum(void);
char *getOsName(void);
char *getArchName(void);
int rpmShowRC(FILE *f);

typedef struct rpmdb * rpmdb;

typedef void (*notifyFunction)(const unsigned long amount,
			       const unsigned long total);

int rpmdbOpen (char * prefix, rpmdb * dbp, int mode, int perms);
    /* 0 on error */
int rpmdbInit(char * prefix, int perms);
    /* nonzero on error */
void rpmdbClose (rpmdb db);

unsigned int rpmdbFirstRecNum(rpmdb db);
unsigned int rpmdbNextRecNum(rpmdb db, unsigned int lastOffset);  
    /* 0 at end */

Header rpmdbGetRecord(rpmdb db, unsigned int offset);
int rpmdbFindByFile(rpmdb db, char * filespec, dbIndexSet * matches);
int rpmdbFindByGroup(rpmdb db, char * group, dbIndexSet * matches);
int rpmdbFindPackage(rpmdb db, char * name, dbIndexSet * matches);
int rpmdbFindByProvides(rpmdb db, char * filespec, dbIndexSet * matches);
int rpmdbFindByRequiredBy(rpmdb db, char * filespec, dbIndexSet * matches);
int rpmdbFindByConflicts(rpmdb db, char * filespec, dbIndexSet * matches);

int rpmArchScore(char * arch);
int rpmOsScore(char * arch);
int rpmInstallSourcePackage(char * prefix, int fd, char ** specFile,
			    notifyFunction notify, char * labelFormat);
int rpmInstallPackage(char * rootdir, rpmdb db, int fd, char * prefix, 
		      int flags, notifyFunction notify, char * labelFormat,
		      char * netsharedPath);
int rpmEnsureOlder(rpmdb db, char * name, char * newVersion, 
		char * newRelease, int dbOffset);
int rpmRemovePackage(char * prefix, rpmdb db, unsigned int offset, int test);
int rpmVerifyFile(char * prefix, Header h, int filenum, int * result);
int rpmdbRebuild(char * prefix);

typedef struct rpmDependencyCheck * rpmDependencies;

struct rpmDependencyConflict {
    char * byName, * byVersion, * byRelease;
    /* these needs fields are misnamed -- they are used for the package
       which isn't needed as well */
    char * needsName, * needsVersion;
    int needsFlags;
    void * suggestedPackage;			/* NULL if none */
    enum { RPMDEP_SENSE_REQUIRES, RPMDEP_SENSE_CONFLICTS } sense;
} ;

rpmDependencies rpmdepDependencies(rpmdb db); 	       /* db may be NULL */
void rpmdepAddPackage(rpmDependencies rpmdep, Header h);
void rpmdepAvailablePackage(rpmDependencies rpmdep, Header h, void * key);
void rpmdepUpgradePackage(rpmDependencies rpmdep, Header h);
void rpmdepRemovePackage(rpmDependencies rpmdep, int dboffset);
int rpmdepCheck(rpmDependencies rpmdep, 
		struct rpmDependencyConflict ** conflicts, int * numConflicts);
void rpmdepDone(rpmDependencies rpmdep);
void rpmdepFreeConflicts(struct rpmDependencyConflict * conflicts, int
			 numConflicts);

int mdfile(char *fn, unsigned char *digest);

#endif
