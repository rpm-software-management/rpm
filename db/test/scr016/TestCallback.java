/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestCallback.java,v 1.3 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * Simple tests for DbErrorHandler, DbFeedbackHandler, DbPanicHandler
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;

public class TestCallback
  implements DbFeedbackHandler, DbErrorHandler, DbPanicHandler,
    DbEnvFeedbackHandler, DbBtreeCompare
{
    public void run()
        throws DbException, FileNotFoundException
    {
        DbEnv dbenv = new DbEnv(0);
        dbenv.setFeedbackHandler(this);
        dbenv.setPanicHandler(this);
        dbenv.setErrorHandler(this);
        dbenv.open(".", Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL | Db.DB_INIT_LOG
                   | Db.DB_INIT_TXN | Db.DB_CREATE, 0);
        Db db = new Db(dbenv, 0);
        db.setFeedbackHandler(this);
        //db.setPanicHandler(this);
        //db.setErrorHandler(this);
        db.open(null, "my.db", null, Db.DB_BTREE, Db.DB_CREATE, 0644);

        TestUtil.populate(db);
        dbenv.txnCheckpoint(0, 0, Db.DB_FORCE);

        System.out.println("before compare");
        try {
            db.setBtreeCompare(null);
        }
        catch (IllegalArgumentException dbe)
        {
            System.out.println("got expected exception: " + dbe);
            // ignore
        }
        System.out.println("after compare");

        /*
        // Pretend we crashed, and reopen the environment
        db = null;
        dbenv = null;

        dbenv = new DbEnv(0);
        dbenv.setFeedbackHandler(this);
        dbenv.open(".", Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL | Db.DB_INIT_LOG
                   | Db.DB_INIT_TXN | Db.DB_RECOVER, 0);
        */

        dbenv.setFlags(Db.DB_PANIC_ENVIRONMENT, true);
        System.out.println("before panic");
        try {
            Dbt key = new Dbt("foo".getBytes());
            Dbt data = new Dbt();
            db.get(null, key, data, 0);
        }
        catch (DbException dbe2)
        {
            System.out.println("got expected exception: " + dbe2);
            // ignore
        }
        System.out.println("after panic");

    }

    public static void main(String[] args)
    {
        try {
            (new TestCallback()).run();
        }
        catch (DbException dbe) {
            System.err.println("Db Exception: " + dbe);
        }
        catch (FileNotFoundException fnfe) {
            System.err.println("FileNotFoundException: " + fnfe);
        }
        System.out.println("finished test");
    }

    public void panic(DbEnv dbenv, DbException e)
    {
        System.out.println("CALLBACK: panic(" +
                           envStr(dbenv) + ", " + e + ")");
    }

    public void error(String prefix, String str)
    {
        System.out.println("CALLBACK: error(" + quotedStr(prefix) +
                           ", " + quotedStr(str) + ")");
    }

    public void feedback(Db db, int opcode, int pct)
    {
        System.out.println("CALLBACK: (db) feedback(" +
                           dbStr(db) + ", " + opcode + ", " + pct + ")");
    }

    public void feedback(DbEnv dbenv, int opcode, int pct)
    {
        System.out.println("CALLBACK: (env) feedback(" +
                           envStr(dbenv) + ", " + opcode + ", " + pct + ")");
    }

    public String quotedStr(String s)
    {
        if (s == null)
            return "null";
        else
            return "\"" + s + "\"";
    }

    public String envStr(DbEnv dbenv)
    {
        if (dbenv == null)
            return "null";
        else
            return "DbEnv";
    }

    public String dbStr(Db db)
    {
        if (db == null)
            return "null";
        else
            return "Db";
    }

    public int compare(Db db, Dbt dbt1, Dbt dbt2)
    {
        System.out.println("**ERROR** btree compare should never be called" +
                           " in this test");
        return 0;
    }
}
