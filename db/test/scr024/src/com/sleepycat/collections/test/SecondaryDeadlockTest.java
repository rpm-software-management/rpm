/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SecondaryDeadlockTest.java,v 1.3 2004/08/02 18:53:08 mjc Exp $
 */

package com.sleepycat.collections.test;

import com.sleepycat.db.Database;
import com.sleepycat.db.DeadlockException;
import com.sleepycat.db.Environment;
import com.sleepycat.collections.StoredSortedMap;
import com.sleepycat.collections.TransactionRunner;
import com.sleepycat.collections.TransactionWorker;
import com.sleepycat.util.ExceptionUnwrapper;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * Tests whether secondary access can cause a self-deadlock when reading via a
 * secondary because the collections API secondary implementation in DB 4.2
 * opens two cursors.   Part of the problem in [#10516] was because the
 * secondary get() was not done in a txn.  This problem should not occur in DB
 * 4.3 and JE -- an ordinary deadlock occurs instead and is detected.
 *
 * @author Mark Hayes
 */
public class SecondaryDeadlockTest extends TestCase {

    private static final Long N_ONE = new Long(1);
    private static final Long N_101 = new Long(101);
    private static final int N_ITERS = 200;
    private static final int MAX_RETRIES = 30;

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

        TestSuite suite = new TestSuite(SecondaryDeadlockTest.class);
        return suite;
    }

    private Environment env;
    private Database store;
    private Database index;
    private StoredSortedMap storeMap;
    private StoredSortedMap indexMap;

    public SecondaryDeadlockTest(String name) {

        super(name);
    }

    public void setUp()
        throws Exception {

        env = TestEnv.TXN.open("SecondaryDeadlockTest");
        store = TestStore.BTREE_UNIQ.open(env, "store.db");
        index = TestStore.BTREE_UNIQ.openIndex(store, "index.db");
        storeMap = new StoredSortedMap(store,
                                       TestStore.BTREE_UNIQ.getKeyBinding(),
                                       TestStore.BTREE_UNIQ.getValueBinding(),
                                       true);
        indexMap = new StoredSortedMap(index,
                                       TestStore.BTREE_UNIQ.getKeyBinding(),
                                       TestStore.BTREE_UNIQ.getValueBinding(),
                                       true);
    }

    public void tearDown() {

        if (index != null) {
            try {
                index.close();
            } catch (Exception e) {
                System.out.println("Ignored exception during tearDown: " + e);
            }
        }
        if (store != null) {
            try {
                store.close();
            } catch (Exception e) {
                System.out.println("Ignored exception during tearDown: " + e);
            }
        }
        if (env != null) {
            try {
                env.close();
            } catch (Exception e) {
                System.out.println("Ignored exception during tearDown: " + e);
            }
        }
        /* Allow GC of DB objects in the test case. */
        env = null;
        store = null;
        index = null;
        storeMap = null;
        indexMap = null;
    }

    public void testSecondaryDeadlock()
        throws Exception {

        final TransactionRunner runner = new TransactionRunner(env);
        runner.setMaxRetries(MAX_RETRIES);

        /*
         * A thread to do put() and delete() via the primary, which will lock
         * the primary first then the secondary.  Uses transactions.
         */
        final Thread thread1 = new Thread(new Runnable() {
            public void run() {
                try {
                    for (int i = 0; i < N_ITERS; i +=1 ) {
                        runner.run(new TransactionWorker() {
                            public void doWork() throws Exception {
                                assertEquals(null, storeMap.put(N_ONE, N_101));
                            }
                        });
                        runner.run(new TransactionWorker() {
                            public void doWork() throws Exception {
                                assertEquals(N_101, storeMap.remove(N_ONE));
                            }
                        });
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    fail(e.toString());
                }
            }
        });

        /*
         * A thread to get() via the secondary, which will lock the secondary
         * first then the primary.  Does not use a transaction.
         */
        final Thread thread2 = new Thread(new Runnable() {
            public void run() {
                try {
                    for (int i = 0; i < N_ITERS; i +=1 ) {
                        for (int j = 0; j < MAX_RETRIES; j += 1) {
                            try {
                                Object value = indexMap.get(N_ONE);
                                assertTrue(value == null ||
                                           N_101.equals(value));
                                break;
                            } catch (Exception e) {
                                e = ExceptionUnwrapper.unwrap(e);
                                if (e instanceof DeadlockException) {
                                    continue; /* Retry on deadlock. */
                                } else {
                                    e.printStackTrace();
                                    fail();
                                }
                            }
                        }
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    fail(e.toString());
                }
            }
        });

        thread1.start();
        thread2.start();
        thread1.join();
        thread2.join();

        index.close();
        index = null;
        store.close();
        store = null;
        env.close();
        env = null;
    }
}
