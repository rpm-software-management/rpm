#include "system.h"

static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */

#include <db1/db.h>

#define	DB_VERSION_MAJOR	0
#define	DB_VERSION_MINOR	0
#define	DB_VERSION_PATCH	0

#define	_mymemset(_a, _b, _c)

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX rpmGenPath */
#include <rpmurl.h>	/* XXX urlGetPath */
#include <rpmio.h>

#include "falloc.h"
#include "misc.h"

#include "rpmdb.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

/* XXX remap DB3 types back into DB0 types */
static inline DBTYPE db3_to_dbtype(int dbitype)
{
    switch(dbitype) {
    case 1:	return DB_BTREE;
    case 2:	return DB_HASH;
    case 3:	return DB_RECNO;
    case 4:	return DB_HASH;		/* XXX W2DO? */
    case 5:	return DB_HASH;		/* XXX W2DO? */
    }
    /*@notreached@*/ return DB_HASH;
}

char * db0basename (int rpmtag) {
    char * base = NULL;
    switch (rpmtag) {
    case 0:			base = "packages.rpm";		break;
    case RPMTAG_NAME:		base = "nameindex.rpm";		break;
    case RPMTAG_BASENAMES:	base = "fileindex.rpm";		break;
    case RPMTAG_GROUP:		base = "groupindex.rpm";	break;
    case RPMTAG_REQUIRENAME:	base = "requiredbyindex.rpm";	break;
    case RPMTAG_PROVIDENAME:	base = "providesindex.rpm";	break;
    case RPMTAG_CONFLICTNAME:	base = "conflictsindex.rpm";	break;
    case RPMTAG_TRIGGERNAME:	base = "triggerindex.rpm";	break;
    default:
      {	const char * tn = tagName(rpmtag);
	base = alloca( strlen(tn) + sizeof(".idx") + 1 );
	(void) stpcpy( stpcpy(base, tn), ".idx");
      }	break;
    }
    return xstrdup(base);
}

static inline /*@observer@*/ /*@null@*/ DB * GetDB(dbiIndex dbi) {
    return ((DB *)dbi->dbi_db);
}

static int cvtdberr(dbiIndex dbi, const char * msg, int error, int printit) {
    int rc = 0;

    if (error == 0)
	rc = 0;
    else if (error < 0)
	rc = -1;
    else if (error > 0)
	rc = 1;

    if (printit && rc) {
	fprintf(stderr, "*** db%d %s rc %d error %d\n", dbi->dbi_major, msg,
		rc, error);
    }

    return rc;
}

#if defined(__USE_DB2)
static int db_init(const char *home, int dbflags,
			DB_ENV **dbenvp, DB_INFO **dbinfop)
{
    DB_ENV *dbenv = xcalloc(1, sizeof(*dbenv));
    DB_INFO *dbinfo = xcalloc(1, sizeof(*dbinfo));
    int rc;

    if (dbenvp) *dbenvp = NULL;
    if (dbinfop) *dbinfop = NULL;

    dbenv->db_errfile = stderr;
    dbenv->db_errpfx = "rpmdb";
    dbenv->mp_size = 1024 * 1024;

    rc = db_appinit(home, NULL, dbenv, dbflags);
    if (rc)
	goto errxit;

    dbinfo->db_pagesize = 1024;

    if (dbenvp)
	*dbenvp = dbenv;
    else
	free(dbenv);

    if (dbinfop)
	*dbinfop = dbinfo;
    else
	free(dbinfo);

    return 0;

errxit:
    if (dbenv)	free(dbenv);
    if (dbinfo)	free(dbinfo);
    return rc;
}
#endif

static int db0sync(dbiIndex dbi, unsigned int flags) {
    DB * db = GetDB(dbi);
    int rc;

#if defined(__USE_DB2)
    rc = db->sync(db, flags);
#else
    rc = db->sync(db, flags);
#endif

    switch (rc) {
    default:
    case RET_ERROR:	/* -1 */
	rc = -1;
	break;
    case RET_SPECIAL:	/* 1 */
	rc = 1;
	break;
    case RET_SUCCESS:	/* 0 */
	rc = 0;
	break;
    }

    return rc;
}

static int db0SearchIndex(dbiIndex dbi, const void * str, size_t len,
		dbiIndexSet * set)
{
    DBT key, data;
    DB * db = GetDB(dbi);
    int rc;

    if (set) *set = NULL;
    if (len == 0) len = strlen(str);
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    key.data = (void *)str;
    key.size = len;
    data.data = NULL;
    data.size = 0;

#if defined(__USE_DB2)
    rc = db->get(db, NULL, &key, &data, 0);
#else
    rc = db->get(db, &key, &data, 0);
#endif

    switch (rc) {
    default:
    case RET_ERROR:	/* -1 */
	rc = -1;
	break;
    case RET_SPECIAL:	/* 1 */
	rc = 1;
	break;
    case RET_SUCCESS:	/* 0 */
	rc = 0;
	if (set) {
	    const char * sdbir = data.data;
	    int i;

	    *set = xmalloc(sizeof(**set));
	    (*set)->count = data.size / sizeof(struct _dbiIR);
	    (*set)->recs = xmalloc((*set)->count * sizeof(*((*set)->recs)));

	    /* Convert from database internal format. */
	    for (i = 0; i < (*set)->count; i++) {
		unsigned int recOffset, fileNumber;

		memcpy(&recOffset, sdbir, sizeof(recOffset));
		sdbir += sizeof(recOffset);
		memcpy(&fileNumber, sdbir, sizeof(fileNumber));
		sdbir += sizeof(fileNumber);

		/* XXX TODO: swab data */
		(*set)->recs[i].recOffset = recOffset;
		(*set)->recs[i].fileNumber = fileNumber;
		(*set)->recs[i].fpNum = 0;
		(*set)->recs[i].dbNum = 0;
	    }
	}
	break;
    }
    return rc;
}

/*@-compmempass@*/
static int db0UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set) {
    DBT key;
    DB * db = GetDB(dbi);
    int rc;

    _mymemset(&key, 0, sizeof(key));
    key.data = (void *)str;
    key.size = strlen(str);

    if (set->count) {
	DBT data;
	DBIR_t dbir = alloca(set->count * sizeof(*dbir));
	int i;

	/* Convert to database internal format */
	for (i = 0; i < set->count; i++) {
	    /* XXX TODO: swab data */
	    dbir[i].recOffset = set->recs[i].recOffset;
	    dbir[i].fileNumber = set->recs[i].fileNumber;
	}

	_mymemset(&data, 0, sizeof(data));
	data.data = dbir;
	data.size = set->count * sizeof(*dbir);

#if defined(__USE_DB2)
	rc = db->put(db, NULL, &key, &data, 0);
#else
	rc = db->put(db, &key, &data, 0);
#endif

	switch (rc) {
	default:
	case RET_ERROR:		/* -1 */
	case RET_SPECIAL:	/* 1 */
	    rc = 1;
	    break;
	case RET_SUCCESS:	/* 0 */
	    rc = 0;
	    break;
	}
    } else {

#if defined(__USE_DB2)
	rc = db->del(db, NULL, &key, 0);
#else
	rc = db->del(db, &key, 0);
#endif

	switch (rc) {
	default:
	case RET_ERROR:		/* -1 */
	case RET_SPECIAL:	/* 1 */
	    rc = 1;
	    break;
	case RET_SUCCESS:	/* 0 */
	    rc = 0;
	    break;
	}
    }

    return rc;
}
/*@=compmempass@*/

static void * doGetRecord(FD_t pkgs, unsigned int offset)
{
    void * uh = NULL;
    Header h;
    const char ** fileNames;
    int fileCount = 0;
    int i;

    (void)Fseek(pkgs, offset, SEEK_SET);

    h = headerRead(pkgs, HEADER_MAGIC_NO);

    if (h == NULL) return NULL;

    /* the RPM used to build much of RH 5.1 could produce packages whose
       file lists did not have leading /'s. Now is a good time to fix
       that */

    /* If this tag isn't present, either no files are in the package or
       we're dealing with a package that has just the compressed file name
       list */
    if (!headerGetEntryMinMemory(h, RPMTAG_OLDFILENAMES, NULL, 
			   (void **) &fileNames, &fileCount)) goto exit;

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

exit:
    uh = headerUnload(h);
    headerFree(h);
    return uh;
}

static int db0copen(dbiIndex dbi) {
    int rc = 0;
#if defined(__USE_DB2)
    {	DBC * dbcursor = NULL;
	rc = dbp->cursor(dbp, NULL, &dbcursor, 0);
	if (rc == 0)
	    dbi->dbi_dbcursor = dbcursor;
    }
#endif
    dbi->dbi_lastoffset = 0;
    return rc;
}

static int db0cclose(dbiIndex dbi) {
    int rc = 0;
#if defined(__USE_DB2)
#endif
    dbi->dbi_lastoffset = 0;
    return rc;
}

static int db0join(dbiIndex dbi) {
    int rc = 1;
    return rc;
}

static int db0cget(dbiIndex dbi, void ** keyp, size_t * keylen,
                void ** datap, size_t * datalen)
{
    DBT key, data;
    DB * db;
    int rc;

    if (dbi == NULL)
	return 1;

    if (dbi->dbi_pkgs) {
	if (dbi->dbi_lastoffset == 0) {
	    dbi->dbi_lastoffset = fadFirstOffset(dbi->dbi_pkgs);
	} else {
	    dbi->dbi_lastoffset = fadNextOffset(dbi->dbi_pkgs, dbi->dbi_lastoffset);
	}
	if (keyp)
	    *keyp = &dbi->dbi_lastoffset;
	if (keylen)
	    *keylen = sizeof(dbi->dbi_lastoffset);
	return 0;
    }

    if (dbi->dbi_db == NULL)
	return 1;
    db = GetDB(dbi);
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    key.data = NULL;
    key.size = 0;

#if defined(__USE_DB2)
    {	DBC * dbcursor;

	if ((dbcursor = dbi->dbi_dbcursor) == NULL) {
	    rc = db2copen(dbi);
	    if (rc)
		return rc;
	    dbcursor = dbi->dbi_dbcursor;
	}

	rc = dbcursor->c_get(dbcursor, &key, &data,
			(dbi->dbi_lastoffset++ ? DB_NEXT : DB_FIRST));
	if (rc == DB_NOTFOUND)
	    db2cclose(dbcursor)
    }
#else
    rc = db->seq(db, &key, &data, (dbi->dbi_lastoffset++ ? R_NEXT : R_FIRST));
#endif

    switch (rc) {
    default:
    case RET_ERROR:	/* -1 */
    case RET_SPECIAL:	/* 1 */
	rc = 1;
	if (keyp)
	    *keyp = NULL;
	break;
    case RET_SUCCESS:	/* 0 */
	rc = 0;
	if (keyp)
	    *keyp = key.data;
	break;
    }

    return rc;
}

static int db0del(dbiIndex dbi, void * keyp, size_t keylen)
{
    int rc = 0;

    if (dbi->dbi_rpmtag == 0) {
	unsigned int offset;
	memcpy(&offset, keyp, sizeof(offset));
	fadFree(dbi->dbi_pkgs, offset);
    } else {
	DBT key;
	DB * db = GetDB(dbi);

	_mymemset(&key, 0, sizeof(key));

	key.data = keyp;
	key.size = keylen;

	rc = db->del(db, &key, 0);
	rc = cvtdberr(dbi, "db->del", rc, _debug);
    }

    return rc;
}

static int db0get(dbiIndex dbi, void * keyp, size_t keylen,
		void ** datap, size_t * datalen)
{
    unsigned int newSize = 0;
    int rc = 0;

    if (datap) *datap = NULL;
    if (datalen) {
	/* XXX hack to pass sizeof header to fadAlloc */
	newSize = *datalen;
	*datalen = 0;
    }

    if (dbi->dbi_rpmtag == 0) {
	unsigned int offset;

	memcpy(&offset, keyp, sizeof(offset));

	if (offset == 0) {
	    offset = fadAlloc(dbi->dbi_pkgs, newSize);
	    if (offset == 0)
		return -1;
	    offset--;	/* XXX hack: caller will increment */
	    *datap = xmalloc(sizeof(offset));
	    memcpy(*datap, &offset, sizeof(offset));
	    *datalen = sizeof(offset);
	} else {
	    void * uh = doGetRecord(dbi->dbi_pkgs, offset);
	    if (uh == NULL)
		return 1;
	    if (datap)
		*datap = uh;
	    if (datalen)
		*datalen = 0;	/* XXX WRONG */
	}
    } else {
	DBT key, data;
	DB * db = GetDB(dbi);

	_mymemset(&key, 0, sizeof(key));
	_mymemset(&data, 0, sizeof(data));

	key.data = keyp;
	key.size = keylen;
	data.data = NULL;
	data.size = 0;

	rc = db->get(db, &key, &data, 0);
	rc = cvtdberr(dbi, "db->get", rc, _debug);

	if (rc == 0) {
	    *datap = data.data;
	    *datalen = data.size;
	}
    }

    return rc;
}

static int db0put(dbiIndex dbi, void * keyp, size_t keylen,
		void * datap, size_t datalen)
{
    int rc = 0;

    if (dbi->dbi_rpmtag == 0) {
	unsigned int offset;

	memcpy(&offset, keyp, sizeof(offset));

	if (offset == 0) {
	    if (datalen == sizeof(offset))
		free(datap);
	} else {
	    Header h = headerLoad(datap);
	    int newSize = headerSizeof(h, HEADER_MAGIC_NO);

	    (void)Fseek(dbi->dbi_pkgs, offset, SEEK_SET);
            fdSetContentLength(dbi->dbi_pkgs, newSize);
            rc = headerWrite(dbi->dbi_pkgs, h, HEADER_MAGIC_NO);
            fdSetContentLength(dbi->dbi_pkgs, -1);
	    if (rc)
		rc = -1;
	    headerFree(h);
	}
    } else {
	DBT key, data;
	DB * db = GetDB(dbi);

	_mymemset(&key, 0, sizeof(key));
	_mymemset(&data, 0, sizeof(data));

	key.data = keyp;
	key.size = keylen;
	data.data = datap;
	data.size = datalen;

	rc = db->put(db, &key, &data, 0);
	rc = cvtdberr(dbi, "db->put", rc, _debug);
    }

    return rc;
}

static int db0close(dbiIndex dbi, unsigned int flags) {
    DB * db = GetDB(dbi);
    int rc = 0;

    if (dbi->dbi_rpmtag == 0) {
	FD_t pkgs;
	if ((pkgs = dbi->dbi_pkgs) != NULL)
	    Fclose(pkgs);
	dbi->dbi_pkgs = NULL;
	db3Free(dbi);
	return 0;
    }

    if (db) {
	rc = db->close(db);
	dbi->dbi_db = NULL;
    }

    switch (rc) {
    default:
    case RET_ERROR:	/* -1 */
	rc = -1;
	break;
    case RET_SPECIAL:	/* 1 */
	rc = 1;
	break;
    case RET_SUCCESS:	/* 0 */
	rc = 0;
	break;
    }

    db3Free(dbi);
    return rc;
}

static int db0open(rpmdb rpmdb, int rpmtag, dbiIndex * dbip)
{
    const char * base = NULL;
    const char * urlfn = NULL;
    const char * fn = NULL;
    dbiIndex dbi = NULL;
    int rc = 0;

    if (dbip)
	*dbip = NULL;
    if ((dbi = db3New(rpmdb, rpmtag)) == NULL)
	return 1;

    base = db0basename(rpmtag);
    urlfn = rpmGenPath(rpmdb->db_root, rpmdb->db_home, base);
    (void) urlPath(urlfn, &fn);
    if (!(fn && *fn != '\0')) {
	rpmError(RPMERR_DBOPEN, _("bad db file %s"), urlfn);
	rc = 1;
	goto exit;
    }

    if (dbi->dbi_rpmtag == 0) {
	struct flock l;
	FD_t pkgs;

	rpmMessage(RPMMESS_DEBUG, _("opening database mode 0x%x in %s\n"),
		dbi->dbi_mode, fn);

	pkgs = fadOpen(fn, dbi->dbi_mode, dbi->dbi_perms);
	if (Ferror(pkgs)) {
	    rpmError(RPMERR_DBOPEN, _("failed to open %s: %s\n"), urlfn,
		Fstrerror(pkgs));
	    rc = 1;
	    goto exit;
	}

	l.l_whence = 0;
	l.l_start = 0;
	l.l_len = 0;
	l.l_type = (dbi->dbi_mode & O_RDWR) ? F_WRLCK : F_RDLCK;

	if (Fcntl(pkgs, F_SETLK, (void *) &l)) {
	    rpmError(RPMERR_FLOCK, _("cannot get %s lock on database"),
		((dbi->dbi_mode & O_RDWR) ? _("exclusive") : _("shared")));
	    rc = 1;
	    goto exit;
	}

	dbi->dbi_pkgs = pkgs;
    } else {
	void * dbopeninfo = NULL;

	dbi->dbi_db = dbopen(fn, dbi->dbi_mode, dbi->dbi_perms,
		db3_to_dbtype(dbi->dbi_type), dbopeninfo);
    }

exit:
    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	rc = 0;
	*dbip = dbi;
if (_debug < 0)
fprintf(stderr, "*** db%dopen: %s\n", dbi->dbi_major, urlfn);
    } else {
	rc = 1;
	db3Free(dbi);
    }

    if (base) {
	xfree(base);
	base = NULL;
    }
    if (urlfn) {
	xfree(urlfn);
	urlfn = NULL;
    }

    return rc;
}

struct _dbiVec db0vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db0open, db0close, db0sync, db0SearchIndex, db0UpdateIndex,
    db0del, db0get, db0put, db0copen, db0cclose, db0join, db0cget
};
