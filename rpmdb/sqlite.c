/*@-bounds@*/
/*@-mustmod@*/
/*@-paramuse@*/
/*@-globuse@*/
/*@-moduncon@*/
/*@-noeffectuncon@*/
/*@-compdef@*/
/*@-compmempass@*/
/*@-branchstate@*/
/*@-modfilesystem@*/
/*@-evalorderuncon@*/

/*
 * sqlite.c
 * sqlite interface for rpmdb
 *
 * Author: Mark Hatle <mhatle@mvista.com> or <fray@kernel.crashing.org>
 * Copyright (c) 2004 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * or GNU Library General Public License, at your option,
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and GNU Library Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>     /* XXX urlPath proto */

#include <rpmdb.h>

#include "sqlite3.h"

#include "debug.h"

/*@access rpmdb @*/
/*@access dbiIndex @*/

/*@unchecked@*/
static int noisy = 0;

/*@unchecked@*/
static int _debug = 0;

#if 0
  /* Turn off some of the COMMIT transactions */
  #define SQL_FAST_DB
#endif

#if 0
  /* define this to trace the functions that are called in here... */
  #define SQL_TRACE_TRANSACTIONS
#endif

/* Define the things normally in a header... */
struct _sql_mem_s;	typedef struct _sql_mem_s	SQL_MEM;
struct _sql_db_s;	typedef struct _sql_db_s	SQL_DB;
struct _sql_dbcursor_s;	typedef struct _sql_dbcursor_s *SCP_t; 

struct _sql_db_s {
    sqlite3 * db;		/* Database pointer */
    int transaction;		/* Do we have a transaction open? */
};

struct _sql_dbcursor_s {
    DB *dbp;

/*@only@*/ /*@relnull@*/
    char * cmd;			/* SQL command string */
/*@only@*/ /*@relnull@*/
    sqlite3_stmt *pStmt;	/* SQL byte code */
    char * pzErrmsg;		/* SQL error msg */

  /* Table -- result of query */
/*@only@*/ /*@null@*/
    char ** av;			/* item ptrs */
/*@only@*/ /*@null@*/
    int * avlen;		/* item sizes */
    int nalloc;
    int ac;			/* no. of items */
    int rx;			/* Which row are we on? 1, 2, 3 ... */
    int nr;			/* no. of rows */
    int nc;			/* no. of columns */

    int all;		/* Cursor is for all items, not a specific key */

    SQL_MEM * memory;
    int count;
};

struct _sql_mem_s {
    void * mem_ptr;
    SQL_MEM * next;
};

union _dbswap {
    unsigned int ui;
    unsigned char uc[4];
};

/*@unchecked@*/
static unsigned int endian = 0x11223344;

static void dbg_keyval(const char * msg, dbiIndex dbi, /*@null@*/ DBC * dbcursor,
		DBT * key, DBT * data, unsigned int flags)
{
if (!_debug) return;

    fprintf(stderr, "%s on %s (%p,%p,%p,0x%x)", msg, dbi->dbi_subfile, dbcursor, key, data, flags);

    /* XXX FIXME: ptr alignment is fubar here. */
    if (key != NULL && key->data != NULL) {
	fprintf(stderr, "  key 0x%x[%d]", *(unsigned int *)key->data, key->size);
	if (dbi->dbi_rpmtag == RPMTAG_NAME)
	    fprintf(stderr, " \"%s\"", (const char *)key->data);
    }
    if (data != NULL && data->data != NULL)
	fprintf(stderr, " data 0x%x[%d]", *(unsigned int *)data->data, data->size);

    fprintf(stderr, "\n");
}

/*@only@*/ 
static SCP_t scpReset(/*@only@*/ SCP_t scp)
	/*@modifies scp @*/
{
    int xx;

if (_debug)
fprintf(stderr, "*** %s(%p)\n", __FUNCTION__, scp);
    if (scp->cmd) {
	sqlite3_free(scp->cmd);
	scp->cmd = NULL;
    }
    if (scp->pStmt) {
	xx = sqlite3_reset(scp->pStmt);
	if (xx) rpmMessage(RPMMESS_WARNING, "reset %d\n", xx);
	xx = sqlite3_finalize(scp->pStmt);
	if (xx) rpmMessage(RPMMESS_WARNING, "finalize %d\n", xx);
	scp->pStmt = NULL;
    }
    if (scp->av) {
	if (scp->nalloc <= 0) {
	    /* Clean up SCP_t used by sqlite3_get_table(). */
	    sqlite3_free_table(scp->av);
	    scp->av = NULL;
	    scp->nalloc = 0;
	} else {
	    /* Clean up SCP_t used by sql_step(). */
	    for (xx = 0; xx < scp->ac; xx++)
		scp->av[xx] = _free(scp->av[xx]);
	    if (scp->av != NULL)
		memset(scp->av, 0, scp->nalloc * sizeof(*scp->av));
	    if (scp->avlen != NULL)
		memset(scp->avlen, 0, scp->nalloc * sizeof(*scp->avlen));
	    scp->av = _free(scp->av);
	    scp->avlen = _free(scp->avlen);
	    scp->nalloc = 0;
	}
    } else
	scp->nalloc = 0;
    scp->ac = 0;
    scp->nr = 0;
    scp->nc = 0;
    scp->rx = 0;
    return scp;
}

/*@null@*/
static SCP_t scpFree(/*@only@*/ SCP_t scp)
	/*@modifies scp @*/
{
    scp = scpReset(scp);
    scp->av = _free(scp->av);
    scp->avlen = _free(scp->avlen);
if (_debug)
fprintf(stderr, "*** %s(%p)\n", __FUNCTION__, scp);
    scp = _free(scp);
    return NULL;
}

static SCP_t scpNew(void)
	/*@*/
{
    SCP_t scp = xcalloc(1, sizeof(*scp));
if (_debug)
fprintf(stderr, "*** %s(%p)\n", __FUNCTION__, scp);
    return scp;
}

/*===================================================================*/

/*
 * Transaction support
 */

static int sql_startTransaction(dbiIndex dbi)
	/*@*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc = 0;

    /* XXX:  Transaction Support */
    if (!sqldb->transaction) {
      char * pzErrmsg;
      rc = sqlite3_exec(sqldb->db, "BEGIN TRANSACTION;", NULL, NULL, &pzErrmsg);

#ifdef SQL_TRACE_TRANSACTIONS
      rpmMessage(RPMMESS_DEBUG, "Begin %s SQL transaction %s (%d)\n",
		dbi->dbi_subfile, pzErrmsg, rc);
#endif

      if (rc == 0)
	sqldb->transaction = 1;
    }

    return rc;
}

static int sql_endTransaction(dbiIndex dbi)
	/*@*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc=0;

    /* XXX:  Transaction Support */
    if (sqldb->transaction) {
      char * pzErrmsg;
      rc = sqlite3_exec(sqldb->db, "END TRANSACTION;", NULL, NULL, &pzErrmsg);

#ifdef SQL_TRACE_TRANSACTIONS
      rpmMessage(RPMMESS_DEBUG, "End %s SQL transaction %s (%d)\n",
		dbi->dbi_subfile, pzErrmsg, rc);
#endif

      if (rc == 0)
	sqldb->transaction = 0;
    }

    return rc;
}

static int sql_commitTransaction(dbiIndex dbi, int flag)
	/*@*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc = 0;

    /* XXX:  Transactions */
    if ( sqldb->transaction ) {
      char * pzErrmsg;
      rc = sqlite3_exec(sqldb->db, "COMMIT;", NULL, NULL, &pzErrmsg);

#ifdef SQL_TRACE_TRANSACTIONS
      rpmMessage(RPMMESS_DEBUG, "Commit %s SQL transaction(s) %s (%d)\n",
		dbi->dbi_subfile, pzErrmsg, rc);
#endif

      sqldb->transaction=0;

      /* Start a new transaction if we were in the middle of one */
      if ( flag == 0 )
	rc = sql_startTransaction(dbi);
    }

    return rc;
}

/*
 * Allocate a temporary buffer
 *
 * Life span of memory in db3 is apparently a db_env,
 * this works for db3 because items are mmaped from
 * the database.. however we can't do that...
 *
 * Life span has been changed to the life of a cursor.
 *
 * Minor changes were required to RPM for this to work
 * valgrind was used to verify...
 *
 */
static void * allocTempBuffer(DBC * dbcursor, size_t len)
	/*@*/
{
    SCP_t scp = (SCP_t)dbcursor;
    SQL_MEM * item;

assert(scp != NULL);

    item = xmalloc(sizeof(*item));
    item->mem_ptr = xmalloc(len);

    item->next = scp->memory;
    scp->memory = item;

    return item->mem_ptr;
}

static int sql_busy_handler(void * dbi_void, int time)
	/*@*/
{
/*@-castexpose@*/
    dbiIndex dbi = (dbiIndex) dbi_void;
/*@=castexpose@*/

    rpmMessage(RPMMESS_WARNING, _("Unable to get lock on db %s, retrying... (%d)\n"),
		dbi->dbi_file, time);

    (void) sleep(1);

    return 1;
}

/*===================================================================*/

/**
 * Verify the DB is setup.. if not initialize it
 *
 * Create the table.. create the db_info
 */
static int sql_initDB(dbiIndex dbi)
	/*@*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = scpNew();
    int rc = 0;

    char cmd[BUFSIZ];

    /* Check if the table exists... */
    sprintf(cmd,
	"SELECT name FROM 'sqlite_master' WHERE type='table' and name='%s';",
		dbi->dbi_subfile);
/*@-nullstate@*/
    rc = sqlite3_get_table(sqldb->db, cmd,
	&scp->av, &scp->nr, &scp->nc, &scp->pzErrmsg);
/*@=nullstate@*/

    if ( rc == 0 && scp->nr < 1 ) {
	const char * keytype = "blob UNIQUE";
	const char * valtype = "blob";

	switch (dbi->dbi_rpmtag) {
	case RPMDBI_PACKAGES:
	    keytype = "int UNIQUE PRIMARY KEY";
	    valtype = "blob";
	    break;
	default:
	    switch (tagType(dbi->dbi_rpmtag)) {
	    case RPM_NULL_TYPE:
	    case RPM_BIN_TYPE:
	    default:
		/*@innerbreak@*/ break;
	    case RPM_CHAR_TYPE:
	    case RPM_INT8_TYPE:
	    case RPM_INT16_TYPE:
	    case RPM_INT32_TYPE:
/*	    case RPM_INT64_TYPE: */
		keytype = "int UNIQUE";
		/*@innerbreak@*/ break;
	    case RPM_STRING_TYPE:
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		keytype = "text UNIQUE";
		/*@innerbreak@*/ break;
	    }
	}
if (_debug)
fprintf(stderr, "\t%s(%d) type(%d) keytype %s\n", tagName(dbi->dbi_rpmtag), dbi->dbi_rpmtag, tagType(dbi->dbi_rpmtag), keytype);
	sprintf(cmd, "CREATE TABLE '%s' (key %s, value %s)",
			dbi->dbi_subfile, keytype, valtype);
	rc = sqlite3_exec(sqldb->db, cmd, NULL, NULL, &scp->pzErrmsg);

	if ( rc == 0 ) {
	    sprintf(cmd, "CREATE TABLE 'db_info' (endian TEXT)");
	    rc = sqlite3_exec(sqldb->db, cmd, NULL, NULL, &scp->pzErrmsg);
	}

	if ( rc == 0 ) {
	    sprintf(cmd, "INSERT INTO 'db_info' values('%d')", ((union _dbswap *)&endian)->uc[0]);
	    rc = sqlite3_exec(sqldb->db, cmd, NULL, NULL, &scp->pzErrmsg);
	}
    }

    sprintf(cmd, "PRAGMA synchronous = OFF;");
    rc = sqlite3_exec(sqldb->db, cmd, NULL, NULL, &scp->pzErrmsg);

    if (rc)
	rpmMessage(RPMMESS_WARNING, "Unable to initDB %s (%d)\n",
		scp->pzErrmsg, rc);

    scp = scpFree(scp);

    return rc;
}

/**   
 * Close database cursor.
 * @param dbi           index database handle
 * @param dbcursor      database cursor
 * @param flags         (unused)
 * @return              0 on success
 */   
static int sql_cclose (dbiIndex dbi, /*@only@*/ DBC * dbcursor,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcursor, fileSystem @*/
{
/*@i@*/    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = (SCP_t)dbcursor;
    int rc = 0;

assert(sqldb->db != NULL);

if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, scp);

    assert(scp != NULL);

    if ( scp->memory ) {
	SQL_MEM * curr_mem = scp->memory;
	SQL_MEM * next_mem;
	int loc_count=0;

	while ( curr_mem ) {
	    next_mem = curr_mem->next;
	    free ( curr_mem->mem_ptr );
	    free ( curr_mem );
	    curr_mem = next_mem;
	    loc_count++;
	}
    }

/*@-kepttrans@*/
    scp = scpFree(scp);
/*@=kepttrans@*/

#ifndef SQL_FAST_DB
    if ( flags == DB_WRITECURSOR ) {
	rc = sql_commitTransaction(dbi, 1);
    } else {
	rc = sql_endTransaction(dbi);
    }
#else
    rc = sql_endTransaction(dbi);
#endif

    return rc;
}

/**
 * Close index database, and destroy database handle.
 * @param dbi           index database handle
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_close(/*@only@*/ dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    int rc = 0;

    if (sqldb) {

	/* Commit, don't open a new one */
	rc = sql_commitTransaction(dbi, 1);

	/* close all cursors */
	/* HACK: unlear whether sqlite cursors need to be chained and closed. */

	(void) sqlite3_close(sqldb->db);

	rpmMessage(RPMMESS_DEBUG, _("closed   sql db         %s\n"),
		dbi->dbi_subfile);

#if 0
	/* Since we didn't setup the memory, don't clear it! */
	/* Free up memory */
	if (rpmdb->db_dbenv != NULL) {
	    dbenv = rpmdb->db_dbenv;
	    if (rpmdb->db_opens == 1) {
		rpmdb->db_dbenv = _free(rpmdb->db_dbenv);
	    }
	    rpmdb->db_opens--;
	}
#endif

	dbi->dbi_stats = _free(dbi->dbi_stats);
	dbi->dbi_file = _free(dbi->dbi_file);
#if 0
	/* They're the same so only free one! */
	dbi->dbi_subfile = _free(dbi->dbi_subfile);
#endif
	dbi->dbi_db = _free(dbi->dbi_db);
    }

    dbi = _free(dbi);

    return 0;
}

/**
 * Return handle for an index database.
 * @param rpmdb         rpm database
 * @param rpmtag        rpm tag
 * @return              0 on success
 */
static int sql_open(rpmdb rpmdb, rpmTag rpmtag, /*@out@*/ dbiIndex * dbip)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *dbip, rpmGlobalMacroContext, fileSystem, internalState @*/
{
/*@-nestedextern -shadow @*/
    extern struct _dbiVec sqlitevec;
/*@=nestedextern -shadow @*/
   
    const char * urlfn = NULL;
    const char * root;
    const char * home;
    const char * dbhome;
    const char * dbfile;  
    const char * dbfname;
    const char * sql_errcode;
    int rc = 0;
    int xx;

    size_t len;
    
    dbiIndex dbi = NULL;

    SQL_DB * sqldb;

    dbi = xcalloc(1, sizeof(*dbi));
    
/*@-assignexpose -newreftrans@*/
/*@i@*/ dbi->dbi_rpmdb = rpmdb;
/*@=assignexpose =newreftrans@*/
    dbi->dbi_rpmtag = rpmtag;
    dbi->dbi_api = 4; /* I have assigned 4 to sqlite */

    /* 
     * Inverted lists have join length of 2, primary data has join length of 1.
     */
    /*@-sizeoftype@*/
    switch (dbi->dbi_rpmtag) {
    case RPMDBI_PACKAGES:
    case RPMDBI_DEPENDS:   
	dbi->dbi_jlen = 1 * sizeof(int_32);
	break;
    default:
	dbi->dbi_jlen = 2 * sizeof(int_32);
	break;
    }  
    /*@=sizeoftype@*/

    dbi->dbi_byteswapped = -1;
  
    if (dbip)
	*dbip = NULL;
   /*
     * Get the prefix/root component and directory path
     */
    root = rpmdb->db_root;
    if ((root[0] == '/' && root[1] == '\0') || rpmdb->db_chrootDone)
	root = NULL;
    home = rpmdb->db_home;
    
    dbi->dbi_root=root;
    dbi->dbi_home=home;
      
    dbfile = tagName(dbi->dbi_rpmtag);

    /*
     * Make a copy of the tagName result..
     * use this for the filename and table name
     */
    {	
      char * t;
      len = strlen(dbfile);
      t = xcalloc(len + 1, sizeof(*t));
      (void) stpcpy( t, dbfile );
      dbi->dbi_file = t;
/*@-kepttrans@*/ /* WRONG */
      dbi->dbi_subfile = t;
/*@=kepttrans@*/
    }

    dbi->dbi_mode=O_RDWR;
       
    /*
     * Either the root or directory components may be a URL. Concatenate,
     * convert the URL to a path, and add the name of the file.
     */
    /*@-mods@*/
    urlfn = rpmGenPath(root, home, NULL);
    /*@=mods@*/
    (void) urlPath(urlfn, &dbhome);

    /* 
     * Create the /var/lib/rpm directory if it doesn't exist (root only).
     */
    (void) rpmioMkpath(dbhome, 0755, getuid(), getgid());
       
    dbfname = rpmGenPath(dbhome, dbi->dbi_file, NULL);

    rpmMessage(RPMMESS_DEBUG, _("opening  sql db         %s (%s) mode=0x%x\n"),
		dbfname, dbi->dbi_subfile, dbi->dbi_mode);

    /* Open the Database */
    sqldb = xcalloc(1,sizeof(*sqldb));
       
    sql_errcode = NULL;
    xx = sqlite3_open(dbfname, &sqldb->db);
    if (xx != SQLITE_OK)
	sql_errcode = sqlite3_errmsg(sqldb->db);

    if (sqldb->db)
	(void) sqlite3_busy_handler(sqldb->db, &sql_busy_handler, dbi);

    sqldb->transaction = 0;	/* Initialize no current transactions */

    dbi->dbi_db = (DB *)sqldb;

    if (sql_errcode != NULL) {
      rpmMessage(RPMMESS_DEBUG, "Unable to open database: %s\n", sql_errcode);
      rc = EINVAL;
    }

#if 0
    /* This setup colides with db3 if we're using it... */
    /* Setup my db_env */
    if ( rpmdb->db_dbenv == NULL ) {
      dbenv = xcalloc(1, sizeof(DB_ENV));

      rpmdb->db_dbenv = dbenv;
      rpmdb->db_opens = 1;
    } else {
      dbenv = rpmdb->db_dbenv;

      rpmdb->db_opens++;
    }
#endif

    /* initialize table */
    if (rc == 0)
	rc = sql_initDB(dbi);

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	dbi->dbi_vec = &sqlitevec;
	*dbip = dbi;
    } else {
	(void) sql_close(dbi, 0);
    }
 
    urlfn = _free(urlfn);
    dbfname = _free(dbfname);
   
    return rc;
}

/**
 * Flush pending operations to disk.
 * @param dbi           index database handle
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_sync (dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int rc = 0;

#ifndef SQL_FAST_DB
    rc = sql_commitTransaction(dbi, 0);
#endif

    return rc;
}

/**
 * Open database cursor.
 * @param dbi           index database handle
 * @param txnid         database transaction handle
 * @retval dbcp         address of new database cursor
 * @param dbiflags      DB_WRITECURSOR or 0
 * @return              0 on success
 */
static int sql_copen (dbiIndex dbi, /*@null@*/ DB_TXN * txnid,
		/*@out@*/ DBC ** dbcp, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *txnid, *dbcp, fileSystem @*/
{
    DBC * dbcursor;
    SCP_t scp = scpNew();
    int rc = 0;

if (_debug)
fprintf(stderr, "==> %s(%s) tag %d type %d scp %p\n", __FUNCTION__, tagName(dbi->dbi_rpmtag), dbi->dbi_rpmtag, tagType(dbi->dbi_rpmtag), scp);

    dbcursor = (DBC *)scp;
    dbcursor->dbp = dbi->dbi_db;

    /* If we're going to write, start a transaction (lock the DB) */
    if (flags == DB_WRITECURSOR) {
	rc = sql_startTransaction(dbi);
    }

    if (dbcp)
	/*@-onlytrans@*/ *dbcp = dbcursor; /*@=onlytrans@*/
    else
	/*@-kepttrans -nullstate @*/ (void) sql_cclose(dbi, dbcursor, 0); /*@=kepttrans =nullstate @*/
     
    return rc;
}

static int sql_bind_key(dbiIndex dbi, SCP_t scp, int pos, DBT * key)
	/*@modifies scp @*/
{
    int rc = 0;

    switch (dbi->dbi_rpmtag) {
	case RPMDBI_PACKAGES:
	    rc = sqlite3_bind_int(scp->pStmt, pos, *(int *)key->data);
	    /*@innerbreak@*/ break;
	default:
	    switch (tagType(dbi->dbi_rpmtag)) {
	    case RPM_NULL_TYPE:   
	    case RPM_BIN_TYPE:
	        rc = sqlite3_bind_blob(scp->pStmt, pos, key->data, key->size, SQLITE_STATIC);
		/*@innerbreak@*/ break;
	    case RPM_CHAR_TYPE:
	    case RPM_INT8_TYPE:
	    case RPM_INT16_TYPE:   
            case RPM_INT32_TYPE:
/*          case RPM_INT64_TYPE: */   
	    default:
	        rc = sqlite3_bind_int(scp->pStmt, pos, *(int *)key->data);
	        /*@innerbreak@*/ break;
            case RPM_STRING_TYPE:
            case RPM_STRING_ARRAY_TYPE:
            case RPM_I18NSTRING_TYPE:
		rc = sqlite3_bind_text(scp->pStmt, pos, key->data, key->size, SQLITE_STATIC);
                /*@innerbreak@*/ break;
            }
    }

    return rc;
}

static int sql_bind_data(dbiIndex dbi, SCP_t scp, int pos, DBT * data)
	/*@modifies scp @*/
{
    int rc = 0;

    rc = sqlite3_bind_blob(scp->pStmt, 2, data->data, data->size, SQLITE_STATIC);

    return rc;
}

static int sql_step(sqlite3 *db, SCP_t scp)
	/*@modifies scp @*/
{
    int loop;
    int need;
    int rc;
    int i;

    scp->nc = sqlite3_column_count(scp->pStmt);

    if (scp->nr == 0 && scp->av != NULL)
	need = 2 * scp->nc;
    else
	need = scp->nc;

    /* XXX scp->nc = need = scp->nalloc = 0 case forces + 1 here */
    if (!scp->ac && !need && !scp->nalloc)
	need++;
   
    if (scp->ac + need >= scp->nalloc) {
	/* XXX +4 is bogus, was +1 */
	scp->nalloc = 2 * scp->nalloc + need + 4;
	scp->av = xrealloc(scp->av, scp->nalloc * sizeof(*scp->av));
	scp->avlen = xrealloc(scp->avlen, scp->nalloc * sizeof(*scp->avlen));
    }

assert(scp->av != NULL);
assert(scp->avlen != NULL);

    if (scp->nr == 0) {
if (noisy)
fprintf(stderr, "*** nc %d\n", scp->nc);
	for (i = 0; i < scp->nc; i++) {
	    scp->av[scp->ac] = xstrdup(sqlite3_column_name(scp->pStmt, i));
	    if (scp->avlen) scp->avlen[scp->ac] = strlen(scp->av[scp->ac]) + 1;
if (noisy)
fprintf(stderr, "\t%d %s\n", i, scp->av[scp->ac]);
	    scp->ac++;
assert(scp->ac <= scp->nalloc);
	}
    }

/*@-infloopsuncon@*/
    loop = 1;
    while (loop) {
	rc = sqlite3_step(scp->pStmt);
	switch (rc) {
	case SQLITE_DONE:
if (_debug)
fprintf(stderr, "sqlite3_step: DONE scp %p [%d:%d] av %p avlen %p\n", scp, scp->ac, scp->nalloc, scp->av, scp->avlen);
	    loop = 0;
	    /*@switchbreak@*/ break;
	case SQLITE_ROW:
	    if (scp->av != NULL)
	    for (i = 0; i < scp->nc; i++) {
	/* Expand the row array for new elements */
    		if (scp->ac + need >= scp->nalloc) {
		    /* XXX +4 is bogus, was +1 */
		    scp->nalloc = 2 * scp->nalloc + need + 4;
		    scp->av = xrealloc(scp->av, scp->nalloc * sizeof(*scp->av));
		    scp->avlen = xrealloc(scp->avlen, scp->nalloc * sizeof(*scp->avlen));
	        }

		const char * cname = sqlite3_column_name(scp->pStmt, i);
		const char * vtype = sqlite3_column_decltype(scp->pStmt, i);
		size_t nb = 0;

		if (!strcmp(vtype, "blob")) {
		    void * v = sqlite3_column_blob(scp->pStmt, i);
		    nb = sqlite3_column_bytes(scp->pStmt, i);
if (_debug)
fprintf(stderr, "\t%d %s %s %p[%d]\n", i, cname, vtype, v, nb);
		    if (nb > 0) {
			void * t = xmalloc(nb);
			scp->av[scp->ac] = memcpy(t, v, nb);
			scp->avlen[scp->ac] = nb;
			scp->ac++;
assert(scp->ac <= scp->nalloc);
		    }
		} else
		if (!strcmp(vtype, "double")) {
		    double v = sqlite3_column_double(scp->pStmt, i);
		    nb = sizeof(v);
if (_debug)
fprintf(stderr, "\t%d %s %s %g\n", i, cname, vtype, v);
		    if (nb > 0) {
			scp->av[scp->ac] = memcpy(xmalloc(nb), &v, nb);
			scp->avlen[scp->ac] = nb;
			scp->ac++;
assert(scp->ac <= scp->nalloc);
		    }
		} else
		if (!strcmp(vtype, "int")) {
		    int32_t v = sqlite3_column_int(scp->pStmt, i);
		    nb = sizeof(v);
if (_debug)
fprintf(stderr, "\t%d %s %s %d\n", i, cname, vtype, v);
		    if (nb > 0) {
			scp->av[scp->ac] = memcpy(xmalloc(nb), &v, nb);
			scp->avlen[scp->ac] = nb;
			scp->ac++;
assert(scp->ac <= scp->nalloc);
		    }
		} else
		if (!strcmp(vtype, "int64")) {
		    int64_t v = sqlite3_column_int64(scp->pStmt, i);
		    nb = sizeof(v);
if (_debug)
fprintf(stderr, "\t%d %s %s %ld\n", i, cname, vtype, v);
		    if (nb > 0) {
			scp->av[scp->ac] = memcpy(xmalloc(nb), &v, nb);
			scp->avlen[scp->ac] = nb;
			scp->ac++;
assert(scp->ac <= scp->nalloc);
		    }
		} else
		if (!strcmp(vtype, "text")) {
		    const char * v = sqlite3_column_text(scp->pStmt, i);
		    nb = strlen(v) + 1;
if (_debug)
fprintf(stderr, "\t%d %s %s \"%s\"\n", i, cname, vtype, v);
		    if (nb > 0) {
			scp->av[scp->ac] = memcpy(xmalloc(nb), v, nb);
			scp->avlen[scp->ac] = nb;
			scp->ac++;
if (_debug)
assert(scp->ac <= scp->nalloc);
		    }
		}
	    }
	    scp->nr++;
	    /*@switchbreak@*/ break;
	case SQLITE_BUSY:
	    fprintf(stderr, "sqlite3_step: BUSY %d\n", rc);
	    /*@switchbreak@*/ break;
	case SQLITE_ERROR:
	    fprintf(stderr, "sqlite3_step: ERROR %d\n", rc);
	    loop = 0;
	    /*@switchbreak@*/ break;
	case SQLITE_MISUSE:
	    fprintf(stderr, "sqlite3_step: MISUSE %d\n", rc);
	    loop = 0;
	    /*@switchbreak@*/ break;
	default:
	    fprintf(stderr, "sqlite3_step: %d %s\n", rc, sqlite3_errmsg(db));
	    loop = 0;
	    /*@switchbreak@*/ break;
	}
    }
/*@=infloopsuncon@*/

    if (rc == SQLITE_DONE)
	rc = SQLITE_OK;

    return rc;
}

/**
 * Delete (key,data) pair(s) using db->del or dbcursor->c_del.
 * @param dbi           index database handle
 * @param dbcursor      database cursor (NULL will use db->del)   
 * @param key           delete key value/length/flags
 * @param data          delete data value/length/flags
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_cdel (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key,
		DBT * data, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/
{
/*@i@*/    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = scpNew();
    int rc = 0;

dbg_keyval(__FUNCTION__, dbi, dbcursor, key, data, flags);

    scp->cmd = sqlite3_mprintf("DELETE FROM '%q' WHERE key=? AND value=?;",
	dbi->dbi_subfile);

    rc = sqlite3_prepare(sqldb->db, scp->cmd, strlen(scp->cmd), &scp->pStmt, &scp->pzErrmsg);
    if (rc) rpmMessage(RPMMESS_WARNING, "cdel(%s) prepare %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
    rc = sql_bind_key(dbi, scp, 1, key);
    if (rc) rpmMessage(RPMMESS_WARNING, "cdel(%s) bind key %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
    rc = sql_bind_data(dbi, scp, 2, data);
    if (rc) rpmMessage(RPMMESS_WARNING, "cdel(%s) bind data %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

    rc = sql_step(sqldb->db, scp);
    if (rc) rpmMessage(RPMMESS_WARNING, "cdel(%s) sql_step %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

    if (rc)
      rpmMessage(RPMMESS_DEBUG, "cdel(%s) %s (%d)\n", dbi->dbi_subfile,
		scp->pzErrmsg, rc);

    scp = scpFree(scp);

    return rc;
}

/**
 * Retrieve (key,data) pair using db->get or dbcursor->c_get.
 * @param dbi           index database handle
 * @param dbcursor      database cursor (NULL will use db->get)
 * @param key           retrieve key value/length/flags
 * @param data          retrieve data value/length/flags
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_cget (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key,
		DBT * data, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, dbcursor, *key, *data, fileSystem @*/
{
/*@i@*/    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp;
    int rc = 0;

assert(sqldb->db != NULL);

dbg_keyval(__FUNCTION__, dbi, dbcursor, key, data, flags);

    /* Find our version of the db3 cursor */
assert(dbcursor != NULL);
    scp = (SCP_t)dbcursor;
if (noisy)
fprintf(stderr, "\tcget(%s) scp %p\n", dbi->dbi_subfile, scp);

    /*
     * First determine if we have a key, or if we're going to
     * scan the whole DB
     */

    if (rc == 0 && key->size == 0) {
	scp->all++;
    }

    /* New retrieval */
    if ( rc == 0 &&  ( ( flags == DB_SET ) || ( scp->av == NULL )) ) {
if (_debug)
fprintf(stderr, "\tcget(%s) scp %p rc %d flags %d av %p\n",
		dbi->dbi_subfile, scp, rc, flags, scp->av);

	scp = scpReset(scp);	/* Free av and avlen, reset counters.*/

	switch (key->size) {
	case 0:
if (_debug)
fprintf(stderr, "\tcget(%s) size 0  key 0x%x[%d] flags %d\n",
		dbi->dbi_subfile,
		key->data == NULL ? 0 : *(unsigned int *)key->data, key->size,
		flags);

	    /* Key's MUST be in order for the PACKAGES db! */
	    if ( dbi->dbi_rpmtag == RPMDBI_PACKAGES )
	       scp->cmd = sqlite3_mprintf("SELECT key,value FROM '%q' ORDER BY key;",
			dbi->dbi_subfile);
	    else
	       scp->cmd = sqlite3_mprintf("SELECT key,value FROM '%q';",
			dbi->dbi_subfile);

	    rc = sqlite3_prepare(sqldb->db, scp->cmd, strlen(scp->cmd), &scp->pStmt, &scp->pzErrmsg);
	    if (rc) rpmMessage(RPMMESS_WARNING, "cget(%s) sequential prepare %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

	    rc = sql_step(sqldb->db, scp);
	    if (rc) rpmMessage(RPMMESS_WARNING, "cget(%s) sequential sql_step %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
	    break;
	default:

    /* XXX FIXME: ptr alignment is fubar here. */
if (_debug)
fprintf(stderr, "\tcget(%s) default key 0x%x[%d], flags %d\n", dbi->dbi_subfile,
		key->data == NULL ? 0 : *(unsigned int *)key->data, key->size, flags);

	    switch (dbi->dbi_rpmtag) {
	    default:
		scp->cmd = sqlite3_mprintf("SELECT key,value FROM '%q' WHERE key=?;",
			dbi->dbi_subfile);
assert(scp->cmd != NULL);
		rc = sqlite3_prepare(sqldb->db, scp->cmd, strlen(scp->cmd), &scp->pStmt, &scp->pzErrmsg);
		if (rc) rpmMessage(RPMMESS_WARNING, "cget(%s) prepare %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
assert(key->data != NULL);
		rc = sql_bind_key(dbi, scp, 1, key);
		if (rc) rpmMessage(RPMMESS_WARNING, "cget(%s)  key bind %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

		rc = sql_step(sqldb->db, scp);
		if (rc) rpmMessage(RPMMESS_WARNING, "cget(%s) sql_step %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

		/*@innerbreak@*/ break;
	    }
	    break;
	}
if (noisy)
fprintf(stderr, "\tcget(%s) got %d rows, %d columns\n",
	dbi->dbi_subfile, scp->nr, scp->nc);
    }

    if (rc == 0 && scp->av == NULL)
	rc = DB_NOTFOUND;

repeat:
    if (rc != 0)
	goto exit;
    {
	scp->rx++;
	if ( scp->rx > scp->nr )
	    rc = DB_NOTFOUND; /* At the end of the list */
	else {
	
	    /* If we're looking at the whole db, return the key */
	    if ( scp->all ) {
		int ix = (2 * scp->rx) + 0;
		void * v;
		int nb;

assert(ix < scp->ac);
assert(scp->av != NULL);
		v = scp->av[ix];
assert(scp->avlen != NULL);
		nb = scp->avlen[ix];
if (_debug)
fprintf(stderr, "\tcget(%s)  key av[%d] = %p[%d]\n", dbi->dbi_subfile, ix, v, nb);

		key->size = nb;
		if (key->flags & DB_DBT_MALLOC)
		    key->data = xmalloc(key->size);
		else
		    key->data = allocTempBuffer(dbcursor, key->size);
		(void) memcpy(key->data, v, key->size);
	    }

	/* Decode the data */
	    switch (dbi->dbi_rpmtag) {
	    default:
	      { int ix = (2 * scp->rx) + 1;
		void * v;
		int nb;

assert(ix < scp->ac);
assert(scp->av != NULL);
		v = scp->av[ix];
assert(scp->avlen != NULL);
		nb = scp->avlen[ix];
if (_debug)
fprintf(stderr, "\tcget(%s) data av[%d] = %p[%d]\n", dbi->dbi_subfile, ix, v, nb);

		data->size = nb;
		if (data->flags & DB_DBT_MALLOC)
		    data->data = xmalloc(data->size);
		else
		    data->data = allocTempBuffer(dbcursor, data->size);
		(void) memcpy(data->data, v, data->size);
	      }	break;
	    }

    /* XXX FIXME: ptr alignment is fubar here. */
if (_debug)
fprintf(stderr, "\tcget(%s) found  key 0x%x (%d)\n", dbi->dbi_subfile,
		key->data == NULL ? 0 : *(unsigned int *)key->data, key->size);
if (_debug)
fprintf(stderr, "\tcget(%s) found data 0x%x (%d)\n", dbi->dbi_subfile,
		key->data == NULL ? 0 : *(unsigned int *)data->data, data->size);
	}
    }

exit:
    if (rc == DB_NOTFOUND) {
if (_debug)
fprintf(stderr, "\tcget(%s) not found\n", dbi->dbi_subfile);
    }

    return rc;
}

/**
 * Store (key,data) pair using db->put or dbcursor->c_put.
 * @param dbi           index database handle
 * @param dbcursor      database cursor (NULL will use db->put)
 * @param key           store key value/length/flags
 * @param data          store data value/length/flags
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_cput (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key,
			DBT * data, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/
{
/*@i@*/    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = scpNew();
    int rc = 0;

assert(sqldb->db != NULL);

dbg_keyval(__FUNCTION__, dbi, dbcursor, key, data, flags);

    switch (dbi->dbi_rpmtag) {
    default:
	scp->cmd = sqlite3_mprintf("INSERT OR REPLACE INTO '%q' VALUES(?, ?);",
		dbi->dbi_subfile);
	rc = sqlite3_prepare(sqldb->db, scp->cmd, strlen(scp->cmd), &scp->pStmt, &scp->pzErrmsg);
	if (rc) rpmMessage(RPMMESS_WARNING, "cput(%s) prepare %s (%d)\n",dbi->dbi_subfile,  sqlite3_errmsg(sqldb->db), rc);
	rc = sql_bind_key(dbi, scp, 1, key);
	if (rc) rpmMessage(RPMMESS_WARNING, "cput(%s)  key bind %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);
	rc = sql_bind_data(dbi, scp, 2, data);
	if (rc) rpmMessage(RPMMESS_WARNING, "cput(%s) data bind %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

	rc = sql_step(sqldb->db, scp);
	if (rc) rpmMessage(RPMMESS_WARNING, "cput(%s) sql_step %s (%d)\n", dbi->dbi_subfile, sqlite3_errmsg(sqldb->db), rc);

	break;
    }

    scp = scpFree(scp);

    return rc;
}

/**
 * Is database byte swapped?
 * @param dbi           index database handle   
 * @return              0 no
 */
static int sql_byteswapped (dbiIndex dbi)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = scpNew();
    int sql_rc, rc = 0;
    union _dbswap db_endian;

/*@-nullstate@*/
    sql_rc = sqlite3_get_table(sqldb->db, "SELECT endian FROM 'db_info';",
	&scp->av, &scp->nr, &scp->nc, &scp->pzErrmsg);
/*@=nullstate@*/

    if (sql_rc == 0 && scp->nr > 0) {
assert(scp->av != NULL);
	db_endian.uc[0] = strtol(scp->av[1], NULL, 10);

	if ( db_endian.uc[0] == ((union _dbswap *)&endian)->uc[0] )
	    rc = 0; /* Native endian */
	else
	    rc = 1; /* swapped */

#if 0
      rpmMessage(RPMMESS_DEBUG, "DB Endian %ld ?= %d = %d\n",
		db_endian.uc[0], ((union _dbswap *)&endian)->uc[0], rc);
#endif
    } else {
	if ( sql_rc ) {
	    rpmMessage(RPMMESS_DEBUG, "db_info failed %s (%d)\n",
		scp->pzErrmsg, sql_rc);
	}
	rpmMessage(RPMMESS_WARNING, "Unable to determine DB endian.\n");
    }

    scp = scpFree(scp);

    return rc;
}

/**************************************************
 *
 *  All of the following are not implemented!
 *  they are not used by the rest of the system
 *
 **************************************************/

/**
 * Associate secondary database with primary.
 * @param dbi           index database handle
 * @param dbisecondary  secondary index database handle
 * @param callback      create secondary key from primary (NULL if DB_RDONLY)
 * @param flags         DB_CREATE or 0
 * @return              0 on success
 */
static int sql_associate (dbiIndex dbi, dbiIndex dbisecondary,
		int (*callback) (DB *, const DBT *, const DBT *, DBT *),
		unsigned int flags)
	/*@*/
{
if (_debug)
fprintf(stderr, "*** %s:\n", __FUNCTION__);
    return EINVAL;
}

/**
 * Return join cursor for list of cursors.
 * @param dbi           index database handle
 * @param curslist      NULL terminated list of database cursors
 * @retval dbcp         address of join database cursor
 * @param flags         DB_JOIN_NOSORT or 0
 * @return              0 on success
 */
static int sql_join (dbiIndex dbi, DBC ** curslist, /*@out@*/ DBC ** dbcp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/
{
if (_debug)
fprintf(stderr, "*** %s:\n", __FUNCTION__);
    return EINVAL;
}

/**
 * Duplicate a database cursor.   
 * @param dbi           index database handle
 * @param dbcursor      database cursor
 * @retval dbcp         address of new database cursor
 * @param flags         DB_POSITION for same position, 0 for uninitialized
 * @return              0 on success
 */
static int sql_cdup (dbiIndex dbi, DBC * dbcursor, /*@out@*/ DBC ** dbcp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/
{
if (_debug)
fprintf(stderr, "*** %s:\n", __FUNCTION__);
    return EINVAL;
}

/**
 * Retrieve (key,data) pair using dbcursor->c_pget.
 * @param dbi           index database handle
 * @param dbcursor      database cursor
 * @param key           secondary retrieve key value/length/flags
 * @param pkey          primary retrieve key value/length/flags
 * @param data          primary retrieve data value/length/flags 
 * @param flags         DB_NEXT, DB_SET, or 0
 * @return              0 on success
 */
static int sql_cpget (dbiIndex dbi, /*@null@*/ DBC * dbcursor,
		DBT * key, DBT * pkey, DBT * data, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *pkey, *data, fileSystem @*/
{
if (_debug)
fprintf(stderr, "*** %s:\n", __FUNCTION__);
    return EINVAL;
}

/**
 * Retrieve count of (possible) duplicate items using dbcursor->c_count.
 * @param dbi           index database handle
 * @param dbcursor      database cursor
 * @param countp        address of count
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_ccount (dbiIndex dbi, /*@unused@*/ DBC * dbcursor,   
		/*@unused@*/ /*@out@*/ unsigned int * countp,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/
{
if (_debug)
fprintf(stderr, "*** %s:\n", __FUNCTION__);
    return EINVAL;
}

/** \ingroup dbi
 * Save statistics in database handle.
 * @param dbi           index database handle
 * @param flags         retrieve statistics that don't require traversal?
 * @return              0 on success
 */
static int sql_stat (dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
/*@i@*/    SQL_DB * sqldb = (SQL_DB *) dbi->dbi_db;
    SCP_t scp = scpNew();
    int rc = 0;
    long nkeys = -1;

assert(sqldb->db != NULL);

    if ( dbi->dbi_stats ) {
	dbi->dbi_stats = _free(dbi->dbi_stats);
    }

/*@-sizeoftype@*/
    dbi->dbi_stats = xcalloc(1, sizeof(DB_HASH_STAT));
/*@=sizeoftype@*/

    scp->cmd = sqlite3_mprintf("SELECT COUNT('key') FROM '%q';", dbi->dbi_subfile);
/*@-nullstate@*/
    rc = sqlite3_get_table(sqldb->db, scp->cmd,
		&scp->av, &scp->nr, &scp->nc, &scp->pzErrmsg);
/*@=nullstate@*/

    if ( rc == 0 && scp->nr > 0) {
assert(scp->av != NULL);
	nkeys = strtol(scp->av[1], NULL, 10);

	rpmMessage(RPMMESS_DEBUG, "  stat on %s nkeys %ld\n",
		dbi->dbi_subfile, nkeys);
    } else {
	if ( rc ) {
	    rpmMessage(RPMMESS_DEBUG, "stat failed %s (%d)\n",
		scp->pzErrmsg, rc);
	}
    }

    if (nkeys < 0)
	nkeys = 4096;  /* Good high value */

    ((DB_HASH_STAT *)(dbi->dbi_stats))->hash_nkeys = nkeys;

    scp = scpFree(scp);

    return rc;
}

/**
 * Compile SQL statement.
 * @param dbi           index database handle
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_compile (dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
if (_debug)
fprintf(stderr, "*** %s:\n", __FUNCTION__);
    return EINVAL;
}

/**
 * Bind arguments to SQL statement.
 * @param dbi           index database handle
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_bind (dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
if (_debug)
fprintf(stderr, "*** %s:\n", __FUNCTION__);
    return EINVAL;
}

/**
 * Exec SQL statement.
 * @param dbi           index database handle
 * @param flags         (unused)
 * @return              0 on success
 */
static int sql_exec (dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
if (_debug)
fprintf(stderr, "*** %s:\n", __FUNCTION__);
    return EINVAL;
}

/* Major, minor, patch version of DB.. we're not using db.. so set to 0 */
/* open, close, sync, associate, join */
/* cursor_open, cursor_close, cursor_dup, cursor_delete, cursor_get, */
/* cursor_pget?, cursor_put, cursor_count */
/* db_bytewapped, stat */
/*@observer@*/ /*@unchecked@*/
struct _dbiVec sqlitevec = {
    0, 0, 0, 
    sql_open, 
    sql_close,
    sql_sync,  
    sql_associate,  
    sql_join,
    sql_copen,
    sql_cclose,
    sql_cdup, 
    sql_cdel,
    sql_cget,
    sql_cpget,
    sql_cput,
    sql_ccount,
    sql_byteswapped,
    sql_stat,
    sql_compile,
    sql_bind,
    sql_exec
};

/*@=evalorderuncon@*/
/*@=modfilesystem@*/
/*@=branchstate@*/
/*@=compmempass@*/
/*@=compdef@*/
/*@=moduncon@*/
/*@=noeffectuncon@*/
/*@=globuse@*/
/*@=paramuse@*/
/*@=mustmod@*/
/*@=bounds@*/
