#include "system.h"

#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */

#include "falloc.h"
#include "fprint.h"
#include "misc.h"
#include "rpmdb.h"

/*@access dbiIndexSet@*/
/*@access dbiIndexRecord@*/

const char *rpmdb_filenames[] = {
    "packages.rpm",
    "nameindex.rpm",
    "fileindex.rpm",
    "groupindex.rpm",
    "requiredby.rpm",
    "providesindex.rpm",
    "conflictsindex.rpm",
    "triggerindex.rpm",
    NULL
};

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

static sigset_t signalMask;

static void blockSignals(void)
{
    sigset_t newMask;

    sigfillset(&newMask);		/* block all signals */
    sigprocmask(SIG_BLOCK, &newMask, &signalMask);
}

static void unblockSignals(void)
{
    sigprocmask(SIG_SETMASK, &signalMask, NULL);
}

int rpmdbOpenForTraversal(const char * prefix, rpmdb * rpmdbp)
{
    const char * dbpath;
    int rc = 0;

    dbpath = rpmGetPath("%{_dbpath}", NULL);
    if (dbpath == NULL || dbpath[0] == '%') {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	return 1;
    }

    if (openDatabase(prefix, dbpath, rpmdbp, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
    }
    xfree(dbpath);
    return rc;
}

int rpmdbOpen (const char * prefix, rpmdb *rpmdbp, int mode, int perms)
{
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

int rpmdbInit (const char * prefix, int perms)
{
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
	 int justCheck, int mode, int perms, dbiIndex ** db, DBTYPE type)
{
    int len = (prefix ? strlen(prefix) : 0) + strlen(dbpath) + strlen(shortName) + 1;
    char * filename = alloca(len);

    *filename = '\0';
    if (prefix && *prefix) strcat(filename, prefix); 
    strcat(filename, dbpath);
    strcat(filename, shortName);

    if (!justCheck || !rpmfileexists(filename)) {
	*db = dbiOpenIndex(filename, mode, perms, type);
	if (!*db) {
	    return 1;
	}
    }

    return 0;
}

static /*@only@*/ rpmdb newRpmdb(void)
{
    rpmdb db = xmalloc(sizeof(*db));
    db->pkgs = NULL;
    db->nameIndex = NULL;
    db->fileIndex = NULL;
    db->groupIndex = NULL;
    db->providesIndex = NULL;
    db->requiredbyIndex = NULL;
    db->conflictsIndex = NULL;
    db->triggerIndex = NULL;
    return db;
}

int openDatabase(const char * prefix, const char * dbpath, rpmdb *rpmdbp, int mode, 
		 int perms, int flags)
{
    char * filename;
    rpmdb db;
    int i, rc;
    struct flock lockinfo;
    int justcheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;
    const char * akey;

    if (mode & O_WRONLY) 
	return 1;

    if (!(perms & 0600))	/* XXX sanity */
	perms = 0644;

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

    db = newRpmdb();

    if (!justcheck || !rpmfileexists(filename)) {
	if ((db->pkgs = faOpen(filename, mode, perms)) == NULL) {
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
	    if (faFcntl(db->pkgs, F_SETLK, (void *) &lockinfo)) {
		rpmError(RPMERR_FLOCK, _("cannot get %s lock on database"), 
			 _("exclusive"));
		rpmdbClose(db);
		return 1;
	    } 
	} else {
	    lockinfo.l_type = F_RDLCK;
	    if (faFcntl(db->pkgs, F_SETLK, (void *) &lockinfo)) {
		rpmError(RPMERR_FLOCK, _("cannot get %s lock on database"), 
			 _("shared"));
		rpmdbClose(db);
		return 1;
	    } 
	}
    }

    rc = openDbFile(prefix, dbpath, "nameindex.rpm", justcheck, mode, perms,
		    &db->nameIndex, DB_HASH);

    if (minimal) {
	*rpmdbp = xmalloc(sizeof(struct rpmdb_s));
	if (rpmdbp)
	    *rpmdbp = db;	/* structure assignment */
	else
	    rpmdbClose(db);
	return 0;
    }

    if (!rc)
	rc = openDbFile(prefix, dbpath, "fileindex.rpm", justcheck, mode, perms,
			&db->fileIndex, DB_HASH);

    /* We used to store the fileindexes as complete paths, rather then
       plain basenames. Let's see which version we are... */
    /*
     * XXX FIXME: db->fileindex can be NULL under pathological (e.g. mixed
     * XXX db1/db2 linkage) conditions.
     */
    if (!justcheck && !dbiGetFirstKey(db->fileIndex, &akey)) {
	if (strchr(akey, '/')) {
	    rpmError(RPMERR_OLDDB, _("old format database is present; "
			"use --rebuilddb to generate a new format database"));
	    rc |= 1;
	}
	xfree(akey);
    }

    if (!rc)
	rc = openDbFile(prefix, dbpath, "providesindex.rpm", justcheck, mode, perms,
			&db->providesIndex, DB_HASH);
    if (!rc)
	rc = openDbFile(prefix, dbpath, "requiredby.rpm", justcheck, mode, perms,
			&db->requiredbyIndex, DB_HASH);
    if (!rc)
	rc = openDbFile(prefix, dbpath, "conflictsindex.rpm", justcheck, mode, perms,
			&db->conflictsIndex, DB_HASH);
    if (!rc)
	rc = openDbFile(prefix, dbpath, "groupindex.rpm", justcheck, mode, perms,
			&db->groupIndex, DB_HASH);
    if (!rc)
	rc = openDbFile(prefix, dbpath, "triggerindex.rpm", justcheck, mode, perms,
			&db->triggerIndex, DB_HASH);

    if (rc || justcheck || rpmdbp == NULL)
	rpmdbClose(db);
     else
	*rpmdbp = db;

     return rc;
}

void rpmdbClose (rpmdb db)
{
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

static Header doGetRecord(rpmdb db, unsigned int offset, int pristine) {
    Header h;
    char ** fileList;
    char ** newList;
    int fileCount = 0;
    int i;

    (void)faLseek(db->pkgs, offset, SEEK_SET);

    h = headerRead(faFileno(db->pkgs), HEADER_MAGIC_NO);

    if (pristine) return h;

    /* the RPM used to build much of RH 5.1 could produce packages whose
       file lists did not have leading /'s. Now is a good time to fix
       that */

    /* If this tag isn't present, either no files are in the package or
       we're dealing with a package that has just the compressed file name
       list */
    if (!headerGetEntryMinMemory(h, RPMTAG_OLDFILENAMES, NULL, 
			   (void **) &fileList, &fileCount)) return h;

    for (i = 0; i < fileCount; i++) 
	if (*fileList[i] != '/') break;

    if (i == fileCount) {
	free(fileList);
    } else {
	/* bad header -- let's clean it up */
	newList = alloca(sizeof(*newList) * fileCount);
	for (i = 0; i < fileCount; i++) {
	    newList[i] = alloca(strlen(fileList[i]) + 2);
	    if (*fileList[i] == '/')
		strcpy(newList[i], fileList[i]);
	    else
		sprintf(newList[i], "/%s", fileList[i]);
	}

	free(fileList);

	headerModifyEntry(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE, 
			  newList, fileCount);
    }

    /* The file list was moved to a more compressed format which not
       only saves memory (nice), but gives fingerprinting a nice, fat
       speed boost (very nice). Go ahead and convert old headers to
       the new style (this is a noop for new headers) */
    compressFilelist(h);

    return h;
}

Header rpmdbGetRecord(rpmdb db, unsigned int offset)
{
    return doGetRecord(db, offset, 0);
}

int rpmdbFindByFile(rpmdb db, const char * filespec, dbiIndexSet * matches)
{
    const char * basename;
    fingerPrint fp1, fp2;
    dbiIndexSet allMatches;
    int i, rc, num;
    Header h;
    char ** baseNames, ** dirNames;
    int_32 * dirIndexes;
    char * otherFile;
    fingerPrintCache fpc;

    fpc = fpCacheCreate(20);
    fp1 = fpLookup(fpc, filespec, 0);

    basename = strrchr(filespec, '/');
    if (!basename) 
	basename = filespec;
    else
	basename++;

    rc = dbiSearchIndex(db->fileIndex, basename, &allMatches);
    if (rc) {
	fpCacheFree(fpc);
	return rc;
    }

    *matches = dbiCreateIndexRecord();
    i = 0;
    while (i < allMatches.count) {
	if ((h = rpmdbGetRecord(db, allMatches.recs[i].recOffset)) == NULL) {
	    i++;
	    continue;
	}

	headerGetEntryMinMemory(h, RPMTAG_COMPFILELIST, NULL, 
				(void **) &baseNames, NULL);
	headerGetEntryMinMemory(h, RPMTAG_COMPFILEDIRS, NULL, 
				(void **) &dirIndexes, NULL);
	headerGetEntryMinMemory(h, RPMTAG_COMPDIRLIST, NULL, 
				(void **) &dirNames, NULL);

	do {
	    num = allMatches.recs[i].fileNumber;
	    otherFile = xmalloc(strlen(dirNames[dirIndexes[num]]) + 
			      strlen(baseNames[num]) + 1);
	    strcpy(otherFile, dirNames[dirIndexes[num]]);
	    strcat(otherFile, baseNames[num]);

	    fp2 = fpLookup(fpc, otherFile, 1);
	    if (FP_EQUAL(fp1, fp2)) 
		dbiAppendIndexRecord(matches, allMatches.recs[i]);

	    free(otherFile);
	    i++;
	} while ((i < allMatches.count) && 
			((i == 0) || (allMatches.recs[i].recOffset == 
				allMatches.recs[i - 1].recOffset)));

	free(baseNames);
	free(dirNames);
	headerFree(h);
    }

    dbiFreeIndexRecord(allMatches);

    fpCacheFree(fpc);

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
		             int tolerant, char * idxName)
{
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

int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant)
{
    Header h;
    char * name, * group;
    int type;
    unsigned int count;
    dbiIndexRecord rec;
    char ** baseFileNames, ** providesList, ** requiredbyList;
    char ** conflictList, ** triggerList;
    int i;

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

    if (headerGetEntry(h, RPMTAG_PROVIDENAME, &type, (void **) &providesList, 
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

    if (headerGetEntry(h, RPMTAG_COMPFILELIST, &type, (void **) &baseFileNames, 
	 &count)) {
	for (i = 0; i < count; i++) {
	    rpmMessage(RPMMESS_DEBUG, _("removing file index for %s\n"), 
			baseFileNames[i]);
	    /* structure assignment */
	    rec = dbiReturnIndexRecordInstance(offset, i);
	    removeIndexEntry(db->fileIndex, baseFileNames[i], rec, tolerant, 
			     "file index");
	}
	free(baseFileNames);
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

static int addIndexEntry(dbiIndex *idx, const char *index, unsigned int offset,
		         unsigned int fileNumber)
{
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
	exit(EXIT_FAILURE);
    dbiFreeIndexRecord(set);
    return 0;
}

int rpmdbAdd(rpmdb db, Header dbentry)
{
    unsigned int dboffset;
    unsigned int i, j;
    const char ** baseFileNames;
    const char ** providesList;
    const char ** requiredbyList;
    const char ** conflictList;
    const char ** triggerList;
    const char * name;
    const char * group;
    int count = 0, providesCount = 0, requiredbyCount = 0, conflictCount = 0;
    int triggerCount = 0;
    int type;
    int rc = 0;

    headerGetEntry(dbentry, RPMTAG_NAME, &type, (void **) &name, &count);
    headerGetEntry(dbentry, RPMTAG_GROUP, &type, (void **) &group, &count);

    if (!group) group = "Unknown";

    count = 0;
    headerGetEntry(dbentry, RPMTAG_COMPFILELIST, &type, (void **) 
		    &baseFileNames, &count);
    headerGetEntry(dbentry, RPMTAG_PROVIDENAME, &type, (void **) &providesList, 
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
	rc = 1;
    } else {
	(void)faLseek(db->pkgs, dboffset, SEEK_SET);
	rc = headerWrite(faFileno(db->pkgs), dbentry, HEADER_MAGIC_NO);
    }

    if (rc) {
	rpmError(RPMERR_DBCORRUPT, _("cannot allocate space for database"));
	goto exit;
    }

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
	rc += addIndexEntry(db->fileIndex, baseFileNames[i], dboffset, i);
    }

    dbiSyncIndex(db->nameIndex);
    dbiSyncIndex(db->groupIndex);
    dbiSyncIndex(db->fileIndex);
    dbiSyncIndex(db->providesIndex);
    dbiSyncIndex(db->requiredbyIndex);
    dbiSyncIndex(db->triggerIndex);

exit:
    unblockSignals();

    if (requiredbyCount) free(requiredbyList);
    if (providesCount) free(providesList);
    if (conflictCount) free(conflictList);
    if (triggerCount) free(triggerList);
    if (count) free(baseFileNames);

    return rc;
}

int rpmdbUpdateRecord(rpmdb db, int offset, Header newHeader)
{
    Header oldHeader;
    int oldSize;
    int rc = 0;

    oldHeader = doGetRecord(db, offset, 1);
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

	rc = headerWrite(faFileno(db->pkgs), newHeader, HEADER_MAGIC_NO);

	unblockSignals();
    }

    return rc;
}

void rpmdbRemoveDatabase(const char * rootdir, const char * dbpath)
{ 
    int i;
    const char **rpmdbfnp;
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

    for (rpmdbfnp = rpmdb_filenames; *rpmdbfnp; rpmdbfnp++) {
	sprintf(filename, "%s/%s/%s", rootdir, dbpath, *rpmdbfnp);
	unlink(filename);
    }

    sprintf(filename, "%s/%s", rootdir, dbpath);
    rmdir(filename);

}

int rpmdbMoveDatabase(const char * rootdir, const char * olddbpath, const char * newdbpath)
{
    int i;
    const char **rpmdbfnp;
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

    for (rpmdbfnp = rpmdb_filenames; *rpmdbfnp; rpmdbfnp++) {
	sprintf(ofilename, "%s/%s/%s", rootdir, olddbpath, *rpmdbfnp);
	sprintf(nfilename, "%s/%s/%s", rootdir, newdbpath, *rpmdbfnp);
	if (rename(ofilename, nfilename)) rc = 1;
    }

    return rc;
}

struct intMatch {
    dbiIndexRecord rec;
    int fpNum;
};

static int intMatchCmp(const void * one, const void * two)
{
    const struct intMatch * a = one;
    const struct intMatch * b = two;

    if (a->rec.recOffset < b->rec.recOffset)
	return -1;
    else if (a->rec.recOffset > b->rec.recOffset)
	return 1;

    return 0;
}

int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, dbiIndexSet * matchList, 
		    int numItems)
{
    int numIntMatches = 0;
    int intMatchesAlloced = numItems;
    struct intMatch * intMatches;
    int i, j;
    int start, end;
    int num;
    int_32 fc;
    const char ** dirNames, ** baseNames;
    const char ** fullBaseNames;
    int_32 * dirIndexes, * fullDirIndexes;
    fingerPrintCache fpc;

    /* this may be worth batching by basename, but probably not as
       basenames are quite unique as it is */

    intMatches = xcalloc(intMatchesAlloced, sizeof(*intMatches));

    /* Gather all matches from the database */
    for (i = 0; i < numItems; i++) {
	dbiIndexSet matches;
	switch (dbiSearchIndex(db->fileIndex, fpList[i].basename, &matches)) {
	default:
	    break;
	case 2:
	    free(intMatches);
	    return 1;
	    /*@notreached@*/ break;
	case 0:
	    if ((numIntMatches + dbiIndexSetCount(matches)) >= intMatchesAlloced) {
		intMatchesAlloced += dbiIndexSetCount(matches);
		intMatchesAlloced += intMatchesAlloced / 5;
		intMatches = xrealloc(intMatches, 
				     sizeof(*intMatches) * intMatchesAlloced);
	    }

	    for (j = 0; j < dbiIndexSetCount(matches); j++) {
		/* structure assignment */
		intMatches[numIntMatches].rec = matches.recs[j];
		intMatches[numIntMatches++].fpNum = i;
	    }

	    dbiFreeIndexRecord(matches);
	    break;
	}
    }

    qsort(intMatches, numIntMatches, sizeof(*intMatches), intMatchCmp);
    /* intMatches is now sorted by (recnum, filenum) */

    for (i = 0; i < numItems; i++)
	matchList[i] = dbiCreateIndexRecord();

    fpc = fpCacheCreate(numIntMatches);

    /* For each set of files matched in a package ... */
    for (start = 0; start < numIntMatches; start = end) {
	struct intMatch * im;
	Header h;
	fingerPrint * fps;

	im = intMatches + start;

	/* Find the end of the set of matched files in this package. */
	for (end = start + 1; end < numIntMatches; end++) {
	    if (im->rec.recOffset != intMatches[end].rec.recOffset)
		break;
	}
	num = end - start;

	/* Compute fingerprints for each file match in this package. */
	h = rpmdbGetRecord(db, im->rec.recOffset);
	if (h == NULL) {
	    free(intMatches);
	    return 1;
	}

	headerGetEntryMinMemory(h, RPMTAG_COMPDIRLIST, NULL, 
			    (void **) &dirNames, NULL);
	headerGetEntryMinMemory(h, RPMTAG_COMPFILELIST, NULL, 
			    (void **) &fullBaseNames, &fc);
	headerGetEntryMinMemory(h, RPMTAG_COMPFILEDIRS, NULL, 
			    (void **) &fullDirIndexes, NULL);

	baseNames = xcalloc(num, sizeof(*baseNames));
	dirIndexes = xcalloc(num, sizeof(*dirIndexes));
	for (i = 0; i < num; i++) {
	    baseNames[i] = fullBaseNames[im[i].rec.fileNumber];
	    dirIndexes[i] = fullDirIndexes[im[i].rec.fileNumber];
	}

	fps = xcalloc(num, sizeof(*fps));
	fpLookupList(fpc, dirNames, baseNames, dirIndexes, num, fps);

	free(dirNames);
	free(fullBaseNames);
	free(baseNames);
	free(dirIndexes);

	/* Add db (recnum,filenum) to list for fingerprint matches. */
	for (i = 0; i < num; i++) {
	    j = im[i].fpNum;
	    if (FP_EQUAL(fps[i], fpList[j]))
		dbiAppendIndexRecord(&matchList[j], im[i].rec);
	}

	headerFree(h);

	free(fps);
    }

    fpCacheFree(fpc);
    free(intMatches);

    return 0;
}
