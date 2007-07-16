/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: PackedIntegerTest.java,v 1.2 2006/08/24 14:46:47 bostic Exp $
 */

package com.sleepycat.util.test;

import com.sleepycat.util.PackedInteger;
import junit.framework.Test;
import junit.framework.TestCase;

public class PackedIntegerTest extends TestCase
{
    static final int V119 = 119;
    static final int BYTE_MAX = 0xFF;
    static final int SHORT_MAX = 0xFFFF;
    static final int THREE_MAX = 0xFFFFFF;

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

    public static Test suite() {

        return new PackedIntegerTest();
    }

    public PackedIntegerTest() {

        super("PackedIntegerTest");
    }

    public void runTest() {

        testRange(-V119, V119, 1);

        testRange(-BYTE_MAX - V119, -1 - V119, 2);
        testRange(1 + V119, BYTE_MAX + V119, 2);

        testRange(-SHORT_MAX - V119, -SHORT_MAX + 99, 3);
        testRange(-BYTE_MAX - V119 - 99, -BYTE_MAX - V119 - 1, 3);
        testRange(BYTE_MAX + V119 + 1, BYTE_MAX + V119 + 99, 3);
        testRange(SHORT_MAX - 99, SHORT_MAX + V119, 3);

        testRange(-THREE_MAX - V119, -THREE_MAX + 99, 4);
        testRange(-SHORT_MAX - V119 - 99, -SHORT_MAX - V119 - 1, 4);
        testRange(SHORT_MAX + V119 + 1, SHORT_MAX + V119 + 99, 4);
        testRange(THREE_MAX - 99, THREE_MAX + V119, 4);

        testRange(Integer.MIN_VALUE, Integer.MIN_VALUE + 99, 5);
        testRange(Integer.MAX_VALUE - 99, Integer.MAX_VALUE, 5);
    }

    private void testRange(long firstValue,
                           long lastValue,
                           int bytesExpected) {

        byte[] buf = new byte[1000];
        int off = 0;

        for (long longI = firstValue; longI <= lastValue; longI += 1) {
            int i = (int) longI;
            int before = off;
            off = PackedInteger.writeInt(buf, off, i);
            int bytes = off - before;
            if (bytes != bytesExpected) {
                fail("output of value=" + i + " bytes=" + bytes +
                     " bytesExpected=" + bytesExpected);
            }
            bytes = PackedInteger.getWriteIntLength(i);
            if (bytes != bytesExpected) {
                fail("count of value=" + i + " bytes=" + bytes +
                     " bytesExpected=" + bytesExpected);
            }
        }

        off = 0;

        for (long longI = firstValue; longI <= lastValue; longI += 1) {
            int i = (int) longI;
            int bytes = PackedInteger.getReadIntLength(buf, off);
            if (bytes != bytesExpected) {
                fail("count of value=" + i + " bytes=" + bytes +
                     " bytesExpected=" + bytesExpected);
            }
            int value = PackedInteger.readInt(buf, off);
            if (value != i) {
                fail("input of value=" + i + " but got=" + value);
            }
            off += bytes;
        }
    }
}
