/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleSerialDbFactoryTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */
package com.sleepycat.bdb.serial.test;

import com.sleepycat.bdb.bind.serial.test.MarshalledObject;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.ForeignKeyIndex;
import com.sleepycat.bdb.StoredClassCatalog;
import com.sleepycat.bdb.TransactionRunner;
import com.sleepycat.bdb.TransactionWorker;
import com.sleepycat.bdb.factory.TupleSerialDbFactory;
import com.sleepycat.bdb.test.TestEnv;
import java.util.Map;
import java.util.SortedMap;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class TupleSerialDbFactoryTest extends TestCase
    implements TransactionWorker {

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite();
        for (int i = 0; i < TestEnv.ALL.length; i += 1) {
            for (int sorted = 0; sorted < 2; sorted += 1) {
                suite.addTest(new TupleSerialDbFactoryTest(TestEnv.ALL[i],
                                                           sorted != 0));
            }
        }
        return suite;
    }

    private TestEnv testEnv;
    private DbEnv env;
    private StoredClassCatalog catalog;
    private TransactionRunner runner;
    private TupleSerialDbFactory factory;
    private DataStore store1;
    private DataStore store2;
    private DataIndex index1;
    private ForeignKeyIndex index2;
    private boolean isSorted;
    private Map storeMap1;
    private Map storeMap2;
    private Map indexMap1;
    private Map indexMap2;

    public TupleSerialDbFactoryTest(TestEnv testEnv, boolean isSorted) {

        super(null);

        this.testEnv = testEnv;
        this.isSorted = isSorted;

        String name = "TupleSerialDbFactoryTest-" + testEnv.getName();
        name += isSorted ? "-sorted" : "-unsorted";
        setName(name);
    }

    public void setUp()
        throws Exception {

        System.out.println(getName());
        env = testEnv.open(getName());
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

        int openFlags = Db.DB_CREATE;
        if (testEnv.isTxnMode())
            openFlags |= Db.DB_AUTO_COMMIT;

        catalog = new StoredClassCatalog(env, "catalog.db", null, openFlags);
        factory = new TupleSerialDbFactory(catalog);
        assertSame(catalog, factory.getCatalog());

        int type = isSorted ? Db.DB_BTREE : Db.DB_HASH;
        Db db;

        db = new Db(env, 0);
        db.open(null, "store1.db", null, type, openFlags, 0);
        store1 = factory.newDataStore(db, MarshalledObject.class, null);

        db = new Db(env, 0);
        db.open(null, "index1.db", null, type, openFlags, 0);
        index1 = factory.newDataIndex(store1, db, "1", false, true);

        db = new Db(env, 0);
        db.open(null, "store2.db", null, type, openFlags, 0);
        store2 = factory.newDataStore(db, MarshalledObject.class, null);

        db = new Db(env, 0);
        db.open(null, "index2.db", null, type, openFlags, 0);
        index2 = factory.newForeignKeyIndex(store2, db, "2", false, true,
                                            store1,
                                            ForeignKeyIndex.ON_DELETE_CASCADE);
    }

    private void createViews()
        throws Exception {

        if (isSorted) {
            storeMap1 = factory.newSortedMap(store1, String.class, true);
            storeMap2 = factory.newSortedMap(store2, String.class, true);
            indexMap1 = factory.newSortedMap(index1, String.class, true);
            indexMap2 = factory.newSortedMap(index2, String.class, true);
        } else {
            storeMap1 = factory.newMap(store1, String.class, true);
            storeMap2 = factory.newMap(store2, String.class, true);
            indexMap1 = factory.newMap(index1, String.class, true);
            indexMap2 = factory.newMap(index2, String.class, true);
        }
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

        storeMap1.remove("pk1");
        assertNull(storeMap1.get("pk1"));
        assertNull(indexMap1.get("ik1"));
        assertNull(storeMap2.get("pk2"));
        assertNull(indexMap2.get("pk1"));
    }
}
