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

int pkgReadHeader(int fd, Header * hdr, int * isSource);
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
#define RPMTAG_EXCLUDE                  1041
#define RPMTAG_EXCLUSIVE                1042
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

#define RPMFILE_STATE_NORMAL 		0
#define RPMFILE_STATE_REPLACED 		1
#define RPMFILE_STATE_NOTINSTALLED	2

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

/* Stuff for maintaining "variables" like SOURCEDIR, BUILDDIR, etc */

#define RPMVAR_SOURCEDIR     		0
#define RPMVAR_BUILDDIR      		1
#define RPMVAR_DOCDIR        		2
#define RPMVAR_OPTFLAGS      		3
#define RPMVAR_TOPDIR        		4
#define RPMVAR_SPECDIR       		5
#define RPMVAR_ROOT          		6
#define RPMVAR_RPMDIR        		7
#define RPMVAR_SRPMDIR       		8
#define RPMVAR_ARCHSENSITIVE 		9
#define RPMVAR_REQUIREDISTRIBUTION	10
#define RPMVAR_REQUIREGROUP		11
#define RPMVAR_REQUIREVENDOR		12
#define RPMVAR_DISTRIBUTION		13
#define RPMVAR_VENDOR			14
#define RPMVAR_MESSAGELEVEL		15
#define RPMVAR_REQUIREICON		16
#define RPMVAR_TIMECHECK		17
#define RPMVAR_SIGTYPE                  18
#define RPMVAR_PGP_PATH                 19
#define RPMVAR_PGP_NAME                 20
#define RPMVAR_PGP_SECRING              21
#define RPMVAR_PGP_PUBRING              22
#define RPMVAR_EXCLUDEDOCS              23
#define RPMVAR_LASTVAR	                24 /* IMPORTANT to keep right! */

char *getVar(int var);
int getBooleanVar(int var);
void setVar(int var, char *val);

int readConfigFiles(char * fn);

typedef struct rpmdb * rpmdb;

typedef void (*notifyFunction)(const unsigned long amount,
			       const unsigned long total);

int rpmdbOpen (char * prefix, rpmdb * dbp, int mode, int perms);
    /* 0 on error */
int rpmdbCreate (rpmdb  db, int mode, int perms);
    /* this fails if any part of the db already exists */
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

int rpmInstallSourcePackage(char * prefix, int fd, char ** specFile);
int rpmInstallPackage(char * prefix, rpmdb db, int fd, int flags, 
		      notifyFunction notify, char * labelFormat);
int rpmRemovePackage(char * prefix, rpmdb db, unsigned int offset, 
		     int upgrade, int test);
int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant);
int rpmdbAdd(rpmdb db, Header dbentry);
int rpmdbUpdateRecord(rpmdb db, int secOffset, Header secHeader);
int rpmVerifyFile(char * prefix, Header h, int filenum, int * result);

typedef struct rpmDependencyCheck * rpmDependencies;

struct rpmDependencyConflict {
    char * byName, * byVersion, * byRelease;
    char * needsName, * needsVersion;
    int needsFlags;
} ;

rpmDependencies rpmdepDependencies(rpmdb db); 	       /* db may be NULL */
void rpmdepAddPackage(rpmDependencies rpmdep, Header h);
void rpmdepRemovePackage(rpmDependencies rpmdep, int dboffset);
int rpmdepCheck(rpmDependencies rpmdep, 
		struct rpmDependencyConflict ** conflicts, int * numConflicts);
void rpmdepDone(rpmDependencies rpmdep);

int mdfile(char *fn, unsigned char *digest);

#endif
