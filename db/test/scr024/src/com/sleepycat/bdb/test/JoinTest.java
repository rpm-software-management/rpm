/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: JoinTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */
package com.sleepycat.bdb.test;

import com.sleepycat.bdb.bind.serial.test.MarshalledObject;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.StoredClassCatalog;
import com.sleepycat.bdb.TransactionRunner;
import com.sleepycat.bdb.TransactionWorker;
import com.sleepycat.bdb.collection.StoredCollection;
import com.sleepycat.bdb.collection.StoredContainer;
import com.sleepycat.bdb.collection.StoredIterator;
import com.sleepycat.bdb.collection.StoredMap;
import com.sleepycat.bdb.factory.TupleSerialDbFactory;
import com.sleepycat.bdb.test.TestEnv;
import java.util.Map;
import junit.framework.Test;
import junit.framework.TestCase;

/**
 * @author Mark Hayes
 */
public class JoinTest extends TestCase
    implements TransactionWorker {

    private static final String MATCH_DATA = "d4"; // matches both keys = "yes"
    private static final String MATCH_KEY  = "k4"; // matches both keys = "yes"
    private static final String[] VALUES = {"yes", "yes"};

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        return new JoinTest();
    }

    private DbEnv env;
    private TransactionRunner runner;
    private StoredClassCatalog catalog;
    private TupleSerialDbFactory factory;
    private DataStore store;
    private DataIndex index1;
    private DataIndex index2;
    private StoredMap storeMap;
    private StoredMap indexMap1;
    private StoredMap indexMap2;

    public JoinTest() {

        super("JoinTest");
    }

    public void setUp()
        throws Exception {

        System.out.println(getName());
        env = TestEnv.TXN.open(getName());
        runner = new TransactionRunner(env);
        createDatabase();
    }

    public void tearDown() {

        try {
            if (store != null) {
                store.close();
                store = null;
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

        catalog = new StoredClassCatalog(env, "catalog.db", null,
                                         Db.DB_CREATE | Db.DB_AUTO_COMMIT);
        factory = new TupleSerialDbFactory(catalog);
        assertSame(catalog, factory.getCatalog());

        store = factory.newDataStore(openDb("store.db", false),
                                        MarshalledObject.class, null);

        index1 = factory.newDataIndex(store, openDb("index1.db", true),
                                        "1", false, true);

        index2 = factory.newDataIndex(store, openDb("index2.db", true),
                                        "2", false, true);
    }

    private Db openDb(String file, boolean dups)
        throws Exception {

        Db db = new Db(env, 0);
        if (dups)
            db.setFlags(Db.DB_DUPSORT);
        db.open(null, file, null, Db.DB_HASH,
                Db.DB_CREATE | Db.DB_AUTO_COMMIT, 0);
        return db;
    }

    private void createViews()
        throws Exception {

        storeMap = factory.newMap(store, String.class, true);
        indexMap1 = factory.newMap(index1, String.class, true);
        indexMap2 = factory.newMap(index2, String.class, true);
    }

    private void writeAndRead()
        throws Exception {

        // write records: Data, PrimaryKey, IndexKey1, IndexKey2
        assertNull(storeMap.put(null,
            new MarshalledObject("d1", "k1", "no",  "yes")));
        assertNull(storeMap.put(null,
            new MarshalledObject("d2", "k2", "no",  "no")));
        assertNull(storeMap.put(null,
            new MarshalledObject("d3", "k3", "no",  "yes")));
        assertNull(storeMap.put(null,
            new MarshalledObject("d4", "k4", "yes", "yes")));
        assertNull(storeMap.put(null,
            new MarshalledObject("d5", "k5", "yes", "no")));

        Object o;
        Map.Entry e;

        // join values with index maps
        o = doJoin((StoredCollection) storeMap.values());
        assertEquals(MATCH_DATA, ((MarshalledObject) o).getData());

        // join keySet with index maps
        o = doJoin((StoredCollection) storeMap.keySet());
        assertEquals(MATCH_KEY, o);

        // join entrySet with index maps
        o = doJoin((StoredCollection) storeMap.entrySet());
        e = (Map.Entry) o;
        assertEquals(MATCH_KEY, e.getKey());
        assertEquals(MATCH_DATA, ((MarshalledObject) e.getValue()).getData());
    }

    private Object doJoin(StoredCollection coll) {

        StoredContainer[] indices = { indexMap1, indexMap2 };
        StoredIterator i = coll.join(indices, VALUES);
        try {
            assertTrue(i.hasNext());
            Object result = i.next();
            assertNotNull(result);
            assertFalse(i.hasNext());
            return result;
        } finally { i.close(); }
    }
}

