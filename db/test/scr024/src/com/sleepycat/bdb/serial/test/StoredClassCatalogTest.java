/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredClassCatalogTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */
package com.sleepycat.bdb.serial.test;

import com.sleepycat.bdb.bind.serial.SerialBinding;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbTxn;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.StoredClassCatalog;
import com.sleepycat.bdb.TransactionRunner;
import com.sleepycat.bdb.TransactionWorker;
import com.sleepycat.bdb.collection.StoredMap;
import com.sleepycat.bdb.test.DbTestUtil;
import com.sleepycat.bdb.test.TestEnv;
import com.sleepycat.bdb.util.ExceptionUnwrapper;
import java.io.File;
import java.util.Map;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class StoredClassCatalogTest extends TestCase
    implements TransactionWorker {

    private static final String CATALOG_FILE = "catalogtest-catalog.db";
    private static final String STORE_FILE = "catalogtest-store.db";

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite();
        for (int i = 0; i < TestEnv.ALL.length; i += 1) {
            suite.addTest(new StoredClassCatalogTest(TestEnv.ALL[i]));
        }
        return suite;
    }

    private TestEnv testEnv;
    private DbEnv env;
    private StoredClassCatalog catalog;
    private StoredClassCatalog catalog2;
    private DataStore store;
    private Map map;
    private TransactionRunner runner;
    private boolean dataExists;
    private int openFlags;

    public StoredClassCatalogTest(TestEnv testEnv) {

        super("StoredClassCatalogTest-" + testEnv.getName());
        this.testEnv = testEnv;
    }

    public void setUp()
        throws Exception {

        System.out.println(getName());
        env = testEnv.open(getName());
        runner = new TransactionRunner(env);
        File dir = testEnv.getDirectory(getName());

        openFlags = Db.DB_CREATE;
        if (testEnv.isTxnMode())
            openFlags |= Db.DB_AUTO_COMMIT;

        if (DbTestUtil.copyResource(getClass(), CATALOG_FILE, dir) &&
            DbTestUtil.copyResource(getClass(), STORE_FILE, dir)) {
            dataExists = true;
        }
        catalog = new StoredClassCatalog(env, CATALOG_FILE,
                                         null, openFlags);
        catalog2 = new StoredClassCatalog(env, "new_catalog.db", null,
                                          openFlags);

        SerialFormat keyFormat = new SerialFormat(catalog,
                                                  String.class);
        SerialFormat valueFormat = new SerialFormat(catalog,
                                                    TestSerial.class);
        store = new DataStore(openDb(STORE_FILE),
                              keyFormat, valueFormat, null);

        SerialBinding keyBinding = new SerialBinding(keyFormat);
        SerialBinding valueBinding = new SerialBinding(valueFormat);
        map = new StoredMap(store, keyBinding, valueBinding, true);
    }

    private Db openDb(String file)
        throws Exception {

        Db db = new Db(env, 0);
        db.open(null, file, null, Db.DB_BTREE, openFlags, 0);
        return db;
    }

    public void tearDown() {

        try {
            if (catalog != null) {
                catalog.close();
                catalog.close(); // should have no effect
                catalog = null;
            }
            if (catalog2 != null) {
                catalog2.close();
                catalog2 = null;
            }
            if (store != null) {
                store.close();
                store = null;
            }
            if (env != null) {
                env.close(0);
                env = null;
            }
        }
        catch (Exception e) {
            e.printStackTrace();
            System.out.println("Ignored exception during tearDown: " + e);
        }
    }

    public void runTest()
        throws Exception {

        runner.run(this);
    }

    public void doWork()
        throws Exception {

        if (dataExists) {
            doTest();
        } else {
            firstTimeInit();
        }
    }

    private void doTest()
        throws Exception {

        TestSerial one = (TestSerial) map.get("one");
        TestSerial two = (TestSerial) map.get("two");
        assertNotNull(one);
        assertNotNull(two);
        assertEquals(one, two.getOther());
        assertNull(one.getStringField());
        assertNull(two.getStringField());

        TestSerial three = new TestSerial(two);
        assertNotNull(three.getStringField());
        map.put("three", three);
        three = (TestSerial) map.get("three");
        assertEquals(two, three.getOther());

        // getClassFormat(String) is not normally called via bindings
        assertNotNull(catalog.getClassFormat(TestSerial.class.getName()));
        assertNotNull(catalog.getClassFormat(TestSerial.class.getName()));

        // test with empty catalog
        assertNotNull(catalog2.getClassFormat(TestSerial.class.getName()));
        assertNotNull(catalog2.getClassFormat(TestSerial.class.getName()));
    }

    private void firstTimeInit() {

        System.out.println("*** WARNING: First time initialization can " +
                "only be performed using a special TestSerial class ***");
        TestSerial one = new TestSerial(null);
        TestSerial two = new TestSerial(one);
        assertNull(one.getStringField());
        assertNull(two.getStringField());
        map.put("one", one);
        map.put("two", two);
        one = (TestSerial) map.get("one");
        two = (TestSerial) map.get("two");
        assertEquals(one, two.getOther());
        assertNull(one.getStringField());
        assertNull(two.getStringField());
    }
}
