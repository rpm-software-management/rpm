/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: KeyRangeTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.test;

import com.sleepycat.bdb.bind.ByteArrayBinding;
import com.sleepycat.bdb.bind.ByteArrayFormat;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import com.sleepycat.bdb.DataCursor;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.DataView;
import java.io.File;
import java.io.IOException;
import java.math.BigInteger;
import java.util.Arrays;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class KeyRangeTest extends TestCase {

    private static boolean VERBOSE = false;

    private DbEnv env;
    private DataStore store;
    private DataView view;
    private DataCursor cursor;

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite() {

        TestSuite suite = new TestSuite();
        suite.addTest(new KeyRangeTest());
        return suite;
    }

    public KeyRangeTest() {

        super("all-tests");
    }

    public void setUp()
        throws Exception {

        System.out.println(DbTestUtil.qualifiedTestName(this));
        File dir = DbTestUtil.getNewDir();
        ByteArrayFormat dataFormat = new ByteArrayFormat();
        ByteArrayBinding dataBinding = new ByteArrayBinding(dataFormat);

        int envFlags = Db.DB_INIT_MPOOL | Db.DB_CREATE;
        env = new DbEnv(0);
        env.open(dir.getAbsolutePath(), envFlags, 0);
        Db db = new Db(env, 0);
        db.open(null, "test.db", null, Db.DB_BTREE, Db.DB_CREATE, 0);
        store = new DataStore(db, dataFormat, dataFormat, null);
        view = new DataView(store, null, dataBinding, dataBinding, null, true);
    }

    public void tearDown()
        throws Exception {

        store.close();
        env.close(0);
    }

    public void runTest() throws Exception {

        final byte FF = (byte) 0xFF;

        byte[][] keyBytes = {
            /* 0 */ {1},
            /* 1 */ {FF},
            /* 2 */ {FF, 0},
            /* 3 */ {FF, 0x7F},
            /* 4 */ {FF, FF},
            /* 5 */ {FF, FF, 0},
            /* 6 */ {FF, FF, 0x7F},
            /* 7 */ {FF, FF, FF},
        };
        byte[][] keys = new byte[keyBytes.length][];
        final int end = keyBytes.length - 1;
        for (int i = 0; i <= end; i++) {
            keys[i] = keyBytes[i];
            view.put(keys[i], keyBytes[i], 0, null);
        }
        byte[][] extremeKeyBytes = {
            /* 0 */ {0},
            /* 1 */ {FF, FF, FF, FF},
        };
        byte[][] extremeKeys = new byte[extremeKeyBytes.length][];
        for (int i = 0; i < extremeKeys.length; i++) {
            extremeKeys[i] = extremeKeyBytes[i];
        }

        // with empty range

        cursor = new DataCursor(view, false);
        expectRange(keyBytes, 0, end);
        cursor.close();

        // begin key only, inclusive

        for (int i = 0; i <= end; i++) {
            cursor = new DataCursor(view, false, keys[i], true, null, false);
            expectRange(keyBytes, i, end);
            cursor.close();
        }

        // begin key only, exclusive

        for (int i = 0; i <= end; i++) {
            cursor = new DataCursor(view, false, keys[i], false, null, false);
            expectRange(keyBytes, i + 1, end);
            cursor.close();
        }

        // end key only, inclusive

        for (int i = 0; i <= end; i++) {
            cursor = new DataCursor(view, false, null, false, keys[i], true);
            expectRange(keyBytes, 0, i);
            cursor.close();
        }

        // end key only, exclusive

        for (int i = 0; i <= end; i++) {
            cursor = new DataCursor(view, false, null, false, keys[i], false);
            expectRange(keyBytes, 0, i - 1);
            cursor.close();
        }

        // begin and end keys, inclusive and exclusive

        for (int i = 0; i <= end; i++) {
            for (int j = i; j <= end; j++) {
                // begin inclusive, end inclusive

                cursor = new DataCursor(view, false, keys[i], true, keys[j],
                                        true);
                expectRange(keyBytes, i, j);
                cursor.close();

                // begin inclusive, end exclusive

                cursor = new DataCursor(view, false, keys[i], true, keys[j],
                                        false);
                expectRange(keyBytes, i, j - 1);
                cursor.close();

                // begin exclusive, end inclusive

                cursor = new DataCursor(view, false, keys[i], false, keys[j],
                                        true);
                expectRange(keyBytes, i + 1, j);
                cursor.close();

                // begin exclusive, end exclusive

                cursor = new DataCursor(view, false, keys[i], false, keys[j],
                                        false);
                expectRange(keyBytes, i + 1, j - 1);
                cursor.close();
            }
        }

        // single key range

        for (int i = 0; i <= end; i++) {
            cursor = new DataCursor(view, false, keys[i]);
            expectRange(keyBytes, i, i);
            cursor.close();
        }

        // start with lower extreme (before any existing key)

        cursor = new DataCursor(view, false, extremeKeys[0], true, null,                                        false);
        expectRange(keyBytes, 0, end);
        cursor.close();

        // start with higher extreme (after any existing key)

        cursor = new DataCursor(view, false, null, false, extremeKeys[1],                                       true);
        expectRange(keyBytes, 0, end);
        cursor.close();
    }

    private void expectRange(byte[][] bytes, int first, int last)
        throws DbException, IOException {

        for (int pos = Db.DB_FIRST, i = first;; i++, pos = Db.DB_NEXT) {
            if (checkRange(bytes, first, last, i <= last, true, pos, i))
                break;
        }

        for (int pos = Db.DB_LAST, i = last;; i--, pos = Db.DB_PREV) {
            if (checkRange(bytes, first, last, i >= first, false, pos, i))
                break;
        }
    }

    private boolean checkRange(byte[][] bytes, int first, int last,
                               boolean inRange, boolean forward, int pos,
                               int i)
        throws DbException, IOException {

        int err = cursor.get(null, null, pos, false);

        // check that moving past ends doesn't move the cursor
        if (err == 0 && i == first) {
            int e2 = cursor.get(null, null, Db.DB_PREV, false);
            assertEquals(Db.DB_NOTFOUND, e2);
        }
        if (err == 0 && i == last) {
            int e2 = cursor.get(null, null, Db.DB_NEXT, false);
            assertEquals(Db.DB_NOTFOUND, e2);
        }

        byte[] val = (err == 0) ? ((byte[]) cursor.getCurrentValue()) : null;

        String msg = " " + (forward ? "fwd" : "rev") + " i=" + i +
                     " first=" + first + " last=" + last;

        if (inRange) {
            assertNotNull("RangeNotFound" + msg, val);

            if (!Arrays.equals(val, bytes[i])){
                printBytes(val);
                printBytes(bytes[i]);
                fail("RangeKeyNotEqual" + msg);
            }
            if (VERBOSE) System.out.println("GotRange" + msg);
            return false;
        } else {
            assertEquals("RangeExceeded" + msg, Db.DB_NOTFOUND, err);
            return true;
        }
    }

    private void printBytes(byte[] bytes) {

        System.out.println(new BigInteger(bytes).toString());
    }
}
