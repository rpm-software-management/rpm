/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDbEnv.java,v 1.12 2003/10/20 21:13:59 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import java.io.IOException;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a dbenv for the Java RPC server.
 */
public class RpcDbEnv extends Timer
{
	DbEnv dbenv;
	String home;
	long idletime, timeout;
	int openflags, onflags, offflags;
	int refcount = 1;

	void dispose()
	{
		if (dbenv != null) {
			try {
				dbenv.close(0);
			} catch(DbException e) {
				e.printStackTrace(DbServer.err);
			}
			dbenv = null;
		}
	}

	public  void close(DbDispatcher server,
		__env_close_msg args, __env_close_reply reply)
	{
		if (--refcount != 0) {
			reply.status = 0;
			return;
		}

		try {
			server.delEnv(this, false);
			dbenv.close(args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch (IllegalArgumentException e) {
			reply.status = DbServer.EINVAL;
		} finally {
			dbenv = null;
		}
	}

	public  void create(DbDispatcher server,
		__env_create_msg args, __env_create_reply reply)
	{
		this.idletime = (args.timeout != 0) ? args.timeout : DbServer.idleto;
		this.timeout = DbServer.defto;
		try {
			dbenv = new DbEnv(0);
			reply.envcl_id = server.addEnv(this);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void dbremove(DbDispatcher server,
		__env_dbremove_msg args, __env_dbremove_reply reply)
	{
		try {
			args.name = (args.name.length() > 0) ? args.name : null;
			args.subdb = (args.subdb.length() > 0) ? args.subdb : null;

			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			dbenv.dbRemove(txn, args.name, args.subdb, args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(FileNotFoundException fnfe) {
			reply.status = Db.DB_NOTFOUND;
		}
	}

	public  void dbrename(DbDispatcher server,
		__env_dbrename_msg args, __env_dbrename_reply reply)
	{
		try {
			args.name = (args.name.length() > 0) ? args.name : null;
			args.subdb = (args.subdb.length() > 0) ? args.subdb : null;
			args.newname = (args.newname.length() > 0) ? args.newname : null;

			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			dbenv.dbRename(txn, args.name, args.subdb, args.newname, args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(FileNotFoundException fnfe) {
			reply.status = Db.DB_NOTFOUND;
		}
	}

	private boolean findSharedDbEnv(DbDispatcher server, __env_open_reply reply)
		throws DbException
	{
		RpcDbEnv rdbenv = null;
		boolean matchFound = false;
		LocalIterator i = ((DbServer)server).env_list.iterator();

		while (!matchFound && i.hasNext()) {
			rdbenv = (RpcDbEnv)i.next();
			if (rdbenv != null && rdbenv != this &&
			    (home == rdbenv.home ||
			     (home != null && home.equals(rdbenv.home))) &&
			    openflags == rdbenv.openflags &&
			    onflags == rdbenv.onflags &&
			    offflags == rdbenv.offflags)
				matchFound = true;
		}

		if (matchFound) {
			/*
			 * The only thing left to check is the timeout.
			 * Since the server timeout set by the client is a hint, for sharing
			 * we'll give them the benefit of the doubt and grant them the
			 * longer timeout.
			 */
			if (rdbenv.timeout < timeout)
				rdbenv.timeout = timeout;

			++rdbenv.refcount;
			reply.envcl_id = ((FreeList.FreeListIterator)i).current;
			reply.status = 0;

			DbServer.err.println("Sharing DbEnv: " + reply.envcl_id);
		}

		return matchFound;
	}

	public  void get_home(DbDispatcher server,
		__env_get_home_msg args, __env_get_home_reply reply)
	{
		try {
			reply.home = dbenv.getDbEnvHome();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_open_flags(DbDispatcher server,
		__env_get_open_flags_msg args, __env_get_open_flags_reply reply)
	{
		try {
			reply.flags = dbenv.getOpenFlags();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void open(DbDispatcher server,
		__env_open_msg args, __env_open_reply reply)
	{
		try {
			home = (args.home.length() > 0) ? args.home : null;

			/*
			 * If they are using locking do deadlock detection for
			 * them, internally.
			 */
			if ((args.flags & Db.DB_INIT_LOCK) != 0)
				dbenv.setLockDetect(Db.DB_LOCK_DEFAULT);

			// adjust flags for RPC
			int newflags = (args.flags & ~DbServer.DB_SERVER_FLAGMASK);
			openflags = (newflags & DbServer.DB_SERVER_ENVFLAGS);

			if (findSharedDbEnv(server, reply)) {
				dbenv.close(0);
				dbenv = null;
			} else if (DbServer.check_home(home)) {
				dbenv.open(home, newflags, args.mode);
				reply.status = 0;
				reply.envcl_id = args.dbenvcl_id;
			} else
				reply.status = Db.DB_NOSERVER_HOME;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(FileNotFoundException e) {
			reply.status = Db.DB_NOTFOUND;
		}

		// System.err.println("DbEnv.open: reply.status = " + reply.status + ", reply.envcl_id = " + reply.envcl_id);
	}

	public  void remove(DbDispatcher server,
		__env_remove_msg args, __env_remove_reply reply)
	{
		try {
			args.home = (args.home.length() > 0) ? args.home : null;
			// TODO: check home?

			dbenv.remove(args.home, args.flags);
			dbenv = null;
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(FileNotFoundException e) {
			reply.status = Db.DB_NOTFOUND;
		} finally {
			server.delEnv(this, false);
		}
	}

	public  void get_cachesize(DbDispatcher server,
		__env_get_cachesize_msg args, __env_get_cachesize_reply reply)
	{
		try {
			long cachesize = dbenv.getCacheSize();
			reply.gbytes = (int)(cachesize / 1073741824);
			reply.bytes = (int)(cachesize % 1073741824);
			reply.ncache = dbenv.getCacheSizeNcache();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_cachesize(DbDispatcher server,
		__env_cachesize_msg args, __env_cachesize_reply reply)
	{
		try {
			long bytes = (long)args.gbytes * 1024*1024*1024;
			bytes += args.bytes;
			dbenv.setCacheSize(bytes, args.ncache);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_encrypt_flags(DbDispatcher server,
		__env_get_encrypt_flags_msg args, __env_get_encrypt_flags_reply reply)
	{
		try {
			reply.flags = dbenv.getEncryptFlags();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_encrypt(DbDispatcher server,
		__env_encrypt_msg args, __env_encrypt_reply reply)
	{
		try {
			dbenv.setEncrypted(args.passwd, args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_flags(DbDispatcher server,
		__env_get_flags_msg args, __env_get_flags_reply reply)
	{
		try {
			reply.flags = dbenv.getFlags();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_flags(DbDispatcher server,
		__env_flags_msg args, __env_flags_reply reply)
	{
		try {
			dbenv.setFlags(args.flags, args.onoff != 0);
			if (args.onoff != 0)
				onflags |= args.flags;
			else
				offflags |= args.flags;
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	// txn_recover implementation
	public  void txn_recover(DbDispatcher server,
		__txn_recover_msg args, __txn_recover_reply reply)
	{
		try {
			DbPreplist[] prep_list = dbenv.txnRecover(args.count, args.flags);
			if (prep_list != null && prep_list.length > 0) {
				int count = prep_list.length;
				reply.retcount = count;
				reply.txn = new int[count];
				reply.gid = new byte[count * Db.DB_XIDDATASIZE];

				for(int i = 0; i < count; i++) {
					reply.txn[i] = server.addTxn(new RpcDbTxn(this, prep_list[i].txn));
					System.arraycopy(prep_list[i].gid, 0, reply.gid, i * Db.DB_XIDDATASIZE, Db.DB_XIDDATASIZE);
				}
			}

			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}
}
