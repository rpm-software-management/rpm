/*@-type@*/ /* FIX: annotate db3 methods */
/** \ingroup db3
 * \file rpmdb/db3.c
 */

/*@unchecked@*/
static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */

#include "system.h"

#if defined(HAVE_FTOK) && defined(HAVE_SYS_IPC_H)
#include <sys/ipc.h>
#endif

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>	/* XXX urlPath proto */

#include <rpmdb.h>

#include "debug.h"

#if !defined(DB_CLIENT)	/* XXX db-4.2.42 retrofit */
#define	DB_CLIENT	DB_RPCCLIENT
#endif

/*@access rpmdb @*/
/*@access dbiIndex @*/
/*@access dbiIndexSet @*/

/** \ingroup dbi
 * Hash database statistics.
 */
/*@-fielduse@*/
struct dbiHStats_s {
    unsigned int hash_magic;	/*!< hash database magic number. */
    unsigned int hash_version;	/*!< version of the hash database. */
    unsigned int hash_nkeys;	/*!< no. of unique keys in the database. */
    unsigned int hash_ndata;	/*!< no. of key/data pairs in the database. */
    unsigned int hash_pagesize;	/*!< db page (and bucket) size, in bytes. */
    unsigned int hash_nelem;	/*!< estimated size of the hash table. */
    unsigned int hash_ffactor;	/*!< no. of items per bucket. */
    unsigned int hash_buckets;	/*!< no. of hash buckets. */
    unsigned int hash_free;	/*!< no. of pages on the free list. */
    unsigned int hash_bfree;	/*!< no. of bytes free on bucket pages. */
    unsigned int hash_bigpages;	/*!< no. of big key/data pages. */
    unsigned int hash_big_bfree;/*!< no. of bytes free on big item pages. */
    unsigned int hash_overflows;/*!< no. of overflow pages. */
    unsigned int hash_ovfl_free;/*!< no. of bytes free on overflow pages. */
    unsigned int hash_dup;	/*!< no. of duplicate pages. */
    unsigned int hash_dup_free;	/*!< no. bytes free on duplicate pages. */
};

/** \ingroup dbi
 * B-tree database statistics.
 */
struct dbiBStats_s {
    unsigned int bt_magic;	/*!< btree database magic. */
    unsigned int bt_version;	/*!< version of the btree database. */
    unsigned int bt_nkeys;	/*!< no. of unique keys in the database. */
    unsigned int bt_ndata;	/*!< no. of key/data pairs in the database. */
    unsigned int bt_pagesize;	/*!< database page size, in bytes. */
    unsigned int bt_minkey;	/*!< minimum keys per page. */
    unsigned int bt_re_len;	/*!< length of fixed-length records. */
    unsigned int bt_re_pad;	/*!< padding byte for fixed-length records. */
    unsigned int bt_levels;	/*!< no. of levels in the database. */
    unsigned int bt_int_pg;	/*!< no. of database internal pages. */
    unsigned int bt_leaf_pg;	/*!< no. of database leaf pages. */
    unsigned int bt_dup_pg;	/*!< no. of database duplicate pages. */
    unsigned int bt_over_pg;	/*!< no. of database overflow pages. */
    unsigned int bt_free;	/*!< no. of pages on the free list. */
    unsigned int bt_int_pgfree;	/*!< no. of bytes free in internal pages. */
    unsigned int bt_leaf_pgfree;/*!< no. of bytes free in leaf pages. */
    unsigned int bt_dup_pgfree;	/*!< no. of bytes free in duplicate pages. */
    unsigned int bt_over_pgfree;/*!< no. of bytes free in overflow pages. */
};
/*@=fielduse@*/

#ifdef	NOTNOW
static const char * bfstring(unsigned int x, const char * xbf)
{
    const char * s = xbf;
    static char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    static char buf[256];
    char * t, * te;
    unsigned radix;
    unsigned c, i, k;

    radix = (s != NULL ? *s++ : 16);

    if (radix <= 1 || radix >= 32)
	radix = 16;

    t = buf;
    switch (radix) {
    case 8:	*t++ = '0';	break;
    case 16:	*t++ = '0';	*t++ = 'x';	break;
    }

    i = 0;
    k = x;
    do { i++; k /= radix; } while (k);

    te = t + i;

    k = x;
    do { --i; t[i] = digits[k % radix]; k /= radix; } while (k);

    t = te;
    i = '<';
    if (s != NULL)
    while ((c = *s++) != '\0') {
	if (c > ' ') continue;

	k = (1 << (c - 1));
	if (!(x & k)) continue;

	if (t == te) *t++ = '=';

	*t++ = i;
	i = ',';
	while (*s > ' ')
	    *t++ = *s++;
    }
    if (t > te)	*t++ = '>';
    *t = '\0';
    return buf;
}

static const char * dbtFlags =
	"\20\1APPMALLOC\2ISSET\3MALLOC\4PARTIAL\5REALLOC\6USERMEM\7DUPOK";

static const char * dbenvOpenFlags =
	"\20\1CREATE\2NO_EXCEPTIONS\3FORCE\4NOMMAP\5RDONLY\6RECOVER\7THREAD\10TXN_NOSYNC\11USE_ENVIRON\12USE_ENVIRON_ROOT\13CDB\14LOCK\15LOG\16MPOOL\17TXN\20JOINENV\21LOCKDOWN\22PRIVATE\23RECOVER_FATAL\24SYSTEM_MEM";

static const char * dbOpenFlags =
	"\20\1CREATE\2NO_EXCEPTIONS\3FORCE\4NOMMAP\5RDONLY\6RECOVER\7THREAD\10TXN_NOSYNC\11USE_ENVIRON\12USE_ENVIRON_ROOT\13EXCL\14FCNTL_LOCKING\15RDWRMASTER\16TRUNCATE\17EXTENT\20APPLY_LOGREG";

static const char * dbenvSetFlags =
	"\20\1CREATE\2NO_EXCEPTIONS\3FORCE\4NOMMAP\5RDONLY\6RECOVER\7THREAD\10TXN_NOSYNC\11USE_ENVIRON\12USE_ENVIRON_ROOT\13CDB_ALLDB\14NOLOCKING\15NOPANIC\16PANIC_ENV\17REGION_INIT\20YIELDCPU";

static const char * dbSetFlags =
	"\20\1DUP\2DUPSORT\3RECNUM\4RENUMBER\5REVSPLITOFF\6SNAPSHOT";

static const char * dbiModeFlags =
	"\20\1WRONLY\2RDWR\7CREAT\10EXCL\11NOCTTY\12TRUNC\13APPEND\14NONBLOCK\15SYNC\16ASYNC\17DIRECT\20LARGEFILE\21DIRECTORY\22NOFOLLOW";
#endif	/* NOTNOW */


/*@-globuse -mustmod @*/	/* FIX: rpmError not annotated yet. */
static int cvtdberr(dbiIndex dbi, const char * msg, int error, int printit)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int rc = error;

    if (printit && rc) {
	/*@-moduncon@*/ /* FIX: annotate db3 methods */
	if (msg)
	    rpmError(RPMERR_DBERR, _("db%d error(%d) from %s: %s\n"),
		dbi->dbi_api, rc, msg, db_strerror(error));
	else
	    rpmError(RPMERR_DBERR, _("db%d error(%d): %s\n"),
		dbi->dbi_api, rc, db_strerror(error));
	/*@=moduncon@*/
    }

    return rc;
}
/*@=globuse =mustmod @*/

static int db_fini(dbiIndex dbi, const char * dbhome,
		/*@null@*/ const char * dbfile,
		/*@unused@*/ /*@null@*/ const char * dbsubfile)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    DB_ENV * dbenv = rpmdb->db_dbenv;
    int rc;

    if (dbenv == NULL)
	return 0;

    rc = dbenv->close(dbenv, 0);
    rc = cvtdberr(dbi, "dbenv->close", rc, _debug);

    if (dbfile)
	rpmMessage(RPMMESS_DEBUG, _("closed   db environment %s/%s\n"),
			dbhome, dbfile);

    if (rpmdb->db_remove_env) {
	int xx;

	/*@-moduncon@*/ /* FIX: annotate db3 methods */
	xx = db_env_create(&dbenv, 0);
	/*@=moduncon@*/
	xx = cvtdberr(dbi, "db_env_create", xx, _debug);
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR != 0) || (DB_VERSION_MAJOR == 4)
	xx = dbenv->remove(dbenv, dbhome, 0);
#else
	xx = dbenv->remove(dbenv, dbhome, NULL, 0);
#endif
	xx = cvtdberr(dbi, "dbenv->remove", xx, _debug);

	if (dbfile)
	    rpmMessage(RPMMESS_DEBUG, _("removed  db environment %s/%s\n"),
			dbhome, dbfile);

    }
    return rc;
}

static int db3_fsync_disable(/*@unused@*/ int fd)
	/*@*/
{
    return 0;
}

#if 0
#if HAVE_LIBPTHREAD
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

/**
 * Check that posix mutexes are shared.
 * @return		0 == shared.
 */
static int db3_pthread_nptl(void)
	/*@*/
{
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexattr, *mutexattrp = NULL;
    pthread_cond_t cond;
    pthread_condattr_t condattr, *condattrp = NULL;
    int ret = 0;

    ret = pthread_mutexattr_init(&mutexattr);
    if (ret == 0) {
	ret = pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
	mutexattrp = &mutexattr;
    }

    if (ret == 0)
	ret = pthread_mutex_init(&mutex, mutexattrp);
    if (mutexattrp != NULL)
	pthread_mutexattr_destroy(mutexattrp);
    if (ret)
	return ret;
    (void) pthread_mutex_destroy(&mutex);

    ret = pthread_condattr_init(&condattr);
    if (ret == 0) {
	ret = pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
	condattrp = &condattr;
    }

    if (ret == 0)
	ret = pthread_cond_init(&cond, condattrp);

    if (condattrp != NULL)
	(void)pthread_condattr_destroy(condattrp);
    if (ret == 0)
	(void) pthread_cond_destroy(&cond);
    return ret;
}
#endif
#endif

/*@-moduncon@*/ /* FIX: annotate db3 methods */
static int db_init(dbiIndex dbi, const char * dbhome,
		/*@null@*/ const char * dbfile,
		/*@unused@*/ /*@null@*/ const char * dbsubfile,
		/*@out@*/ DB_ENV ** dbenvp)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem @*/
	/*@modifies dbi, *dbenvp, fileSystem @*/
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    DB_ENV *dbenv = NULL;
    int eflags;
    int rc;

    if (dbenvp == NULL)
	return 1;

    /* XXX HACK */
    /*@-assignexpose@*/
    if (rpmdb->db_errfile == NULL)
	rpmdb->db_errfile = stderr;
    /*@=assignexpose@*/

    eflags = (dbi->dbi_oeflags | dbi->dbi_eflags);
    if (eflags & DB_JOINENV) eflags &= DB_JOINENV;

    if (dbfile)
	rpmMessage(RPMMESS_DEBUG, _("opening  db environment %s/%s %s\n"),
		dbhome, dbfile, prDbiOpenFlags(eflags, 1));

    /* XXX Can't do RPC w/o host. */
    if (dbi->dbi_host == NULL)
	dbi->dbi_ecflags &= ~DB_CLIENT;

    /* XXX Set a default shm_key. */
    if ((dbi->dbi_eflags & DB_SYSTEM_MEM) && dbi->dbi_shmkey == 0) {
#if defined(HAVE_FTOK)
	dbi->dbi_shmkey = ftok(dbhome, 0);
#else
	dbi->dbi_shmkey = 0x44631380;
#endif
    }

    rc = db_env_create(&dbenv, dbi->dbi_ecflags);
    rc = cvtdberr(dbi, "db_env_create", rc, _debug);
    if (dbenv == NULL || rc)
	goto errxit;

  { int xx;
    /*@-noeffectuncon@*/ /* FIX: annotate db3 methods */

 /* 4.1: dbenv->set_app_dispatch(???) */
 /* 4.1: dbenv->set_alloc(???) */
 /* 4.1: dbenv->set_data_dir(???) */
 /* 4.1: dbenv->set_encrypt(???) */

    dbenv->set_errcall(dbenv, rpmdb->db_errcall);
    dbenv->set_errfile(dbenv, rpmdb->db_errfile);
    dbenv->set_errpfx(dbenv, rpmdb->db_errpfx);
    /*@=noeffectuncon@*/

 /* 4.1: dbenv->set_feedback(???) */
 /* 4.1: dbenv->set_flags(???) */

 /* dbenv->set_paniccall(???) */

    if ((dbi->dbi_ecflags & DB_CLIENT) && dbi->dbi_host) {
	const char * home;
	int retry = 0;

	if ((home = strrchr(dbhome, '/')) != NULL)
	    dbhome = ++home;

	while (retry++ < 5) {
/* XXX 3.3.4 change. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3) || (DB_VERSION_MAJOR == 4)
	    xx = dbenv->set_rpc_server(dbenv, NULL, dbi->dbi_host,
		dbi->dbi_cl_timeout, dbi->dbi_sv_timeout, 0);
	    xx = cvtdberr(dbi, "dbenv->set_server", xx, _debug);
#else
	    xx = dbenv->set_server(dbenv, dbi->dbi_host,
		dbi->dbi_cl_timeout, dbi->dbi_sv_timeout, 0);
	    xx = cvtdberr(dbi, "dbenv->set_server", xx, _debug);
#endif
	    if (!xx)
		break;
	    (void) sleep(15);
	}
    } else {
#if !(DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
	xx = dbenv->set_verbose(dbenv, DB_VERB_CHKPOINT,
		(dbi->dbi_verbose & DB_VERB_CHKPOINT));
#endif
	xx = dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK,
		(dbi->dbi_verbose & DB_VERB_DEADLOCK));
	xx = dbenv->set_verbose(dbenv, DB_VERB_RECOVERY,
		(dbi->dbi_verbose & DB_VERB_RECOVERY));
	xx = dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR,
		(dbi->dbi_verbose & DB_VERB_WAITSFOR));

	if (dbi->dbi_mmapsize) {
	    xx = dbenv->set_mp_mmapsize(dbenv, dbi->dbi_mmapsize);
	    xx = cvtdberr(dbi, "dbenv->set_mp_mmapsize", xx, _debug);
	}
	if (dbi->dbi_tmpdir) {
	    const char * root;
	    const char * tmpdir;

	    root = (dbi->dbi_root ? dbi->dbi_root : rpmdb->db_root);
/*@-boundsread@*/
	    if ((root[0] == '/' && root[1] == '\0') || rpmdb->db_chrootDone)
		root = NULL;
/*@=boundsread@*/
/*@-mods@*/
	    tmpdir = rpmGenPath(root, dbi->dbi_tmpdir, NULL);
/*@=mods@*/
	    xx = dbenv->set_tmp_dir(dbenv, tmpdir);
	    xx = cvtdberr(dbi, "dbenv->set_tmp_dir", xx, _debug);
	    tmpdir = _free(tmpdir);
	}
    }

 /* dbenv->set_lk_conflicts(???) */
 /* dbenv->set_lk_detect(???) */
 /* 4.1: dbenv->set_lk_max_lockers(???) */
 /* 4.1: dbenv->set_lk_max_locks(???) */
 /* 4.1: dbenv->set_lk_max_objects(???) */

 /* 4.1: dbenv->set_lg_bsize(???) */
 /* 4.1: dbenv->set_lg_dir(???) */
 /* 4.1: dbenv->set_lg_max(???) */
 /* 4.1: dbenv->set_lg_regionmax(???) */

    if (dbi->dbi_cachesize) {
	xx = dbenv->set_cachesize(dbenv, 0, dbi->dbi_cachesize, 0);
	xx = cvtdberr(dbi, "dbenv->set_cachesize", xx, _debug);
    }

 /* 4.1 dbenv->set_timeout(???) */
 /* dbenv->set_tx_max(???) */
 /* 4.1: dbenv->set_tx_timestamp(???) */
 /* dbenv->set_tx_recover(???) */

 /* dbenv->set_rep_transport(???) */
 /* dbenv->set_rep_limit(???) */

    if (dbi->dbi_no_fsync) {
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR != 0) || (DB_VERSION_MAJOR == 4)
	xx = db_env_set_func_fsync(db3_fsync_disable);
#else
	xx = dbenv->set_func_fsync(dbenv, db3_fsync_disable);
#endif
	xx = cvtdberr(dbi, "db_env_set_func_fsync", xx, _debug);
    }

    if (dbi->dbi_shmkey) {
	xx = dbenv->set_shm_key(dbenv, dbi->dbi_shmkey);
	xx = cvtdberr(dbi, "dbenv->set_shm_key", xx, _debug);
    }
  }

#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR != 0) || (DB_VERSION_MAJOR == 4)
    rc = dbenv->open(dbenv, dbhome, eflags, dbi->dbi_perms);
#else
    rc = dbenv->open(dbenv, dbhome, NULL, eflags, dbi->dbi_perms);
#endif
    rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
    if (rc)
	goto errxit;

/*@-boundswrite@*/
    *dbenvp = dbenv;
/*@=boundswrite@*/

    return 0;

errxit:
    if (dbenv) {
	int xx;
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
    }
    return rc;
}
/*@=moduncon@*/

static int db3sync(dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc = 0;
    int _printit;

    if (db != NULL)
	rc = db->sync(db, flags);
    /* XXX DB_INCOMPLETE is returned occaisionally with multiple access. */
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    _printit = _debug;
#else
    _printit = (rc == DB_INCOMPLETE ? 0 : _debug);
#endif
    rc = cvtdberr(dbi, "db->sync", rc, _printit);
    return rc;
}

static int db3cdup(dbiIndex dbi, DBC * dbcursor, DBC ** dbcp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcp, fileSystem @*/
{
    int rc;

/*@-boundswrite@*/
    if (dbcp) *dbcp = NULL;
/*@=boundswrite@*/
    rc = dbcursor->c_dup(dbcursor, dbcp, flags);
    rc = cvtdberr(dbi, "dbcursor->c_dup", rc, _debug);
    /*@-nullstate @*/ /* FIX: *dbcp can be NULL */
    return rc;
    /*@=nullstate @*/
}

/*@-mustmod@*/
static int db3cclose(dbiIndex dbi, /*@only@*/ /*@null@*/ DBC * dbcursor,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    int rc = -2;

    /* XXX db3copen error pathways come through here. */
    if (dbcursor != NULL) {
	rc = dbcursor->c_close(dbcursor);
	rc = cvtdberr(dbi, "dbcursor->c_close", rc, _debug);
    }
    return rc;
}
/*@=mustmod@*/

static int db3copen(dbiIndex dbi, DB_TXN * txnid,
		/*@null@*/ /*@out@*/ DBC ** dbcp, unsigned int dbiflags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DBC * dbcursor = NULL;
    int flags;
    int rc;

   /* XXX DB_WRITECURSOR cannot be used with sunrpc dbenv. */
    assert(db != NULL);
    if ((dbiflags & DB_WRITECURSOR) &&
	(dbi->dbi_eflags & DB_INIT_CDB) && !(dbi->dbi_oflags & DB_RDONLY))
    {
	flags = DB_WRITECURSOR;
    } else
	flags = 0;

    rc = db->cursor(db, txnid, &dbcursor, flags);
    rc = cvtdberr(dbi, "db->cursor", rc, _debug);

    if (dbcp)
	/*@-boundswrite -onlytrans@*/ *dbcp = dbcursor; /*@=boundswrite =onlytrans@*/
    else
	(void) db3cclose(dbi, dbcursor, 0);

    return rc;
}

static int db3cput(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc;

    assert(db != NULL);
    if (dbcursor == NULL) {
	rc = db->put(db, dbi->dbi_txnid, key, data, 0);
	rc = cvtdberr(dbi, "db->put", rc, _debug);
    } else {
	rc = dbcursor->c_put(dbcursor, key, data, DB_KEYLAST);
	rc = cvtdberr(dbi, "dbcursor->c_put", rc, _debug);
    }

    return rc;
}

/*@-mustmod@*/
static int db3cdel(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc;

    assert(db != NULL);
    if (dbcursor == NULL) {
	rc = db->del(db, dbi->dbi_txnid, key, flags);
	rc = cvtdberr(dbi, "db->del", rc, _debug);
    } else {
	int _printit;

	/* XXX TODO: insure that cursor is positioned with duplicates */
	rc = dbcursor->c_get(dbcursor, key, data, DB_SET);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "dbcursor->c_get", rc, _printit);

	if (rc == 0) {
	    rc = dbcursor->c_del(dbcursor, flags);
	    rc = cvtdberr(dbi, "dbcursor->c_del", rc, _debug);
	}
    }

    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3cget(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *data, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int _printit;
    int rc;

    assert(db != NULL);
    if (dbcursor == NULL) {
	/* XXX duplicates require cursors. */
	rc = db->get(db, dbi->dbi_txnid, key, data, 0);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "db->get", rc, _printit);
    } else {
	/* XXX db3 does DB_FIRST on uninitialized cursor */
	rc = dbcursor->c_get(dbcursor, key, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "dbcursor->c_get", rc, _printit);
    }

    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3cpget(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * pkey,
		DBT * data, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *data, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int _printit;
    int rc;

    assert(db != NULL);
    assert(dbcursor != NULL);

    /* XXX db3 does DB_FIRST on uninitialized cursor */
    rc = dbcursor->c_pget(dbcursor, key, pkey, data, flags);
    /* XXX DB_NOTFOUND can be returned */
    _printit = (rc == DB_NOTFOUND ? 0 : _debug);
    rc = cvtdberr(dbi, "dbcursor->c_pget", rc, _printit);

    return rc;
}
/*@=mustmod@*/

static int db3ccount(dbiIndex dbi, DBC * dbcursor,
		/*@null@*/ /*@out@*/ unsigned int * countp,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *countp, fileSystem @*/
{
    db_recno_t count = 0;
    int rc = 0;

    flags = 0;
    rc = dbcursor->c_count(dbcursor, &count, flags);
    rc = cvtdberr(dbi, "dbcursor->c_count", rc, _debug);
    if (rc) return rc;
/*@-boundswrite@*/
    if (countp) *countp = count;
/*@=boundswrite@*/

    return rc;
}

static int db3byteswapped(dbiIndex dbi)	/*@*/
{
    DB * db = dbi->dbi_db;
    int rc = 0;

    if (db != NULL) {
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3 && DB_VERSION_PATCH == 11) \
 || (DB_VERSION_MAJOR == 4)
	int isswapped = 0;
	rc = db->get_byteswapped(db, &isswapped);
	if (rc == 0)
	    rc = isswapped;
#else
	rc = db->get_byteswapped(db);
#endif
    }

    return rc;
}

static int db3stat(dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
    DB_TXN * txnid = NULL;
#endif
    int rc = 0;

    assert(db != NULL);
#if defined(DB_FAST_STAT)
    if (flags)
	flags = DB_FAST_STAT;
    else
#endif
	flags = 0;
    dbi->dbi_stats = _free(dbi->dbi_stats);
/* XXX 3.3.4 change. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3) || (DB_VERSION_MAJOR == 4)
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
    rc = db->stat(db, txnid, &dbi->dbi_stats, flags);
#else
    rc = db->stat(db, &dbi->dbi_stats, flags);
#endif
#else
    rc = db->stat(db, &dbi->dbi_stats, NULL, flags);
#endif
    rc = cvtdberr(dbi, "db->stat", rc, _debug);
    return rc;
}

/*@-mustmod@*/
static int db3associate(dbiIndex dbi, dbiIndex dbisecondary,
		int (*callback)(DB *, const DBT *, const DBT *, DBT *),
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB * secondary = dbisecondary->dbi_db;
    int rc;

/*@-moduncon@*/ /* FIX: annotate db3 methods */
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    DB_TXN * txnid = NULL;

assert(db != NULL);
    rc = db->associate(db, txnid, secondary, callback, flags);
#else
assert(db != NULL);
    rc = db->associate(db, secondary, callback, flags);
#endif
/*@=moduncon@*/
    rc = cvtdberr(dbi, "db->associate", rc, _debug);
    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3join(dbiIndex dbi, DBC ** curslist, DBC ** dbcp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc;

assert(db != NULL);
/*@-moduncon@*/ /* FIX: annotate db3 methods */
    rc = db->join(db, curslist, dbcp, flags);
/*@=moduncon@*/
    rc = cvtdberr(dbi, "db->join", rc, _debug);
    return rc;
}
/*@=mustmod@*/

/*@-moduncon@*/ /* FIX: annotate db3 methods */
static int db3close(/*@only@*/ dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    const char * urlfn = NULL;
    const char * root;
    const char * home;
    const char * dbhome;
    const char * dbfile;
    const char * dbsubfile;
    DB * db = dbi->dbi_db;
    int _printit;
    int rc = 0, xx;

    flags = 0;	/* XXX unused */

    /*
     * Get the prefix/root component and directory path.
     */
    root = (dbi->dbi_root ? dbi->dbi_root : rpmdb->db_root);
/*@-boundsread@*/
    if ((root[0] == '/' && root[1] == '\0') || rpmdb->db_chrootDone)
	root = NULL;
/*@=boundsread@*/
    home = (dbi->dbi_home ? dbi->dbi_home : rpmdb->db_home);

    /*
     * Either the root or directory components may be a URL. Concatenate,
     * convert the URL to a path, and add the name of the file.
     */
    /*@-mods@*/
    urlfn = rpmGenPath(root, home, NULL);
    /*@=mods@*/
    (void) urlPath(urlfn, &dbhome);
    if (dbi->dbi_temporary) {
	dbfile = NULL;
	dbsubfile = NULL;
    } else {
#ifdef	HACK	/* XXX necessary to support dbsubfile */
	dbfile = (dbi->dbi_file ? dbi->dbi_file : db3basename);
	dbsubfile = (dbi->dbi_subfile ? dbi->dbi_subfile : tagName(dbi->dbi_rpmtag));
#else
	dbfile = (dbi->dbi_file ? dbi->dbi_file : tagName(dbi->dbi_rpmtag));
	dbsubfile = NULL;
#endif
    }

    if (db) {
	rc = db->close(db, 0);
	/* XXX ignore not found error messages. */
	_printit = (rc == ENOENT ? 0 : _debug);
	rc = cvtdberr(dbi, "db->close", rc, _printit);
	db = dbi->dbi_db = NULL;

	rpmMessage(RPMMESS_DEBUG, _("closed   db index       %s/%s\n"),
		dbhome, (dbfile ? dbfile : tagName(dbi->dbi_rpmtag)));

    }

    if (rpmdb->db_dbenv != NULL && dbi->dbi_use_dbenv) {
	if (rpmdb->db_opens == 1) {
	    /*@-nullstate@*/
	    xx = db_fini(dbi, (dbhome ? dbhome : ""), dbfile, dbsubfile);
	    /*@=nullstate@*/
	    rpmdb->db_dbenv = NULL;
	}
	rpmdb->db_opens--;
    }

    if (dbi->dbi_verify_on_close && !dbi->dbi_temporary) {
	DB_ENV * dbenv = NULL;

	/*@-moduncon@*/ /* FIX: annotate db3 methods */
	rc = db_env_create(&dbenv, 0);
	/*@=moduncon@*/
	rc = cvtdberr(dbi, "db_env_create", rc, _debug);
	if (rc || dbenv == NULL) goto exit;

	/*@-noeffectuncon@*/ /* FIX: annotate db3 methods */
	dbenv->set_errcall(dbenv, rpmdb->db_errcall);
	dbenv->set_errfile(dbenv, rpmdb->db_errfile);
	dbenv->set_errpfx(dbenv, rpmdb->db_errpfx);
 /*	dbenv->set_paniccall(???) */
	/*@=noeffectuncon@*/
#if !(DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
	xx = dbenv->set_verbose(dbenv, DB_VERB_CHKPOINT,
		(dbi->dbi_verbose & DB_VERB_CHKPOINT));
#endif
	xx = dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK,
		(dbi->dbi_verbose & DB_VERB_DEADLOCK));
	xx = dbenv->set_verbose(dbenv, DB_VERB_RECOVERY,
		(dbi->dbi_verbose & DB_VERB_RECOVERY));
	xx = dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR,
		(dbi->dbi_verbose & DB_VERB_WAITSFOR));

	if (dbi->dbi_tmpdir) {
	    /*@-mods@*/
	    const char * tmpdir = rpmGenPath(root, dbi->dbi_tmpdir, NULL);
	    /*@=mods@*/
	    rc = dbenv->set_tmp_dir(dbenv, tmpdir);
	    rc = cvtdberr(dbi, "dbenv->set_tmp_dir", rc, _debug);
	    tmpdir = _free(tmpdir);
	    if (rc) goto exit;
	}
	    
	rc = dbenv->open(dbenv, dbhome,
            DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON, 0);
	rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
	if (rc) goto exit;

	/*@-moduncon -nullstate@*/ /* FIX: annotate db3 methods */
	rc = db_create(&db, dbenv, 0);
	/*@=moduncon =nullstate@*/
	rc = cvtdberr(dbi, "db_create", rc, _debug);

	if (db != NULL) {
		/*@-mods@*/
		const char * dbf = rpmGetPath(dbhome, "/", dbfile, NULL);
		/*@=mods@*/

		rc = db->verify(db, dbf, NULL, NULL, flags);
		rc = cvtdberr(dbi, "db->verify", rc, _debug);

		rpmMessage(RPMMESS_DEBUG, _("verified db index       %s/%s\n"),
			(dbhome ? dbhome : ""),
			(dbfile ? dbfile : tagName(dbi->dbi_rpmtag)));

	        /*
		 * The DB handle may not be accessed again after
		 * DB->verify is called, regardless of its return.
		 */
		db = NULL;
		dbf = _free(dbf);
	}
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
	if (rc == 0 && xx) rc = xx;
    }

exit:
    dbi->dbi_db = NULL;

    urlfn = _free(urlfn);

    dbi = db3Free(dbi);

    return rc;
}
/*@=moduncon@*/

static int db3open(rpmdb rpmdb, rpmTag rpmtag, dbiIndex * dbip)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies *dbip, fileSystem, internalState @*/
{
    /*@-nestedextern -shadow@*/
    extern struct _dbiVec db3vec;
    /*@=nestedextern =shadow@*/
    const char * urlfn = NULL;
    const char * root;
    const char * home;
    const char * dbhome;
    const char * dbfile;
    const char * dbsubfile;
    dbiIndex dbi = NULL;
    int rc = 0;
    int xx;

    DB * db = NULL;
    DB_ENV * dbenv = NULL;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    DB_TXN * txnid = NULL;
#endif
    u_int32_t oflags;
    int _printit;

/*@-boundswrite@*/
    if (dbip)
	*dbip = NULL;
/*@=boundswrite@*/

    /*
     * Parse db configuration parameters.
     */
    /*@-mods@*/
    if ((dbi = db3New(rpmdb, rpmtag)) == NULL)
	/*@-nullstate@*/
	return 1;
	/*@=nullstate@*/
    /*@=mods@*/
    dbi->dbi_api = DB_VERSION_MAJOR;

    /*
     * Get the prefix/root component and directory path.
     */
    root = (dbi->dbi_root ? dbi->dbi_root : rpmdb->db_root);
/*@-boundsread@*/
    if ((root[0] == '/' && root[1] == '\0') || rpmdb->db_chrootDone)
	root = NULL;
/*@=boundsread@*/
    home = (dbi->dbi_home ? dbi->dbi_home : rpmdb->db_home);

    /*
     * Either the root or directory components may be a URL. Concatenate,
     * convert the URL to a path, and add the name of the file.
     */
    /*@-mods@*/
    urlfn = rpmGenPath(root, home, NULL);
    /*@=mods@*/
    (void) urlPath(urlfn, &dbhome);
    if (dbi->dbi_temporary) {
	dbfile = NULL;
	dbsubfile = NULL;
    } else {
#ifdef	HACK	/* XXX necessary to support dbsubfile */
	dbfile = (dbi->dbi_file ? dbi->dbi_file : db3basename);
	dbsubfile = (dbi->dbi_subfile ? dbi->dbi_subfile : tagName(dbi->dbi_rpmtag));
#else
	dbfile = (dbi->dbi_file ? dbi->dbi_file : tagName(dbi->dbi_rpmtag));
	dbsubfile = NULL;
#endif
    }

    oflags = (dbi->dbi_oeflags | dbi->dbi_oflags);
    oflags &= ~DB_TRUNCATE;	/* XXX this is dangerous */

#if 0	/* XXX rpmdb: illegal flag combination specified to DB->open */
    if ( dbi->dbi_mode & O_EXCL) oflags |= DB_EXCL;
#endif

    /*
     * Map open mode flags onto configured database/environment flags.
     */
    if (dbi->dbi_temporary) {
	oflags |= DB_CREATE;
	dbi->dbi_oeflags |= DB_CREATE;
	oflags &= ~DB_RDONLY;
	dbi->dbi_oflags &= ~DB_RDONLY;
    } else {
	if (!(dbi->dbi_mode & (O_RDWR|O_WRONLY))) oflags |= DB_RDONLY;
	if (dbi->dbi_mode & O_CREAT) {
	    oflags |= DB_CREATE;
	    dbi->dbi_oeflags |= DB_CREATE;
	}
#ifdef	DANGEROUS
	if ( dbi->dbi_mode & O_TRUNC) oflags |= DB_TRUNCATE;
#endif
    }

    /*
     * Create the /var/lib/rpm directory if it doesn't exist (root only).
     */
    (void) rpmioMkpath(dbhome, 0755, getuid(), getgid());

    /*
     * Avoid incompatible DB_CREATE/DB_RDONLY flags on DBENV->open.
     */
    if (dbi->dbi_use_dbenv) {

#if 0
#if HAVE_LIBPTHREAD
	if (rpmdb->db_dbenv == NULL) {
	    /* Set DB_PRIVATE if posix mutexes are not shared. */
	    xx = db3_pthread_nptl();
	    if (xx) {
		dbi->dbi_eflags |= DB_PRIVATE;
		rpmMessage(RPMMESS_DEBUG, _("unshared posix mutexes found(%d), adding DB_PRIVATE, using fcntl lock\n"), xx);
	    }
	}
#endif
#endif

	if (access(dbhome, W_OK) == -1) {

	    /* dbhome is unwritable, don't attempt DB_CREATE on DB->open ... */
	    oflags &= ~DB_CREATE;

	    /* ... but DBENV->open might still need DB_CREATE ... */
	    if (dbi->dbi_eflags & DB_PRIVATE) {
		dbi->dbi_eflags &= ~DB_JOINENV;
	    } else {
		dbi->dbi_eflags |= DB_JOINENV;
		dbi->dbi_oeflags &= ~DB_CREATE;
		dbi->dbi_oeflags &= ~DB_THREAD;
		/* ... but, unless DB_PRIVATE is used, skip DBENV. */
		dbi->dbi_use_dbenv = 0;
	    }

	    /* ... DB_RDONLY maps dbhome perms across files ...  */
	    if (dbi->dbi_temporary) {
		oflags |= DB_CREATE;
		dbi->dbi_oeflags |= DB_CREATE;
		oflags &= ~DB_RDONLY;
		dbi->dbi_oflags &= ~DB_RDONLY;
	    } else {
		oflags |= DB_RDONLY;
		/* ... and DB_WRITECURSOR won't be needed ...  */
		dbi->dbi_oflags |= DB_RDONLY;
	    }

	} else {	/* dbhome is writable, check for persistent dbenv. */
	    /*@-mods@*/
	    const char * dbf = rpmGetPath(dbhome, "/__db.001", NULL);
	    /*@=mods@*/

	    if (access(dbf, F_OK) == -1) {
		/* ... non-existent (or unwritable) DBENV, will create ... */
		dbi->dbi_oeflags |= DB_CREATE;
		dbi->dbi_eflags &= ~DB_JOINENV;
	    } else {
		/* ... pre-existent (or bogus) DBENV, will join ... */
		if (dbi->dbi_eflags & DB_PRIVATE) {
		    dbi->dbi_eflags &= ~DB_JOINENV;
		} else {
		    dbi->dbi_eflags |= DB_JOINENV;
		    dbi->dbi_oeflags &= ~DB_CREATE;
		    dbi->dbi_oeflags &= ~DB_THREAD;
		}
	    }
	    dbf = _free(dbf);
	}
    }

    /*
     * Avoid incompatible DB_CREATE/DB_RDONLY flags on DB->open.
     */
    if ((oflags & DB_CREATE) && (oflags & DB_RDONLY)) {
	/* dbhome is writable, and DB->open flags may conflict. */
	const char * dbfn = (dbfile ? dbfile : tagName(dbi->dbi_rpmtag));
	/*@-mods@*/
	const char * dbf = rpmGetPath(dbhome, "/", dbfn, NULL);
	/*@=mods@*/

	if (access(dbf, F_OK) == -1) {
	    /* File does not exist, DB->open might create ... */
	    oflags &= ~DB_RDONLY;
	} else {
	    /* File exists, DB->open need not create ... */
	    oflags &= ~DB_CREATE;
	}

	/* Only writers need DB_WRITECURSOR ... */
	if (!(oflags & DB_RDONLY) && access(dbf, W_OK) == 0) {
	    dbi->dbi_oflags &= ~DB_RDONLY;
	} else {
	    dbi->dbi_oflags |= DB_RDONLY;
	}
	dbf = _free(dbf);
    }

    /*
     * Turn off verify-on-close if opening read-only.
     */
    if (oflags & DB_RDONLY)
	dbi->dbi_verify_on_close = 0;

    if (dbi->dbi_use_dbenv) {
	/*@-mods@*/
	if (rpmdb->db_dbenv == NULL) {
	    rc = db_init(dbi, dbhome, dbfile, dbsubfile, &dbenv);
	    if (rc == 0) {
		rpmdb->db_dbenv = dbenv;
		rpmdb->db_opens = 1;
	    }
	} else {
	    dbenv = rpmdb->db_dbenv;
	    rpmdb->db_opens++;
	}
	/*@=mods@*/
    }

    rpmMessage(RPMMESS_DEBUG, _("opening  db index       %s/%s %s mode=0x%x\n"),
		dbhome, (dbfile ? dbfile : tagName(dbi->dbi_rpmtag)),
		prDbiOpenFlags(oflags, 0), dbi->dbi_mode);

    if (rc == 0) {
	static int _lockdbfd = 0;

	/*@-moduncon@*/ /* FIX: annotate db3 methods */
	rc = db_create(&db, dbenv, dbi->dbi_cflags);
	/*@=moduncon@*/
	rc = cvtdberr(dbi, "db_create", rc, _debug);
	if (rc == 0 && db != NULL) {

/* XXX 3.3.4 change. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3) || (DB_VERSION_MAJOR == 4)
	    if (rc == 0 &&
			rpmdb->db_malloc && rpmdb->db_realloc && rpmdb->db_free)
	    {
		rc = db->set_alloc(db,
			rpmdb->db_malloc, rpmdb->db_realloc, rpmdb->db_free);
		rc = cvtdberr(dbi, "db->set_alloc", rc, _debug);
	    }
#else
	    if (rc == 0 && rpmdb->db_malloc) {
		rc = db->set_malloc(db, rpmdb->db_malloc);
		rc = cvtdberr(dbi, "db->set_malloc", rc, _debug);
	    }
#endif

/* 4.1: db->set_cache_priority(???) */
	    if (rc == 0 && !dbi->dbi_use_dbenv && dbi->dbi_cachesize) {
		rc = db->set_cachesize(db, 0, dbi->dbi_cachesize, 0);
		rc = cvtdberr(dbi, "db->set_cachesize", rc, _debug);
	    }
/* 4.1: db->set_encrypt(???) */
/* 4.1: db->set_errcall(dbenv, rpmdb->db_errcall); */
/* 4.1: db->set_errfile(dbenv, rpmdb->db_errfile); */
/* 4.1: db->set_errpfx(dbenv, rpmdb->db_errpfx); */
 /* 4.1: db->set_feedback(???) */

	    if (rc == 0 && dbi->dbi_lorder) {
		rc = db->set_lorder(db, dbi->dbi_lorder);
		rc = cvtdberr(dbi, "db->set_lorder", rc, _debug);
	    }
	    if (rc == 0 && dbi->dbi_pagesize) {
		rc = db->set_pagesize(db, dbi->dbi_pagesize);
		rc = cvtdberr(dbi, "db->set_pagesize", rc, _debug);
	    }
 /* 4.1: db->set_paniccall(???) */
	    if (rc == 0 && oflags & DB_CREATE) {
		switch(dbi->dbi_type) {
		default:
		case DB_HASH:
		    if (dbi->dbi_h_ffactor) {
			rc = db->set_h_ffactor(db, dbi->dbi_h_ffactor);
			rc = cvtdberr(dbi, "db->set_h_ffactor", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_h_nelem) {
			rc = db->set_h_nelem(db, dbi->dbi_h_nelem);
			rc = cvtdberr(dbi, "db->set_h_nelem", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_h_flags) {
			rc = db->set_flags(db, dbi->dbi_h_flags);
			rc = cvtdberr(dbi, "db->set_h_flags", rc, _debug);
			if (rc) break;
		    }
/* XXX db-3.2.9 has added a DB arg to the call. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR > 2) || (DB_VERSION_MAJOR == 4)
		    if (dbi->dbi_h_hash_fcn) {
			rc = db->set_h_hash(db, dbi->dbi_h_hash_fcn);
			rc = cvtdberr(dbi, "db->set_h_hash", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_h_dup_compare_fcn) {
			rc = db->set_dup_compare(db, dbi->dbi_h_dup_compare_fcn);
			rc = cvtdberr(dbi, "db->set_dup_compare", rc, _debug);
			if (rc) break;
		    }
#endif
		    break;
		case DB_BTREE:
/* 4.1: db->set_append_recno(???) */
		    if (dbi->dbi_bt_flags) {
			rc = db->set_flags(db, dbi->dbi_bt_flags);
			rc = cvtdberr(dbi, "db->set_bt_flags", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_bt_minkey) {
			rc = db->set_bt_minkey(db, dbi->dbi_bt_minkey);
			rc = cvtdberr(dbi, "db->set_bt_minkey", rc, _debug);
			if (rc) break;
		    }
/* XXX db-3.2.9 has added a DB arg to the call. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR > 2) || (DB_VERSION_MAJOR == 4)
		    if (dbi->dbi_bt_compare_fcn) {
			rc = db->set_bt_compare(db, dbi->dbi_bt_compare_fcn);
			rc = cvtdberr(dbi, "db->set_bt_compare", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_bt_dup_compare_fcn) {
			rc = db->set_dup_compare(db, dbi->dbi_bt_dup_compare_fcn);
			rc = cvtdberr(dbi, "db->set_dup_compare", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_bt_prefix_fcn) {
			rc = db->set_bt_prefix(db, dbi->dbi_bt_prefix_fcn);
			rc = cvtdberr(dbi, "db->set_bt_prefix", rc, _debug);
			if (rc) break;
		    }
#endif
		    break;
		case DB_RECNO:
		    if (dbi->dbi_re_delim) {
/* 4.1: db->set_append_recno(???) */
			rc = db->set_re_delim(db, dbi->dbi_re_delim);
			rc = cvtdberr(dbi, "db->set_re_selim", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_re_len) {
			rc = db->set_re_len(db, dbi->dbi_re_len);
			rc = cvtdberr(dbi, "db->set_re_len", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_re_pad) {
			rc = db->set_re_pad(db, dbi->dbi_re_pad);
			rc = cvtdberr(dbi, "db->set_re_pad", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_re_source) {
			rc = db->set_re_source(db, dbi->dbi_re_source);
			rc = cvtdberr(dbi, "db->set_re_source", rc, _debug);
			if (rc) break;
		    }
		    break;
		case DB_QUEUE:
		    if (dbi->dbi_q_extentsize) {
			rc = db->set_q_extentsize(db, dbi->dbi_q_extentsize);
			rc = cvtdberr(dbi, "db->set_q_extentsize", rc, _debug);
			if (rc) break;
		    }
		    break;
		}
	    }

	    if (rc == 0) {
		const char * dbfullpath;
		const char * dbpath;
		char * t;
		int nb;

		nb = strlen(dbhome);
		if (dbfile)	nb += 1 + strlen(dbfile);
		dbfullpath = t = alloca(nb + 1);

/*@-boundswrite@*/
		t = stpcpy(t, dbhome);
		if (dbfile)
		    t = stpcpy( stpcpy( t, "/"), dbfile);
/*@=boundswrite@*/
#ifdef	HACK	/* XXX necessary to support dbsubfile */
		dbpath = (!dbi->dbi_use_dbenv && !dbi->dbi_temporary)
			? dbfullpath : dbfile;
#else
		dbpath = (!dbi->dbi_temporary)
			? dbfullpath : dbfile;
#endif

#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
		rc = db->open(db, txnid, dbpath, dbsubfile,
		    dbi->dbi_type, oflags, dbi->dbi_perms);
#else
		rc = db->open(db, dbpath, dbsubfile,
		    dbi->dbi_type, oflags, dbi->dbi_perms);
#endif

		if (rc == 0 && dbi->dbi_type == DB_UNKNOWN) {
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3 && DB_VERSION_PATCH == 11) \
 || (DB_VERSION_MAJOR == 4)
		    DBTYPE dbi_type = DB_UNKNOWN;
		    xx = db->get_type(db, &dbi_type);
		    if (xx == 0)
			dbi->dbi_type = dbi_type;
#else
		    dbi->dbi_type = db->get_type(db);
#endif
		}
	    }

	    /* XXX return rc == errno without printing */
	    _printit = (rc > 0 ? 0 : _debug);
	    xx = cvtdberr(dbi, "db->open", rc, _printit);

	    dbi->dbi_txnid = NULL;

	    /*
	     * Lock a file using fcntl(2). Traditionally this is Packages,
	     * the file used to store metadata of installed header(s),
	     * as Packages is always opened, and should be opened first,
	     * for any rpmdb access.
	     *
	     * If no DBENV is used, then access is protected with a
	     * shared/exclusive locking scheme, as always.
	     *
	     * With a DBENV, the fcntl(2) lock is necessary only to keep
	     * the riff-raff from playing where they don't belong, as
	     * the DBENV should provide it's own locking scheme. So try to
	     * acquire a lock, but permit failures, as some other
	     * DBENV player may already have acquired the lock.
	     *
	     * With NPTL posix mutexes, revert to fcntl lock on non-functioning
	     * glibc/kernel combinations.
	     */
	    if (rc == 0 && dbi->dbi_lockdbfd &&
		!((dbi->dbi_ecflags & DB_CLIENT) && dbi->dbi_host) &&
		(!dbi->dbi_use_dbenv || _lockdbfd++ == 0))
	    {
		int fdno = -1;

		if (!(db->fd(db, &fdno) == 0 && fdno >= 0)) {
		    rc = 1;
		} else {
		    struct flock l;
/*@-boundswrite@*/
		    memset(&l, 0, sizeof(l));
/*@=boundswrite@*/
		    l.l_whence = 0;
		    l.l_start = 0;
		    l.l_len = 0;
		    l.l_type = (dbi->dbi_mode & (O_RDWR|O_WRONLY))
				? F_WRLCK : F_RDLCK;
		    l.l_pid = 0;

		    rc = fcntl(fdno, F_SETLK, (void *) &l);
		    if (rc) {
			/* Warning iff using non-private CDB locking. */
			rc = ((dbi->dbi_use_dbenv &&
				(dbi->dbi_eflags & DB_INIT_CDB) &&
				!(dbi->dbi_eflags & DB_PRIVATE))
			    ? 0 : 1);
			rpmError( (rc ? RPMERR_FLOCK : RPMWARN_FLOCK),
				_("cannot get %s lock on %s/%s\n"),
				((dbi->dbi_mode & (O_RDWR|O_WRONLY))
					? _("exclusive") : _("shared")),
				dbhome, (dbfile ? dbfile : ""));
		    } else if (dbfile) {
			rpmMessage(RPMMESS_DEBUG,
				_("locked   db index       %s/%s\n"),
				dbhome, dbfile);
		    }
		}
	    }
	}
    }

    dbi->dbi_db = db;

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	dbi->dbi_vec = &db3vec;
/*@-boundswrite@*/
	*dbip = dbi;
/*@=boundswrite@*/
    } else {
	dbi->dbi_verify_on_close = 0;
	(void) db3close(dbi, 0);
    }

    urlfn = _free(urlfn);

    /*@-nullstate -compmempass@*/
    return rc;
    /*@=nullstate =compmempass@*/
}

/** \ingroup db3
 */
/*@-exportheadervar@*/
/*@observer@*/ /*@unchecked@*/
struct _dbiVec db3vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db3open, db3close, db3sync, db3associate, db3join,
    db3copen, db3cclose, db3cdup, db3cdel, db3cget, db3cpget, db3cput, db3ccount,
    db3byteswapped, db3stat
};
/*@=exportheadervar@*/
/*@=type@*/
