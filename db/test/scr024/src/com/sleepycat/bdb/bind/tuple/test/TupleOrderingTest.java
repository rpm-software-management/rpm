/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleOrderingTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple.test;

import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class TupleOrderingTest extends TestCase {

    private TupleFormat format;
    private TupleOutput out;
    private byte[] prevBuf;

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite(TupleOrderingTest.class);
        return suite;
    }

    public TupleOrderingTest(String name) {

        super(name);
    }

    public void setUp() {

        System.out.println("TupleOrderingTest." + getName());
        format = new TupleFormat();
        out = format.newOutput();
        prevBuf = null;
    }

    /*
    Each tuple written must be strictly less than (by comparison of bytes) the
    tuple written just before it.  The check() method compares bytes just
    written to those written before the previous call to check().
    */

    private void check() {

        check(-1);
    }

    private void check(int dataIndex) {

        byte[] buf = new byte[out.size()];
        System.arraycopy(out.getBufferBytes(), out.getBufferOffset(),
                         buf, 0, buf.length);
        if (prevBuf != null) {
            int errOffset = -1;
            int len = Math.min(prevBuf.length,  buf.length);
            boolean areEqual = true;
            for (int i = 0; i < len; i += 1) {
                int val1 = prevBuf[i] & 0xFF;
                int val2 = buf[i] & 0xFF;
                if (val1 < val2) {
                    areEqual = false;
                    break;
                } else if (val1 > val2) {
                    errOffset = i;
                    break;
                }
            }
            if (areEqual) {
                if (prevBuf.length < buf.length) {
                    areEqual = false;
                } else if (prevBuf.length > buf.length) {
                    areEqual = false;
                    errOffset = buf.length + 1;
                }
            }
            if (errOffset != -1 || areEqual) {
                StringBuffer msg = new StringBuffer();
                if (errOffset != -1)
                    msg.append("Left >= right at byte offset " + errOffset);
                else if (areEqual)
                    msg.append("Bytes are equal");
                else
                    throw new IllegalStateException();
                msg.append("\nLeft hex bytes: ");
                for (int i = 0; i < prevBuf.length; i += 1) {
                    msg.append(' ');
                    int val = prevBuf[i] & 0xFF;
                    if ((val & 0xF0) == 0) msg.append('0');
                    msg.append(Integer.toHexString(val));
                }
                msg.append("\nRight hex bytes:");
                for (int i = 0; i < buf.length; i += 1) {
                    msg.append(' ');
                    int val = buf[i] & 0xFF;
                    if ((val & 0xF0) == 0) msg.append('0');
                    msg.append(Integer.toHexString(val));
                }
                if (dataIndex >= 0) {
                    msg.append("\nData index: " + dataIndex);
                }
                fail(msg.toString());
            }
        }
        prevBuf = buf;
        out.reset();
    }

    private void reset() {

        prevBuf = null;
        out.reset();
    }

    public void testString()
        throws IOException {

        final String[] DATA = {
            "", "a", "ab", "b", "bb", "bba",
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeString(DATA[i]);
            check(i);
        }
        reset();
        out.writeString("a");
        check();
        out.writeString("a");
        out.writeString("");
        check();
        out.writeString("a");
        out.writeString("");
        out.writeString("a");
        check();
        out.writeString("a");
        out.writeString("b");
        check();
        out.writeString("aa");
        check();
        out.writeString("b");
        check();
    }

    public void testFixedString()
        throws IOException {

        final char[][] DATA = {
            {}, {'a'}, {'a', 'b'}, {'b'}, {'b', 'b'}, {0x7F}, {0xFF},
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeString(DATA[i]);
            check(i);
        }
    }

    public void testChars()
        throws IOException {

        final char[][] DATA = {
            {}, {0}, {'a'}, {'a', 0}, {'a', 'b'}, {'b'}, {'b', 'b'},
            {0x7F}, {0x7F, 0}, {0xFF}, {0xFF, 0},
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeChars(DATA[i]);
            check(i);
        }
    }

    public void testBytes()
        throws IOException {

        final char[][] DATA = {
            {}, {0}, {'a'}, {'a', 0}, {'a', 'b'}, {'b'}, {'b', 'b'},
            {0x7F}, {0xFF},
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeBytes(DATA[i]);
            check(i);
        }
    }

    public void testBoolean()
        throws IOException {

        final boolean[] DATA = {
            false, true
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeBoolean(DATA[i]);
            check(i);
        }
    }

    public void testUnsignedByte()
        throws IOException {

        final int[] DATA = {
            0, 1, 0x7F, 0xFF
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeUnsignedByte(DATA[i]);
            check(i);
        }
    }

    public void testUnsignedShort()
        throws IOException {

        final int[] DATA = {
            0, 1, 0xFE, 0xFF, 0x800, 0x7FFF, 0xFFFF
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeUnsignedShort(DATA[i]);
            check(i);
        }
    }

    public void testUnsignedInt()
        throws IOException {

        final long[] DATA = {
            0, 1, 0xFE, 0xFF, 0x800, 0x7FFF, 0xFFFF, 0x80000,
            0x7FFFFFFF, 0x80000000, 0xFFFFFFFF
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeUnsignedInt(DATA[i]);
            check(i);
        }
    }

    public void testByte()
        throws IOException {

        final byte[] DATA = {
            Byte.MIN_VALUE, Byte.MIN_VALUE + 1,
            -1, 0, 1,
            Byte.MAX_VALUE - 1, Byte.MAX_VALUE,
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeByte(DATA[i]);
            check(i);
        }
    }

    public void testShort()
        throws IOException {

        final short[] DATA = {
            Short.MIN_VALUE, Short.MIN_VALUE + 1,
            Byte.MIN_VALUE, Byte.MIN_VALUE + 1,
            -1, 0, 1,
            Byte.MAX_VALUE - 1, Byte.MAX_VALUE,
            Short.MAX_VALUE - 1, Short.MAX_VALUE,
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeShort(DATA[i]);
            check(i);
        }
    }

    public void testInt()
        throws IOException {

        final int[] DATA = {
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1,
            Short.MIN_VALUE, Short.MIN_VALUE + 1,
            Byte.MIN_VALUE, Byte.MIN_VALUE + 1,
            -1, 0, 1,
            Byte.MAX_VALUE - 1, Byte.MAX_VALUE,
            Short.MAX_VALUE - 1, Short.MAX_VALUE,
            Integer.MAX_VALUE - 1, Integer.MAX_VALUE,
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeInt(DATA[i]);
            check(i);
        }
    }

    public void testLong()
        throws IOException {

        final long[] DATA = {
            Long.MIN_VALUE, Long.MIN_VALUE + 1,
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1,
            Short.MIN_VALUE, Short.MIN_VALUE + 1,
            Byte.MIN_VALUE, Byte.MIN_VALUE + 1,
            -1, 0, 1,
            Byte.MAX_VALUE - 1, Byte.MAX_VALUE,
            Short.MAX_VALUE - 1, Short.MAX_VALUE,
            Integer.MAX_VALUE - 1, Integer.MAX_VALUE,
            Long.MAX_VALUE - 1, Long.MAX_VALUE,
        };
        for (int i = 0; i < DATA.length; i += 1) {
            out.writeLong(DATA[i]);
            check(i);
        }
    }

    // floats and doubles are not ordered deterministically
}

