#include "system.h"

static int _debug = 0;

#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>

#include <rpmlib.h>
#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */

#include "dbindex.h"
/*@access dbiIndexSet@*/
/*@access dbiIndexRecord@*/

#include "falloc.h"
#include "fprint.h"
#include "misc.h"
#include "rpmdb.h"

extern int _noDirTokens;

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

struct _dbiIndex rpmdbi[] = {
    { "packages.db3", 0,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#define	RPMDBI_PACKAGES		0
    { "nameindex.rpm", RPMTAG_NAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#define	RPMDBI_NAME		1
    { "fileindex.rpm", RPMTAG_BASENAMES,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#define	RPMDBI_FILE		2
    { "groupindex.rpm", RPMTAG_GROUP,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#define	RPMDBI_GROUP		3
    { "requiredby.rpm", RPMTAG_REQUIRENAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#define	RPMDBI_REQUIREDBY	4
    { "providesindex.rpm", RPMTAG_PROVIDENAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#define	RPMDBI_PROVIDES		5
    { "conflictsindex.rpm", RPMTAG_CONFLICTNAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#define	RPMDBI_CONFLICTS	6
    { "triggerindex.rpm", RPMTAG_TRIGGERNAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#define	RPMDBI_TRIGGER		7
    { NULL }
#define	RPMDBI_MIN		0
#define	RPMDBI_MAX		8
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
    FD_t pkgs;
    dbiIndex _dbi[RPMDBI_MAX];
#ifdef	DYING
#define	nameIndex		_dbi[RPMDBI_NAME]
#define	fileIndex		_dbi[RPMDBI_FILE]
#define	groupIndex		_dbi[RPMDBI_GROUP]
#define	requiredbyIndex		_dbi[RPMDBI_REQUIREDBY]
#define	providesIndex		_dbi[RPMDBI_PROVIDES]
#define	conflictsIndex		_dbi[RPMDBI_CONFLICTS]
#define	triggerIndex		_dbi[RPMDBI_TRIGGER]
#endif
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

static int openDbFile(const char * prefix, const char * dbpath,
	const dbiIndex dbi, int justCheck, int mode, dbiIndex * dbip)
{
    char * filename, * fn;
    int len;

    if (dbi == NULL || dbip == NULL)
	return 1;
    *dbip = NULL;

    len = (prefix ? strlen(prefix) : 0) +
		strlen(dbpath) + strlen(dbi->dbi_basename) + 1;
    fn = filename = alloca(len);
    *fn = '\0';
    switch (urlIsURL(dbpath)) {
    case URL_IS_UNKNOWN:
	if (prefix && *prefix &&
	    !(prefix[0] == '/' && prefix[1] == '\0' && dbpath[0] == '/'))
		fn = stpcpy(fn, prefix);
	break;
    default:
	break;
    }
    fn = stpcpy(fn, dbpath);
    if (fn > filename && !(fn[-1] == '/' || dbi->dbi_basename[0] == '/'))
	fn = stpcpy(fn, "/");
    fn = stpcpy(fn, dbi->dbi_basename);

    if (!justCheck || !rpmfileexists(filename)) {
	if ((*dbip = dbiOpenIndex(filename, mode, dbi)) == NULL)
	    return 1;
    }

    return 0;
}

static /*@only@*/ rpmdb newRpmdb(void)
{
    rpmdb db = xcalloc(sizeof(*db), 1);
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
    *filename = '\0';

    switch (urlIsURL(dbpath)) {
    case URL_IS_UNKNOWN:
	strcat(filename, prefix); 
	break;
    default:
	break;
    }
    strcat(filename, dbpath);
    (void)rpmCleanPath(filename);

    rpmMessage(RPMMESS_DEBUG, _("opening database mode 0x%x in %s\n"),
	mode, filename);

    if (filename[strlen(filename)-1] != '/')
	strcat(filename, "/");
    strcat(filename, "packages.rpm");

    db = newRpmdb();

    if (!justcheck || !rpmfileexists(filename)) {
	db->pkgs = fadOpen(filename, mode, perms);
	if (Ferror(db->pkgs)) {
	    rpmError(RPMERR_DBOPEN, _("failed to open %s: %s\n"), filename,
		Fstrerror(db->pkgs));
	    return 1;
	}

	/* try and get a lock - this is released by the kernel when we close
	   the file */
	lockinfo.l_whence = 0;
	lockinfo.l_start = 0;
	lockinfo.l_len = 0;
	
	if (mode & O_RDWR) {
	    lockinfo.l_type = F_WRLCK;
	    if (Fcntl(db->pkgs, F_SETLK, (void *) &lockinfo)) {
		rpmError(RPMERR_FLOCK, _("cannot get %s lock on database"), 
			 _("exclusive"));
		rpmdbClose(db);
		return 1;
	    } 
	} else {
	    lockinfo.l_type = F_RDLCK;
	    if (Fcntl(db->pkgs, F_SETLK, (void *) &lockinfo)) {
		rpmError(RPMERR_FLOCK, _("cannot get %s lock on database"), 
			 _("shared"));
		rpmdbClose(db);
		return 1;
	    } 
	}
    }

    {	int dbix;

	rc = 0;
	for (dbix = RPMDBI_MIN; rc == 0 && dbix < RPMDBI_MAX; dbix++) {
	    dbiIndex dbiTemplate;

	    dbiTemplate = rpmdbi + dbix;

	    rc = openDbFile(prefix, dbpath, dbiTemplate, justcheck, mode,
			&db->_dbi[dbix]);
	    if (dbix == 0) rc = 0;	/* XXX HACK */
	    if (rc)
		continue;

	    switch (dbix) {
	    case 1:
		if (minimal) {
		    *rpmdbp = xmalloc(sizeof(struct rpmdb_s));
		    if (rpmdbp)
			*rpmdbp = db;	/* structure assignment */
		    else
			rpmdbClose(db);
		    return 0;
		}
		break;
	    case 2:

    /* We used to store the fileindexes as complete paths, rather then
       plain basenames. Let's see which version we are... */
    /*
     * XXX FIXME: db->fileindex can be NULL under pathological (e.g. mixed
     * XXX db1/db2 linkage) conditions.
     */
		if (!justcheck && !dbiGetFirstKey(db->_dbi[RPMDBI_FILE], &akey)) {
		    if (strchr(akey, '/')) {
			rpmError(RPMERR_OLDDB, _("old format database is present; "
				"use --rebuilddb to generate a new format database"));
			rc |= 1;
		    }
		    xfree(akey);
		}
	    default:
		break;
	    }
	}
    }

    if (rc || justcheck || rpmdbp == NULL)
	rpmdbClose(db);
     else
	*rpmdbp = db;

     return rc;
}

static int doRpmdbOpen (const char * prefix, /*@out@*/ rpmdb * rpmdbp,
			int mode, int perms, int flags)
{
    const char * dbpath = rpmGetPath("%{_dbpath}", NULL);
    int rc;

    if (!(dbpath && dbpath[0] != '%')) {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	rc = 1;
    } else
    	rc = openDatabase(prefix, dbpath, rpmdbp, mode, perms, flags);
    xfree(dbpath);
    return rc;
}

/* XXX called from python/upgrade.c */
int rpmdbOpenForTraversal(const char * prefix, rpmdb * rpmdbp)
{
    return doRpmdbOpen(prefix, rpmdbp, O_RDONLY, 0644, RPMDB_FLAG_MINIMAL);
}

/* XXX called from python/rpmmodule.c */
int rpmdbOpen (const char * prefix, rpmdb *rpmdbp, int mode, int perms)
{
    return doRpmdbOpen(prefix, rpmdbp, mode, perms, 0);
}

int rpmdbInit (const char * prefix, int perms)
{
    rpmdb db;
    return doRpmdbOpen(prefix, &db, (O_CREAT | O_RDWR), perms, RPMDB_FLAG_JUSTCHECK);
}

void rpmdbClose (rpmdb db)
{
    int dbix;

    if (db->pkgs != NULL) Fclose(db->pkgs);
    for (dbix = RPMDBI_MAX; --dbix >= RPMDBI_MAX; ) {
	if (db->_dbi[dbix] == NULL)
	    continue;
    	dbiCloseIndex(db->_dbi[dbix]);
    	db->_dbi[dbix] = NULL;
    }
    free(db);
}

int rpmdbFirstRecNum(rpmdb db) {
    return fadFirstOffset(db->pkgs);
}

int rpmdbNextRecNum(rpmdb db, unsigned int lastOffset) {
    /* 0 at end */
    return fadNextOffset(db->pkgs, lastOffset);
}

static Header doGetRecord(rpmdb db, unsigned int offset, int pristine)
{
    Header h;
    const char ** fileNames;
    int fileCount = 0;
    int i;

    (void)Fseek(db->pkgs, offset, SEEK_SET);

    h = headerRead(db->pkgs, HEADER_MAGIC_NO);

    if (pristine || h == NULL) return h;

    /* the RPM used to build much of RH 5.1 could produce packages whose
       file lists did not have leading /'s. Now is a good time to fix
       that */

    /* If this tag isn't present, either no files are in the package or
       we're dealing with a package that has just the compressed file name
       list */
    if (!headerGetEntryMinMemory(h, RPMTAG_OLDFILENAMES, NULL, 
			   (void **) &fileNames, &fileCount)) return h;

    for (i = 0; i < fileCount; i++) 
	if (*fileNames[i] != '/') break;

    if (i == fileCount) {
	free(fileNames);
    } else {	/* bad header -- let's clean it up */
	const char ** newFileNames = alloca(sizeof(*newFileNames) * fileCount);
	for (i = 0; i < fileCount; i++) {
	    char * newFileName = alloca(strlen(fileNames[i]) + 2);
	    if (*fileNames[i] != '/') {
		newFileName[0] = '/';
		newFileName[1] = '\0';
	    } else
		newFileName[0] = '\0';
	    strcat(newFileName, fileNames[i]);
	    newFileNames[i] = newFileName;
	}

	free(fileNames);

	headerModifyEntry(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE, 
			  newFileNames, fileCount);
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
    int _use_falloc = rpmExpandNumeric("%{_db3_use_falloc}");
    dbiIndex dbi;

    if (!_use_falloc && (dbi = db->_dbi[RPMDBI_PACKAGES]) != NULL) {
	void * uh;
	size_t uhlen;
	int rc;

	rc = (*dbi->dbi_vec->get) (dbi, &offset, sizeof(offset), &uh, &uhlen);
	if (rc)
	    return NULL;
	return headerLoad(uh);
    }

    return doGetRecord(db, offset, 0);
}

int rpmdbFindByFile(rpmdb db, const char * filespec, dbiIndexSet * matches)
{
    const char * dirName;
    const char * baseName;
    fingerPrintCache fpc;
    fingerPrint fp1;
    dbiIndexSet allMatches = NULL;
    dbiIndexRecord rec = NULL;
    int i;
    int rc;

    *matches = NULL;
    if ((baseName = strrchr(filespec, '/')) != NULL) {
    	char * t;
	size_t len;

    	len = baseName - filespec + 1;
	t = strncpy(alloca(len + 1), filespec, len);
	t[len] = '\0';
	dirName = t;
	baseName++;
    } else {
	dirName = "";
	baseName = filespec;
    }

    fpc = fpCacheCreate(20);
    fp1 = fpLookup(fpc, dirName, baseName, 1);

    rc = dbiSearchIndex(db->_dbi[RPMDBI_FILE], baseName, &allMatches);
    if (rc) {
	dbiFreeIndexSet(allMatches);
	allMatches = NULL;
	fpCacheFree(fpc);
	return rc;
    }

    *matches = dbiCreateIndexSet();
    rec = dbiReturnIndexRecordInstance(0, 0);
    i = 0;
    while (i < allMatches->count) {
	const char ** baseNames, ** dirNames;
	int_32 * dirIndexes;
	unsigned int offset = dbiIndexRecordOffset(allMatches, i);
	unsigned int prevoff;
	Header h;

	if ((h = rpmdbGetRecord(db, offset)) == NULL) {
	    i++;
	    continue;
	}

	headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL, 
				(void **) &baseNames, NULL);
	headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL, 
				(void **) &dirIndexes, NULL);
	headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL, 
				(void **) &dirNames, NULL);

	do {
	    fingerPrint fp2;
	    int num = dbiIndexRecordFileNumber(allMatches, i);

	    fp2 = fpLookup(fpc, dirNames[dirIndexes[num]], baseNames[num], 1);
	    if (FP_EQUAL(fp1, fp2)) {
		rec->recOffset = dbiIndexRecordOffset(allMatches, i);
		rec->fileNumber = dbiIndexRecordFileNumber(allMatches, i);
		dbiAppendIndexRecord(*matches, rec);
	    }

	    prevoff = offset;
	    i++;
	    offset = dbiIndexRecordOffset(allMatches, i);
	} while (i < allMatches->count && 
		(i == 0 || offset == prevoff));

	free(baseNames);
	free(dirNames);
	headerFree(h);
    }

    if (rec) {
	dbiFreeIndexRecordInstance(rec);
	rec = NULL;
    }
    if (allMatches) {
	dbiFreeIndexSet(allMatches);
	allMatches = NULL;
    }

    fpCacheFree(fpc);

    if ((*matches)->count == 0) {
	dbiFreeIndexSet(*matches);
	*matches = NULL; 
	return 1;
    }

    return 0;
}

int rpmdbFindByProvides(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    return dbiSearchIndex(db->_dbi[RPMDBI_PROVIDES], filespec, matches);
}

int rpmdbFindByRequiredBy(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    return dbiSearchIndex(db->_dbi[RPMDBI_REQUIREDBY], filespec, matches);
}

int rpmdbFindByConflicts(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    return dbiSearchIndex(db->_dbi[RPMDBI_CONFLICTS], filespec, matches);
}

int rpmdbFindByTriggeredBy(rpmdb db, const char * filespec, dbiIndexSet * matches) {
    return dbiSearchIndex(db->_dbi[RPMDBI_TRIGGER], filespec, matches);
}

int rpmdbFindByGroup(rpmdb db, const char * group, dbiIndexSet * matches) {
    return dbiSearchIndex(db->_dbi[RPMDBI_GROUP], group, matches);
}

int rpmdbFindPackage(rpmdb db, const char * name, dbiIndexSet * matches) {
    return dbiSearchIndex(db->_dbi[RPMDBI_NAME], name, matches);
}

static void removeIndexEntry(dbiIndex dbi, const char * key, dbiIndexRecord rec,
		             int tolerant, const char * idxName)
{
    dbiIndexSet matches = NULL;
    int rc;
    
    rc = dbiSearchIndex(dbi, key, &matches);
    switch (rc) {
      case 0:
	if (dbiRemoveIndexRecord(matches, rec)) {
	    if (!tolerant)
		rpmError(RPMERR_DBCORRUPT, _("package not found with key \"%s\" in %s"),
				key, idxName);
	} else {
	    dbiUpdateIndex(dbi, key, matches);
	       /* errors from above will be reported from dbindex.c */
	}
	break;
      case 1:
	if (!tolerant) 
	    rpmError(RPMERR_DBCORRUPT, _("key \"%s\" not found in %s"), 
			key, idxName);
	break;
      case 2:
	break;   /* error message already generated from dbindex.c */
    }
    if (matches) {
	dbiFreeIndexSet(matches);
	matches = NULL;
    }
}

int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant)
{
    Header h;

    h = rpmdbGetRecord(db, offset);
    if (h == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("rpmdbRemove: cannot read header at 0x%x"),
	      offset);
	return 1;
    }

    blockSignals();

    {	int dbix;
	dbiIndexRecord rec = dbiReturnIndexRecordInstance(offset, 0);

	for (dbix = RPMDBI_MIN; dbix < RPMDBI_MAX; dbix++) {
	    dbiIndex dbi;
	    const char **rpmvals = NULL;
	    int rpmtype = 0;
	    int rpmcnt = 0;

	    dbi = db->_dbi[dbix];

if (_debug)
fprintf(stderr, "*** removing dbix %d tag %d offset 0x%x\n", dbix, dbi->dbi_rpmtag, offset);
	    if (dbi->dbi_rpmtag == 0) {
		/* XXX TODO: remove h to packages.rpm */
		(void) (*dbi->dbi_vec->del) (dbi, &offset, sizeof(offset));
		continue;
	    }
	
	    if (!headerGetEntry(h, dbi->dbi_rpmtag, &rpmtype,
		(void **) &rpmvals, &rpmcnt)) {
		rpmMessage(RPMMESS_DEBUG, _("removing 0 %s entries.\n"),
			tagName(dbi->dbi_rpmtag));
		continue;
	    }

	    if (rpmtype == RPM_STRING_TYPE) {
		rpmMessage(RPMMESS_DEBUG, _("removing \"%s\" from %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		removeIndexEntry(dbi, (const char *)rpmvals,
			rec, tolerant, dbi->dbi_basename);
	    } else {
		int i, mytolerant;

		rpmMessage(RPMMESS_DEBUG, _("removing %d entries in %s index:\n"), 
			rpmcnt, tagName(dbi->dbi_rpmtag));

		for (i = 0; i < rpmcnt; i++) {
		    rpmMessage(RPMMESS_DEBUG, _("\t%6d %s\n"),
			i, rpmvals[i]);

		    mytolerant = tolerant;
		    rec->fileNumber = 0;

		    switch (dbi->dbi_rpmtag) {
		    case RPMTAG_BASENAMES:
			rec->fileNumber = i;
			break;
		    /*
		     * There could be dups in the sorted list. Rather then
		     * sort the list, be tolerant of missing entries as they
		     * should just indicate duplicated entries.
		     */
		    case RPMTAG_REQUIRENAME:
		    case RPMTAG_TRIGGERNAME:
			mytolerant = 1;
			break;
		    }

		    removeIndexEntry(dbi, rpmvals[i],
			rec, mytolerant, dbi->dbi_basename);
		}
	    }

	    dbiSyncIndex(dbi);

	    switch (rpmtype) {
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		xfree(rpmvals);
		rpmvals = NULL;
		break;
	    }
	    rpmtype = 0;
	    rpmcnt = 0;
	}
	dbiFreeIndexRecordInstance(rec);
    }

    fadFree(db->pkgs, offset);

    unblockSignals();

    headerFree(h);

    return 0;
}

static int addIndexEntry(dbiIndex dbi, const char *index, dbiIndexRecord rec)
{
    dbiIndexSet set = NULL;
    int rc;

    rc = dbiSearchIndex(dbi, index, &set);

    switch (rc) {
    case -1:			/* error */
	if (set) {
	    dbiFreeIndexSet(set);
	    set = NULL;
	}
	return 1;
	/*@notreached@*/ break;
    case 1:			/* new item */
	set = dbiCreateIndexSet();
	break;
    default:
	break;
    }

    dbiAppendIndexRecord(set, rec);
    if (dbiUpdateIndex(dbi, index, set))
	exit(EXIT_FAILURE);	/* XXX W2DO? return 1; */

    if (set) {
	dbiFreeIndexSet(set);
	set = NULL;
    }

    return 0;
}

int rpmdbAdd(rpmdb db, Header h)
{
    const char ** baseNames;
    int count = 0;
    int type;
    unsigned int offset;
    int rc = 0;

    /*
     * If old style filename tags is requested, the basenames need to be
     * retrieved early, and the header needs to be converted before
     * being written to the package header database.
     */

    headerGetEntry(h, RPMTAG_BASENAMES, &type, (void **) &baseNames, &count);

    if (_noDirTokens)
	expandFilelist(h);

    blockSignals();

    {	int newSize;
	newSize = headerSizeof(h, HEADER_MAGIC_NO);
	offset = fadAlloc(db->pkgs, newSize);
	if (offset == 0) {
	    rc = 1;
	} else {
	    (void)Fseek(db->pkgs, offset, SEEK_SET);
	    fdSetContentLength(db->pkgs, newSize);
	    rc = headerWrite(db->pkgs, h, HEADER_MAGIC_NO);
	    fdSetContentLength(db->pkgs, -1);
	}

	if (rc) {
	    rpmError(RPMERR_DBCORRUPT, _("cannot allocate space for database"));
	    goto exit;
	}
    }

    /* Now update the indexes */

    {	int dbix;
	dbiIndexRecord rec = dbiReturnIndexRecordInstance(offset, 0);

	for (dbix = RPMDBI_MIN; dbix < RPMDBI_MAX; dbix++) {
	    dbiIndex dbi;
	    const char **rpmvals = NULL;
	    int rpmtype = 0;
	    int rpmcnt = 0;

	    dbi = db->_dbi[dbix];

if (_debug)
fprintf(stderr, "*** adding dbix %d tag %d offset 0x%x\n", dbix, dbi->dbi_rpmtag, offset);
	    if (dbi->dbi_rpmtag == 0) {
		size_t uhlen = headerSizeof(h, HEADER_MAGIC_NO);
		void * uh = headerUnload(h);
		/* XXX TODO: add h to packages.rpm */
		(void) (*dbi->dbi_vec->put) (dbi, &offset, sizeof(offset), uh, uhlen);
		free(uh);
		continue;
	    }
	
	    /* XXX preserve legacy behavior */
	    switch (dbi->dbi_rpmtag) {
	    case RPMTAG_BASENAMES:
		rpmtype = type;
		rpmvals = baseNames;
		rpmcnt = count;
		break;
	    default:
		headerGetEntry(h, dbi->dbi_rpmtag, &rpmtype,
			(void **) &rpmvals, &rpmcnt);
		break;
	    }

	    if (rpmcnt <= 0) {
		if (dbi->dbi_rpmtag != RPMTAG_GROUP) {
		    rpmMessage(RPMMESS_DEBUG, _("adding 0 %s entries.\n"),
			tagName(dbi->dbi_rpmtag));
		    continue;
		}

		/* XXX preserve legacy behavior */
		rpmtype = RPM_STRING_TYPE;
		rpmvals = (const char **) "Unknown";
		rpmcnt = 1;
	    }

	    if (rpmtype == RPM_STRING_TYPE) {
		rpmMessage(RPMMESS_DEBUG, _("adding \"%s\" to %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		rc += addIndexEntry(dbi, (const char *)rpmvals, rec);
	    } else {
		int i, j;

		rpmMessage(RPMMESS_DEBUG, _("adding %d entries to %s index:\n"), 
			rpmcnt, tagName(dbi->dbi_rpmtag));

		for (i = 0; i < rpmcnt; i++) {
		    rpmMessage(RPMMESS_DEBUG, _("\t%6d %s\n"),
			i, rpmvals[i]);

		    rec->fileNumber = 0;

		    switch (dbi->dbi_rpmtag) {
		    case RPMTAG_BASENAMES:
			rec->fileNumber = i;
			break;
		    case RPMTAG_TRIGGERNAME:	/* don't add duplicates */
			if (i == 0)
			   break;
			for (j = 0; j < i; j++) {
			    if (!strcmp(rpmvals[i], rpmvals[j]))
				break;
			}
			if (j < i)
			    continue;
			break;
		    }

		    rc += addIndexEntry(dbi, rpmvals[i], rec);
		}
	    }

	    dbiSyncIndex(dbi);

	    switch (rpmtype) {
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		xfree(rpmvals);
		rpmvals = NULL;
		break;
	    }
	    rpmtype = 0;
	    rpmcnt = 0;
	}
	dbiFreeIndexRecordInstance(rec);
    }

exit:
    unblockSignals();

    return rc;
}

int rpmdbUpdateRecord(rpmdb db, int offset, Header newHeader)
{
    Header oldHeader;
    int oldSize, newSize;
    int rc = 0;

    oldHeader = doGetRecord(db, offset, 1);
    if (oldHeader == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("cannot read header at %d for update"),
		offset);
	return 1;
    }

    oldSize = headerSizeof(oldHeader, HEADER_MAGIC_NO);
    headerFree(oldHeader);

    if (_noDirTokens)
	expandFilelist(newHeader);

    newSize = headerSizeof(newHeader, HEADER_MAGIC_NO);
    if (oldSize != newSize) {
	rpmMessage(RPMMESS_DEBUG, _("header changed size!"));
	if (rpmdbRemove(db, offset, 1))
	    return 1;

	if (rpmdbAdd(db, newHeader)) 
	    return 1;
    } else {
	blockSignals();

	(void)Fseek(db->pkgs, offset, SEEK_SET);
	fdSetContentLength(db->pkgs, newSize);
	rc = headerWrite(db->pkgs, newHeader, HEADER_MAGIC_NO);
	fdSetContentLength(db->pkgs, -1);

	unblockSignals();
    }

    return rc;
}

void rpmdbRemoveDatabase(const char * rootdir, const char * dbpath)
{ 
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

    {	dbiIndex dbi;
	for (dbi = rpmdbi; dbi->dbi_basename != NULL; dbi++) {
	    sprintf(filename, "%s/%s/%s", rootdir, dbpath, dbi->dbi_basename);
	    unlink(filename);
	}
    }

    sprintf(filename, "%s/%s", rootdir, dbpath);
    rmdir(filename);

}

int rpmdbMoveDatabase(const char * rootdir, const char * olddbpath, const char * newdbpath)
{
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

    {	dbiIndex dbi;
	for (dbi = rpmdbi; dbi->dbi_basename != NULL; dbi++) {
	    sprintf(ofilename, "%s/%s/%s", rootdir, olddbpath, dbi->dbi_basename);
	    sprintf(nfilename, "%s/%s/%s", rootdir, newdbpath, dbi->dbi_basename);
	    if (Rename(ofilename, nfilename)) rc = 1;
	}
    }

    return rc;
}

#ifdef	DYING
typedef struct intMatch {
    unsigned int recOffset;
    unsigned int fileNumber;
    int fpNum;
} IM_t;
#else
typedef	struct _dbiIndexRecord IM_t;
#endif

static int intMatchCmp(const void * one, const void * two)
{
    const IM_t * a = one;
    const IM_t * b = two;

    if (a->recOffset < b->recOffset)
	return -1;
    else if (a->recOffset > b->recOffset)
	return 1;

    return 0;
}

int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, dbiIndexSet * matchList, 
		    int numItems)
{
    int numIntMatches = 0;
    int intMatchesAlloced = numItems;
    IM_t * intMatches;
    int i, j;
    int start, end;
    int num;
    int_32 fc;
    const char ** dirNames, ** baseNames;
    const char ** fullBaseNames;
    int_32 * dirIndexes, * fullDirIndexes;
    fingerPrintCache fpc;
    dbiIndexRecord rec = NULL;

    /* this may be worth batching by baseName, but probably not as
       baseNames are quite unique as it is */

    intMatches = xcalloc(intMatchesAlloced, sizeof(*intMatches));

    /* Gather all matches from the database */
    for (i = 0; i < numItems; i++) {
	dbiIndexSet matches = NULL;
	switch (dbiSearchIndex(db->_dbi[RPMDBI_FILE], fpList[i].baseName, &matches)) {
	default:
	    break;
	case 2:
	    if (matches) {
		dbiFreeIndexSet(matches);
		matches = NULL;
	    }
	    free(intMatches);
	    return 1;
	    /*@notreached@*/ break;
	case 0:
	    if ((numIntMatches + matches->count) >= intMatchesAlloced) {
		intMatchesAlloced += matches->count;
		intMatchesAlloced += intMatchesAlloced / 5;
		intMatches = xrealloc(intMatches, 
				     sizeof(*intMatches) * intMatchesAlloced);
	    }

	    for (j = 0; j < matches->count; j++) {
		IM_t * im;
		
		im = intMatches + numIntMatches;
		im->recOffset = dbiIndexRecordOffset(matches, j);
		im->fileNumber = dbiIndexRecordFileNumber(matches, j);
		im->fpNum = i;
		numIntMatches++;
	    }

	    break;
	}
	if (matches) {
	    dbiFreeIndexSet(matches);
	    matches = NULL;
	}
    }

    qsort(intMatches, numIntMatches, sizeof(*intMatches), intMatchCmp);
    /* intMatches is now sorted by (recnum, filenum) */

    for (i = 0; i < numItems; i++)
	matchList[i] = dbiCreateIndexSet();

    fpc = fpCacheCreate(numIntMatches);

    rec = dbiReturnIndexRecordInstance(0, 0);

    /* For each set of files matched in a package ... */
    for (start = 0; start < numIntMatches; start = end) {
	IM_t * im;
	Header h;
	fingerPrint * fps;

	im = intMatches + start;

	/* Find the end of the set of matched files in this package. */
	for (end = start + 1; end < numIntMatches; end++) {
	    if (im->recOffset != intMatches[end].recOffset)
		break;
	}
	num = end - start;

	/* Compute fingerprints for each file match in this package. */
	h = rpmdbGetRecord(db, im->recOffset);
	if (h == NULL) {
	    free(intMatches);
	    return 1;
	}

	headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL, 
			    (void **) &dirNames, NULL);
	headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL, 
			    (void **) &fullBaseNames, &fc);
	headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL, 
			    (void **) &fullDirIndexes, NULL);

	baseNames = xcalloc(num, sizeof(*baseNames));
	dirIndexes = xcalloc(num, sizeof(*dirIndexes));
	for (i = 0; i < num; i++) {
	    baseNames[i] = fullBaseNames[im[i].fileNumber];
	    dirIndexes[i] = fullDirIndexes[im[i].fileNumber];
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
	    if (FP_EQUAL_DIFFERENT_CACHE(fps[i], fpList[j])) {
		rec->recOffset = im[i].recOffset;
		rec->fileNumber = im[i].fileNumber;
		dbiAppendIndexRecord(matchList[j], rec);
	    }
	}

	headerFree(h);

	free(fps);
    }

    dbiFreeIndexRecordInstance(rec);
    fpCacheFree(fpc);
    free(intMatches);

    return 0;
}
