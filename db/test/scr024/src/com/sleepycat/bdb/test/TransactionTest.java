/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TransactionTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.test;

import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbTxn;
import com.sleepycat.bdb.CurrentTransaction;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.TransactionRunner;
import com.sleepycat.bdb.TransactionWorker;
import com.sleepycat.bdb.collection.StoredContainer;
import com.sleepycat.bdb.collection.StoredCollections;
import com.sleepycat.bdb.collection.StoredIterator;
import com.sleepycat.bdb.collection.StoredList;
import com.sleepycat.bdb.collection.StoredSortedMap;
import com.sleepycat.bdb.util.RuntimeExceptionWrapper;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.SortedMap;
import java.util.SortedSet;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class TransactionTest extends TestCase {

    private static final Long ONE = new Long(1);
    private static final Long TWO = new Long(2);
    private static final Long THREE = new Long(3);

    /**
     * Runs a command line collection test.
     * @see #usage
     */
    public static void main(String[] args)
        throws Exception {

        if (args.length == 1 &&
            (args[0].equals("-h") || args[0].equals("-help"))) {
            usage();
        } else {
            junit.textui.TestRunner.run(suite());
        }
    }

    private static void usage() {

        System.out.println(
          "Usage: java com.sleepycat.bdb.test.TransactionTest [-h | -help]\n");
        System.exit(2);
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite(TransactionTest.class);
        return suite;
    }

    private DbEnv env;
    private CurrentTransaction currentTxn;
    private DataStore store;
    private StoredSortedMap map;
    private TestStore testStore = TestStore.BTREE_UNIQ;

    public TransactionTest(String name) {

        super(name);
    }

    public void setUp()
        throws Exception {

        System.out.println(DbTestUtil.qualifiedTestName(this));
        env = TestEnv.TXN.open("TransactionTests");
        currentTxn = CurrentTransaction.getInstance(env);
        store = testStore.open(env, null);
        map = new StoredSortedMap(store, testStore.getKeyBinding(),
                                  testStore.getValueBinding(), true);
    }

    public void tearDown() {

        try {
            if (store != null) {
                store.close();
                store = null;
            }
            if (env != null) {
                env.close(0);
                env = null;
            }
        } catch (Exception e) {
            System.out.println("Ignored exception during tearDown: " + e);
        }
    }

    public void testGetters()
        throws Exception {

        assertNotNull(env);
        assertNotNull(currentTxn);
        assertNull(currentTxn.getTxn());

        currentTxn.beginTxn();
        assertNotNull(currentTxn.getTxn());
        currentTxn.commitTxn();
        assertNull(currentTxn.getTxn());

        currentTxn.beginTxn();
        assertNotNull(currentTxn.getTxn());
        currentTxn.abortTxn();
        assertNull(currentTxn.getTxn());

        // dirty-read property should be inherited

        assertTrue(!map.isDirtyReadEnabled());
        assertTrue(!((StoredContainer) map.values()).isDirtyReadEnabled());
        assertTrue(!((StoredContainer) map.keySet()).isDirtyReadEnabled());
        assertTrue(!((StoredContainer) map.entrySet()).isDirtyReadEnabled());

        StoredSortedMap other =
            (StoredSortedMap) StoredCollections.dirtyReadMap(map);
        assertTrue(other.isDirtyReadEnabled());
        assertTrue(((StoredContainer) other.values()).isDirtyReadEnabled());
        assertTrue(((StoredContainer) other.keySet()).isDirtyReadEnabled());
        assertTrue(((StoredContainer) other.entrySet()).isDirtyReadEnabled());
        assertTrue(!map.isDirtyReadEnabled());
        assertTrue(!((StoredContainer) map.values()).isDirtyReadEnabled());
        assertTrue(!((StoredContainer) map.keySet()).isDirtyReadEnabled());
        assertTrue(!((StoredContainer) map.entrySet()).isDirtyReadEnabled());

        // auto-commit property should be inherited

        assertTrue(!map.isAutoCommit());
        assertTrue(!((StoredContainer) map.values()).isAutoCommit());
        assertTrue(!((StoredContainer) map.keySet()).isAutoCommit());
        assertTrue(!((StoredContainer) map.entrySet()).isAutoCommit());

        other = (StoredSortedMap) StoredCollections.autoCommitMap(map);
        assertTrue(other.isAutoCommit());
        assertTrue(((StoredContainer) other.values()).isAutoCommit());
        assertTrue(((StoredContainer) other.keySet()).isAutoCommit());
        assertTrue(((StoredContainer) other.entrySet()).isAutoCommit());
        assertTrue(!map.isAutoCommit());
        assertTrue(!((StoredContainer) map.values()).isAutoCommit());
        assertTrue(!((StoredContainer) map.keySet()).isAutoCommit());
        assertTrue(!((StoredContainer) map.entrySet()).isAutoCommit());

        // auto-commit property should be ORed with environment auto-commit

        env.setFlags(Db.DB_AUTO_COMMIT, true);
        assertTrue(map.isAutoCommit());
        assertTrue(((StoredContainer) map.values()).isAutoCommit());
        assertTrue(((StoredContainer) map.keySet()).isAutoCommit());
        assertTrue(((StoredContainer) map.entrySet()).isAutoCommit());

        env.setFlags(Db.DB_AUTO_COMMIT, false);
        assertTrue(!map.isAutoCommit());
        assertTrue(!((StoredContainer) map.values()).isAutoCommit());
        assertTrue(!((StoredContainer) map.keySet()).isAutoCommit());
        assertTrue(!((StoredContainer) map.entrySet()).isAutoCommit());
    }

    public void testTransactional()
        throws Exception {

        // is transactional because DB_AUTO_COMMIT was passed to Db.open()
        //
        assertTrue(map.isTransactional());
        store.close();
        store = null;

        // is not transactional because is not opened in a transaction
        //
        Db db = new Db(env, 0);
        db.open(null, null, null, Db.DB_BTREE, 0, 0);
        store = new DataStore(db, TestStore.BYTE_FORMAT,
                                  TestStore.BYTE_FORMAT, null);
        map = new StoredSortedMap(store,
                testStore.getKeyBinding(), testStore.getValueBinding(), true);
        assertTrue(!map.isTransactional());
        map.put(ONE, ONE);
        readCheck(map, ONE, ONE);
        store.close();

        // is transactional because is opened in a transaction
        //
        currentTxn.beginTxn();
        db = new Db(env, 0);
        db.open(currentTxn.getTxn(), null, null, Db.DB_BTREE, 0, 0);
        currentTxn.commitTxn();
        store = new DataStore(db, TestStore.BYTE_FORMAT,
                                  TestStore.BYTE_FORMAT, null);
        map = new StoredSortedMap(store,
                testStore.getKeyBinding(), testStore.getValueBinding(), true);
        assertTrue(map.isTransactional());
        currentTxn.beginTxn();
        map.put(ONE, ONE);
        readCheck(map, ONE, ONE);
        currentTxn.commitTxn();
    }

    public void testExceptions()
        throws Exception {

        try {
            currentTxn.commitTxn();
            fail();
        } catch (IllegalStateException expected) {}

        try {
            currentTxn.abortTxn();
            fail();
        } catch (IllegalStateException expected) {}
    }

    public void testNested()
        throws Exception {

        assertNull(currentTxn.getTxn());

        DbTxn txn1 = currentTxn.beginTxn();
        assertNotNull(txn1);
        assertTrue(txn1 == currentTxn.getTxn());

        assertNull(map.get(ONE));
        assertNull(map.put(ONE, ONE));
        assertEquals(ONE, map.get(ONE));

        DbTxn txn2 = currentTxn.beginTxn();
        assertNotNull(txn2);
        assertTrue(txn2 == currentTxn.getTxn());
        assertTrue(txn1 != txn2);

        assertNull(map.put(TWO, TWO));
        assertEquals(TWO, map.get(TWO));

        DbTxn txn3 = currentTxn.beginTxn();
        assertNotNull(txn3);
        assertTrue(txn3 == currentTxn.getTxn());
        assertTrue(txn1 != txn2);
        assertTrue(txn1 != txn3);
        assertTrue(txn2 != txn3);

        assertNull(map.put(THREE, THREE));
        assertEquals(THREE, map.get(THREE));

        DbTxn txn = currentTxn.abortTxn();
        assertTrue(txn == txn2);
        assertTrue(txn == currentTxn.getTxn());
        assertNull(map.get(THREE));
        assertEquals(TWO, map.get(TWO));

        txn3 = currentTxn.beginTxn();
        assertNotNull(txn3);
        assertTrue(txn3 == currentTxn.getTxn());
        assertTrue(txn1 != txn2);
        assertTrue(txn1 != txn3);
        assertTrue(txn2 != txn3);

        assertNull(map.put(THREE, THREE));
        assertEquals(THREE, map.get(THREE));

        txn = currentTxn.commitTxn();
        assertTrue(txn == txn2);
        assertTrue(txn == currentTxn.getTxn());
        assertEquals(THREE, map.get(THREE));
        assertEquals(TWO, map.get(TWO));

        txn = currentTxn.commitTxn();
        assertTrue(txn == txn1);
        assertTrue(txn == currentTxn.getTxn());
        assertEquals(THREE, map.get(THREE));
        assertEquals(TWO, map.get(TWO));
        assertEquals(ONE, map.get(ONE));

        txn = currentTxn.commitTxn();
        assertNull(txn);
        assertNull(currentTxn.getTxn());
        assertEquals(THREE, map.get(THREE));
        assertEquals(TWO, map.get(TWO));
        assertEquals(ONE, map.get(ONE));
    }

    public void testRunnerCommit()
        throws Exception {

        commitTest(false);
    }

    public void testExplicitCommit()
        throws Exception {

        commitTest(true);
    }

    private void commitTest(final boolean explicit)
        throws Exception {

        final TransactionRunner runner = new TransactionRunner(env);

        assertNull(currentTxn.getTxn());

        runner.run(new TransactionWorker() {
            public void doWork() throws Exception {
                final DbTxn txn1 = currentTxn.getTxn();
                assertNotNull(txn1);
                assertNull(map.put(ONE, ONE));
                assertEquals(ONE, map.get(ONE));

                runner.run(new TransactionWorker() {
                    public void doWork() throws Exception {
                        final DbTxn txn2 = currentTxn.getTxn();
                        assertNotNull(txn2);
                        assertTrue(txn1 != txn2);
                        assertNull(map.put(TWO, TWO));
                        assertEquals(TWO, map.get(TWO));
                        assertEquals(ONE, map.get(ONE));
                        if (explicit) currentTxn.commitTxn();
                    }
                });

                DbTxn txn3 = currentTxn.getTxn();
                assertSame(txn1, txn3);

                assertEquals(TWO, map.get(TWO));
                assertEquals(ONE, map.get(ONE));
            }
        });

        assertNull(currentTxn.getTxn());
    }

    public void testRunnerAbort()
        throws Exception {

        abortTest(false);
    }

    public void testExplicitAbort()
        throws Exception {

        abortTest(true);
    }

    private void abortTest(final boolean explicit)
        throws Exception {

        final TransactionRunner runner = new TransactionRunner(env);

        assertNull(currentTxn.getTxn());

        runner.run(new TransactionWorker() {
            public void doWork() throws Exception {
                final DbTxn txn1 = currentTxn.getTxn();
                assertNotNull(txn1);
                assertNull(map.put(ONE, ONE));
                assertEquals(ONE, map.get(ONE));

                try {
                    runner.run(new TransactionWorker() {
                        public void doWork() throws Exception {
                            final DbTxn txn2 = currentTxn.getTxn();
                            assertNotNull(txn2);
                            assertTrue(txn1 != txn2);
                            assertNull(map.put(TWO, TWO));
                            assertEquals(TWO, map.get(TWO));
                            if (explicit)
                                currentTxn.abortTxn();
                            else
                                throw new IllegalArgumentException(
                                                                "test-abort");
                        }
                    });
                    assertTrue(explicit);
                } catch (IllegalArgumentException e) {
                    assertTrue(!explicit);
                    assertEquals("test-abort", e.getMessage());
                }

                DbTxn txn3 = currentTxn.getTxn();
                assertSame(txn1, txn3);

                assertEquals(ONE, map.get(ONE));
                assertNull(map.get(TWO));
            }
        });

        assertNull(currentTxn.getTxn());
    }

    public void testDirtyReadCollection()
        throws Exception {

        StoredSortedMap dirtyMap =
            (StoredSortedMap) StoredCollections.dirtyReadSortedMap(map);

        // original map is not dirty-read
        assertTrue(map.isDirtyReadAllowed());
        assertTrue(!map.isDirtyReadEnabled());

        // all dirty-read containers are dirty-read
        checkDirtyReadProperty(dirtyMap);
        checkDirtyReadProperty(StoredCollections.dirtyReadMap(map));
        checkDirtyReadProperty(StoredCollections.dirtyReadCollection(
                                map.values()));
        checkDirtyReadProperty(StoredCollections.dirtyReadSet(
                                map.keySet()));
        checkDirtyReadProperty(StoredCollections.dirtyReadSortedSet(
                                (SortedSet) map.keySet()));

        // create a list just so we can call dirtyReadList()
        DataStore listStore = TestStore.RECNO_RENUM.open(env, null);
        List list = new StoredList(listStore, TestStore.VALUE_BINDING, true);
        checkDirtyReadProperty(StoredCollections.dirtyReadList(list));
        listStore.close();

        doDirtyRead(dirtyMap);
    }

    private void checkDirtyReadProperty(Object container) {

        assertTrue(((StoredContainer) container).isDirtyReadAllowed());
        assertTrue(((StoredContainer) container).isDirtyReadEnabled());
    }

    public void testDirtyReadTransaction()
        throws Exception {

        TransactionRunner runner = new TransactionRunner(env);
        runner.setDirtyRead(true);
        assertNull(currentTxn.getTxn());
        runner.run(new TransactionWorker() {
            public void doWork() throws Exception {
                assertNotNull(currentTxn.getTxn());
                doDirtyRead(map);
            }
        });
        assertNull(currentTxn.getTxn());
    }

    private synchronized void doDirtyRead(StoredSortedMap dirtyMap)
        throws Exception {

        // start thread one
        DirtyReadThreadOne t1 = new DirtyReadThreadOne(env, this);
        t1.start();
        wait();

        // put ONE
        synchronized (t1) { t1.notify(); }
        wait();
        readCheck(dirtyMap, ONE, ONE);
        assertTrue(!dirtyMap.isEmpty());

        // abort ONE
        synchronized (t1) { t1.notify(); }
        t1.join();
        readCheck(dirtyMap, ONE, null);
        assertTrue(dirtyMap.isEmpty());

        // start thread two
        DirtyReadThreadTwo t2 = new DirtyReadThreadTwo(env, this);
        t2.start();
        wait();

        // put TWO
        synchronized (t2) { t2.notify(); }
        wait();
        readCheck(dirtyMap, TWO, TWO);
        assertTrue(!dirtyMap.isEmpty());

        // commit TWO
        synchronized (t2) { t2.notify(); }
        t2.join();
        readCheck(dirtyMap, TWO, TWO);
        assertTrue(!dirtyMap.isEmpty());
    }

    private static class DirtyReadThreadOne extends Thread {

        private DbEnv env;
        private CurrentTransaction currentTxn;
        private TransactionTest parent;
        private StoredSortedMap map;

        private DirtyReadThreadOne(DbEnv env, TransactionTest parent) {

            this.env = env;
            this.currentTxn = CurrentTransaction.getInstance(env);
            this.parent = parent;
            this.map = parent.map;
        }

        public synchronized void run() {

            try {
                assertNull(currentTxn.getTxn());
                assertNotNull(currentTxn.beginTxn());
                assertNotNull(currentTxn.getTxn());
                readCheck(map, ONE, null);
                synchronized (parent) { parent.notify(); }
                wait();

                // put ONE
                assertNull(map.put(ONE, ONE));
                readCheck(map, ONE, ONE);
                synchronized (parent) { parent.notify(); }
                wait();

                // abort ONE
                assertNull(currentTxn.abortTxn());
                assertNull(currentTxn.getTxn());
            } catch (Exception e) {
                throw new RuntimeExceptionWrapper(e);
            }
        }
    }

    private static class DirtyReadThreadTwo extends Thread {

        private DbEnv env;
        private CurrentTransaction currentTxn;
        private TransactionTest parent;
        private StoredSortedMap map;

        private DirtyReadThreadTwo(DbEnv env, TransactionTest parent) {

            this.env = env;
            this.currentTxn = CurrentTransaction.getInstance(env);
            this.parent = parent;
            this.map = parent.map;
        }

        public synchronized void run() {

            try {
                final TransactionRunner runner = new TransactionRunner(env);
                final Object thread = this;
                assertNull(currentTxn.getTxn());

                runner.run(new TransactionWorker() {
                    public void doWork() throws Exception {
                        assertNotNull(currentTxn.getTxn());
                        readCheck(map, TWO, null);
                        synchronized (parent) { parent.notify(); }
                        thread.wait();

                        // put TWO
                        assertNull(map.put(TWO, TWO));
                        readCheck(map, TWO, TWO);
                        synchronized (parent) { parent.notify(); }
                        thread.wait();

                        // commit TWO
                    }
                });
                assertNull(currentTxn.getTxn());
            } catch (Exception e) {
                throw new RuntimeExceptionWrapper(e);
            }
        }
    }

    private static void readCheck(StoredSortedMap checkMap, Object key,
                                  Object expect) {
        if (expect == null) {
            assertNull(checkMap.get(key));
            assertTrue(checkMap.tailMap(key).isEmpty());
            assertTrue(!checkMap.tailMap(key).containsKey(key));
            assertTrue(!checkMap.keySet().contains(key));
            assertTrue(checkMap.duplicates(key).isEmpty());
            Iterator i = checkMap.keySet().iterator();
            try {
                while (i.hasNext()) {
                    assertTrue(!key.equals(i.next()));
                }
            } finally { StoredIterator.close(i); }
        } else {
            assertEquals(expect, checkMap.get(key));
            assertEquals(expect, checkMap.tailMap(key).get(key));
            assertTrue(!checkMap.tailMap(key).isEmpty());
            assertTrue(checkMap.tailMap(key).containsKey(key));
            assertTrue(checkMap.keySet().contains(key));
            assertTrue(checkMap.values().contains(expect));
            assertTrue(!checkMap.duplicates(key).isEmpty());
            assertTrue(checkMap.duplicates(key).contains(expect));
            Iterator i = checkMap.keySet().iterator();
            try {
                boolean found = false;
                while (i.hasNext()) {
                    if (expect.equals(i.next()))
                        found = true;
                }
                assertTrue(found);
            }
            finally { StoredIterator.close(i); }
        }
    }
}
