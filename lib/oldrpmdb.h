#ifndef _H_RPMDB
#define _H_RPMDB

#include <gdbm.h>

#include "oldrpmfile.h"

typedef enum { RPMDB_NONE, RPMDB_GDBM_ERROR, RPMDB_NO_MEMORY } rpm_error;

struct rpmdb {
    GDBM_FILE packages;
    GDBM_FILE nameIndex;
    GDBM_FILE pathIndex;
    GDBM_FILE groupIndex;
    GDBM_FILE iconIndex;
    GDBM_FILE postIndex;
    rpm_error rpmdbError;
    gdbm_error gdbmError;
};

enum rpmdbFreeType { RPMDB_NOFREE, RPMDB_FREENAME, RPMDB_FREEALL } ;

struct rpmdbLabel {
    char * name, * version, * release;
    enum rpmdbFreeType freeType;
    struct rpmdbLabel * next;
    int fileNumber;                     /* -1 means invalid */
};

struct rpmdbPackageInfo {
    char * name, * version, * release;
    char * labelstr;
    unsigned int installTime, buildTime;
    unsigned int size;
    char * description;
    char * distribution;
    char * vendor;
    char * buildHost;
    char * preamble;
    char * copyright;
    unsigned int fileCount;
    struct rpmFileInfo * files;
} ;

#define RPMDB_READER 1

int rpmdbOpen(struct rpmdb * rpmdb);
void rpmdbClose(struct rpmdb * rpmdb);
struct rpmdbLabel * rpmdbGetAllLabels(struct rpmdb * rpmdb);
struct rpmdbLabel * rpmdbFindPackagesByFile(struct rpmdb * rpmdb, char * path);
struct rpmdbLabel * rpmdbFindPackagesByLabel(struct rpmdb * rpmdb, 
					     struct rpmdbLabel label);

char * rpmdbGetPackageGroup(struct rpmdb * rpmdb, struct rpmdbLabel label);
char * rpmdbGetPackageGif(struct rpmdb * rpmdb, struct rpmdbLabel label,
			  int * size);
int rpmdbGetPackageInfo(struct rpmdb * rpmdb, struct rpmdbLabel label,
			struct rpmdbPackageInfo * pinfo);
void rpmdbFreePackageInfo(struct rpmdbPackageInfo package);

struct rpmdbLabel rpmdbMakeLabel(char * name, char * version, char * release,
				 int fileNumber, enum rpmdbFreeType freeType);
void rpmdbFreeLabelList(struct rpmdbLabel * list);
void rpmdbFreeLabel(struct rpmdbLabel label);
int rpmdbWasError(struct rpmdb * rpmdb);

int rpmdbLabelstrToLabel(char * str, int length, struct rpmdbLabel * label);
char * rpmdbLabelToLabelstr(struct rpmdbLabel label, int withFileNum);
int rpmdbLabelCmp(struct rpmdbLabel * one, struct rpmdbLabel * two);

void rpmdbSetPrefix(char * new);

#endif
