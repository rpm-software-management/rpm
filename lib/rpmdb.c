#include <alloca.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
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
int addIndexEntry(dbIndex * idx, char * index, unsigned int offset,
		  unsigned int fileNumber);

int rpmdbOpen (char * prefix, rpmdb *rpmdbp, int mode, int perms) {
    char * filename;
    struct rpmdb db;
    
    filename = alloca(strlen(prefix) + 40);

    if (mode & O_WRONLY) 
	return 1;

    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/packages.rpm");
    db.pkgs = faOpen(filename, mode, 0644);
    if (!db.pkgs) {
	error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	return 1;
    }

    /* try and get a lock - this is released by the kernel when we close
       the file */
    if (mode & O_RDWR) {
        if (flock(db.pkgs->fd, LOCK_EX | LOCK_NB)) {
	    error(RPMERR_FLOCK, "cannot get %s lock on database", "exclusive");
	    return 1;
	} 
    } else {
        if (flock(db.pkgs->fd, LOCK_SH | LOCK_NB)) {
	    error(RPMERR_FLOCK, "cannot get %s lock on database", "shared");
	    return 1;
	} 
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/nameindex.rpm");
    db.nameIndex = openDBIndex(filename, mode, 0644);
    if (!db.nameIndex) {
	faClose(db.pkgs);
	return 1;
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/fileindex.rpm");
    db.fileIndex = openDBIndex(filename, mode, 0644);
    if (!db.fileIndex) {
	faClose(db.pkgs);
	closeDBIndex(db.nameIndex);
	return 1;
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/groupindex.rpm");
    db.groupIndex = openDBIndex(filename, mode, 0644);
    if (!db.groupIndex) {
	faClose(db.pkgs);
	closeDBIndex(db.nameIndex);
	return 1;
    }

    *rpmdbp = malloc(sizeof(struct rpmdb));
    **rpmdbp = db;

    return 0;
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
	break;
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
	return 1;
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

    faFree(db->pkgs, offset);

    return 0;
}

int addIndexEntry(dbIndex * idx, char * index, unsigned int offset,
		  unsigned int fileNumber) {
    dbIndexSet set;
    dbIndexRecord irec;   
    int rc;

    irec.recOffset = offset;
    irec.fileNumber = fileNumber;

    rc = searchDBIndex(idx, index, &set);
    if (rc == -1)  		/* error */
	return 1;

    if (rc == 1)  		/* new item */
	set = createDBIndexRecord();
    appendDBIndexRecord(&set, irec);
    if (updateDBIndex(idx, index, &set))
	exit(1);
    freeDBIndexRecord(set);
    return 0;
}

int rpmdbAdd(rpmdb db, Header dbentry) {
    unsigned int dboffset;
    unsigned int i;
    char ** fileList;
    char * name, * group;
    int count;
    int type;

    getEntry(dbentry, RPMTAG_NAME, &type, (void **) &name, &count);
    getEntry(dbentry, RPMTAG_GROUP, &type, (void **) &group, &count);

    if (!getEntry(dbentry, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	 &count)) {
	count = 0;
    } 
   
    dboffset = faAlloc(db->pkgs, sizeofHeader(dbentry));
    if (!dboffset) {
	error(RPMERR_DBCORRUPT, "cannot allocate space for database");
	return 1;
    }
    lseek(db->pkgs->fd, dboffset, SEEK_SET);

    writeHeader(db->pkgs->fd, dbentry);

    /* Now update the appropriate indexes */
    if (addIndexEntry(db->nameIndex, name, dboffset, 0))
	return 1;
    if (addIndexEntry(db->groupIndex, group, dboffset, 0))
	return 1;

    for (i = 0; i < count; i++) {
	if (addIndexEntry(db->fileIndex, fileList[i], dboffset, i))
	    return 1;
    }

    if (count) free(fileList);

    return 0;
}

int rpmdbUpdateRecord(rpmdb db, int offset, Header newHeader) {
    Header oldHeader;

    oldHeader = rpmdbGetRecord(db, offset);
    if (!oldHeader) {
	error(RPMERR_DBCORRUPT, "cannot read header at %d for update",
		offset);
	return 1;
    }

    if (sizeofHeader(oldHeader) != sizeofHeader(newHeader)) {
	message(MESS_DEBUG, "header changed size!");
	if (rpmdbRemove(db, offset, 1))
	    return 1;

	if (rpmdbAdd(db, newHeader)) 
	    return 1;
    } else {
	lseek(db->pkgs->fd, offset, SEEK_SET);

	writeHeader(db->pkgs->fd, newHeader);
    }

    return 0;
}
