#include <alloca.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "falloc.h"
#include "rpmerr.h"
#include "rpmlib.h"

struct rpmdb {
    faFile pkgs;
    dbIndex * nameIndex, * fileIndex, * groupIndex;
};

int rpmdbOpen (char * prefix, rpmdb *rpmdbp, int mode, int perms) {
    char * filename;
    struct rpmdb db;
    
    filename = alloca(strlen(prefix) + 40);

    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/packages.rpm");
    db.pkgs = faOpen(filename, O_RDWR | O_CREAT, 0644);
    if (!db.pkgs) {
	error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	return 0;
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/nameindex.rpm");
    db.nameIndex = openDBIndex("/var/lib/rpm/nameindex.rpm", 
				 O_RDWR | O_CREAT, 0644);
    if (!db.nameIndex) {
	faClose(db.pkgs);
	error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	return 0;
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/fileindex.rpm");
    db.fileIndex = openDBIndex("/var/lib/rpm/fileindex.rpm", 
				 O_RDWR | O_CREAT, 0644);
    if (!db.fileIndex) {
	faClose(db.pkgs);
	closeDBIndex(db.nameIndex);
	error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	return 0;
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/groupindex.rpm");
    db.groupIndex = openDBIndex("/var/lib/rpm/groupindex.rpm", 
				 O_RDWR | O_CREAT, 0644);
    if (!db.groupIndex) {
	faClose(db.pkgs);
	closeDBIndex(db.nameIndex);
	closeDBIndex(db.fileIndex);
	error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	return 0;
    }

    *rpmdbp = malloc(sizeof(struct rpmdb));
    **rpmdbp = db;

    return 1;
}

int rpmdbCreate (rpmdb db, int mode, int perms);
    /* this fails if any part of the db already exists */

void rpmdbClose (rpmdb db) {
    faClose(db->pkgs);
    closeDBIndex(db->fileIndex);
    closeDBIndex(db->groupIndex);
    closeDBIndex(db->nameIndex);
    free(db);
}

unsigned int rpmdbFirstRecNum(rpmdb db) {
    return faFirstOffset(db->pkgs);
}

unsigned int rpmdbNextRecNum(rpmdb db, unsigned int lastOffset) {
    /* 0 at end */
    return faNextOffset(db->pkgs, lastOffset);
}

Header rpmdbGetRecord(rpmdb db, unsigned int offset) {
    lseek(db->pkgs->fd, offset, SEEK_SET);

    return readHeader(db->pkgs->fd);
}

int rpmdbFindByFile(rpmdb db, char * filespec, 
		    dbIndexSet * matches) {
    return searchDBIndex(db->fileIndex, filespec, matches);
}
