/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleOutput.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.util.UtfOps;
import com.sleepycat.bdb.util.FastOutputStream;
import java.io.IOException;

/**
 * Used by tuple bindings to write tuple data.
 *
 * <p>This class has many methods that have the same signatures as methods in
 * the {@link java.io.DataOutput} interface.  The reason this class does not
 * implement {@link java.io.DataOutput} is because it would break the interface
 * contract for those methods because of data format differences.</p>
 *
 * <p>Signed numbers are stored in the buffer in MSB (most significant byte
 * first) order with their sign bit (high-order bit) inverted to cause negative
 * numbers to be sorted first when comparing values as unsigned byte arrays,
 * as done in a database.  Unsigned numbers, including characters, are stored
 * in MSB order with no change to their sign bit.</p>
 *
 * <p>Strings and character arrays are stored either as a fixed length array of
 * unicode characters, where the length must be known by the application, or as
 * a null-terminated UTF byte array.</p>
 *
 * <p>Floats and doubles are stored in standard integer-bit representation and
 * are therefore not ordered by numeric value.</p>
 *
 * @author Mark Hayes
 */
public class TupleOutput extends FastOutputStream {

    /**
     * Creates a tuple output object for writing a byte array of tuple data.
     */
    public TupleOutput() {

        super();
    }

    /**
     * Creates a tuple output object for writing a byte array of tuple data,
     * using a given buffer.  A new buffer will be allocated only if the number
     * of bytes needed is greater than the length of this buffer.  A reference
     * to the byte array will be kept by this object and therefore the byte
     * array should not be modified while this object is in use.
     *
     * @param buffer is the byte array to use as the buffer.
     */
    public TupleOutput(byte[] buffer) {

        super(buffer);
    }

    // --- begin DataOutput compatible methods ---

    /**
     * Writes the specified bytes to the buffer, converting each character to
     * an unsigned byte value.
     * Writes values that can be read using {@link TupleInput#readBytes}.
     * Only characters with values below 0x100 may be written using this
     * method, since the high-order 8 bits of all characters are discarded.
     *
     * @param val is the string containing the values to be written.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeBytes(String val) throws IOException {

        writeBytes(val.toCharArray());
    }

    /**
     * Writes the specified characters to the buffer, converting each character
     * to a two byte unsigned value.
     * Writes values that can be read using {@link TupleInput#readChars}.
     *
     * @param val is the string containing the characters to be written.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeChars(String val) throws IOException {

        writeChars(val.toCharArray());
    }

    /**
     * Writes the specified characters to the buffer, converting each character
     * to UTF format, and adding a null terminator byte.
     * Writes values that can be read using {@link TupleInput#readString()}.
     *
     * @param val is the string containing the characters to be written.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeString(String val) throws IOException {

        if (val != null) writeString(val.toCharArray());
        write(0);
    }

    /**
     * Writes a char (two byte) unsigned value to the buffer.
     * Writes values that can be read using {@link TupleInput#readChar}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeChar(int val) throws IOException {

        write((byte) (val >>> 8));
        write((byte) val);
    }

    /**
     * Writes a boolean (one byte) unsigned value to the buffer, writing one
     * if the value is true and zero if it is false.
     * Writes values that can be read using {@link TupleInput#readBoolean}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeBoolean(boolean val) throws IOException {

        write(val ? (byte)1 : (byte)0);
    }

    /**
     * Writes an signed byte (one byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readByte}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeByte(int val) throws IOException {

        byte b = (byte) val;
        if (b < 0)
            b &= (byte) ~0x80;
        else
            b |= (byte) 0x80;
        writeUnsignedByte(b);
    }

    /**
     * Writes an signed short (two byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readShort}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeShort(int val) throws IOException {

        short s = (short) val;
        if (s < 0)
            s &= (short) ~0x8000;
        else
            s |= (short) 0x8000;
        writeUnsignedShort(s);
    }

    /**
     * Writes an signed int (four byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readInt}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeInt(int val) throws IOException {

        if (val < 0)
            val &= ~0x80000000;
        else
            val |= 0x80000000;
        writeUnsignedInt(val);
    }

    /**
     * Writes an signed long (eight byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readLong}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeLong(long val) throws IOException {

        if (val < 0)
            val &= ~0x8000000000000000L;
        else
            val |= 0x8000000000000000L;
        writeUnsignedLong(val);
    }

    /**
     * Writes an signed float (four byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readFloat}.
     * <code>Float.floatToIntBits</code> is used to convert the signed float
     * value.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeFloat(float val) throws IOException {

        writeUnsignedInt(Float.floatToIntBits(val));
    }

    /**
     * Writes an signed double (eight byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readDouble}.
     * <code>Double.doubleToLongBits</code> is used to convert the signed
     * double value.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeDouble(double val) throws IOException {

        writeUnsignedLong(Double.doubleToLongBits(val));
    }

    // --- end DataOutput compatible methods ---

    /**
     * Writes the specified bytes to the buffer, converting each character to
     * an unsigned byte value.
     * Writes values that can be read using {@link TupleInput#readBytes}.
     * Only characters with values below 0x100 may be written using this
     * method, since the high-order 8 bits of all characters are discarded.
     *
     * @param chars is the array of values to be written.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeBytes(char[] chars) throws IOException {

        for (int i = 0; i < chars.length; i++) {
            write((byte) chars[i]);
        }
    }

    /**
     * Writes the specified characters to the buffer, converting each character
     * to a two byte unsigned value.
     * Writes values that can be read using {@link TupleInput#readChars}.
     *
     * @param chars is the array of characters to be written.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeChars(char[] chars) throws IOException {

        for (int i = 0; i < chars.length; i++) {
            write((byte) (chars[i] >>> 8));
            write((byte) chars[i]);
        }
    }

    /**
     * Writes the specified characters to the buffer, converting each character
     * to UTF format.
     * Writes values that can be read using {@link TupleInput#readString(int)}
     * or {@link TupleInput#readString(char[])}.
     *
     * @param chars is the array of characters to be written.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeString(char[] chars) throws IOException {

        if (chars.length == 0) return;

        int utfLength = UtfOps.getByteLength(chars);

        makeSpace(utfLength);
        UtfOps.charsToBytes(chars, 0, getBufferBytes(), getBufferLength(),
                            chars.length);
        addSize(utfLength);
    }

    /**
     * Writes an unsigned byte (one byte) value to the buffer.
     * Writes values that can be read using {@link
     * TupleInput#readUnsignedByte}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeUnsignedByte(int val) throws IOException {

        write(val);
    }

    /**
     * Writes an unsigned short (two byte) value to the buffer.
     * Writes values that can be read using {@link
     * TupleInput#readUnsignedShort}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeUnsignedShort(int val) throws IOException {

        write((byte) (val >>> 8));
        write((byte) val);
    }

    /**
     * Writes an unsigned int (four byte) value to the buffer.
     * Writes values that can be read using {@link
     * TupleInput#readUnsignedInt}.
     *
     * @param val is the value to write to the buffer.
     *
     * @throws IOException is never thrown but is declared for compatibility
     * with {java.io.OutputStream#write}.
     */
    public final void writeUnsignedInt(long val) throws IOException {

        write((byte) (val >>> 24));
        write((byte) (val >>> 16));
        write((byte) (val >>> 8));
        write((byte) val);
    }

    /**
     * This method is private since an unsigned long cannot be treated as
     * such in Java, nor converted to a BigInteger of the same value.
     */
    private final void writeUnsignedLong(long val) throws IOException {

        write((byte) (val >>> 56));
        write((byte) (val >>> 48));
        write((byte) (val >>> 40));
        write((byte) (val >>> 32));
        write((byte) (val >>> 24));
        write((byte) (val >>> 16));
        write((byte) (val >>> 8));
        write((byte) val);
    }
}
