/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDbc.java,v 1.9 2003/10/29 16:02:57 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import java.io.IOException;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a dbc object for the Java RPC server.
 */
public class RpcDbc extends Timer
{
	static final byte[] empty = new byte[0];
	RpcDbEnv rdbenv;
	RpcDb rdb;
	Dbc dbc;
	Timer orig_timer;
	boolean isJoin;

	public RpcDbc(RpcDb rdb, Dbc dbc, boolean isJoin)
	{
		this.rdb = rdb;
		this.rdbenv = rdb.rdbenv;
		this.dbc = dbc;
		this.isJoin = isJoin;
	}

	void dispose()
	{
		if (dbc != null) {
			try {
				dbc.close();
			} catch(DbException e) {
				e.printStackTrace(DbServer.err);
			}
			dbc = null;
		}
	}

	public  void close(DbDispatcher server,
		__dbc_close_msg args, __dbc_close_reply reply)
	{
		try {
			dbc.close();
			dbc = null;

			if (isJoin)
				for(LocalIterator i = ((DbServer)server).cursor_list.iterator(); i.hasNext(); ) {
					RpcDbc rdbc = (RpcDbc)i.next();
					// Unjoin cursors that were joined to create this
					if (rdbc != null && rdbc.timer == this)
						rdbc.timer = rdbc.orig_timer;
				}

			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		} finally {
			server.delCursor(this, false);
		}
	}

	public  void count(DbDispatcher server,
		__dbc_count_msg args, __dbc_count_reply reply)
	{
		try {
			reply.dupcount = dbc.count(args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void del(DbDispatcher server,
		__dbc_del_msg args, __dbc_del_reply reply)
	{
		try {
			reply.status = dbc.delete(args.flags);
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void dup(DbDispatcher server,
		__dbc_dup_msg args, __dbc_dup_reply reply)
	{
		try {
			Dbc newdbc = dbc.dup(args.flags);
			RpcDbc rdbc = new RpcDbc(rdb, newdbc, false);
			/* If this cursor has a parent txn, we need to use it too. */
			if (timer != this)
				rdbc.timer = timer;
			reply.dbcidcl_id = server.addCursor(rdbc);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void get(DbDispatcher server,
		__dbc_get_msg args, __dbc_get_reply reply)
	{
		try {
			Dbt key = new Dbt(args.keydata);
			key.setPartialLength(args.keydlen);
			key.setUserBufferLength(args.keyulen);
			key.setPartialOffset(args.keydoff);
			key.setFlags(Db.DB_DBT_MALLOC |
			    (args.keyflags & Db.DB_DBT_PARTIAL));

			Dbt data = new Dbt(args.datadata);
			data.setPartialLength(args.datadlen);
			data.setUserBufferLength(args.dataulen);
			data.setPartialOffset(args.datadoff);
			if ((args.flags & Db.DB_MULTIPLE) != 0 ||
			    (args.flags & Db.DB_MULTIPLE_KEY) != 0) {
				if (data.getData().length == 0)
					data.setData(new byte[data.getUserBufferLength()]);
				data.setFlags(Db.DB_DBT_USERMEM |
				    (args.dataflags & Db.DB_DBT_PARTIAL));
			} else
				data.setFlags(Db.DB_DBT_MALLOC |
				    (args.dataflags & Db.DB_DBT_PARTIAL));

			reply.status = dbc.get(key, data, args.flags);

			if (key.getData() == args.keydata) {
				reply.keydata = new byte[key.getSize()];
				System.arraycopy(key.getData(), 0, reply.keydata, 0, key.getSize());
			} else
				reply.keydata = key.getData();

			if (data.getData() == args.datadata) {
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

	public  void pget(DbDispatcher server,
		__dbc_pget_msg args, __dbc_pget_reply reply)
	{
		try {
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

			reply.status = dbc.get(skey, pkey, data, args.flags);

			if (skey.getData() == args.skeydata) {
				reply.skeydata = new byte[skey.getSize()];
				System.arraycopy(skey.getData(), 0, reply.skeydata, 0, skey.getSize());
			} else
				reply.skeydata = skey.getData();

			if (pkey.getData() == args.pkeydata) {
				reply.pkeydata = new byte[pkey.getSize()];
				System.arraycopy(pkey.getData(), 0, reply.pkeydata, 0, pkey.getSize());
			} else
				reply.pkeydata = pkey.getData();

			if (data.getData() == args.datadata) {
				reply.datadata = new byte[data.getSize()];
				System.arraycopy(data.getData(), 0, reply.datadata, 0, data.getSize());
			} else
				reply.datadata = data.getData();
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
		}
	}

	public  void put(DbDispatcher server,
		__dbc_put_msg args, __dbc_put_reply reply)
	{
		try {
			Dbt key = new Dbt(args.keydata);
			key.setPartialLength(args.keydlen);
			key.setUserBufferLength(args.keyulen);
			key.setPartialOffset(args.keydoff);
			key.setFlags(args.keyflags & Db.DB_DBT_PARTIAL);

			Dbt data = new Dbt(args.datadata);
			data.setPartialLength(args.datadlen);
			data.setUserBufferLength(args.dataulen);
			data.setPartialOffset(args.datadoff);
			data.setFlags(args.dataflags & Db.DB_DBT_PARTIAL);

			reply.status = dbc.put(key, data, args.flags);

			if (reply.status == 0 &&
			    (args.flags == Db.DB_AFTER || args.flags == Db.DB_BEFORE) &&
			    rdb.db.getDbType() == Db.DB_RECNO)
				reply.keydata = key.getData();
			else
				reply.keydata = empty;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.getErrno();
			reply.keydata = empty;
		} catch(IllegalArgumentException e) {
			reply.status = DbServer.EINVAL;
			reply.keydata = empty;
		}
	}
}
