#include "rpmpkg.h"
#include "rpmxdb.h"

struct rpmidxdb_s;
typedef struct rpmidxdb_s *rpmidxdb;

int rpmidxOpen(rpmidxdb *idxdbp, rpmpkgdb pkgdb, const char *filename, int flags, int mode);
int rpmidxOpenXdb(rpmidxdb *idxdbp, rpmpkgdb pkgdb, rpmxdb xdb, unsigned int xdbtag, int flags);
int rpmidxDelXdb(rpmpkgdb pkgdb, rpmxdb xdb, unsigned int xdbtag);
void rpmidxClose(rpmidxdb idxdbp);

int rpmidxGet(rpmidxdb idxdb, const unsigned char *key, unsigned int keyl, unsigned int **pkgidxlist, unsigned int *pkgidxnum);
int rpmidxPut(rpmidxdb idxdb, const unsigned char *key, unsigned int keyl, unsigned int pkgidx, unsigned int datidx);
int rpmidxDel(rpmidxdb idxdb, const unsigned char *key, unsigned int keyl, unsigned int pkgidx, unsigned int datidx);
int rpmidxList(rpmidxdb idxdb, unsigned int **keylistp, unsigned int *nkeylistp, unsigned char **datap);

int rpmidxStats(rpmidxdb idxdb);

