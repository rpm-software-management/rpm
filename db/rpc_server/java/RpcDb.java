/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDb.java,v 1.16 2003/10/29 16:02:56 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import java.io.IOException;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a db object for the Java RPC server.
 */
public class RpcDb extends Timer
{
	static final byte[] empty = new byte[0];
	Db db;
	RpcDbEnv rdbenv;
	int refcount = 1;
	String dbname, subdbname;
	int type, setflags, openflags;

	public RpcDb(RpcDbEnv rdbenv)
	{
		this.rdbenv = rdbenv;
	}

	void dispose()
	{
		if (db != null) {
			try {
				db.close(0);
			} catch(DbException e) {
				e.printStackTrace(DbServer.err);
			}
			db = null;
		}
	}

	public  void associate(DbDispatcher server,
		__db_associate_msg args, __db_associate_reply reply)
	{
		try {
			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			/*
			 * We do not support DB_CREATE for associate.   Users
			 * can only access secondary indices on a read-only basis,
			 * so whatever they are looking for needs to be there already.
			 */
			db.associate(txn, server.getDb(args.sdbpcl_id).db, null, args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(IllegalArgumentException e) {
			reply.status = DbServer.EINVAL;
		}
	}

	public  void close(DbDispatcher server,
		__db_close_msg args, __db_close_reply reply)
	{
		if (--refcount != 0) {
			reply.status = 0;
			return;
		}

		try {
			server.delDb(this, false);
			db.close(args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(IllegalArgumentException e) {
			reply.status = DbServer.EINVAL;
		} finally {
			db = null;
		}
	}

	public  void create(DbDispatcher server,
		__db_create_msg args, __db_create_reply reply)
	{
		try {
			db = new Db(server.getEnv(args.dbenvcl_id).dbenv, args.flags);
			reply.dbcl_id = server.addDb(this);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void cursor(DbDispatcher server,
		__db_cursor_msg args, __db_cursor_reply reply)
	{
		try {
			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			Dbc dbc = db.cursor(txn, args.flags);
			RpcDbc rdbc = new RpcDbc(this, dbc, false);
			rdbc.timer = (rtxn != null) ? rtxn.timer : this;
			reply.dbcidcl_id = server.addCursor(rdbc);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void del(DbDispatcher server,
		__db_del_msg args, __db_del_reply reply)
	{
		try {
			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			Dbt key = new Dbt(args.keydata);
			key.setPartialLength(args.keydlen);
			key.setPartialOffset(args.keydoff);
			key.setUserBufferLength(args.keyulen);
			key.setFlags(args.keyflags);

			db.delete(txn, key, args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get(DbDispatcher server,
		__db_get_msg args, __db_get_reply reply)
	{
		try {
			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			Dbt key = new Dbt(args.keydata);
			key.setPartialLength(args.keydlen);
			key.setPartialOffset(args.keydoff);
			key.setUserBufferLength(args.keyulen);
			key.setFlags(Db.DB_DBT_MALLOC |
				    (args.keyflags & Db.DB_DBT_PARTIAL));

			Dbt data = new Dbt(args.datadata);
			data.setPartialLength(args.datadlen);
			data.setPartialOffset(args.datadoff);
			data.setUserBufferLength(args.dataulen);
			if ((args.flags & Db.DB_MULTIPLE) != 0) {
				if (data.getData().length == 0)
					data.setData(new byte[data.getUserBufferLength()]);
				data.setFlags(Db.DB_DBT_USERMEM |
				    (args.dataflags & Db.DB_DBT_PARTIAL));
			} else
				data.setFlags(Db.DB_DBT_MALLOC |
				    (args.dataflags & Db.DB_DBT_PARTIAL));

			reply.status = db.get(txn, key, data, args.flags);

			if (key.getData() == args.keydata ||
			    key.getData().length != key.getSize()) {
				reply.keydata = new byte[key.getSize()];
				System.arraycopy(key.getData(), 0, reply.keydata, 0, key.getSize());
			} else
				reply.keydata = key.getData();

			if (data.getData() == args.datadata ||
			    data.getData().length != data.getSize()) {
				reply.datadata = new byte[data.getSize()];
				System.arraycopy(data.getData(), 0, reply.datadata, 0, data.getSize());
			} else
				reply.datadata = data.getData();
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
			reply.keydata = reply.datadata = empty;
		}
	}

	public  void join(DbDispatcher server,
		__db_join_msg args, __db_join_reply reply)
	{
		try {
			Dbc[] cursors = new Dbc[args.curs.length + 1];
			for(int i = 0; i < args.curs.length; i++) {
				RpcDbc rdbc = server.getCursor(args.curs[i]);
				if (rdbc == null) {
					reply.status = Db.DB_NOSERVER_ID;
					return;
				}
				cursors[i] = rdbc.dbc;
			}
			cursors[args.curs.length] = null;

			Dbc jdbc = db.join(cursors, args.flags);

			RpcDbc rjdbc = new RpcDbc(this, jdbc, true);
			/*
			 * If our curslist has a parent txn, we need to use it too
			 * for the activity timeout.  All cursors must be part of
			 * the same transaction, so just check the first.
			 */
			RpcDbc rdbc0 = server.getCursor(args.curs[0]);
			if (rdbc0.timer != rdbc0)
				rjdbc.timer = rdbc0.timer;

			/*
			 * All of the curslist cursors must point to the join
			 * cursor's timeout so that we do not timeout any of the
			 * curlist cursors while the join cursor is active.
			 */
			for(int i = 0; i < args.curs.length; i++) {
				RpcDbc rdbc = server.getCursor(args.curs[i]);
				rdbc.orig_timer = rdbc.timer;
				rdbc.timer = rjdbc;
			}
			reply.dbcidcl_id = server.addCursor(rjdbc);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void key_range(DbDispatcher server,
		__db_key_range_msg args, __db_key_range_reply reply)
	{
		try {
			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			Dbt key = new Dbt(args.keydata);
			key.setPartialLength(args.keydlen);
			key.setPartialOffset(args.keydoff);
			key.setUserBufferLength(args.keyulen);
			key.setFlags(args.keyflags);

			DbKeyRange range = new DbKeyRange();

			db.keyRange(txn, key, range, args.flags);
			reply.status = 0;
			reply.less = range.less;
			reply.equal = range.equal;
			reply.greater = range.greater;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	private boolean findSharedDb(DbDispatcher server, __db_open_reply reply)
		throws DbException
	{
		RpcDb rdb = null;
		boolean matchFound = false;
		LocalIterator i = ((DbServer)server).db_list.iterator();

		while (!matchFound && i.hasNext()) {
			rdb = (RpcDb)i.next();
			if (rdb != null && rdb != this && rdb.rdbenv == rdbenv &&
			    (type == Db.DB_UNKNOWN || rdb.type == type) &&
			    openflags == rdb.openflags &&
			    setflags == rdb.setflags &&
			    dbname != null && rdb.dbname != null &&
			    dbname.equals(rdb.dbname) &&
			    (subdbname == rdb.subdbname ||
			     (subdbname != null && rdb.subdbname != null &&
			      subdbname.equals(rdb.subdbname))))
				matchFound = true;
		}

		if (matchFound) {
			++rdb.refcount;
			reply.dbcl_id = ((FreeList.FreeListIterator)i).current;
			reply.type = rdb.db.getDbType();
			reply.dbflags = rdb.db.get_flags_raw();
			// FIXME: not possible to work out byteorder from Java?
			reply.lorder = rdb.db.isByteSwapped() ? 4321 : 1234;
			reply.status = 0;

			DbServer.err.println("Sharing Db: " + reply.dbcl_id);
		}

		return matchFound;
	}

	public  void get_name(DbDispatcher server,
		__db_get_name_msg args, __db_get_name_reply reply)
	{
		try {
			reply.filename = db.getFileName();
			reply.dbname = db.getDatabaseName();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_open_flags(DbDispatcher server,
		__db_get_open_flags_msg args, __db_get_open_flags_reply reply)
	{
		try {
			reply.flags = db.getOpenFlags();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void open(DbDispatcher server,
		__db_open_msg args, __db_open_reply reply)
	{
		try {
			dbname = (args.name.length() > 0) ? args.name : null;
			subdbname = (args.subdb.length() > 0) ? args.subdb : null;
			type = args.type;
			openflags = args.flags & DbServer.DB_SERVER_DBFLAGS;

			if (findSharedDb(server, reply)) {
				server.delDb(this, true);
			} else {
				DbServer.err.println("Calling db.open(" + null + ", " + dbname + ", " + subdbname + ", " + args.type + ", " + Integer.toHexString(args.flags) + ", " + args.mode + ")");
				db.open(null, dbname, subdbname, args.type, args.flags, args.mode);

				reply.dbcl_id = args.dbpcl_id;
				reply.type = this.type = db.getDbType();
				reply.dbflags = db.get_flags_raw();
				// FIXME: not possible to work out byteorder from Java?
				reply.lorder = db.isByteSwapped() ? 4321 : 1234;
				reply.status = 0;
			}
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(FileNotFoundException e) {
			e.printStackTrace(DbServer.err);
			reply.status = Db.DB_NOTFOUND;
		}

		// System.err.println("Db.open: reply.status = " + reply.status + ", reply.dbcl_id = " + reply.dbcl_id);
	}

	public  void pget(DbDispatcher server,
		__db_pget_msg args, __db_pget_reply reply)
	{
		try {
			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			Dbt skey = new Dbt(args.skeydata);
			skey.setPartialLength(args.skeydlen);
			skey.setPartialOffset(args.skeydoff);
			skey.setUserBufferLength(args.skeyulen);
			skey.setFlags(Db.DB_DBT_MALLOC |
			    (args.skeyflags & Db.DB_DBT_PARTIAL));

			Dbt pkey = new Dbt(args.pkeydata);
			pkey.setPartialLength(args.pkeydlen);
			pkey.setPartialOffset(args.pkeydoff);
			pkey.setUserBufferLength(args.pkeyulen);
			pkey.setFlags(Db.DB_DBT_MALLOC |
			    (args.pkeyflags & Db.DB_DBT_PARTIAL));

			Dbt data = new Dbt(args.datadata);
			data.setPartialLength(args.datadlen);
			data.setPartialOffset(args.datadoff);
			data.setUserBufferLength(args.dataulen);
			data.setFlags(Db.DB_DBT_MALLOC |
			    (args.dataflags & Db.DB_DBT_PARTIAL));

			db.get(txn, skey, pkey, data, args.flags);

			if (skey.getData() == args.skeydata ||
			    skey.getData().length != skey.getSize()) {
				reply.skeydata = new byte[skey.getSize()];
				System.arraycopy(skey.getData(), 0, reply.skeydata, 0, skey.getSize());
			} else
				reply.skeydata = skey.getData();

			if (pkey.getData() == args.pkeydata ||
			    pkey.getData().length != pkey.getSize()) {
				reply.pkeydata = new byte[pkey.getSize()];
				System.arraycopy(pkey.getData(), 0, reply.pkeydata, 0, pkey.getSize());
			} else
				reply.pkeydata = pkey.getData();

			if (data.getData() == args.datadata ||
			    data.getData().length != data.getSize()) {
				reply.datadata = new byte[data.getSize()];
				System.arraycopy(data.getData(), 0, reply.datadata, 0, data.getSize());
			} else
				reply.datadata = data.getData();

			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
			reply.skeydata = reply.pkeydata = reply.datadata = empty;
		}
	}

	public  void put(DbDispatcher server,
		__db_put_msg args, __db_put_reply reply)
	{
		try {
			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;

			Dbt key = new Dbt(args.keydata);
			key.setPartialLength(args.keydlen);
			key.setPartialOffset(args.keydoff);
			key.setUserBufferLength(args.keyulen);
			key.setFlags(Db.DB_DBT_MALLOC |
			    (args.keyflags & Db.DB_DBT_PARTIAL));

			Dbt data = new Dbt(args.datadata);
			data.setPartialLength(args.datadlen);
			data.setPartialOffset(args.datadoff);
			data.setUserBufferLength(args.dataulen);
			data.setFlags(args.dataflags & Db.DB_DBT_PARTIAL);

			reply.status = db.put(txn, key, data, args.flags);

			/*
			 * If the client did a DB_APPEND, set up key in reply.
			 * Otherwise just status.
			 */
			if ((args.flags & Db.DB_APPEND) != 0) {
				if (key.getData() == args.keydata ||
				    key.getData().length != key.getSize()) {
					reply.keydata = new byte[key.getSize()];
					System.arraycopy(key.getData(), 0, reply.keydata, 0, key.getSize());
				} else
					reply.keydata = key.getData();
			} else
				reply.keydata = empty;
		} catch(DbException e) {
			reply.keydata = empty;
			reply.status = e.getErrno();
			DbServer.err.println("Exception, setting status to " + reply.status);
			e.printStackTrace(DbServer.err);
		} catch(IllegalArgumentException e) {
			reply.status = DbServer.EINVAL;
		}
	}

	public  void remove(DbDispatcher server,
		__db_remove_msg args, __db_remove_reply reply)
	{
		try {
			args.name = (args.name.length() > 0) ? args.name : null;
			args.subdb = (args.subdb.length() > 0) ? args.subdb : null;
			db.remove(args.name, args.subdb, args.flags);
			db = null;
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(FileNotFoundException e) {
			e.printStackTrace(DbServer.err);
			reply.status = Db.DB_NOTFOUND;
		} finally {
			server.delDb(this, false);
		}
	}

	public  void rename(DbDispatcher server,
		__db_rename_msg args, __db_rename_reply reply)
	{
		try {
			args.name = (args.name.length() > 0) ? args.name : null;
			args.subdb = (args.subdb.length() > 0) ? args.subdb : null;
			args.newname = (args.newname.length() > 0) ? args.newname : null;
			db.rename(args.name, args.subdb, args.newname, args.flags);
			db = null;
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} catch(FileNotFoundException e) {
			e.printStackTrace(DbServer.err);
			reply.status = Db.DB_NOTFOUND;
		} finally {
			server.delDb(this, false);
		}
	}

	public  void set_bt_maxkey(DbDispatcher server,
		__db_bt_maxkey_msg args, __db_bt_maxkey_reply reply)
	{
		try {
			db.set_bt_maxkey(args.maxkey);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_bt_minkey(DbDispatcher server,
		__db_get_bt_minkey_msg args, __db_get_bt_minkey_reply reply)
	{
		try {
			reply.minkey = db.getBtreeMinKey();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_bt_minkey(DbDispatcher server,
		__db_bt_minkey_msg args, __db_bt_minkey_reply reply)
	{
		try {
			db.setBtreeMinKey(args.minkey);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_encrypt_flags(DbDispatcher server,
		__db_get_encrypt_flags_msg args, __db_get_encrypt_flags_reply reply)
	{
		try {
			reply.flags = db.getEncryptFlags();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_encrypt(DbDispatcher server,
		__db_encrypt_msg args, __db_encrypt_reply reply)
	{
		try {
			db.setEncrypted(args.passwd, args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_flags(DbDispatcher server,
		__db_get_flags_msg args, __db_get_flags_reply reply)
	{
		try {
			reply.flags = db.getFlags();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_flags(DbDispatcher server,
		__db_flags_msg args, __db_flags_reply reply)
	{
		try {
			// DbServer.err.println("Calling db.setflags(" + Integer.toHexString(args.flags) + ")");
			db.setFlags(args.flags);
			setflags |= args.flags;
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_h_ffactor(DbDispatcher server,
		__db_get_h_ffactor_msg args, __db_get_h_ffactor_reply reply)
	{
		try {
			reply.ffactor = db.getHashFillFactor();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_h_ffactor(DbDispatcher server,
		__db_h_ffactor_msg args, __db_h_ffactor_reply reply)
	{
		try {
			db.setHashFillFactor(args.ffactor);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_h_nelem(DbDispatcher server,
		__db_get_h_nelem_msg args, __db_get_h_nelem_reply reply)
	{
		try {
			reply.nelem = db.getHashNumElements();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_h_nelem(DbDispatcher server,
		__db_h_nelem_msg args, __db_h_nelem_reply reply)
	{
		try {
			db.setHashNumElements(args.nelem);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_lorder(DbDispatcher server,
		__db_get_lorder_msg args, __db_get_lorder_reply reply)
	{
		try {
			reply.lorder = db.getByteOrder();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_lorder(DbDispatcher server,
		__db_lorder_msg args, __db_lorder_reply reply)
	{
		try {
			db.setByteOrder(args.lorder);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_pagesize(DbDispatcher server,
		__db_get_pagesize_msg args, __db_get_pagesize_reply reply)
	{
		try {
			reply.pagesize = db.getPageSize();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_pagesize(DbDispatcher server,
		__db_pagesize_msg args, __db_pagesize_reply reply)
	{
		try {
			db.setPageSize(args.pagesize);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_q_extentsize(DbDispatcher server,
		__db_get_extentsize_msg args, __db_get_extentsize_reply reply)
	{
		try {
			reply.extentsize = db.getQueueExtentSize();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_q_extentsize(DbDispatcher server,
		__db_extentsize_msg args, __db_extentsize_reply reply)
	{
		try {
			db.setQueueExtentSize(args.extentsize);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_re_delim(DbDispatcher server,
		__db_get_re_delim_msg args, __db_get_re_delim_reply reply)
	{
		try {
			reply.delim = db.getRecordDelimiter();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_re_delim(DbDispatcher server,
		__db_re_delim_msg args, __db_re_delim_reply reply)
	{
		try {
			db.setRecordDelimiter(args.delim);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_re_len(DbDispatcher server,
		__db_get_re_len_msg args, __db_get_re_len_reply reply)
	{
		try {
			reply.len = db.getRecordLength();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_re_len(DbDispatcher server,
		__db_re_len_msg args, __db_re_len_reply reply)
	{
		try {
			db.setRecordLength(args.len);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get_re_pad(DbDispatcher server,
		__db_get_re_pad_msg args, __db_get_re_pad_reply reply)
	{
		try {
			reply.pad = db.getRecordPad();
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void set_re_pad(DbDispatcher server,
		__db_re_pad_msg args, __db_re_pad_reply reply)
	{
		try {
			db.setRecordPad(args.pad);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void stat(DbDispatcher server,
		__db_stat_msg args, __db_stat_reply reply)
	{
		try {
			Object raw_stat = db.stat(args.flags);

			if (raw_stat instanceof DbHashStat) {
				DbHashStat hs = (DbHashStat)raw_stat;
				int[] raw_stats = {
					hs.hash_magic, hs.hash_version,
					hs.hash_metaflags, hs.hash_nkeys,
					hs.hash_ndata, hs.hash_pagesize,
					hs.hash_ffactor, hs.hash_buckets,
					hs.hash_free, hs.hash_bfree,
					hs.hash_bigpages, hs.hash_big_bfree,
					hs.hash_overflows, hs.hash_ovfl_free,
					hs.hash_dup, hs.hash_dup_free
				};
				reply.stats = raw_stats;
			} else if (raw_stat instanceof DbQueueStat) {
				DbQueueStat qs = (DbQueueStat)raw_stat;
				int[] raw_stats = {
					qs.qs_magic, qs.qs_version,
					qs.qs_metaflags, qs.qs_nkeys,
					qs.qs_ndata, qs.qs_pagesize,
					qs.qs_extentsize, qs.qs_pages,
					qs.qs_re_len, qs.qs_re_pad,
					qs.qs_pgfree, qs.qs_first_recno,
					qs.qs_cur_recno
				};
				reply.stats = raw_stats;
			} else if (raw_stat instanceof DbBtreeStat) {
				DbBtreeStat bs = (DbBtreeStat)raw_stat;
				int[] raw_stats = {
					bs.bt_magic, bs.bt_version,
					bs.bt_metaflags, bs.bt_nkeys,
					bs.bt_ndata, bs.bt_pagesize,
					bs.bt_maxkey, bs.bt_minkey,
					bs.bt_re_len, bs.bt_re_pad,
					bs.bt_levels, bs.bt_int_pg,
					bs.bt_leaf_pg, bs.bt_dup_pg,
					bs.bt_over_pg, bs.bt_free,
					bs.bt_int_pgfree, bs.bt_leaf_pgfree,
					bs.bt_dup_pgfree, bs.bt_over_pgfree
				};
				reply.stats = raw_stats;
			} else
				throw new DbException("Invalid return type from db.stat()", Db.DB_NOTFOUND);

			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
			reply.stats = new int[0];
		}
	}

	public  void sync(DbDispatcher server,
		__db_sync_msg args, __db_sync_reply reply)
	{
		try {
			db.sync(args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void truncate(DbDispatcher server,
		__db_truncate_msg args, __db_truncate_reply reply)
	{
		try {
			RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
			DbTxn txn = (rtxn != null) ? rtxn.txn : null;
			reply.count = db.truncate(txn, args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}
}
