/*@-type@*/ /* FIX: shrug */
/** \ingroup db1
 * \file rpmdb/db1.c
 */

#include "system.h"

#define	_mymemset(_a, _b, _c)

#include <rpmio_internal.h>
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX rpmGenPath */
#include <rpmurl.h>	/* XXX urlGetPath */

#include "falloc.h"
#include "legacy.h"	/* XXX providePackageNVR() and compressFileList() */
#include "rpmdb.h"

/* XXX must follow rpmdb.h */
#define	DB_VERSION_MAJOR	1
#define	DB_VERSION_MINOR	85
#define	DB_VERSION_PATCH	0

struct _DBT1 {
    void * data;	/* data */
    size_t size;	/* data length */
};

#undef	DBT
#define	DBT struct _DBT1

#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */
/*@access rpmdb@*/
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/
/*@-onlytrans@*/

#ifdef	DYING
/* XXX remap DB3 types back into DB1 types */
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

/*@-shadow@*/
static /*@observer@*/ char * db_strerror(int error)
/*@=shadow@*/
{
    if (error == 0)
	return ("Successful return: 0");
    if (error > 0)
	return (strerror(error));

    switch (error) {
    default:
      {
	/*
	 * !!!
	 * Room for a 64-bit number + slop.  This buffer is only used
	 * if we're given an unknown error, which should never happen.
	 * Note, however, we're no longer thread-safe if it does.
	 */
	static char ebuf[40];
	char * t = ebuf;

	*t = '\0';
	t = stpcpy(t, "Unknown error: ");
	sprintf(t, "%d", error);
	return(ebuf);
      }
    }
    /*@notreached@*/
}

static int cvtdberr(dbiIndex dbi, const char * msg, int error, int printit)
	/*@*/
{
    int rc = 0;

    if (error == 0)
	rc = 0;
    else if (error < 0)
	rc = errno;
    else if (error > 0)
	rc = -1;

    if (printit && rc) {
	if (msg)
	    rpmError(RPMERR_DBERR, _("db%d error(%d) from %s: %s\n"),
		dbi->dbi_api, rc, msg, db_strerror(error));
	else
	    rpmError(RPMERR_DBERR, _("db%d error(%d): %s\n"),
		dbi->dbi_api, rc, db_strerror(error));
    }

    return rc;
}
#endif	/* DYING */

static int db1sync(dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int rc = 0;

    if (dbi->dbi_db) {
	if (dbi->dbi_rpmtag == RPMDBI_PACKAGES) {
	    FD_t pkgs = dbi->dbi_db;
	    int fdno = Fileno(pkgs);
	    if (fdno >= 0 && (rc = fsync(fdno)) != 0)
		rc = errno;
	}
#ifdef	DYING
	else {
	    DB * db = dbi->dbi_db;
	    rc = db->sync(db, flags);
	    rc = cvtdberr(dbi, "db->sync", rc, _debug);
	}
#endif
    }

    return rc;
}

/*@null@*/ static void * doGetRecord(dbiIndex dbi, unsigned int offset)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    FD_t pkgs = dbi->dbi_db;
    void * uh = NULL;
    Header h = NULL;
    const char ** fileNames;
    int fileCount = 0;
    int lasto = 0;
    int i;

retry:
    if (offset >= fadGetFileSize(pkgs))
	goto exit;

    (void)Fseek(pkgs, offset, SEEK_SET);

    h = headerRead(pkgs, HEADER_MAGIC_NO);

    /* let's sanity check this record a bit, otherwise just skip it */
    if (h != NULL &&
	!(	headerIsEntry(h, RPMTAG_NAME) &&
		headerIsEntry(h, RPMTAG_VERSION) &&
		headerIsEntry(h, RPMTAG_RELEASE) &&
		headerIsEntry(h, RPMTAG_BUILDTIME)))
    {
	h = headerFree(h);
    }

    if (h == NULL) {
	/* XXX HACK: try to reconnect broken chain. */
	if (lasto == 0) {
	    rpmMessage(RPMMESS_WARNING,
  _("Broken package chain at offset %d(0x%08x), attempting to reconnect ...\n"),
			(int) offset, offset);
	    lasto = (offset ? offset : -1);
	    offset = fadNextOffset(pkgs, offset);
	    if (offset > 0)
		goto retry;
	}
	goto exit;
    }

    if (lasto) {
	rpmMessage(RPMMESS_WARNING,
		_("Reconnecting broken chain at offset %d(0x%08x).\n"),
		(int) offset, offset);
	dbi->dbi_lastoffset = offset;
    }

    /* Retrofit "Provide: name = EVR" for binary packages. */
    providePackageNVR(h);

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
			   (const void **) &fileNames, &fileCount))
	goto exit;

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

	(void) headerModifyEntry(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE, 
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
    /*@-branchstate@*/
    if (h != NULL) {
	uh = headerUnload(h);
	h = headerFree(h);
    }
    /*@=branchstate@*/
    return uh;
}

static int db1copen(/*@unused@*/ dbiIndex dbi,
		/*@unused@*/ DBC ** dbcp, unsigned int flags)
	/*@modifies *dbcp @*/
{
    /* XXX per-iterator cursors need to be set to non-NULL. */
    if (flags)
	*dbcp = (DBC *)-1;
    return 0;
}

static int db1cclose(dbiIndex dbi,
		/*@unused@*/ DBC * dbcursor, /*@unused@*/ unsigned int flags)
	/*@modifies dbi @*/
{
    dbi->dbi_lastoffset = 0;
    return 0;
}

/*@-compmempass@*/
static int db1cget(dbiIndex dbi, /*@unused@*/ DBC * dbcursor,
		/*@null@*/ void ** keyp,
		/*@null@*/ size_t * keylen, 
		/*@null@*/ void ** datap, 
		/*@null@*/ size_t * datalen,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *keyp, *keylen, *datap, *datalen, fileSystem @*/
{
    DBT key, data;
    int rc = 0;

    if (dbi == NULL)
	return EFAULT;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    /*@-unqualifiedtrans@*/
    if (keyp)		key.data = *keyp;
    if (keylen)		key.size = *keylen;
    if (datap)		data.data = *datap;
    if (datalen)	data.size = *datalen;
    /*@=unqualifiedtrans@*/

    if (dbi->dbi_rpmtag == RPMDBI_PACKAGES) {
	FD_t pkgs = dbi->dbi_db;
	unsigned int offset;
	unsigned int newSize;

	if (key.data == NULL) {	/* XXX simulated DB_NEXT */
	    if (dbi->dbi_lastoffset == 0) {
		dbi->dbi_lastoffset = fadFirstOffset(pkgs);
	    } else {
		dbi->dbi_lastoffset = fadNextOffset(pkgs, dbi->dbi_lastoffset);
	    }
	    /*@-immediatetrans@*/
	    key.data = &dbi->dbi_lastoffset;
	    /*@=immediatetrans@*/
	    key.size = sizeof(dbi->dbi_lastoffset);

	    /* Catch end-of-chain conditions. */
	    if (dbi->dbi_lastoffset == 0)
		goto bail;
	}

	memcpy(&offset, key.data, sizeof(offset));
	/* XXX hack to pass sizeof header to fadAlloc */
	newSize = data.size;

	if (offset == 0) {	/* XXX simulated offset 0 record */
	    offset = fadAlloc(pkgs, newSize);
	    if (offset == 0)
		return ENOMEM;
	    offset--;	/* XXX hack: caller will increment */
	    /* XXX hack: return offset as data, free in db1cput */
	    data.data = xmalloc(sizeof(offset));
	    memcpy(data.data, &offset, sizeof(offset));
	    data.size = sizeof(offset);
	} else {		/* XXX simulated retrieval */
	    data.data = doGetRecord(dbi, offset);
	    data.size = 0;	/* XXX WRONG */
	}
    }
#ifdef	DYING
    else {
	DB * db;
	int _printit;

	if ((db = dbi->dbi_db) == NULL)
	    return EFAULT;

	if (key.data == NULL) {
	    rc = db->seq(db, &key, &data, (dbi->dbi_lastoffset++ ? R_NEXT : R_FIRST));
	    _printit = (rc == 1 ? 0 : _debug);
	    rc = cvtdberr(dbi, "db->seq", rc, _printit);
	} else {
	    rc = db->get(db, &key, &data, 0);
	    _printit = (rc == 1 ? 0 : _debug);
	    rc = cvtdberr(dbi, "db1cget", rc, _printit);
	}
    }
#else
    else
	rc = EINVAL;
#endif

bail:
    if (rc == 0) {
	if (keyp)	*keyp = key.data;
	if (keylen)	*keylen = key.size;
	if (datap)	*datap = data.data;
	if (datalen)	*datalen = data.size;
    }

    /*@-nullstate@*/
    return rc;
    /*@=nullstate@*/
}
/*@=compmempass@*/

static int db1cdel(dbiIndex dbi, /*@unused@*/ DBC * dbcursor, const void * keyp,
		size_t keylen, /*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DBT key;
    int rc = 0;

    memset(&key, 0, sizeof(key));
    key.data = (void *)keyp;
    key.size = keylen;

    if (dbi->dbi_rpmtag == RPMDBI_PACKAGES) {
	FD_t pkgs = dbi->dbi_db;
	unsigned int offset;
	memcpy(&offset, keyp, sizeof(offset));
	fadFree(pkgs, offset);
    }
#ifdef	DYING
    else {
	DB * db = dbi->dbi_db;

	if (db)
	    rc = db->del(db, &key, 0);
	rc = cvtdberr(dbi, "db->del", rc, _debug);
    }
#else
    else
	rc = EINVAL;
#endif

    return rc;
}

static int db1cput(dbiIndex dbi, /*@unused@*/ DBC * dbcursor,
		const void * keyp, size_t keylen,
		const void * datap, size_t datalen,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, datap, fileSystem @*/
{
    DBT key, data;
    int rc = 0;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = (void *)keyp;
    key.size = keylen;
    data.data = (void *)datap;
    data.size = datalen;

    if (dbi->dbi_rpmtag == RPMDBI_PACKAGES) {
	FD_t pkgs = dbi->dbi_db;
	unsigned int offset;

	memcpy(&offset, key.data, sizeof(offset));

	if (offset == 0) {	/* XXX simulated offset 0 record */
	    /* XXX hack: return offset as data, free in db1cput */
	    if (data.size == sizeof(offset))
		/*@-unqualifiedtrans@*/ free(data.data); /*@=unqualifiedtrans@*/
	} else {		/* XXX simulated DB_KEYLAST */
	    Header h = headerLoad(data.data);
	    int newSize = headerSizeof(h, HEADER_MAGIC_NO);

	    (void)Fseek(pkgs, offset, SEEK_SET);
            fdSetContentLength(pkgs, newSize);
            rc = headerWrite(pkgs, h, HEADER_MAGIC_NO);
            fdSetContentLength(pkgs, -1);
	    if (rc)
		rc = EIO;
	    h = headerFree(h);
	}
    }
#ifdef	DYING
    else {
	DB * db = dbi->dbi_db;

	if (db)
	    rc = db->put(db, &key, &data, 0);
	rc = cvtdberr(dbi, "db->put", rc, _debug);
    }
#else
    else
	rc = EINVAL;
#endif

    return rc;
}

static int db1ccount(/*@unused@*/ dbiIndex dbi, /*@unused@*/ DBC * dbcursor,
		/*@unused@*/ /*@out@*/ unsigned int * countp,
		/*@unused@*/ unsigned int flags)
	/*@*/
{
    return EINVAL;
}

static int db1byteswapped(/*@unused@*/dbiIndex dbi)
	/*@*/
{
    return 0;
}

static int db1stat(/*@unused@*/ dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@*/
{
    return EINVAL;
}

static int db1close(/*@only@*/ dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals rpmGlobalMacroContext,
		fileSystem @*/
	/*@modifies dbi, rpmGlobalMacroContext, fileSystem @*/
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    const char * base = db1basename(dbi->dbi_rpmtag);
    const char * urlfn = rpmGenPath(rpmdb->db_root, rpmdb->db_home, base);
    const char * fn;
    int rc = 0;

    (void) urlPath(urlfn, &fn);

    /*@-branchstate@*/
    if (dbi->dbi_db) {
	if (dbi->dbi_rpmtag == RPMDBI_PACKAGES) {
	    FD_t pkgs = dbi->dbi_db;
	    rc = Fclose(pkgs);
	}
#ifdef	DYING
	else {
	    DB * db = dbi->dbi_db;
	    rc = db->close(db);
	    rc = cvtdberr(dbi, "db->close", rc, _debug);
	}
#else
	else
	    rc = EINVAL;
#endif
	dbi->dbi_db = NULL;
    }
    /*@=branchstate@*/

    rpmMessage(RPMMESS_DEBUG, _("closed  db file        %s\n"), urlfn);
    /* Remove temporary databases */
    if (dbi->dbi_temporary) {
	rpmMessage(RPMMESS_DEBUG, _("removed db file        %s\n"), urlfn);
	(void) unlink(fn);
    }

    dbi = db3Free(dbi);
    base = _free(base);
    urlfn = _free(urlfn);
    return rc;
}

static int db1open(rpmdb rpmdb, int rpmtag,
		/*@out@*/ dbiIndex * dbip)
	/*@globals rpmGlobalMacroContext,
		fileSystem @*/
	/*@modifies *dbip, rpmGlobalMacroContext, fileSystem @*/
{
    /*@-nestedextern@*/
    extern struct _dbiVec db1vec;
    /*@=nestedextern@*/
    const char * base = NULL;
    const char * urlfn = NULL;
    const char * fn = NULL;
    dbiIndex dbi = NULL;
    int rc = 0;

    if (dbip)
	*dbip = NULL;
    if ((dbi = db3New(rpmdb, rpmtag)) == NULL)
	return EFAULT;
    dbi->dbi_api = DB_VERSION_MAJOR;

    base = db1basename(rpmtag);
    urlfn = rpmGenPath(rpmdb->db_root, rpmdb->db_home, base);
    (void) urlPath(urlfn, &fn);
    if (!(fn && *fn != '\0')) {
	rpmError(RPMERR_DBOPEN, _("bad db file %s\n"), urlfn);
	rc = EFAULT;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("opening db file        %s mode 0x%x\n"),
		urlfn, dbi->dbi_mode);

    if (dbi->dbi_rpmtag == RPMDBI_PACKAGES) {
	FD_t pkgs;

	pkgs = fadOpen(fn, dbi->dbi_mode, dbi->dbi_perms);
	if (Ferror(pkgs)) {
	    rc = errno;		/* XXX check errno validity */
	    goto exit;
	}

	/* XXX HACK: fcntl lock if db3 (DB_INIT_CDB | DB_INIT_LOCK) specified */
	if (dbi->dbi_lockdbfd || (dbi->dbi_eflags & 0x30)) {
	    struct flock l;

	    l.l_whence = 0;
	    l.l_start = 0;
	    l.l_len = 0;
	    l.l_type = (dbi->dbi_mode & O_RDWR) ? F_WRLCK : F_RDLCK;

	    if (Fcntl(pkgs, F_SETLK, (void *) &l)) {
		rc = errno;	/* XXX check errno validity */
		rpmError(RPMERR_FLOCK, _("cannot get %s lock on database\n"),
		    ((dbi->dbi_mode & O_RDWR) ? _("exclusive") : _("shared")));
		goto exit;
	    }
	}

	dbi->dbi_db = pkgs;
    }
#ifdef	DYING
    else {
	void * dbopeninfo = NULL;
	int dbimode = dbi->dbi_mode;

	if (dbi->dbi_temporary)
	    dbimode |= (O_CREAT | O_RDWR);

	dbi->dbi_db = dbopen(fn, dbimode, dbi->dbi_perms,
		db3_to_dbtype(dbi->dbi_type), dbopeninfo);
	if (dbi->dbi_db == NULL) rc = errno;
    }
#else
    else
	rc = EINVAL;
#endif

exit:
    if (rc == 0 && dbi->dbi_db != NULL && dbip) {
	dbi->dbi_vec = &db1vec;
	if (dbip) *dbip = dbi;
    } else
	(void) db1close(dbi, 0);

    base = _free(base);
    urlfn = _free(urlfn);

    return rc;
}
/*@=onlytrans@*/

/** \ingroup db1
 */
/*@-exportheadervar@*/
/*@observer@*/ /*@unchecked@*/
struct _dbiVec db1vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db1open, db1close, db1sync, db1copen, db1cclose, db1cdel, db1cget, db1cput,
    db1ccount, db1byteswapped, db1stat
};
/*@=exportheadervar@*/
/*@=type@*/
