#ifndef _H_OLDRPMDB
#define _H_OLDRPMDB

/**
 * \file lib/oldrpmdb.h
 *
 */

#include <gdbm.h>

#include <oldheader.h>

typedef enum
  {
    RPMDB_NONE, RPMDB_GDBM_ERROR, RPMDB_NO_MEMORY
  }
rpm_error;

struct oldrpmdb
  {
    GDBM_FILE packages;
    GDBM_FILE nameIndex;
    GDBM_FILE pathIndex;
    GDBM_FILE groupIndex;
    GDBM_FILE iconIndex;
    GDBM_FILE postIndex;
    rpm_error rpmdbError;
    gdbm_error gdbmError;
  };

enum oldrpmdbFreeType
  {
    RPMDB_NOFREE, RPMDB_FREENAME, RPMDB_FREEALL
  };

struct oldrpmdbLabel
  {
    char *name, *version, *release;
    enum oldrpmdbFreeType freeType;
    struct oldrpmdbLabel *next;
    int fileNumber;		/* -1 means invalid */
  };

struct oldrpmdbPackageInfo
  {
    char *name, *version, *release;
    char *labelstr;
    unsigned int installTime, buildTime;
    unsigned int size;
    char *description;
    char *distribution;
    char *vendor;
    char *buildHost;
    char *preamble;
    char *copyright;
    unsigned int fileCount;
    struct oldrpmFileInfo *files;
  };

#define RPMDB_READER 1

#ifdef __cplusplus
extern "C" {
#endif

int oldrpmdbOpen (struct oldrpmdb *oldrpmdb);
void oldrpmdbClose (struct oldrpmdb *oldrpmdb);
struct oldrpmdbLabel *oldrpmdbGetAllLabels (struct oldrpmdb *oldrpmdb);
struct oldrpmdbLabel *oldrpmdbFindPackagesByFile (struct oldrpmdb *oldrpmdb, char *path);
struct oldrpmdbLabel *oldrpmdbFindPackagesByLabel (struct oldrpmdb *oldrpmdb,
						struct oldrpmdbLabel label);

char *oldrpmdbGetPackageGroup (struct oldrpmdb *oldrpmdb,
			       struct oldrpmdbLabel label);
char *oldrpmdbGetPackagePostun (struct oldrpmdb *oldrpmdb,
				struct oldrpmdbLabel label);
char *oldrpmdbGetPackagePreun (struct oldrpmdb *oldrpmdb,
				struct oldrpmdbLabel label);
char *oldrpmdbGetPackageGif (struct oldrpmdb *oldrpmdb, struct oldrpmdbLabel label,
			     int *size);
int oldrpmdbGetPackageInfo (struct oldrpmdb *oldrpmdb, struct oldrpmdbLabel label,
			    struct oldrpmdbPackageInfo *pinfo);
void oldrpmdbFreePackageInfo (struct oldrpmdbPackageInfo package);

struct oldrpmdbLabel oldrpmdbMakeLabel (char *name, char *version, char *release,
			    int fileNumber, enum oldrpmdbFreeType freeType);
void oldrpmdbFreeLabelList (struct oldrpmdbLabel *list);
void oldrpmdbFreeLabel (struct oldrpmdbLabel label);
int oldrpmdbWasError (struct oldrpmdb *oldrpmdb);

int oldrpmdbLabelstrToLabel (char *str, int length, struct oldrpmdbLabel *label);
char *oldrpmdbLabelToLabelstr (struct oldrpmdbLabel label, int withFileNum);
int oldrpmdbLabelCmp (struct oldrpmdbLabel *one, struct oldrpmdbLabel *two);

void oldrpmdbSetPrefix (char *new);

#ifdef __cplusplus
}
#endif

#endif	/* _H_OLDRPMDB */
