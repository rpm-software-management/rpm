/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: ForeignKeyTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */
package com.sleepycat.bdb.test;

import com.sleepycat.bdb.bind.serial.test.MarshalledObject;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.ForeignKeyIndex;
import com.sleepycat.bdb.IntegrityConstraintException;
import com.sleepycat.bdb.StoredClassCatalog;
import com.sleepycat.bdb.TransactionRunner;
import com.sleepycat.bdb.TransactionWorker;
import com.sleepycat.bdb.factory.TupleSerialDbFactory;
import com.sleepycat.bdb.test.TestEnv;
import java.util.Map;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class ForeignKeyTest extends TestCase
    implements TransactionWorker {

    private static final int[] ON_DELETE = {
        ForeignKeyIndex.ON_DELETE_ABORT,
        ForeignKeyIndex.ON_DELETE_CLEAR,
        ForeignKeyIndex.ON_DELETE_CASCADE,
    };
    private static final String[] ON_DELETE_LABEL = {
        "ON_DELETE_ABORT",
        "ON_DELETE_CLEAR",
        "ON_DELETE_CASCADE",
    };

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite();
        for (int i = 0; i < TestEnv.ALL.length; i += 1) {
            for (int j = 0; j < ON_DELETE.length; j += 1) {
                suite.addTest(new ForeignKeyTest(TestEnv.ALL[i],
                                                 ON_DELETE[j],
                                                 ON_DELETE_LABEL[j]));
            }
        }
        return suite;
    }

    private TestEnv testEnv;
    private DbEnv env;
    private TransactionRunner runner;
    private StoredClassCatalog catalog;
    private TupleSerialDbFactory factory;
    private DataStore store1;
    private DataStore store2;
    private DataIndex index1;
    private ForeignKeyIndex index2;
    private Map storeMap1;
    private Map storeMap2;
    private Map indexMap1;
    private Map indexMap2;
    private int onDelete;
    private int openFlags;

    public ForeignKeyTest(TestEnv testEnv, int onDelete,
                          String onDeleteLabel) {

        super("ForeignKeyTest-" + testEnv.getName() + '-' + onDeleteLabel);

        this.testEnv = testEnv;
        this.onDelete = onDelete;
    }

    public void setUp()
        throws Exception {

        System.out.println(getName());
        env = testEnv.open(getName());
        openFlags = Db.DB_CREATE;
        if (testEnv.isTxnMode())
            openFlags |= Db.DB_AUTO_COMMIT;
        runner = new TransactionRunner(env);
        createDatabase();
    }

    public void tearDown() {

        try {
            if (store1 != null) {
                store1.close();
                store1 = null;
            }
            if (store2 != null) {
                store2.close();
                store2 = null;
            }
            if (catalog != null) {
                catalog.close();
                catalog = null;
            }
            if (env != null) {
                env.close(0);
                env = null;
            }
        } catch (Exception e) {
            System.out.println("Ignored exception during tearDown: " + e);
        }
    }

    public void runTest()
        throws Exception {

        runner.run(this);
    }

    public void doWork()
        throws Exception {

        createViews();
        writeAndRead();
    }

    private void createDatabase()
        throws Exception {

        catalog = new StoredClassCatalog(env, "catalog.db", null, openFlags);
        factory = new TupleSerialDbFactory(catalog);
        assertSame(catalog, factory.getCatalog());

        store1 = factory.newDataStore(openDb("store1.db"),
                                        MarshalledObject.class, null);

        index1 = factory.newDataIndex(store1, openDb("index1.db"),
                                        "1", false, true);

        store2 = factory.newDataStore(openDb("store2.db"),
                                        MarshalledObject.class, null);

        index2 = factory.newForeignKeyIndex(store2, openDb("index2.db"),
                                        "2", false, true, store1, onDelete);

        assertSame(index2.getForeignStore(), store1);
        assertEquals(index2.getDeleteAction(), onDelete);
    }

    private Db openDb(String file)
        throws Exception {

        Db db = new Db(env, 0);
        db.open(null, file, null, Db.DB_HASH, openFlags, 0);
        return db;
    }

    private void createViews()
        throws Exception {

        storeMap1 = factory.newMap(store1, String.class, true);
        storeMap2 = factory.newMap(store2, String.class, true);
        indexMap1 = factory.newMap(index1, String.class, true);
        indexMap2 = factory.newMap(index2, String.class, true);
    }

    private void writeAndRead()
        throws Exception {

        MarshalledObject o1 = new MarshalledObject("data1", "pk1", "ik1", "");
        assertNull(storeMap1.put(null, o1));

        assertEquals(o1, storeMap1.get("pk1"));
        assertEquals(o1, indexMap1.get("ik1"));

        MarshalledObject o2 = new MarshalledObject("data2", "pk2", "", "pk1");
        assertNull(storeMap2.put(null, o2));

        assertEquals(o2, storeMap2.get("pk2"));
        assertEquals(o2, indexMap2.get("pk1"));

        /*
         * store1 contains o1 with primary key "pk1" and index key "ik1"
         * store2 contains o2 with primary key "pk2" and foreign key "pk1"
         * which is the primary key of store1
         */

        switch (onDelete) {

        case ForeignKeyIndex.ON_DELETE_ABORT:
            try {
                storeMap1.remove("pk1");
                fail();
            }
            catch (IntegrityConstraintException expected) {}
            o2 = new MarshalledObject("data2", "pk2", "", "");
            assertNotNull(storeMap2.put(null, o2));
            assertEquals(o2, storeMap2.get("pk2"));
            assertNull(indexMap2.get("pk1"));
            storeMap1.remove("pk1");
            assertNull(storeMap1.get("pk1"));
            assertNull(indexMap1.get("ik1"));
            break;
        case ForeignKeyIndex.ON_DELETE_CLEAR:
            storeMap1.remove("pk1");
            assertNull(storeMap1.get("pk1"));
            assertNull(indexMap1.get("ik1"));
            o2 = (MarshalledObject) storeMap2.get("pk2");
            assertNotNull(o2);
            assertEquals("data2", o2.getData());
            assertEquals("pk2", o2.getPrimaryKey());
            assertEquals("", o2.getIndexKey1());
            assertEquals("", o2.getIndexKey2());
            break;
        case ForeignKeyIndex.ON_DELETE_CASCADE:
            storeMap1.remove("pk1");
            assertNull(storeMap1.get("pk1"));
            assertNull(indexMap1.get("ik1"));
            assertNull(storeMap2.get("pk2"));
            assertNull(indexMap2.get("pk1"));
            break;
        default:
            throw new IllegalStateException();
        }
    }
}

