/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Server.java,v 1.1 2004/04/06 20:43:42 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import com.sleepycat.db.internal.DbConstants;
import java.io.*;
import java.util.*;
import org.acplt.oncrpc.OncRpcException;
import org.acplt.oncrpc.server.OncRpcCallInformation;

/**
 * Main entry point for the Java version of the Berkeley DB RPC server
 */
public class Server extends Dispatcher {
    public static long idleto = 10 * 60 * 1000; // 5 minutes
    public static long defto = 5 * 60 * 1000;   // 5 minutes
    public static long maxto = 60 * 60 * 1000;  // 1 hour
    public static String passwd = null;
    public static OutputStream errstream;
    public static PrintWriter err;

    long now, hint; // updated each operation
    FreeList env_list = new FreeList();
    FreeList db_list = new FreeList();
    FreeList txn_list = new FreeList();
    FreeList cursor_list = new FreeList();

    public Server() throws IOException, OncRpcException {
        super();
        init_lists();
    }

    public void dispatchOncRpcCall(OncRpcCallInformation call, int program,
                                   int version, int procedure) throws OncRpcException, IOException {
        long newnow = System.currentTimeMillis();
        // Server.err.println("Dispatching RPC call " + procedure + " after delay of " + (newnow - now));
        now = newnow;
        try {
            super.dispatchOncRpcCall(call, program, version, procedure);
            doTimeouts();
        } catch (Throwable t) {
            System.err.println("Caught " + t + " while dispatching RPC call " + procedure);
            t.printStackTrace(Server.err);
        } finally {
            Server.err.flush();
        }
    }

    // Internal methods to track context
    private void init_lists() {
        // We do this so that getEnv/Database/etc(0) == null
        env_list.add(null);
        db_list.add(null);
        txn_list.add(null);
        cursor_list.add(null);
    }

    int addEnv(RpcDbEnv rdbenv) {
        rdbenv.timer.last_access = now;
        int id = env_list.add(rdbenv);
        return id;
    }

    int addDatabase(RpcDb rdb) {
        int id = db_list.add(rdb);
        return id;
    }

    int addTxn(RpcDbTxn rtxn) {
        rtxn.timer.last_access = now;
        int id = txn_list.add(rtxn);
        return id;
    }

    int addCursor(RpcDbc rdbc) {
        rdbc.timer.last_access = now;
        int id = cursor_list.add(rdbc);
        return id;
    }

    void delEnv(RpcDbEnv rdbenv, boolean dispose) {
        env_list.del(rdbenv);

        // cursors and transactions will already have been cleaned up
        for (LocalIterator i = db_list.iterator(); i.hasNext();) {
            RpcDb rdb = (RpcDb)i.next();
            if (rdb != null && rdb.rdbenv == rdbenv)
                delDatabase(rdb, true);
        }

        if (dispose)
            rdbenv.dispose();
    }

    void delDatabase(RpcDb rdb, boolean dispose) {
        db_list.del(rdb);

        for (LocalIterator i = cursor_list.iterator(); i.hasNext();) {
            RpcDbc rdbc = (RpcDbc)i.next();
            if (rdbc != null && rdbc.timer == rdb) {
                i.remove();
                rdbc.dispose();
            }
        }

        if (dispose)
            rdb.dispose();
    }

    void delTxn(RpcDbTxn rtxn, boolean dispose) {
        txn_list.del(rtxn);

        for (LocalIterator i = cursor_list.iterator(); i.hasNext();) {
            RpcDbc rdbc = (RpcDbc)i.next();
            if (rdbc != null && rdbc.timer == rtxn) {
                i.remove();
                rdbc.dispose();
            }
        }

        for (LocalIterator i = txn_list.iterator(); i.hasNext();) {
            RpcDbTxn rtxn_child = (RpcDbTxn)i.next();
            if (rtxn_child != null && rtxn_child.timer == rtxn) {
                i.remove();
                rtxn_child.dispose();
            }
        }

        if (dispose)
            rtxn.dispose();
    }

    void delCursor(RpcDbc rdbc, boolean dispose) {
        cursor_list.del(rdbc);
        if (dispose)
            rdbc.dispose();
    }

    RpcDbEnv getEnv(int envid) {
        RpcDbEnv rdbenv = (RpcDbEnv)env_list.get(envid);
        if (rdbenv != null)
            rdbenv.timer.last_access = now;
        return rdbenv;
    }

    RpcDb getDatabase(int dbid) {
        RpcDb rdb = (RpcDb)db_list.get(dbid);
        if (rdb != null)
            rdb.rdbenv.timer.last_access = now;
        return rdb;
    }

    RpcDbTxn getTxn(int txnid) {
        RpcDbTxn rtxn = (RpcDbTxn)txn_list.get(txnid);
        if (rtxn != null)
            rtxn.timer.last_access = rtxn.rdbenv.timer.last_access = now;
        return rtxn;
    }

    RpcDbc getCursor(int dbcid) {
        RpcDbc rdbc = (RpcDbc)cursor_list.get(dbcid);
        if (rdbc != null)
            rdbc.last_access = rdbc.timer.last_access = rdbc.rdbenv.timer.last_access = now;
        return rdbc;
    }

    void doTimeouts() {
        if (now < hint) {
            // Server.err.println("Skipping cleaner sweep - now = " + now + ", hint = " + hint);
            return;
        }

        // Server.err.println("Starting a cleaner sweep");
        hint = now + Server.maxto;

        for (LocalIterator i = cursor_list.iterator(); i.hasNext();) {
            RpcDbc rdbc = (RpcDbc)i.next();
            if (rdbc == null)
                continue;

            long end_time = rdbc.timer.last_access + rdbc.rdbenv.timeout;
            // Server.err.println("Examining " + rdbc + ", time left = " + (end_time - now));
            if (end_time < now) {
                Server.err.println("Cleaning up " + rdbc);
                delCursor(rdbc, true);
            } else if (end_time < hint)
                hint = end_time;
        }

        for (LocalIterator i = txn_list.iterator(); i.hasNext();) {
            RpcDbTxn rtxn = (RpcDbTxn)i.next();
            if (rtxn == null)
                continue;

            long end_time = rtxn.timer.last_access + rtxn.rdbenv.timeout;
            // Server.err.println("Examining " + rtxn + ", time left = " + (end_time - now));
            if (end_time < now) {
                Server.err.println("Cleaning up " + rtxn);
                delTxn(rtxn, true);
            } else if (end_time < hint)
                hint = end_time;
        }

        for (LocalIterator i = env_list.iterator(); i.hasNext();) {
            RpcDbEnv rdbenv = (RpcDbEnv)i.next();
            if (rdbenv == null)
                continue;

            long end_time = rdbenv.timer.last_access + rdbenv.idletime;
            // Server.err.println("Examining " + rdbenv + ", time left = " + (end_time - now));
            if (end_time < now) {
                Server.err.println("Cleaning up " + rdbenv);
                delEnv(rdbenv, true);
            }
        }

        // if we didn't find anything, reset the hint
        if (hint == now + Server.maxto)
            hint = 0;

        // Server.err.println("Finishing a cleaner sweep");
    }

    // Some constants that aren't available elsewhere
    static final int ENOENT = 2;
    static final int EACCES = 13;
    static final int EEXIST = 17;
    static final int EINVAL = 22;
    static final int DB_SERVER_FLAGMASK = DbConstants.DB_LOCKDOWN |
        DbConstants.DB_PRIVATE | DbConstants.DB_RECOVER | DbConstants.DB_RECOVER_FATAL |
        DbConstants.DB_SYSTEM_MEM | DbConstants.DB_USE_ENVIRON |
        DbConstants.DB_USE_ENVIRON_ROOT;
    static final int DB_SERVER_ENVFLAGS = DbConstants.DB_INIT_CDB |
        DbConstants.DB_INIT_LOCK | DbConstants.DB_INIT_LOG | DbConstants.DB_INIT_MPOOL |
        DbConstants.DB_INIT_TXN | DbConstants.DB_JOINENV;
    static final int DB_SERVER_DBFLAGS = DbConstants.DB_DIRTY_READ |
        DbConstants.DB_NOMMAP | DbConstants.DB_RDONLY;
    static final int DB_SERVER_DBNOSHARE = DbConstants.DB_EXCL | DbConstants.DB_TRUNCATE;
    static final int DB_MODIFIER_MASK = 0xff000000;

    static Vector homes = new Vector();

    static void add_home(String home) {
        File f = new File(home);
        try {
            home = f.getCanonicalPath();
        } catch (IOException e) {
            // ignored
        }
        homes.addElement(home);
    }

    static boolean check_home(String home) {
        if (home == null)
            return false;
        File f = new File(home);
        try {
            home = f.getCanonicalPath();
        } catch (IOException e) {
            // ignored
        }
        return homes.contains(home);
    }

    public static void main(String[] args) {
        System.out.println("Starting Server...");
        for (int i = 0; i < args.length; i++) {
            if (args[i].charAt(0) != '-')
                usage();

            switch (args[i].charAt(1)) {
            case 'h':
                add_home(args[++i]);
                break;
            case 'I':
                idleto = Long.parseLong(args[++i]) * 1000L;
                break;
            case 'P':
                passwd = args[++i];
                break;
            case 't':
                defto = Long.parseLong(args[++i]) * 1000L;
                break;
            case 'T':
                maxto = Long.parseLong(args[++i]) * 1000L;
                break;
            case 'V':
                // version;
                break;
            case 'v':
                // verbose
                break;
            default:
                usage();
            }
        }

        try {
            // Server.errstream = System.err;
            Server.errstream = new FileOutputStream("JavaRPCServer.trace", true);
            Server.err = new PrintWriter(Server.errstream);
            Server server = new Server();
            server.run();
        } catch (Throwable e) {
            System.err.println("Server exception:");
            e.printStackTrace(Server.err);
        } finally {
            if (Server.err != null)
                Server.err.close();
        }

        System.out.println("Server stopped.");
    }

    static void usage() {
        System.err.println("usage: java com.sleepycat.db.rpcserver.Server \\");
        System.err.println("[-Vv] [-h home] [-P passwd] [-I idletimeout] [-L logfile] [-t def_timeout] [-T maxtimeout]");
        System.exit(1);
    }
}
