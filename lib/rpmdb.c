#include <alloca.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dbindex.h"
#include "falloc.h"
#include "header.h"
#include "rpmerr.h"
#include "rpmlib.h"

struct rpmdb {
    faFile pkgs;
    dbIndex * nameIndex, * fileIndex, * groupIndex;
};

void removeIndexEntry(dbIndex * dbi, char * name, dbIndexRecord rec,
		     int tolerant, char * idxName);

int rpmdbOpen (char * prefix, rpmdb *rpmdbp, int mode, int perms) {
    char * filename;
    struct rpmdb db;
    
    filename = alloca(strlen(prefix) + 40);

    if (mode & O_WRONLY) 
	return 0;

    if (mode & O_RDWR)
	mode |= O_EXCL;

    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/packages.rpm");
    db.pkgs = faOpen(filename, mode, 0644);
    if (!db.pkgs) {
	error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	return 0;
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/nameindex.rpm");
    db.nameIndex = openDBIndex("/var/lib/rpm/nameindex.rpm", 
				 mode, 0644);
    if (!db.nameIndex) {
	faClose(db.pkgs);
	error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	return 0;
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/fileindex.rpm");
    db.fileIndex = openDBIndex("/var/lib/rpm/fileindex.rpm", 
				 mode, 0644);
    if (!db.fileIndex) {
	faClose(db.pkgs);
	closeDBIndex(db.nameIndex);
	error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	return 0;
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/groupindex.rpm");
    db.groupIndex = openDBIndex("/var/lib/rpm/groupindex.rpm", 
				 mode, 0644);
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

int rpmdbFindByFile(rpmdb db, char * filespec, dbIndexSet * matches) {
    return searchDBIndex(db->fileIndex, filespec, matches);
}

int rpmdbFindByGroup(rpmdb db, char * group, dbIndexSet * matches) {
    return searchDBIndex(db->groupIndex, group, matches);
}

int rpmdbFindPackage(rpmdb db, char * name, dbIndexSet * matches) {
    return searchDBIndex(db->nameIndex, name, matches);
}

void removeIndexEntry(dbIndex * dbi, char * key, dbIndexRecord rec,
		     int tolerant, char * idxName) {
    int rc;
    dbIndexSet matches;
    
    rc = searchDBIndex(dbi, key, &matches);
    switch (rc) {
      case 0:
	if (removeDBIndexRecord(&matches, rec) && !tolerant) {
	    error(RPMERR_DBCORRUPT, "package %s not listed in %s",
		  key, idxName);
	} else {
	    updateDBIndex(dbi, key, &matches);
	       /* errors from above will be reported from dbindex.c */
	}
      case 1:
	if (!tolerant) 
	    error(RPMERR_DBCORRUPT, "package %s not found in %s", key, idxName);
	break;
      case 2:
	break;   /* error message already generated from dbindex.c */
    }
}

int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant) {
    Header h;
    char * name, * group;
    int type;
    unsigned int count;
    dbIndexRecord rec;
    char ** fileList;
    int i;

    rec.recOffset = offset;
    rec.fileNumber = 0;

    h = rpmdbGetRecord(db, offset);
    if (!h) {
	error(RPMERR_DBCORRUPT, "cannot read header at %d for uninstall",
	      offset);
	return 0;
    }

    if (!getEntry(h, RPMTAG_NAME, &type, (void **) &name, &count)) {
	error(RPMERR_DBCORRUPT, "package has no name");
    } else {
	message(MESS_DEBUG, "removing name index\n");
	removeIndexEntry(db->nameIndex, name, rec, tolerant, "name index");
    }

    if (!getEntry(h, RPMTAG_GROUP, &type, (void **) &group, &count)) {
	message(MESS_DEBUG, "package has no group\n");
    } else {
	message(MESS_DEBUG, "removing group index\n");
	removeIndexEntry(db->groupIndex, group, rec, tolerant, "group index");
    }

    if (getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	 &count)) {
	for (i = 0; i < count; i++) {
	    message(MESS_DEBUG, "removing file index for %s\n", fileList[i]);
	    rec.fileNumber = i;
	    removeIndexEntry(db->fileIndex, fileList[i], rec, tolerant, 
			     "file index");
	}
    } else {
	message(MESS_DEBUG, "package has no files\n");
    }

    if (!faFree(db->pkgs, offset)) {
	printf("faFree failed!\n");
    }

    return 1;
}
