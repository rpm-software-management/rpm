/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: CatalogCornerCaseTest.java,v 12.5 2007/05/04 00:28:29 mark Exp $
 */
package com.sleepycat.collections.test.serial;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import com.sleepycat.bind.serial.StoredClassCatalog;
import com.sleepycat.collections.test.DbTestUtil;
import com.sleepycat.collections.test.TestEnv;
import com.sleepycat.compat.DbCompat;
import com.sleepycat.db.Database;
import com.sleepycat.db.DatabaseConfig;
import com.sleepycat.db.Environment;

/**
 * @author Mark Hayes
 */
public class CatalogCornerCaseTest extends TestCase {

    public static void main(String[] args)
        throws Exception {

        junit.framework.TestResult tr =
            junit.textui.TestRunner.run(suite());
        if (tr.errorCount() > 0 ||
            tr.failureCount() > 0) {
            System.exit(1);
        } else {
            System.exit(0);
        }
    }

    public static Test suite()
        throws Exception {

        return new TestSuite(CatalogCornerCaseTest.class);
    }

    private Environment env;

    public CatalogCornerCaseTest(String name) {

        super(name);
    }

    public void setUp()
        throws Exception {

        DbTestUtil.printTestName(getName());
        env = TestEnv.BDB.open(getName());
    }

    public void tearDown() {

        try {
            if (env != null) {
                env.close();
            }
        } catch (Exception e) {
            System.out.println("Ignored exception during tearDown: " + e);
        } finally {
            /* Ensure that GC can cleanup. */
            env = null;
        }
    }

    public void testReadOnlyEmptyCatalog()
        throws Exception {

        String file = "catalog.db";

        /* Create an empty database. */
        DatabaseConfig config = new DatabaseConfig();
        config.setAllowCreate(true);
        DbCompat.setTypeBtree(config);
        Database db = DbCompat.openDatabase(env, null, file, null, config);
        db.close();

        /* Open the empty database read-only. */
        config.setAllowCreate(false);
        config.setReadOnly(true);
        db = DbCompat.openDatabase(env, null, file, null, config);

        /* Expect exception when creating the catalog. */
        try {
            new StoredClassCatalog(db);
            fail();
        } catch (IllegalStateException e) { }
        db.close();
    }
}
