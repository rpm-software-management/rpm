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

#define	DBC	void
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

static int db0sync(dbiIndex dbi, unsigned int flags) {
    DB * db = GetDB(dbi);
    int rc;

    rc = db->sync(db, flags);

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

static int db0byteswapped(dbiIndex dbi)
{
    return 0;
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

    rc = db->get(db, &key, &data, 0);

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

	rc = db->put(db, &key, &data, 0);

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

	rc = db->del(db, &key, 0);

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

    if (h == NULL)
	goto exit;

    /*
     * The RPM used to build much of RH 5.1 could produce packages whose
     * file lists did not have leading /'s. Now is a good time to fix that.
     */

    /*
     * If this tag isn't present, either no files are in the package or
     * we're dealing with a package that has just the compressed file name
     * list.
     */
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

    /*
     * The file list was moved to a more compressed format which not
     * only saves memory (nice), but gives fingerprinting a nice, fat
     * speed boost (very nice). Go ahead and convert old headers to
     * the new style (this is a noop for new headers).
     */
    compressFilelist(h);

exit:
    if (h) {
	uh = headerUnload(h);
	headerFree(h);
    }
    return uh;
}

static int db0copen(dbiIndex dbi) {
    int rc = 0;
    return rc;
}

static int db0cclose(dbiIndex dbi) {
    int rc = 0;
    return rc;
}

static int db0cget(dbiIndex dbi, void ** keyp, size_t * keylen,
                void ** datap, size_t * datalen)
{
    DBT key, data;
    int rc = 0;

    if (dbi == NULL)
	return 1;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    if (keyp)		key.data = *keyp;
    if (keylen)		key.size = *keylen;
    if (datap)		data.data = *datap;
    if (datalen)	data.size = *datalen;

    if (dbi->dbi_rpmtag == 0) {
	unsigned int offset;
	unsigned int newSize;

	if (key.data == NULL) {	/* XXX simulated DB_NEXT */
	    if (dbi->dbi_lastoffset == 0) {
		dbi->dbi_lastoffset = fadFirstOffset(dbi->dbi_pkgs);
	    } else {
		dbi->dbi_lastoffset = fadNextOffset(dbi->dbi_pkgs, dbi->dbi_lastoffset);
	    }
	    key.data = &dbi->dbi_lastoffset;
	    key.size = sizeof(dbi->dbi_lastoffset);
	}

	memcpy(&offset, key.data, sizeof(offset));
	/* XXX hack to pass sizeof header to fadAlloc */
	newSize = data.size;

	if (offset == 0) {	/* XXX simulated offset 0 record */
	    offset = fadAlloc(dbi->dbi_pkgs, newSize);
	    if (offset == 0)
		return -1;
	    offset--;	/* XXX hack: caller will increment */
	    /* XXX hack: return offset as data, free in db0cput */
	    data.data = xmalloc(sizeof(offset));
	    memcpy(data.data, &offset, sizeof(offset));
	    data.size = sizeof(offset);
	} else {		/* XXX simulated retrieval */
	    data.data = doGetRecord(dbi->dbi_pkgs, offset);
	    data.size = 0;	/* XXX WRONG */
	    if (data.data == NULL)
		rc = 1;
	}
    } else {
	DB * db;
	int _printit;

	if ((db = dbi->dbi_db) == NULL)
	    return 1;

	if (key.data == NULL) {
	    rc = db->seq(db, &key, &data, (dbi->dbi_lastoffset++ ? R_NEXT : R_FIRST));
	    _printit = (rc == 1 ? 0 : _debug);
	    rc = cvtdberr(dbi, "db->seq", rc, _printit);
	} else {
	    rc = db->get(db, &key, &data, 0);
	    _printit = (rc == 1 ? 0 : _debug);
	    rc = cvtdberr(dbi, "db0cget", rc, _printit);
	}
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
	if (keyp)	*keyp = key.data;
	if (keylen)	*keylen = key.size;
	if (datap)	*datap = data.data;
	if (datalen)	*datalen = data.size;
	break;
    }

    return rc;
}

static int db0cdel(dbiIndex dbi, void * keyp, size_t keylen)
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

#ifndef	DYING
static int db0get(dbiIndex dbi, void * keyp, size_t keylen,
		void ** datap, size_t * datalen)
{
    DBT key, data;
    int rc = 0;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    if (keyp)		key.data = keyp;
    if (keylen)		key.size = keylen;
    if (datap)		data.data = *datap;
    if (datalen)	data.size = *datalen;

    if (dbi->dbi_rpmtag == 0) {
	unsigned int offset;
	unsigned int newSize;

	memcpy(&offset, key.data, sizeof(offset));
	/* XXX hack to pass sizeof header to fadAlloc */
	newSize = data.size;

	if (offset == 0) {	/* XXX simulated offset 0 record */
	    offset = fadAlloc(dbi->dbi_pkgs, newSize);
	    if (offset == 0)
		return -1;
	    offset--;	/* XXX hack: caller will increment */
	    /* XXX hack: return offset as data, free in db0cput */
	    data.data = xmalloc(sizeof(offset));
	    memcpy(data.data, &offset, sizeof(offset));
	    data.size = sizeof(offset);
	} else {		/* XXX simulated retrieval */
	    data.data = doGetRecord(dbi->dbi_pkgs, offset);
	    data.size = 0;	/* XXX WRONG */
	    if (data.data == NULL)
		rc = 1;
	}
    } else {
	DB * db = GetDB(dbi);
	int _printit;

	rc = db->get(db, &key, &data, 0);
	_printit = (rc == 1 ? 0 : _debug);
	rc = cvtdberr(dbi, "db0get", rc, _printit);
    }

    if (rc == 0) {
	if (datap)	*datap = data.data;
	if (datalen)	*datalen = data.size;
    }

    return rc;
}
#endif	/* DYING */

static int db0cput(dbiIndex dbi, void * keyp, size_t keylen,
		void * datap, size_t datalen)
{
    DBT key, data;
    int rc = 0;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = keyp;
    key.size = keylen;
    data.data = datap;
    data.size = datalen;

    if (dbi->dbi_rpmtag == 0) {
	unsigned int offset;

	memcpy(&offset, key.data, sizeof(offset));

	if (offset == 0) {	/* XXX simulated offset 0 record */
	    /* XXX hack: return offset as data, free in db0cput */
	    if (data.size == sizeof(offset)) {
		free(data.data);
	    }
	} else {		/* XXX simulated DB_KEYLAST */
	    Header h = headerLoad(data.data);
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
	DB * db = GetDB(dbi);

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
    if (rc == 0 && dbip) {
	rc = 0;
	*dbip = dbi;
if (_debug < 0)
fprintf(stderr, "*** db%dopen: rpmtag %d dbi %p %s\n", dbi->dbi_major, rpmtag, dbi, urlfn);
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
    db0cdel, db0get, db0cput, db0copen, db0cclose, db0cdel, db0cget, db0cput,
    db0byteswapped
};
