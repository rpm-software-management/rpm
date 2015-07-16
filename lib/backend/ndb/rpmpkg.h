struct rpmpkgdb_s;
typedef struct rpmpkgdb_s *rpmpkgdb;

int rpmpkgOpen(rpmpkgdb *pkgdbp, const char *filename, int flags, int mode);
void rpmpkgClose(rpmpkgdb pkgdbp);
void rpmpkgSetFsync(rpmpkgdb pkgdbp, int dofsync);

int rpmpkgLock(rpmpkgdb pkgdb, int excl);
int rpmpkgUnlock(rpmpkgdb pkgdb, int excl);

int rpmpkgGet(rpmpkgdb pkgdb, unsigned int pkgidx, unsigned char **blobp, unsigned int *bloblp);
int rpmpkgPut(rpmpkgdb pkgdb, unsigned int pkgidx, unsigned char *blob, unsigned int blobl);
int rpmpkgDel(rpmpkgdb pkgdb, unsigned int pkgidx);
int rpmpkgList(rpmpkgdb pkgdb, unsigned int **pkgidxlistp, unsigned int *npkgidxlistp);

int rpmpkgNextPkgIdx(rpmpkgdb pkgdb, unsigned int *pkgidxp);
int rpmpkgGeneration(rpmpkgdb pkgdb, unsigned int *generationp);

int rpmpkgStats(rpmpkgdb pkgdb);

