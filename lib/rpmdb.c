#include <alloca.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>
#include <unistd.h>

#include "dbindex.h"
#include "falloc.h"
#include "header.h"
#include "misc.h"
#include "rpmerr.h"
#include "rpmlib.h"

/* XXX the signal handling in here is not thread safe */

/* the requiredbyIndex isn't stricly necessary. In a perfect world, we could
   have each header keep a list of packages that need it. However, we
   can't reserve space in the header for extra information so all of the
   required packages would move in the database every time a package was
   added or removed. Instead, each package (or virtual package) name
   keeps a list of package offsets of packages that might depend on this
   one. Version numbers still need verification, but it gets us in the
   right area w/o a linear search through the database. */

struct rpmdb {
    faFile pkgs;
    dbIndex * nameIndex, * fileIndex, * groupIndex, * providesIndex;
    dbIndex * requiredbyIndex;
};

static void removeIndexEntry(dbIndex * dbi, char * name, dbIndexRecord rec,
		             int tolerant, char * idxName);
static int addIndexEntry(dbIndex * idx, char * index, unsigned int offset,
		         unsigned int fileNumber);
static void blockSignals(void);
static void unblockSignals(void);
static int doopen (char * prefix, rpmdb *rpmdbp, int mode, int perms,
	           int justcheck);

static sigset_t signalMask;

int rpmdbOpen (char * prefix, rpmdb *rpmdbp, int mode, int perms) {
    return doopen(prefix, rpmdbp, mode, perms, 0);
}

int rpmdbInit (char * prefix, int perms) {
    rpmdb db;

    return doopen(prefix, &db, O_CREAT | O_RDWR, perms, 1);
}

static int doopen (char * prefix, rpmdb *rpmdbp, int mode, int perms, 
	       int justcheck) {
    char * filename;
    struct rpmdb db;
    
    filename = alloca(strlen(prefix) + 40);

    if (mode & O_WRONLY) 
	return 1;

    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/packages.rpm");

    memset(&db, 0, sizeof(db));

    if (!justcheck || !exists(filename)) {
	db.pkgs = faOpen(filename, mode, 0644);
	if (!db.pkgs) {
	    error(RPMERR_DBOPEN, "failed to open %s\n", filename);
	    return 1;
	}

	/* try and get a lock - this is released by the kernel when we close
	   the file */
	if (mode & O_RDWR) {
	    if (flock(db.pkgs->fd, LOCK_EX | LOCK_NB)) {
		error(RPMERR_FLOCK, "cannot get %s lock on database", 
			"exclusive");
		return 1;
	    } 
	} else {
	    if (flock(db.pkgs->fd, LOCK_SH | LOCK_NB)) {
		error(RPMERR_FLOCK, "cannot get %s lock on database", "shared");
		return 1;
	    } 
	}
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/nameindex.rpm");

    if (!justcheck || !exists(filename)) {
	db.nameIndex = openDBIndex(filename, mode, 0644);
	if (!db.nameIndex) {
	    faClose(db.pkgs);
	    return 1;
	}
    }

    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/fileindex.rpm");

    if (!justcheck || !exists(filename)) {
	db.fileIndex = openDBIndex(filename, mode, 0644);
	if (!db.fileIndex) {
	    faClose(db.pkgs);
	    closeDBIndex(db.nameIndex);
	    return 1;
	}
    }
    
    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/groupindex.rpm");

    if (!justcheck || !exists(filename)) {
	db.groupIndex = openDBIndex(filename, mode, 0644);
	if (!db.groupIndex) {
	    faClose(db.pkgs);
	    closeDBIndex(db.nameIndex);
	    closeDBIndex(db.fileIndex);
	    return 1;
	}
    }

    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/providesindex.rpm");

    if (!justcheck || !exists(filename)) {
	db.providesIndex = openDBIndex(filename, mode, 0644);
	if (!db.providesIndex) {
	    faClose(db.pkgs);
	    closeDBIndex(db.fileIndex);
	    closeDBIndex(db.nameIndex);
	    closeDBIndex(db.groupIndex);
	    return 1;
	}
    }

    strcpy(filename, prefix); 
    strcat(filename, "/var/lib/rpm/requiredby.rpm");

    if (!justcheck || !exists(filename)) {
	db.requiredbyIndex = openDBIndex(filename, mode, 0644);
	if (!db.requiredbyIndex) {
	    faClose(db.pkgs);
	    closeDBIndex(db.fileIndex);
	    closeDBIndex(db.nameIndex);
	    closeDBIndex(db.groupIndex);
	    closeDBIndex(db.providesIndex);
	    return 1;
	}
    }

    *rpmdbp = malloc(sizeof(struct rpmdb));
    **rpmdbp = db;

    if (justcheck) {
	rpmdbClose(*rpmdbp);
    }

    return 0;
}

void rpmdbClose (rpmdb db) {
    if (db->pkgs) faClose(db->pkgs);
    if (db->fileIndex) closeDBIndex(db->fileIndex);
    if (db->groupIndex) closeDBIndex(db->groupIndex);
    if (db->nameIndex) closeDBIndex(db->nameIndex);
    if (db->providesIndex) closeDBIndex(db->providesIndex);
    if (db->requiredbyIndex) closeDBIndex(db->requiredbyIndex);
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

int rpmdbFindByProvides(rpmdb db, char * filespec, dbIndexSet * matches) {
    return searchDBIndex(db->providesIndex, filespec, matches);
}

int rpmdbFindByRequiredBy(rpmdb db, char * filespec, dbIndexSet * matches) {
    return searchDBIndex(db->requiredbyIndex, filespec, matches);
}

int rpmdbFindByGroup(rpmdb db, char * group, dbIndexSet * matches) {
    return searchDBIndex(db->groupIndex, group, matches);
}

int rpmdbFindPackage(rpmdb db, char * name, dbIndexSet * matches) {
    return searchDBIndex(db->nameIndex, name, matches);
}

static void removeIndexEntry(dbIndex * dbi, char * key, dbIndexRecord rec,
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
    char ** fileList, ** providesList, ** requiredbyList;
    int i;

    rec.recOffset = offset;
    rec.fileNumber = 0;

    h = rpmdbGetRecord(db, offset);
    if (!h) {
	error(RPMERR_DBCORRUPT, "cannot read header at %d for uninstall",
	      offset);
	return 1;
    }

    blockSignals();

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

    if (getEntry(h, RPMTAG_PROVIDES, &type, (void **) &providesList, 
	 &count)) {
	for (i = 0; i < count; i++) {
	    message(MESS_DEBUG, "removing provides index for %s\n", 
		    providesList[i]);
	    removeIndexEntry(db->providesIndex, providesList[i], rec, tolerant, 
			     "providesfile index");
	}
	free(providesList);
    }

    if (getEntry(h, RPMTAG_REQUIRENAME, &type, (void **) &requiredbyList, 
	 &count)) {
	for (i = 0; i < count; i++) {
	    message(MESS_DEBUG, "removing requiredby index for %s\n", 
		    requiredbyList[i]);
	    removeIndexEntry(db->requiredbyIndex, requiredbyList[i], rec, 
			     tolerant, "requiredby index");
	}
	free(requiredbyList);
    }

    if (getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	 &count)) {
	for (i = 0; i < count; i++) {
	    message(MESS_DEBUG, "removing file index for %s\n", fileList[i]);
	    rec.fileNumber = i;
	    removeIndexEntry(db->fileIndex, fileList[i], rec, tolerant, 
			     "file index");
	}
	free(fileList);
    } else {
	message(MESS_DEBUG, "package has no files\n");
    }

    faFree(db->pkgs, offset);

    syncDBIndex(db->nameIndex);
    syncDBIndex(db->groupIndex);
    syncDBIndex(db->fileIndex);

    unblockSignals();

    return 0;
}

static int addIndexEntry(dbIndex * idx, char * index, unsigned int offset,
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
    char ** providesList;
    char ** requiredbyList;
    char * name, * group;
    int count, providesCount, requiredbyCount;
    int type;
    int rc = 0;

    getEntry(dbentry, RPMTAG_NAME, &type, (void **) &name, &count);
    getEntry(dbentry, RPMTAG_GROUP, &type, (void **) &group, &count);

    if (!group) group = "Unknown";

    if (!getEntry(dbentry, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	 &count)) {
	count = 0;
    } 

    if (!getEntry(dbentry, RPMTAG_PROVIDES, &type, (void **) &providesList, 
	 &providesCount)) {
	providesCount = 0;
    } 

    if (!getEntry(dbentry, RPMTAG_REQUIRENAME, &type, 
		  (void **) &requiredbyList, &requiredbyCount)) {
	requiredbyCount = 0;
    } 

    blockSignals();

    dboffset = faAlloc(db->pkgs, sizeofHeader(dbentry));
    if (!dboffset) {
	error(RPMERR_DBCORRUPT, "cannot allocate space for database");
	unblockSignals();
	if (providesCount) free(providesList);
	if (requiredbyCount) free(requiredbyList);
	if (count) free(fileList);
	return 1;
    }
    lseek(db->pkgs->fd, dboffset, SEEK_SET);

    writeHeader(db->pkgs->fd, dbentry);

    /* Now update the appropriate indexes */
    if (addIndexEntry(db->nameIndex, name, dboffset, 0))
	rc = 1;
    if (addIndexEntry(db->groupIndex, group, dboffset, 0))
	rc = 1;

    for (i = 0; i < requiredbyCount; i++) {
	if (addIndexEntry(db->requiredbyIndex, requiredbyList[i], dboffset, 0))
	    rc = 1;
    }

    for (i = 0; i < providesCount; i++) {
	if (addIndexEntry(db->providesIndex, providesList[i], dboffset, 0))
	    rc = 1;
    }

    for (i = 0; i < count; i++) {
	if (addIndexEntry(db->fileIndex, fileList[i], dboffset, i))
	    rc = 1;
    }

    syncDBIndex(db->nameIndex);
    syncDBIndex(db->groupIndex);
    syncDBIndex(db->fileIndex);
    syncDBIndex(db->providesIndex);
    syncDBIndex(db->requiredbyIndex);

    unblockSignals();

    if (requiredbyCount) free(requiredbyList);
    if (providesCount) free(providesList);
    if (count) free(fileList);

    return rc;
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
	blockSignals();

	lseek(db->pkgs->fd, offset, SEEK_SET);

	writeHeader(db->pkgs->fd, newHeader);

	unblockSignals();
    }

    return 0;
}

static void blockSignals(void) {
    sigset_t newMask;

    sigfillset(&newMask);		/* block all signals */
    sigprocmask(SIG_BLOCK, &newMask, &signalMask);
}

static void unblockSignals(void) {
    sigprocmask(SIG_SETMASK, &signalMask, NULL);
}
