#ifndef H_RPMLIB
#define H_RPMLIB

/* This is the *only* module users of rpmlib should need to include */

#include <db.h>

/* it shouldn't need these :-( */
#include "dbindex.h"
#include "header.h"
#include "messages.h"

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

#define RPMFILE_STATE_NORMAL 		0
#define RPMFILE_STATE_REPLACED 		1

/* these can be ORed together */
#define RPMFILE_CONFIG			1
#define RPMFILE_DOC			2

#define INSTALL_REPLACEPKG	1
#define INSTALL_REPLACEFILES	2
#define INSTALL_PROGRESS	4

typedef struct rpmdb * rpmdb;

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

int rpmInstallPackage(char * prefix, rpmdb db, int fd, int test);
int rpmRemovePackage(char * prefix, rpmdb db, unsigned int offset, int test);
int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant);
int rpmdbAdd(rpmdb db, Header dbentry);

#endif
