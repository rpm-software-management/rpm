#include "system.h"

#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>

#include "rpmlib.h"

#include "falloc.h"
#include "fprint.h"
#include "misc.h"
#include "rpmdb.h"

/* XXX the signal handling in here is not thread safe */

/* the requiredbyIndex isn't stricly necessary. In a perfect world, we could
   have each header keep a list of packages that need it. However, we
   can't reserve space in the header for extra information so all of the
   required packages would move in the database every time a package was
   added or removed. Instead, each package (or virtual package) name
   keeps a list of package offsets of packages that might depend on this
   one. Version numbers still need verification, but it gets us in the
   right area w/o a linear search through the database. */

struct rpmdb_s {
    faFile pkgs;
    dbiIndex * nameIndex, * fileIndex, * groupIndex, * providesIndex;
    dbiIndex * requiredbyIndex, * conflictsIndex, * triggerIndex;
};

struct intMatch {
    dbiIndexRecord rec;
    int fpNum;
};

static void removeIndexEntry(dbiIndex * dbi, char * name, dbiIndexRecord rec,
		             int tolerant, char * idxName);
static int addIndexEntry(dbiIndex * idx, char * index, unsigned int offset,
		         unsigned int fileNumber);
static void blockSignals(void);
static void unblockSignals(void);
static int intMatchCmp(const void * one, const void * two);

static sigset_t signalMask;

static int intMatchCmp(const void * one, const void * two) {
    const struct intMatch * a = one;
    const struct intMatch * b = two;

    if (a->rec.recOffset < b->rec.recOffset)
	return -1;
    else if (a->rec.recOffset > b->rec.recOffset)
	return 1;

    return 0;
};

int rpmdbOpenForTraversal(const char * prefix, rpmdb * rpmdbp) {
    const char * dbpath;

    dbpath = rpmGetPath("%{_dbpath}", NULL);
    if (dbpath == NULL || dbpath[0] == '%') {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	return 1;
    }

    if (openDatabase(prefix, dbpath, rpmdbp, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	return 1;
    }

    return 0;
}

int rpmdbOpen (const char * prefix, rpmdb *rpmdbp, int mode, int perms) {
    const char * dbpath;
    int rc;

    dbpath = rpmGetPath("%{_dbpath}", NULL);
    if (dbpath == NULL || dbpath[0] == '%') {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	return 1;
    }

    rc = openDatabase(prefix, dbpath, rpmdbp, mode, perms, 0);
    xfree(dbpath);
    return rc;
}

int rpmdbInit (const char * prefix, int perms) {
    rpmdb db;
    const char * dbpath;
    int rc;

    dbpath = rpmGetPath("%{_dbpath}", NULL);
    if (dbpath == NULL || dbpath[0] == '%') {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	return 1;
    }

    rc = openDatabase(prefix, dbpath, &db, O_CREAT | O_RDWR, perms, 
			RPMDB_FLAG_JUSTCHECK);
    xfree(dbpath);
    return rc;
}

static int openDbFile(const char * prefix, const char * dbpath, const char * shortName, 
		      int justCheck, int perms, dbiIndex ** db, DBTYPE type){
    char * filename = alloca(strlen(prefix) + strlen(dbpath) + 
			     strlen(shortName) + 20);

    if (!prefix) prefix="";

    strcpy(filename, prefix); 
    strcat(filename, dbpath);
    strcat(filename, shortName);

    if (!justCheck || !rpmfileexists(filename)) {
	*db = dbiOpenIndex(filename, perms, 0644, type);
	if (!*db) {
	    return 1;
	}
    }

    return 0;
}

int openDatabase(const char * prefix, const char * dbpath, rpmdb *rpmdbp, int mode, 
		 int perms, int flags) {
    char * filename;
    struct rpmdb_s db;
    int i, rc;
    struct flock lockinfo;
    int justcheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;
    const char * akey;

    if (mode & O_WRONLY) 
	return 1;

    /* we should accept NULL as a valid prefix */
    if (!prefix) prefix="";

    i = strlen(dbpath);
    if (dbpath[i - 1] != '/') {
	filename = alloca(i + 2);
	strcpy(filename, dbpath);
	filename[i] = '/';
	filename[i + 1] = '\0';
	dbpath = filename;
    }
    
    filename = alloca(strlen(prefix) + strlen(dbpath) + 40);

    strcpy(filename, prefix); 
    strcat(filename, dbpath);

    rpmMessage(RPMMESS_DEBUG, _("opening database mode 0x%x in %s\n"),
	mode, filename);

    strcat(filename, "packages.rpm");

    memset(&db, 0, sizeof(db));

    if (!justcheck || !rpmfileexists(filename)) {
	db.pkgs = faOpen(filename, mode, 0644);
	if (db.pkgs == NULL) {
	    rpmError(RPMERR_DBOPEN, _("failed to open %s\n"), filename);
	    return 1;
	}

	/* try and get a lock - this is released by the kernel when we close
	   the file */
	lockinfo.l_whence = 0;
	lockinfo.l_start = 0;
	lockinfo.l_len = 0;
	
	if (mode & O_RDWR) {
	    lockinfo.l_type = F_WRLCK;
	    if (faFcntl(db.pkgs, F_SETLK, (void *) &lockinfo)) {
		rpmError(RPMERR_FLOCK, _("cannot get %s lock on database"), 
			 _("exclusive"));
		return 1;
	    } 
	} else {
	    lockinfo.l_type = F_RDLCK;
	    if (faFcntl(db.pkgs, F_SETLK, (void *) &lockinfo)) {
		rpmError(RPMERR_FLOCK, _("cannot get %s lock on database"), 
			 _("shared"));
		return 1;
	    } 
	}
    }

    rc = openDbFile(prefix, dbpath, "nameindex.rpm", justcheck, mode,
		    &db.nameIndex, DB_HASH);

    if (minimal) {
	*rpmdbp = malloc(sizeof(struct rpmdb_s));
	**rpmdbp = db;
	return 0;
    }

    if (!rc)
	rc = openDbFile(prefix, dbpath, "fileindex.rpm", justcheck, mode,
			&db.fileIndex, DB_HASH);

    /* We used to store the fileindexes as complete paths, rather then
       plain basenames. Let's see which version we are... */
    if (!justcheck && !dbiGetFirstKey(db.fileIndex, &akey)) {
	if (strchr(akey, '/')) {
	    rpmError(RPMERR_OLDDB, _("old format database is present; "
			"use --rebuilddb to generate a new format database"));
	    rc |= 1;
	}
	xfree(akey);
    }

    if (!rc)
	rc = openDbFile(prefix, dbpath, "providesindex.rpm", justcheck, mode,
			&db.providesIndex, DB_HASH);
    if (!rc)
	rc = openDbFile(prefix, dbpath, "requiredby.rpm", justcheck, mode,
			&db.requiredbyIndex, DB_HASH);
    if (!rc)
	rc = openDbFile(prefix, dbpath, "conflictsindex.rpm", justcheck, mode,
			&db.conflictsIndex, DB_HASH);
    if (!rc)
	rc = openDbFile(prefix, dbpath, "groupindex.rpm", justcheck, mode,
			&db.groupIndex, DB_HASH);
    if (!rc)
	rc = openDbFile(prefix, dbpath, "triggerindex.rpm", justcheck, mode,
			&db.triggerIndex, DB_HASH);

    if (rc) {
	faClose(db.pkgs);
	if (db.nameIndex) dbiCloseIndex(db.nameIndex);
	if (db.fileIndex) dbiCloseIndex(db.fileIndex);
	if (db.providesIndex) dbiCloseIndex(db.providesIndex);
	if (db.requiredbyIndex) dbiCloseIndex(db.requiredbyIndex);
	if (db.conflictsIndex) dbiCloseIndex(db.conflictsIndex);
	if (db.groupIndex) dbiCloseIndex(db.groupIndex);
	if (db.triggerIndex) dbiCloseIndex(db.triggerIndex);
	return 1;
    }

    *rpmdbp = malloc(sizeof(struct rpmdb_s));
    **rpmdbp = db;

    if (justcheck) {
	rpmdbClose(*rpmdbp);
    }

    return 0;
}

void rpmdbClose (rpmdb db) {
    if (db->pkgs != NULL) faClose(db->pkgs);
    if (db->fileIndex) dbiCloseIndex(db->fileIndex);
    if (db->groupIndex) dbiCloseIndex(db->groupIndex);
    if (db->nameIndex) dbiCloseIndex(db->nameIndex);
    if (db->providesIndex) dbiCloseIndex(db->providesIndex);
    if (db->requiredbyIndex) dbiCloseIndex(db->requiredbyIndex);
    if (db->conflictsIndex) dbiCloseIndex(db->conflictsIndex);
    if (db->triggerIndex) dbiCloseIndex(db->triggerIndex);
    free(db);
}

int rpmdbFirstRecNum(rpmdb db) {
    return faFirstOffset(db->pkgs);
}

int rpmdbNextRecNum(rpmdb db, unsigned int lastOffset) {
    /* 0 at end */
    return faNextOffset(db->pkgs, lastOffset);
}

Header rpmdbGetRecord(rpmdb db, unsigned int offset) {
    (void)faLseek(db->pkgs, offset, SEEK_SET);

    return headerRead(faFileno(db->pkgs), HEADER_MAGIC_NO);
}

int rpmdbFindByFile(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    const char * basename;
    fingerPrint fp1, fp2;
    dbiIndexSet allMatches;
    int i, rc;
    Header h;
    char ** fileList;

    fp1 = fpLookup(filespec, 0);

    basename = strrchr(filespec, '/');
    if (!basename) 
	basename = filespec;
    else
	basename++;

    rc = dbiSearchIndex(db->fileIndex, basename, &allMatches);
    if (rc) return rc;

    *matches = dbiCreateIndexRecord();
    i = 0;
    while (i < allMatches.count) {
	if (!(h = rpmdbGetRecord(db, allMatches.recs[i].recOffset))) {
	    i++;
	    continue;
	}

	headerGetEntryMinMemory(h, RPMTAG_FILENAMES, NULL, 
				(void **) &fileList, NULL);

	do {
	    fp2 = fpLookup(fileList[allMatches.recs[i].fileNumber], 1);
	    if (FP_EQUAL(fp1, fp2)) 
		dbiAppendIndexRecord(matches, allMatches.recs[i]);

	    i++;
	} while ((i < allMatches.count) && 
			((i == 0) || (allMatches.recs[i].recOffset == 
				allMatches.recs[i - 1].recOffset)));

	free(fileList);
	headerFree(h);
    }

    dbiFreeIndexRecord(allMatches);

    if (!matches->count) {
	dbiFreeIndexRecord(*matches);
	return 1;
    }

    return 0;
}

int rpmdbFindByProvides(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    return dbiSearchIndex(db->providesIndex, filespec, matches);
}

int rpmdbFindByRequiredBy(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    return dbiSearchIndex(db->requiredbyIndex, filespec, matches);
}

int rpmdbFindByConflicts(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    return dbiSearchIndex(db->conflictsIndex, filespec, matches);
}

int rpmdbFindByTriggeredBy(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    return dbiSearchIndex(db->triggerIndex, filespec, matches);
}

int rpmdbFindByGroup(rpmdb db, const char * group, dbiIndexSet * matches) {
    return dbiSearchIndex(db->groupIndex, group, matches);
}

int rpmdbFindPackage(rpmdb db, const char * name, dbiIndexSet * matches) {
    return dbiSearchIndex(db->nameIndex, name, matches);
}

static void removeIndexEntry(dbiIndex * dbi, char * key, dbiIndexRecord rec,
		             int tolerant, char * idxName) {
    int rc;
    dbiIndexSet matches;
    
    rc = dbiSearchIndex(dbi, key, &matches);
    switch (rc) {
      case 0:
	if (dbiRemoveIndexRecord(&matches, rec) && !tolerant) {
	    rpmError(RPMERR_DBCORRUPT, _("package %s not listed in %s"),
		  key, idxName);
	} else {
	    dbiUpdateIndex(dbi, key, &matches);
	       /* errors from above will be reported from dbindex.c */
	}

	dbiFreeIndexRecord(matches);
	break;
      case 1:
	if (!tolerant) 
	    rpmError(RPMERR_DBCORRUPT, _("package %s not found in %s"), 
			key, idxName);
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
    dbiIndexRecord rec;
    char ** fileList, ** providesList, ** requiredbyList;
    char ** conflictList, ** triggerList;
    int i;
    char * basename;

    /* structure assignment */
    rec = dbiReturnIndexRecordInstance(offset, 0);

    h = rpmdbGetRecord(db, offset);
    if (h == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("cannot read header at %d for uninstall"),
	      offset);
	return 1;
    }

    blockSignals();

    if (!headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &count)) {
	rpmError(RPMERR_DBCORRUPT, _("package has no name"));
    } else {
	rpmMessage(RPMMESS_DEBUG, _("removing name index\n"));
	removeIndexEntry(db->nameIndex, name, rec, tolerant, "name index");
    }

    if (!headerGetEntry(h, RPMTAG_GROUP, &type, (void **) &group, &count)) {
	rpmMessage(RPMMESS_DEBUG, _("package has no group\n"));
    } else {
	rpmMessage(RPMMESS_DEBUG, _("removing group index\n"));
	removeIndexEntry(db->groupIndex, group, rec, tolerant, "group index");
    }

    if (headerGetEntry(h, RPMTAG_PROVIDES, &type, (void **) &providesList, 
	 &count)) {
	for (i = 0; i < count; i++) {
	    rpmMessage(RPMMESS_DEBUG, _("removing provides index for %s\n"), 
		    providesList[i]);
	    removeIndexEntry(db->providesIndex, providesList[i], rec, tolerant, 
			     "providesfile index");
	}
	free(providesList);
    }

    if (headerGetEntry(h, RPMTAG_REQUIRENAME, &type, (void **) &requiredbyList, 
	 &count)) {
	/* There could be dups in requiredByLIst, and the list is sorted.
	   Rather then sort the list, be tolerant of missing entries
	   as they should just indicate duplicated requirements. */

	for (i = 0; i < count; i++) {
	    rpmMessage(RPMMESS_DEBUG, _("removing requiredby index for %s\n"), 
		    requiredbyList[i]);
	    removeIndexEntry(db->requiredbyIndex, requiredbyList[i], rec, 
			     1, "requiredby index");
	}
	free(requiredbyList);
    }

    if (headerGetEntry(h, RPMTAG_TRIGGERNAME, &type, (void **) &triggerList, 
	 &count)) {
	/* triggerList often contains duplicates */
	for (i = 0; i < count; i++) {
	    rpmMessage(RPMMESS_DEBUG, _("removing trigger index for %s\n"), 
		       triggerList[i]);
	    removeIndexEntry(db->triggerIndex, triggerList[i], rec, 
			     1, "trigger index");
	}
	free(triggerList);
    }

    if (headerGetEntry(h, RPMTAG_CONFLICTNAME, &type, (void **) &conflictList, 
	 &count)) {
	for (i = 0; i < count; i++) {
	    rpmMessage(RPMMESS_DEBUG, _("removing conflict index for %s\n"), 
		    conflictList[i]);
	    removeIndexEntry(db->conflictsIndex, conflictList[i], rec, 
			     tolerant, "conflict index");
	}
	free(conflictList);
    }

    if (headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	 &count)) {
	for (i = 0; i < count; i++) {
	    basename = strrchr(fileList[i], '/');
	    if (!basename) 
		basename = fileList[i];
	    else
		basename++;

	    rpmMessage(RPMMESS_DEBUG, _("removing file index for %s\n"), 
			basename);
	    /* structure assignment */
	    rec = dbiReturnIndexRecordInstance(offset, i);
	    removeIndexEntry(db->fileIndex, basename, rec, tolerant, 
			     "file index");
	}
	free(fileList);
    } else {
	rpmMessage(RPMMESS_DEBUG, _("package has no files\n"));
    }

    faFree(db->pkgs, offset);

    dbiSyncIndex(db->nameIndex);
    dbiSyncIndex(db->groupIndex);
    dbiSyncIndex(db->fileIndex);

    unblockSignals();

    headerFree(h);

    return 0;
}

static int addIndexEntry(dbiIndex * idx, char * index, unsigned int offset,
		         unsigned int fileNumber) {
    dbiIndexSet set;
    dbiIndexRecord irec;   
    int rc;

    irec = dbiReturnIndexRecordInstance(offset, fileNumber);

    rc = dbiSearchIndex(idx, index, &set);
    if (rc == -1)  		/* error */
	return 1;

    if (rc == 1)  		/* new item */
	set = dbiCreateIndexRecord();
    dbiAppendIndexRecord(&set, irec);
    if (dbiUpdateIndex(idx, index, &set))
	exit(1);
    dbiFreeIndexRecord(set);
    return 0;
}

int rpmdbAdd(rpmdb db, Header dbentry) {
    unsigned int dboffset;
    unsigned int i, j;
    char ** fileList;
    char ** providesList;
    char ** requiredbyList;
    char ** conflictList;
    char ** triggerList;
    char * name, * group;
    int count = 0, providesCount = 0, requiredbyCount = 0, conflictCount = 0;
    int triggerCount = 0;
    int type;
    int rc = 0;
    char * basename;

    headerGetEntry(dbentry, RPMTAG_NAME, &type, (void **) &name, &count);
    headerGetEntry(dbentry, RPMTAG_GROUP, &type, (void **) &group, &count);

    if (!group) group = "Unknown";

    count = 0;
    headerGetEntry(dbentry, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	           &count);
    headerGetEntry(dbentry, RPMTAG_PROVIDES, &type, (void **) &providesList, 
	           &providesCount);
    headerGetEntry(dbentry, RPMTAG_REQUIRENAME, &type, 
		   (void **) &requiredbyList, &requiredbyCount);
    headerGetEntry(dbentry, RPMTAG_CONFLICTNAME, &type, 
		   (void **) &conflictList, &conflictCount);
    headerGetEntry(dbentry, RPMTAG_TRIGGERNAME, &type, 
		   (void **) &triggerList, &triggerCount);

    blockSignals();

    dboffset = faAlloc(db->pkgs, headerSizeof(dbentry, HEADER_MAGIC_NO));
    if (!dboffset) {
	rpmError(RPMERR_DBCORRUPT, _("cannot allocate space for database"));
	unblockSignals();
	if (providesCount) free(providesList);
	if (requiredbyCount) free(requiredbyList);
	if (conflictCount) free(conflictList);
	if (triggerCount) free(triggerList);
	if (count) free(fileList);
	return 1;
    }
    (void)faLseek(db->pkgs, dboffset, SEEK_SET);

    headerWrite(faFileno(db->pkgs), dbentry, HEADER_MAGIC_NO);

    /* Now update the appropriate indexes */
    if (addIndexEntry(db->nameIndex, name, dboffset, 0))
	rc = 1;
    if (addIndexEntry(db->groupIndex, group, dboffset, 0))
	rc = 1;

    for (i = 0; i < triggerCount; i++) {
	/* don't add duplicates */
	for (j = 0; j < i; j++)
	    if (!strcmp(triggerList[i], triggerList[j])) break;
	if (j == i)
	    rc += addIndexEntry(db->triggerIndex, triggerList[i], dboffset, 0);
    }

    for (i = 0; i < conflictCount; i++)
	rc += addIndexEntry(db->conflictsIndex, conflictList[i], dboffset, 0);

    for (i = 0; i < requiredbyCount; i++)
	rc += addIndexEntry(db->requiredbyIndex, requiredbyList[i], 
			    dboffset, 0);

    for (i = 0; i < providesCount; i++)
	rc += addIndexEntry(db->providesIndex, providesList[i], dboffset, 0);

    for (i = 0; i < count; i++) {
	basename = strrchr(fileList[i], '/');
	if (!basename) 
	    basename = fileList[i];
	else
	    basename++;
	if (*basename)
	    rc += addIndexEntry(db->fileIndex, basename, dboffset, i);
    }

    dbiSyncIndex(db->nameIndex);
    dbiSyncIndex(db->groupIndex);
    dbiSyncIndex(db->fileIndex);
    dbiSyncIndex(db->providesIndex);
    dbiSyncIndex(db->requiredbyIndex);
    dbiSyncIndex(db->triggerIndex);

    unblockSignals();

    if (requiredbyCount) free(requiredbyList);
    if (providesCount) free(providesList);
    if (conflictCount) free(conflictList);
    if (triggerCount) free(triggerList);
    if (count) free(fileList);

    return rc;
}

int rpmdbUpdateRecord(rpmdb db, int offset, Header newHeader) {
    Header oldHeader;
    int oldSize;

    oldHeader = rpmdbGetRecord(db, offset);
    if (oldHeader == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("cannot read header at %d for update"),
		offset);
	return 1;
    }

    oldSize = headerSizeof(oldHeader, HEADER_MAGIC_NO);
    headerFree(oldHeader);

    if (oldSize != headerSizeof(newHeader, HEADER_MAGIC_NO)) {
	rpmMessage(RPMMESS_DEBUG, _("header changed size!"));
	if (rpmdbRemove(db, offset, 1))
	    return 1;

	if (rpmdbAdd(db, newHeader)) 
	    return 1;
    } else {
	blockSignals();

	(void)faLseek(db->pkgs, offset, SEEK_SET);

	headerWrite(faFileno(db->pkgs), newHeader, HEADER_MAGIC_NO);

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

void rpmdbRemoveDatabase(const char * rootdir, const char * dbpath) { 
    int i;
    char * filename;

    i = strlen(dbpath);
    if (dbpath[i - 1] != '/') {
	filename = alloca(i);
	strcpy(filename, dbpath);
	filename[i] = '/';
	filename[i + 1] = '\0';
	dbpath = filename;
    }
    
    filename = alloca(strlen(rootdir) + strlen(dbpath) + 40);

    sprintf(filename, "%s/%s/packages.rpm", rootdir, dbpath);
    unlink(filename);

    sprintf(filename, "%s/%s/nameindex.rpm", rootdir, dbpath);
    unlink(filename);

    sprintf(filename, "%s/%s/fileindex.rpm", rootdir, dbpath);
    unlink(filename);

    sprintf(filename, "%s/%s/groupindex.rpm", rootdir, dbpath);
    unlink(filename);

    sprintf(filename, "%s/%s/requiredby.rpm", rootdir, dbpath);
    unlink(filename);

    sprintf(filename, "%s/%s/providesindex.rpm", rootdir, dbpath);
    unlink(filename);

    sprintf(filename, "%s/%s/conflictsindex.rpm", rootdir, dbpath);
    unlink(filename);
}

int rpmdbMoveDatabase(const char * rootdir, const char * olddbpath, const char * newdbpath) {
    int i;
    char * ofilename, * nfilename;
    int rc = 0;
 
    i = strlen(olddbpath);
    if (olddbpath[i - 1] != '/') {
	ofilename = alloca(i + 2);
	strcpy(ofilename, olddbpath);
	ofilename[i] = '/';
	ofilename[i + 1] = '\0';
	olddbpath = ofilename;
    }
    
    i = strlen(newdbpath);
    if (newdbpath[i - 1] != '/') {
	nfilename = alloca(i + 2);
	strcpy(nfilename, newdbpath);
	nfilename[i] = '/';
	nfilename[i + 1] = '\0';
	newdbpath = nfilename;
    }
    
    ofilename = alloca(strlen(rootdir) + strlen(olddbpath) + 40);
    nfilename = alloca(strlen(rootdir) + strlen(newdbpath) + 40);

    sprintf(ofilename, "%s/%s/packages.rpm", rootdir, olddbpath);
    sprintf(nfilename, "%s/%s/packages.rpm", rootdir, newdbpath);
    if (rename(ofilename, nfilename)) rc = 1;

    sprintf(ofilename, "%s/%s/nameindex.rpm", rootdir, olddbpath);
    sprintf(nfilename, "%s/%s/nameindex.rpm", rootdir, newdbpath);
    if (rename(ofilename, nfilename)) rc = 1;

    sprintf(ofilename, "%s/%s/fileindex.rpm", rootdir, olddbpath);
    sprintf(nfilename, "%s/%s/fileindex.rpm", rootdir, newdbpath);
    if (rename(ofilename, nfilename)) rc = 1;

    sprintf(ofilename, "%s/%s/groupindex.rpm", rootdir, olddbpath);
    sprintf(nfilename, "%s/%s/groupindex.rpm", rootdir, newdbpath);
    if (rename(ofilename, nfilename)) rc = 1;

    sprintf(ofilename, "%s/%s/requiredby.rpm", rootdir, olddbpath);
    sprintf(nfilename, "%s/%s/requiredby.rpm", rootdir, newdbpath);
    if (rename(ofilename, nfilename)) rc = 1;

    sprintf(ofilename, "%s/%s/providesindex.rpm", rootdir, olddbpath);
    sprintf(nfilename, "%s/%s/providesindex.rpm", rootdir, newdbpath);
    if (rename(ofilename, nfilename)) rc = 1;

    sprintf(ofilename, "%s/%s/conflictsindex.rpm", rootdir, olddbpath);
    sprintf(nfilename, "%s/%s/conflictsindex.rpm", rootdir, newdbpath);
    if (rename(ofilename, nfilename)) rc = 1;

    sprintf(ofilename, "%s/%s/triggerindex.rpm", rootdir, olddbpath);
    sprintf(nfilename, "%s/%s/triggerindex.rpm", rootdir, newdbpath);
    if (rename(ofilename, nfilename)) rc = 1;

    return rc;
}

int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, dbiIndexSet * matchList, 
		    int numItems) {
    struct intMatch * intMatches, * im;
    int i, j;
    dbiIndexSet matches;
    int intMatchesAlloced, numIntMatches;
    int start, end;
    int num, rc;
    const char ** fullfl, **fl;
    int_32 fc;
    fingerPrint * fps;
    Header h;

    /* this may be worth batching by basename, but probably not as
       basenames are quite unique as it is */

    intMatchesAlloced = numItems;
    numIntMatches = 0;
    im = intMatches = malloc(sizeof(*intMatches) * intMatchesAlloced);

    for (i = 0; i < numItems; i++, im++) {
        rc = dbiSearchIndex(db->fileIndex, fpList[i].basename, &matches);
	if (rc == 2) {
	    im = intMatches;
	    free(intMatches);
	    return 1;
	} else if (!rc) {
	    if ((numIntMatches + matches.count) >= intMatchesAlloced) {
		intMatchesAlloced += matches.count;
		intMatchesAlloced += intMatchesAlloced / 5;
		intMatches = realloc(intMatches, 
				     sizeof(*intMatches) * intMatchesAlloced);
	    }

	    for (j = 0; j < matches.count; j++) {
		intMatches[numIntMatches].rec = matches.recs[j];
		intMatches[numIntMatches++].fpNum = i;
	    }

	    dbiFreeIndexRecord(matches);
	}
    }

    qsort(intMatches, numIntMatches, sizeof(*intMatches), intMatchCmp);
    /* intMatches is now sorted by (recnum, filenum) */

    for (i = 0; i < numItems; i++)
	matchList[i] = dbiCreateIndexRecord();

    start = 0;
    while (start < numIntMatches) {
	im = intMatches + start;
	end = start + 1;
	while (end < numIntMatches && 
	    (im->rec.recOffset == intMatches[end].rec.recOffset))
	    end++;

	num = end - start;
	h = rpmdbGetRecord(db, im->rec.recOffset);
	if (!h) {
	    free(intMatches);
	    return 1;
	}

	headerGetEntryMinMemory(h, RPMTAG_FILENAMES, NULL, 
				(void **) &fullfl, &fc);

	fl = malloc(sizeof(*fl) * num);
	for (i = 0; i < num; i++)
	    fl[i] = fullfl[im[i].rec.fileNumber];
	free(fullfl);
	fps = malloc(sizeof(*fps) * num);
	fpLookupList(fl, fps, num, 1);
	free(fl);

	for (i = 0; i < num; i++) {
	    j = im[i].fpNum;
	    if (FP_EQUAL(fps[i], fpList[j]))
		dbiAppendIndexRecord(&matchList[j], im[i].rec);
	}

	headerFree(h);

	free(fps);

	start = end;
    }

    free(intMatches);

    return 0;
}
