/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestEnv.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.test;

import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import java.io.File;
import java.io.IOException;

/**
 * @author Mark Hayes
 */
public class TestEnv {

    public static final TestEnv BDB =
       new TestEnv("bdb", Db.DB_INIT_MPOOL);
    public static final TestEnv CDB =
       new TestEnv("cdb", Db.DB_INIT_CDB | Db.DB_INIT_MPOOL);
    public static final TestEnv TXN =
       new TestEnv("txn", Db.DB_INIT_TXN | Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL);

    //public static final TestEnv[] ALL = { TXN };
    public static final TestEnv[] ALL = { BDB, CDB, TXN };

    private String name;
    private int flags;

    private TestEnv(String name, int flags) {

        this.name = name;
        this.flags = flags | Db.DB_CREATE;
    }

    public String getName() {

        return name;
    }

    public boolean isTxnMode() {

        return (flags & Db.DB_INIT_TXN) != 0;
    }

    public boolean isCdbMode() {

        return (flags & Db.DB_INIT_CDB) != 0;
    }

    public DbEnv open(String testName)
        throws IOException, DbException {

        File dir = getDirectory(testName);
        DbEnv dbEnv = new DbEnv(0);
        dbEnv.open(dir.getAbsolutePath(), flags, 0);
        return dbEnv;
    }

    public File getDirectory(String testName)
        throws IOException {

        return DbTestUtil.getNewDir("db-test/" + testName);
    }
}
