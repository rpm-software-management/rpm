/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleFormatTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple.test;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.SimpleBuffer;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;
import java.util.Arrays;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class TupleFormatTest extends TestCase {

    private TupleFormat format;
    private TupleInput in;
    private TupleOutput out;
    private DataBuffer buffer;

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite(TupleFormatTest.class);
        return suite;
    }

    public TupleFormatTest(String name) {

        super(name);
    }

    public void setUp() {

        System.out.println("TupleFormatTest." + getName());
        format = new TupleFormat();
        buffer = new SimpleBuffer();
        out = format.newOutput();
    }

    private void copyOutputToInput() {

        format.outputToData(out, buffer);
        assertEquals(out.size(), buffer.getDataLength());
        in = format.dataToInput(buffer);
        assertEquals(in.available(), buffer.getDataLength());
        assertEquals(in.getBufferLength(), buffer.getDataLength());
    }

    private void stringTest(String val)
        throws IOException {

        out.reset();
        out.writeString(val);
        assertEquals(val.length() + 1, out.size()); // assume 1-byte chars
        copyOutputToInput();
        assertEquals(val, in.readString());
        assertEquals(0, in.available());
    }

    public void testString()
        throws IOException {

        stringTest("");
        stringTest("a");
        stringTest("abc");

        out.reset();
        out.writeString("abc");
        out.writeString("defg");
        assertEquals(9, out.size());
        copyOutputToInput();
        assertEquals("abc", in.readString());
        assertEquals("defg", in.readString());
        assertEquals(0, in.available());

        out.reset();
        out.writeString("abc");
        out.writeString("defg");
        out.writeString("hijkl");
        assertEquals(15, out.size());
        copyOutputToInput();
        assertEquals("abc", in.readString());
        assertEquals("defg", in.readString());
        assertEquals("hijkl", in.readString());
        assertEquals(0, in.available());
    }

    private void fixedStringTest(char[] val)
        throws IOException {

        out.reset();
        out.writeString(val);
        assertEquals(val.length, out.size()); // assume 1 byte chars
        copyOutputToInput();
        char[] val2 = new char[val.length];
        in.readString(val2);
        assertTrue(Arrays.equals(val, val2));
        assertEquals(0, in.available());
        in.reset();
        String val3 = in.readString(val.length);
        assertTrue(Arrays.equals(val, val3.toCharArray()));
        assertEquals(0, in.available());
    }

    public void testFixedString()
        throws IOException {

        fixedStringTest(new char[0]);
        fixedStringTest(new char[] {'a'});
        fixedStringTest(new char[] {'a', 'b', 'c'});

        out.reset();
        out.writeString(new char[] {'a', 'b', 'c'});
        out.writeString(new char[] {'d', 'e', 'f', 'g'});
        assertEquals(7, out.size());
        copyOutputToInput();
        assertEquals("abc", in.readString(3));
        assertEquals("defg", in.readString(4));
        assertEquals(0, in.available());

        out.reset();
        out.writeString(new char[] {'a', 'b', 'c'});
        out.writeString(new char[] {'d', 'e', 'f', 'g'});
        out.writeString(new char[] {'h', 'i', 'j', 'k', 'l'});
        assertEquals(12, out.size());
        copyOutputToInput();
        assertEquals("abc", in.readString(3));
        assertEquals("defg", in.readString(4));
        assertEquals("hijkl", in.readString(5));
        assertEquals(0, in.available());
    }

    private void charsTest(char[] val)
        throws IOException {

        for (int mode = 0; mode < 2; mode += 1) {
            out.reset();
            switch (mode) {
                case 0: out.writeChars(val); break;
                case 1: out.writeChars(new String(val)); break;
                default: throw new IllegalStateException();
            }
            assertEquals(val.length * 2, out.size());
            copyOutputToInput();
            char[] val2 = new char[val.length];
            in.readChars(val2);
            assertTrue(Arrays.equals(val, val2));
            assertEquals(0, in.available());
            in.reset();
            String val3 = in.readChars(val.length);
            assertTrue(Arrays.equals(val, val3.toCharArray()));
            assertEquals(0, in.available());
        }
    }

    public void testChars()
        throws IOException {

        charsTest(new char[0]);
        charsTest(new char[] {'a'});
        charsTest(new char[] {'a', 'b', 'c'});

        out.reset();
        out.writeChars("abc");
        out.writeChars("defg");
        assertEquals(7 * 2, out.size());
        copyOutputToInput();
        assertEquals("abc", in.readChars(3));
        assertEquals("defg", in.readChars(4));
        assertEquals(0, in.available());

        out.reset();
        out.writeChars("abc");
        out.writeChars("defg");
        out.writeChars("hijkl");
        assertEquals(12 * 2, out.size());
        copyOutputToInput();
        assertEquals("abc", in.readChars(3));
        assertEquals("defg", in.readChars(4));
        assertEquals("hijkl", in.readChars(5));
        assertEquals(0, in.available());
    }

    private void bytesTest(char[] val)
        throws IOException {

        char[] valBytes = new char[val.length];
        for (int i = 0; i < val.length; i += 1)
            valBytes[i] = (char) (val[i] & 0xFF);

        for (int mode = 0; mode < 2; mode += 1) {
            out.reset();
            switch (mode) {
                case 0: out.writeBytes(val); break;
                case 1: out.writeBytes(new String(val)); break;
                default: throw new IllegalStateException();
            }
            assertEquals(val.length, out.size());
            copyOutputToInput();
            char[] val2 = new char[val.length];
            in.readBytes(val2);
            assertTrue(Arrays.equals(valBytes, val2));
            assertEquals(0, in.available());
            in.reset();
            String val3 = in.readBytes(val.length);
            assertTrue(Arrays.equals(valBytes, val3.toCharArray()));
            assertEquals(0, in.available());
        }
    }

    public void testBytes()
        throws IOException {

        bytesTest(new char[0]);
        bytesTest(new char[] {'a'});
        bytesTest(new char[] {'a', 'b', 'c'});
        bytesTest(new char[] {0x7F00, 0x7FFF, 0xFF00, 0xFFFF});

        out.reset();
        out.writeBytes("abc");
        out.writeBytes("defg");
        assertEquals(7, out.size());
        copyOutputToInput();
        assertEquals("abc", in.readBytes(3));
        assertEquals("defg", in.readBytes(4));
        assertEquals(0, in.available());

        out.reset();
        out.writeBytes("abc");
        out.writeBytes("defg");
        out.writeBytes("hijkl");
        assertEquals(12, out.size());
        copyOutputToInput();
        assertEquals("abc", in.readBytes(3));
        assertEquals("defg", in.readBytes(4));
        assertEquals("hijkl", in.readBytes(5));
        assertEquals(0, in.available());
    }

    private void booleanTest(boolean val)
        throws IOException {

        out.reset();
        out.writeBoolean(val);
        assertEquals(1, out.size());
        copyOutputToInput();
        assertEquals(val, in.readBoolean());
        assertEquals(0, in.available());
    }

    public void testBoolean()
        throws IOException {

        booleanTest(true);
        booleanTest(false);

        out.reset();
        out.writeBoolean(true);
        out.writeBoolean(false);
        assertEquals(2, out.size());
        copyOutputToInput();
        assertEquals(true, in.readBoolean());
        assertEquals(false, in.readBoolean());
        assertEquals(0, in.available());

        out.reset();
        out.writeBoolean(true);
        out.writeBoolean(false);
        out.writeBoolean(true);
        assertEquals(3, out.size());
        copyOutputToInput();
        assertEquals(true, in.readBoolean());
        assertEquals(false, in.readBoolean());
        assertEquals(true, in.readBoolean());
        assertEquals(0, in.available());
    }

    private void unsignedByteTest(int val)
        throws IOException {

        unsignedByteTest(val, val);
    }

    private void unsignedByteTest(int val, int expected)
        throws IOException {

        out.reset();
        out.writeUnsignedByte(val);
        assertEquals(1, out.size());
        copyOutputToInput();
        assertEquals(expected, in.readUnsignedByte());
    }

    public void testUnsignedByte()
        throws IOException {

        unsignedByteTest(0);
        unsignedByteTest(1);
        unsignedByteTest(254);
        unsignedByteTest(255);
        unsignedByteTest(256, 0);
        unsignedByteTest(-1, 255);
        unsignedByteTest(-2, 254);
        unsignedByteTest(-255, 1);

        out.reset();
        out.writeUnsignedByte(0);
        out.writeUnsignedByte(1);
        out.writeUnsignedByte(255);
        assertEquals(3, out.size());
        copyOutputToInput();
        assertEquals(0, in.readUnsignedByte());
        assertEquals(1, in.readUnsignedByte());
        assertEquals(255, in.readUnsignedByte());
        assertEquals(0, in.available());
    }

    private void unsignedShortTest(int val)
        throws IOException {

        unsignedShortTest(val, val);
    }

    private void unsignedShortTest(int val, int expected)
        throws IOException {

        out.reset();
        out.writeUnsignedShort(val);
        assertEquals(2, out.size());
        copyOutputToInput();
        assertEquals(expected, in.readUnsignedShort());
    }

    public void testUnsignedShort()
        throws IOException {

        unsignedShortTest(0);
        unsignedShortTest(1);
        unsignedShortTest(255);
        unsignedShortTest(256);
        unsignedShortTest(257);
        unsignedShortTest(Short.MAX_VALUE - 1);
        unsignedShortTest(Short.MAX_VALUE);
        unsignedShortTest(Short.MAX_VALUE + 1);
        unsignedShortTest(0xFFFF - 1);
        unsignedShortTest(0xFFFF);
        unsignedShortTest(0xFFFF + 1, 0);
        unsignedShortTest(0x7FFF0000, 0);
        unsignedShortTest(0xFFFF0000, 0);
        unsignedShortTest(-1, 0xFFFF);
        unsignedShortTest(-2, 0xFFFF - 1);
        unsignedShortTest(-0xFFFF, 1);

        out.reset();
        out.writeUnsignedShort(0);
        out.writeUnsignedShort(1);
        out.writeUnsignedShort(0xFFFF);
        assertEquals(6, out.size());
        copyOutputToInput();
        assertEquals(0, in.readUnsignedShort());
        assertEquals(1, in.readUnsignedShort());
        assertEquals(0xFFFF, in.readUnsignedShort());
        assertEquals(0, in.available());
    }

    private void unsignedIntTest(long val)
        throws IOException {

        unsignedIntTest(val, val);
    }

    private void unsignedIntTest(long val, long expected)
        throws IOException {

        out.reset();
        out.writeUnsignedInt(val);
        assertEquals(4, out.size());
        copyOutputToInput();
        assertEquals(expected, in.readUnsignedInt());
    }

    public void testUnsignedInt()
        throws IOException {

        unsignedIntTest(0L);
        unsignedIntTest(1L);
        unsignedIntTest(255L);
        unsignedIntTest(256L);
        unsignedIntTest(257L);
        unsignedIntTest(Short.MAX_VALUE - 1L);
        unsignedIntTest(Short.MAX_VALUE);
        unsignedIntTest(Short.MAX_VALUE + 1L);
        unsignedIntTest(Integer.MAX_VALUE - 1L);
        unsignedIntTest(Integer.MAX_VALUE);
        unsignedIntTest(Integer.MAX_VALUE + 1L);
        unsignedIntTest(0xFFFFFFFFL - 1L);
        unsignedIntTest(0xFFFFFFFFL);
        unsignedIntTest(0xFFFFFFFFL + 1L, 0L);
        unsignedIntTest(0x7FFFFFFF00000000L, 0L);
        unsignedIntTest(0xFFFFFFFF00000000L, 0L);
        unsignedIntTest(-1, 0xFFFFFFFFL);
        unsignedIntTest(-2, 0xFFFFFFFFL - 1L);
        unsignedIntTest(-0xFFFFFFFFL, 1L);

        out.reset();
        out.writeUnsignedInt(0L);
        out.writeUnsignedInt(1L);
        out.writeUnsignedInt(0xFFFFFFFFL);
        assertEquals(12, out.size());
        copyOutputToInput();
        assertEquals(0L, in.readUnsignedInt());
        assertEquals(1L, in.readUnsignedInt());
        assertEquals(0xFFFFFFFFL, in.readUnsignedInt());
        assertEquals(0L, in.available());
    }

    private void byteTest(int val)
        throws IOException {

        out.reset();
        out.writeByte(val);
        assertEquals(1, out.size());
        copyOutputToInput();
        assertEquals((byte) val, in.readByte());
    }

    public void testByte()
        throws IOException {

        byteTest(0);
        byteTest(1);
        byteTest(-1);
        byteTest(Byte.MAX_VALUE - 1);
        byteTest(Byte.MAX_VALUE);
        byteTest(Byte.MAX_VALUE + 1);
        byteTest(Byte.MIN_VALUE + 1);
        byteTest(Byte.MIN_VALUE);
        byteTest(Byte.MIN_VALUE - 1);
        byteTest(0x7F);
        byteTest(0xFF);
        byteTest(0x7FFF);
        byteTest(0xFFFF);
        byteTest(0x7FFFFFFF);
        byteTest(0xFFFFFFFF);

        out.reset();
        out.writeByte(0);
        out.writeByte(1);
        out.writeByte(-1);
        assertEquals(3, out.size());
        copyOutputToInput();
        assertEquals(0, in.readByte());
        assertEquals(1, in.readByte());
        assertEquals(-1, in.readByte());
        assertEquals(0, in.available());
    }

    private void shortTest(int val)
        throws IOException {

        out.reset();
        out.writeShort(val);
        assertEquals(2, out.size());
        copyOutputToInput();
        assertEquals((short) val, in.readShort());
    }

    public void testShort()
        throws IOException {

        shortTest(0);
        shortTest(1);
        shortTest(-1);
        shortTest(Short.MAX_VALUE - 1);
        shortTest(Short.MAX_VALUE);
        shortTest(Short.MAX_VALUE + 1);
        shortTest(Short.MIN_VALUE + 1);
        shortTest(Short.MIN_VALUE);
        shortTest(Short.MIN_VALUE - 1);
        shortTest(0x7F);
        shortTest(0xFF);
        shortTest(0x7FFF);
        shortTest(0xFFFF);
        shortTest(0x7FFFFFFF);
        shortTest(0xFFFFFFFF);

        out.reset();
        out.writeShort(0);
        out.writeShort(1);
        out.writeShort(-1);
        assertEquals(3 * 2, out.size());
        copyOutputToInput();
        assertEquals(0, in.readShort());
        assertEquals(1, in.readShort());
        assertEquals(-1, in.readShort());
        assertEquals(0, in.available());
    }

    private void intTest(int val)
        throws IOException {

        out.reset();
        out.writeInt(val);
        assertEquals(4, out.size());
        copyOutputToInput();
        assertEquals((int) val, in.readInt());
    }

    public void testInt()
        throws IOException {

        intTest(0);
        intTest(1);
        intTest(-1);
        intTest(Integer.MAX_VALUE - 1);
        intTest(Integer.MAX_VALUE);
        intTest(Integer.MAX_VALUE + 1);
        intTest(Integer.MIN_VALUE + 1);
        intTest(Integer.MIN_VALUE);
        intTest(Integer.MIN_VALUE - 1);
        intTest(0x7F);
        intTest(0xFF);
        intTest(0x7FFF);
        intTest(0xFFFF);
        intTest(0x7FFFFFFF);
        intTest(0xFFFFFFFF);

        out.reset();
        out.writeInt(0);
        out.writeInt(1);
        out.writeInt(-1);
        assertEquals(3 * 4, out.size());
        copyOutputToInput();
        assertEquals(0, in.readInt());
        assertEquals(1, in.readInt());
        assertEquals(-1, in.readInt());
        assertEquals(0, in.available());
    }

    private void longTest(long val)
        throws IOException {

        out.reset();
        out.writeLong(val);
        assertEquals(8, out.size());
        copyOutputToInput();
        assertEquals((long) val, in.readLong());
    }

    public void testLong()
        throws IOException {

        longTest(0);
        longTest(1);
        longTest(-1);
        longTest(Long.MAX_VALUE - 1);
        longTest(Long.MAX_VALUE);
        longTest(Long.MAX_VALUE + 1);
        longTest(Long.MIN_VALUE + 1);
        longTest(Long.MIN_VALUE);
        longTest(Long.MIN_VALUE - 1);
        longTest(0x7F);
        longTest(0xFF);
        longTest(0x7FFF);
        longTest(0xFFFF);
        longTest(0x7FFFFFFF);
        longTest(0xFFFFFFFF);
        longTest(0x7FFFFFFFFFFFFFFFL);
        longTest(0xFFFFFFFFFFFFFFFFL);

        out.reset();
        out.writeLong(0);
        out.writeLong(1);
        out.writeLong(-1);
        assertEquals(3 * 8, out.size());
        copyOutputToInput();
        assertEquals(0, in.readLong());
        assertEquals(1, in.readLong());
        assertEquals(-1, in.readLong());
        assertEquals(0, in.available());
    }

    private void floatTest(double val)
        throws IOException {

        out.reset();
        out.writeFloat((float) val);
        assertEquals(4, out.size());
        copyOutputToInput();
        if (Double.isNaN(val))
            assertTrue(Float.isNaN(in.readFloat()));
        else
            assertEquals((float) val, in.readFloat(), 0);
    }

    public void testFloat()
        throws IOException {

        floatTest(0);
        floatTest(1);
        floatTest(-1);
        floatTest(1.0);
        floatTest(0.1);
        floatTest(-1.0);
        floatTest(-0.1);
        floatTest(Float.NaN);
        floatTest(Float.NEGATIVE_INFINITY);
        floatTest(Float.POSITIVE_INFINITY);
        floatTest(Short.MAX_VALUE);
        floatTest(Short.MIN_VALUE);
        floatTest(Integer.MAX_VALUE);
        floatTest(Integer.MIN_VALUE);
        floatTest(Long.MAX_VALUE);
        floatTest(Long.MIN_VALUE);
        floatTest(Float.MAX_VALUE);
        floatTest(Float.MAX_VALUE + 1);
        floatTest(Float.MIN_VALUE + 1);
        floatTest(Float.MIN_VALUE);
        floatTest(Float.MIN_VALUE - 1);
        floatTest(0x7F);
        floatTest(0xFF);
        floatTest(0x7FFF);
        floatTest(0xFFFF);
        floatTest(0x7FFFFFFF);
        floatTest(0xFFFFFFFF);
        floatTest(0x7FFFFFFFFFFFFFFFL);
        floatTest(0xFFFFFFFFFFFFFFFFL);

        out.reset();
        out.writeFloat(0);
        out.writeFloat(1);
        out.writeFloat(-1);
        assertEquals(3 * 4, out.size());
        copyOutputToInput();
        assertEquals(0, in.readFloat(), 0);
        assertEquals(1, in.readFloat(), 0);
        assertEquals(-1, in.readFloat(), 0);
        assertEquals(0, in.available(), 0);
    }

    private void doubleTest(double val)
        throws IOException {

        out.reset();
        out.writeDouble((double) val);
        assertEquals(8, out.size());
        copyOutputToInput();
        if (Double.isNaN(val))
            assertTrue(Double.isNaN(in.readDouble()));
        else
            assertEquals((double) val, in.readDouble(), 0);
    }

    public void testDouble()
        throws IOException {

        doubleTest(0);
        doubleTest(1);
        doubleTest(-1);
        doubleTest(1.0);
        doubleTest(0.1);
        doubleTest(-1.0);
        doubleTest(-0.1);
        doubleTest(Double.NaN);
        doubleTest(Double.NEGATIVE_INFINITY);
        doubleTest(Double.POSITIVE_INFINITY);
        doubleTest(Short.MAX_VALUE);
        doubleTest(Short.MIN_VALUE);
        doubleTest(Integer.MAX_VALUE);
        doubleTest(Integer.MIN_VALUE);
        doubleTest(Long.MAX_VALUE);
        doubleTest(Long.MIN_VALUE);
        doubleTest(Float.MAX_VALUE);
        doubleTest(Float.MIN_VALUE);
        doubleTest(Double.MAX_VALUE - 1);
        doubleTest(Double.MAX_VALUE);
        doubleTest(Double.MAX_VALUE + 1);
        doubleTest(Double.MIN_VALUE + 1);
        doubleTest(Double.MIN_VALUE);
        doubleTest(Double.MIN_VALUE - 1);
        doubleTest(0x7F);
        doubleTest(0xFF);
        doubleTest(0x7FFF);
        doubleTest(0xFFFF);
        doubleTest(0x7FFFFFFF);
        doubleTest(0xFFFFFFFF);
        doubleTest(0x7FFFFFFFFFFFFFFFL);
        doubleTest(0xFFFFFFFFFFFFFFFFL);

        out.reset();
        out.writeDouble(0);
        out.writeDouble(1);
        out.writeDouble(-1);
        assertEquals(3 * 8, out.size());
        copyOutputToInput();
        assertEquals(0, in.readDouble(), 0);
        assertEquals(1, in.readDouble(), 0);
        assertEquals(-1, in.readDouble(), 0);
        assertEquals(0, in.available(), 0);
    }
}

