/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TransactionTest.java,v 1.2 2004/09/22 18:01:06 bostic Exp $
 */

package com.sleepycat.collections.test;

import java.util.Iterator;
import java.util.List;
import java.util.SortedSet;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import com.sleepycat.collections.CurrentTransaction;
import com.sleepycat.collections.StoredCollections;
import com.sleepycat.collections.StoredContainer;
import com.sleepycat.collections.StoredIterator;
import com.sleepycat.collections.StoredList;
import com.sleepycat.collections.StoredSortedMap;
import com.sleepycat.collections.TransactionRunner;
import com.sleepycat.collections.TransactionWorker;
import com.sleepycat.compat.DbCompat;
import com.sleepycat.db.Database;
import com.sleepycat.db.DatabaseConfig;
import com.sleepycat.db.Environment;
import com.sleepycat.db.Transaction;
import com.sleepycat.db.TransactionConfig;
import com.sleepycat.util.RuntimeExceptionWrapper;

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
            junit.framework.TestResult tr =
                junit.textui.TestRunner.run(suite());
            if (tr.errorCount() > 0 ||
                tr.failureCount() > 0) {
                System.exit(1);
            } else {
                System.exit(0);
            }
        }
    }

    private static void usage() {

        System.out.println(
              "Usage: java com.sleepycat.collections.test.TransactionTest"
            + " [-h | -help]\n");
        System.exit(2);
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite(TransactionTest.class);
        return suite;
    }

    private Environment env;
    private CurrentTransaction currentTxn;
    private Database store;
    private StoredSortedMap map;
    private TestStore testStore = TestStore.BTREE_UNIQ;

    public TransactionTest(String name) {

        super(name);
    }

    public void setUp()
        throws Exception {

        DbTestUtil.printTestName(DbTestUtil.qualifiedTestName(this));
        env = TestEnv.TXN.open("TransactionTests");
        currentTxn = CurrentTransaction.getInstance(env);
        store = testStore.open(env, dbName(0));
        map = new StoredSortedMap(store, testStore.getKeyBinding(),
                                  testStore.getValueBinding(), true);
    }

    public void tearDown() {

        try {
            if (store != null) {
                store.close();
            }
            if (env != null) {
                env.close();
            }
        } catch (Exception e) {
            System.out.println("Ignored exception during tearDown: " + e);
        } finally {
            /* Ensure that GC can cleanup. */
            store = null;
            env = null;
            currentTxn = null;
            map = null;
            testStore = null;
        }
    }

    private String dbName(int i) {

        return "txn-test-" + getName() + '-' + i;
    }

    public void testGetters()
        throws Exception {

        assertNotNull(env);
        assertNotNull(currentTxn);
        assertNull(currentTxn.getTransaction());

        currentTxn.beginTransaction(null);
        assertNotNull(currentTxn.getTransaction());
        currentTxn.commitTransaction();
        assertNull(currentTxn.getTransaction());

        currentTxn.beginTransaction(null);
        assertNotNull(currentTxn.getTransaction());
        currentTxn.abortTransaction();
        assertNull(currentTxn.getTransaction());

        // dirty-read property should be inherited

        assertTrue(!map.isDirtyRead());
        assertTrue(!((StoredContainer) map.values()).isDirtyRead());
        assertTrue(!((StoredContainer) map.keySet()).isDirtyRead());
        assertTrue(!((StoredContainer) map.entrySet()).isDirtyRead());

        StoredSortedMap other =
            (StoredSortedMap) StoredCollections.dirtyReadMap(map);
        assertTrue(other.isDirtyRead());
        assertTrue(((StoredContainer) other.values()).isDirtyRead());
        assertTrue(((StoredContainer) other.keySet()).isDirtyRead());
        assertTrue(((StoredContainer) other.entrySet()).isDirtyRead());
        assertTrue(!map.isDirtyRead());
        assertTrue(!((StoredContainer) map.values()).isDirtyRead());
        assertTrue(!((StoredContainer) map.keySet()).isDirtyRead());
        assertTrue(!((StoredContainer) map.entrySet()).isDirtyRead());
    }

    public void testTransactional()
        throws Exception {

        // is transactional because DB_AUTO_COMMIT was passed to
        // Database.open()
        //
        assertTrue(map.isTransactional());
        store.close();
        store = null;

        // is not transactional
        //
        DatabaseConfig dbConfig = new DatabaseConfig();
        DbCompat.setTypeBtree(dbConfig);
        dbConfig.setAllowCreate(true);
        Database db = DbCompat.openDatabase(env, null,
                                            dbName(1), null,
                                            dbConfig);
        map = new StoredSortedMap(db, testStore.getKeyBinding(),
                                      testStore.getValueBinding(), true);
        assertTrue(!map.isTransactional());
        map.put(ONE, ONE);
        readCheck(map, ONE, ONE);
        db.close();

        // is transactional
        //
        dbConfig.setTransactional(true);
        currentTxn.beginTransaction(null);
        db = DbCompat.openDatabase(env, currentTxn.getTransaction(),
                                   dbName(2), null, dbConfig);
        currentTxn.commitTransaction();
        map = new StoredSortedMap(db, testStore.getKeyBinding(),
                                      testStore.getValueBinding(), true);
        assertTrue(map.isTransactional());
        currentTxn.beginTransaction(null);
        map.put(ONE, ONE);
        readCheck(map, ONE, ONE);
        currentTxn.commitTransaction();
        db.close();
    }

    public void testExceptions()
        throws Exception {

        try {
            currentTxn.commitTransaction();
            fail();
        } catch (IllegalStateException expected) {}

        try {
            currentTxn.abortTransaction();
            fail();
        } catch (IllegalStateException expected) {}
    }

    public void testNested()
        throws Exception {

        if (!DbCompat.NESTED_TRANSACTIONS) {
            return;
        }
        assertNull(currentTxn.getTransaction());

        Transaction txn1 = currentTxn.beginTransaction(null);
        assertNotNull(txn1);
        assertTrue(txn1 == currentTxn.getTransaction());

        assertNull(map.get(ONE));
        assertNull(map.put(ONE, ONE));
        assertEquals(ONE, map.get(ONE));

        Transaction txn2 = currentTxn.beginTransaction(null);
        assertNotNull(txn2);
        assertTrue(txn2 == currentTxn.getTransaction());
        assertTrue(txn1 != txn2);

        assertNull(map.put(TWO, TWO));
        assertEquals(TWO, map.get(TWO));

        Transaction txn3 = currentTxn.beginTransaction(null);
        assertNotNull(txn3);
        assertTrue(txn3 == currentTxn.getTransaction());
        assertTrue(txn1 != txn2);
        assertTrue(txn1 != txn3);
        assertTrue(txn2 != txn3);

        assertNull(map.put(THREE, THREE));
        assertEquals(THREE, map.get(THREE));

        Transaction txn = currentTxn.abortTransaction();
        assertTrue(txn == txn2);
        assertTrue(txn == currentTxn.getTransaction());
        assertNull(map.get(THREE));
        assertEquals(TWO, map.get(TWO));

        txn3 = currentTxn.beginTransaction(null);
        assertNotNull(txn3);
        assertTrue(txn3 == currentTxn.getTransaction());
        assertTrue(txn1 != txn2);
        assertTrue(txn1 != txn3);
        assertTrue(txn2 != txn3);

        assertNull(map.put(THREE, THREE));
        assertEquals(THREE, map.get(THREE));

        txn = currentTxn.commitTransaction();
        assertTrue(txn == txn2);
        assertTrue(txn == currentTxn.getTransaction());
        assertEquals(THREE, map.get(THREE));
        assertEquals(TWO, map.get(TWO));

        txn = currentTxn.commitTransaction();
        assertTrue(txn == txn1);
        assertTrue(txn == currentTxn.getTransaction());
        assertEquals(THREE, map.get(THREE));
        assertEquals(TWO, map.get(TWO));
        assertEquals(ONE, map.get(ONE));

        txn = currentTxn.commitTransaction();
        assertNull(txn);
        assertNull(currentTxn.getTransaction());
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
        runner.setAllowNestedTransactions(DbCompat.NESTED_TRANSACTIONS);

        assertNull(currentTxn.getTransaction());

        runner.run(new TransactionWorker() {
            public void doWork() throws Exception {
                final Transaction txn1 = currentTxn.getTransaction();
                assertNotNull(txn1);
                assertNull(map.put(ONE, ONE));
                assertEquals(ONE, map.get(ONE));

                runner.run(new TransactionWorker() {
                    public void doWork() throws Exception {
                        final Transaction txn2 = currentTxn.getTransaction();
                        assertNotNull(txn2);
                        if (DbCompat.NESTED_TRANSACTIONS) {
                            assertTrue(txn1 != txn2);
                        } else {
                            assertTrue(txn1 == txn2);
                        }
                        assertNull(map.put(TWO, TWO));
                        assertEquals(TWO, map.get(TWO));
                        assertEquals(ONE, map.get(ONE));
                        if (DbCompat.NESTED_TRANSACTIONS && explicit) {
                            currentTxn.commitTransaction();
                        }
                    }
                });

                Transaction txn3 = currentTxn.getTransaction();
                assertSame(txn1, txn3);

                assertEquals(TWO, map.get(TWO));
                assertEquals(ONE, map.get(ONE));
            }
        });

        assertNull(currentTxn.getTransaction());
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
        runner.setAllowNestedTransactions(DbCompat.NESTED_TRANSACTIONS);

        assertNull(currentTxn.getTransaction());

        runner.run(new TransactionWorker() {
            public void doWork() throws Exception {
                final Transaction txn1 = currentTxn.getTransaction();
                assertNotNull(txn1);
                assertNull(map.put(ONE, ONE));
                assertEquals(ONE, map.get(ONE));

                if (DbCompat.NESTED_TRANSACTIONS) {
                    try {
                        runner.run(new TransactionWorker() {
                            public void doWork() throws Exception {
                                final Transaction txn2 =
                                        currentTxn.getTransaction();
                                assertNotNull(txn2);
                                assertTrue(txn1 != txn2);
                                assertNull(map.put(TWO, TWO));
                                assertEquals(TWO, map.get(TWO));
                                if (explicit) {
                                    currentTxn.abortTransaction();
                                } else {
                                    throw new IllegalArgumentException(
                                                                "test-abort");
                                }
                            }
                        });
                        assertTrue(explicit);
                    } catch (IllegalArgumentException e) {
                        assertTrue(!explicit);
                        assertEquals("test-abort", e.getMessage());
                    }
                }

                Transaction txn3 = currentTxn.getTransaction();
                assertSame(txn1, txn3);

                assertEquals(ONE, map.get(ONE));
                assertNull(map.get(TWO));
            }
        });

        assertNull(currentTxn.getTransaction());
    }

    public void testDirtyReadCollection()
        throws Exception {

        StoredSortedMap dirtyMap =
            (StoredSortedMap) StoredCollections.dirtyReadSortedMap(map);

        // original map is not dirty-read
        assertTrue(map.isDirtyReadAllowed());
        assertTrue(!map.isDirtyRead());

        // all dirty-read containers are dirty-read
        checkDirtyReadProperty(dirtyMap);
        checkDirtyReadProperty(StoredCollections.dirtyReadMap(map));
        checkDirtyReadProperty(StoredCollections.dirtyReadCollection(
                                map.values()));
        checkDirtyReadProperty(StoredCollections.dirtyReadSet(
                                map.keySet()));
        checkDirtyReadProperty(StoredCollections.dirtyReadSortedSet(
                                (SortedSet) map.keySet()));

        if (DbCompat.RECNO_METHOD) {
            // create a list just so we can call dirtyReadList()
            Database listStore = TestStore.RECNO_RENUM.open(env, null);
            List list = new StoredList(listStore, TestStore.VALUE_BINDING,
                                       true);
            checkDirtyReadProperty(StoredCollections.dirtyReadList(list));
            listStore.close();
        }

        doDirtyRead(dirtyMap);
    }

    private void checkDirtyReadProperty(Object container) {

        assertTrue(((StoredContainer) container).isDirtyReadAllowed());
        assertTrue(((StoredContainer) container).isDirtyRead());
    }

    public void testDirtyReadTransaction()
        throws Exception {

        TransactionRunner runner = new TransactionRunner(env);
        TransactionConfig config = new TransactionConfig();
        config.setDirtyRead(true);
        runner.setTransactionConfig(config);
        assertNull(currentTxn.getTransaction());
        runner.run(new TransactionWorker() {
            public void doWork() throws Exception {
                assertNotNull(currentTxn.getTransaction());
                doDirtyRead(map);
            }
        });
        assertNull(currentTxn.getTransaction());
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

        private Environment env;
        private CurrentTransaction currentTxn;
        private TransactionTest parent;
        private StoredSortedMap map;

        private DirtyReadThreadOne(Environment env, TransactionTest parent) {

            this.env = env;
            this.currentTxn = CurrentTransaction.getInstance(env);
            this.parent = parent;
            this.map = parent.map;
        }

        public synchronized void run() {

            try {
                assertNull(currentTxn.getTransaction());
                assertNotNull(currentTxn.beginTransaction(null));
                assertNotNull(currentTxn.getTransaction());
                readCheck(map, ONE, null);
                synchronized (parent) { parent.notify(); }
                wait();

                // put ONE
                assertNull(map.put(ONE, ONE));
                readCheck(map, ONE, ONE);
                synchronized (parent) { parent.notify(); }
                wait();

                // abort ONE
                assertNull(currentTxn.abortTransaction());
                assertNull(currentTxn.getTransaction());
            } catch (Exception e) {
                throw new RuntimeExceptionWrapper(e);
            }
        }
    }

    private static class DirtyReadThreadTwo extends Thread {

        private Environment env;
        private CurrentTransaction currentTxn;
        private TransactionTest parent;
        private StoredSortedMap map;

        private DirtyReadThreadTwo(Environment env, TransactionTest parent) {

            this.env = env;
            this.currentTxn = CurrentTransaction.getInstance(env);
            this.parent = parent;
            this.map = parent.map;
        }

        public synchronized void run() {

            try {
                final TransactionRunner runner = new TransactionRunner(env);
                final Object thread = this;
                assertNull(currentTxn.getTransaction());

                runner.run(new TransactionWorker() {
                    public void doWork() throws Exception {
                        assertNotNull(currentTxn.getTransaction());
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
                assertNull(currentTxn.getTransaction());
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
                    if (expect.equals(i.next())) {
                        found = true;
                    }
                }
                assertTrue(found);
            }
            finally { StoredIterator.close(i); }
        }
    }
}
