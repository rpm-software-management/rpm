/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleOutput.java,v 1.4 2004/09/01 14:34:20 mark Exp $
 */

package com.sleepycat.bind.tuple;

import com.sleepycat.util.FastOutputStream;
import com.sleepycat.util.UtfOps;

/**
 * An <code>OutputStream</code> with <code>DataOutput</code>-like methods for
 * writing tuple fields.  It is used by <code>TupleBinding</code>.
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
 * <ul>
 * <li>Null strings are UTF encoded as { 0xFF }, which is not allowed in a
 * standard UTF encoding.  This allows null strings, as distinct from empty or
 * zero length strings, to be represented in a tuple.  Using the default
 * comparator, null strings will be ordered last.</li>
 * <li>Zero (0x0000) character values are UTF encoded as non-zero values, and
 * therefore embedded zeros in the string are supported.  The sequence { 0xC0,
 * 0x80 } is used to encode a zero character.  This UTF encoding is the same
 * one used by native Java UTF libraries.  However, this encoding of zero does
 * impact the lexicographical ordering, and zeros will not be sorted first (the
 * natural order) or last.  For all character values other than zero, the
 * default UTF byte ordering is the same as the Unicode lexicographical
 * character ordering.</li>
 * </ul>
 *
 * <p>Floats and doubles are stored in standard Java integer-bit representation
 * (IEEE 754). Non-negative numbers are correctly ordered by numeric value.
 * However, negative numbers are not correctly ordered; therefore, if you use
 * negative floating point numbers in a key, you'll need to implement and
 * configure a custom comparator to get correct numeric ordering.</p>
 *
 * @author Mark Hayes
 */
public class TupleOutput extends FastOutputStream {

    /**
     * We represent a null string as a single FF UTF character, which cannot
     * occur in a UTF encoded string.
     */
    static final int NULL_STRING_UTF_VALUE = ((byte) 0xFF);

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
     * @return this tuple output object.
     *
     * @throws NullPointerException if the val parameter is null.
     */
    public final TupleOutput writeBytes(String val) {

        writeBytes(val.toCharArray());
        return this;
    }

    /**
     * Writes the specified characters to the buffer, converting each character
     * to a two byte unsigned value.
     * Writes values that can be read using {@link TupleInput#readChars}.
     *
     * @param val is the string containing the characters to be written.
     *
     * @return this tuple output object.
     *
     * @throws NullPointerException if the val parameter is null.
     */
    public final TupleOutput writeChars(String val) {

        writeChars(val.toCharArray());
        return this;
    }

    /**
     * Writes the specified characters to the buffer, converting each character
     * to UTF format, and adding a null terminator byte.
     * Note that zero (0x0000) character values are encoded as non-zero values
     * and a null String parameter is encoded as 0xFF.
     * Writes values that can be read using {@link TupleInput#readString()}.
     *
     * @param val is the string containing the characters to be written.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeString(String val) {

        if (val != null) {
            writeString(val.toCharArray());
        } else {
            writeFast(NULL_STRING_UTF_VALUE);
        }
        writeFast(0);
        return this;
    }

    /**
     * Writes a char (two byte) unsigned value to the buffer.
     * Writes values that can be read using {@link TupleInput#readChar}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeChar(int val) {

        writeFast((byte) (val >>> 8));
        writeFast((byte) val);
        return this;
    }

    /**
     * Writes a boolean (one byte) unsigned value to the buffer, writing one
     * if the value is true and zero if it is false.
     * Writes values that can be read using {@link TupleInput#readBoolean}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeBoolean(boolean val) {

        writeFast(val ? (byte)1 : (byte)0);
        return this;
    }

    /**
     * Writes an signed byte (one byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readByte}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeByte(int val) {

        writeUnsignedByte(val ^ 0x80);
        return this;
    }

    /**
     * Writes an signed short (two byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readShort}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeShort(int val) {

        writeUnsignedShort(val ^ 0x8000);
        return this;
    }

    /**
     * Writes an signed int (four byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readInt}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeInt(int val) {

        writeUnsignedInt(val ^ 0x80000000);
        return this;
    }

    /**
     * Writes an signed long (eight byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readLong}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeLong(long val) {

        writeUnsignedLong(val ^ 0x8000000000000000L);
        return this;
    }

    /**
     * Writes an signed float (four byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readFloat}.
     * <code>Float.floatToIntBits</code> is used to convert the signed float
     * value.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeFloat(float val) {

        writeUnsignedInt(Float.floatToIntBits(val));
        return this;
    }

    /**
     * Writes an signed double (eight byte) value to the buffer.
     * Writes values that can be read using {@link TupleInput#readDouble}.
     * <code>Double.doubleToLongBits</code> is used to convert the signed
     * double value.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeDouble(double val) {

        writeUnsignedLong(Double.doubleToLongBits(val));
        return this;
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
     * @return this tuple output object.
     *
     * @throws NullPointerException if the chars parameter is null.
     */
    public final TupleOutput writeBytes(char[] chars) {

        for (int i = 0; i < chars.length; i++) {
            writeFast((byte) chars[i]);
        }
        return this;
    }

    /**
     * Writes the specified characters to the buffer, converting each character
     * to a two byte unsigned value.
     * Writes values that can be read using {@link TupleInput#readChars}.
     *
     * @param chars is the array of characters to be written.
     *
     * @return this tuple output object.
     *
     * @throws NullPointerException if the chars parameter is null.
     */
    public final TupleOutput writeChars(char[] chars) {

        for (int i = 0; i < chars.length; i++) {
            writeFast((byte) (chars[i] >>> 8));
            writeFast((byte) chars[i]);
        }
        return this;
    }

    /**
     * Writes the specified characters to the buffer, converting each character
     * to UTF format.
     * Note that zero (0x0000) character values are encoded as non-zero values.
     * Writes values that can be read using {@link TupleInput#readString(int)}
     * or {@link TupleInput#readString(char[])}.
     *
     * @param chars is the array of characters to be written.
     *
     * @return this tuple output object.
     *
     * @throws NullPointerException if the chars parameter is null.
     */
    public final TupleOutput writeString(char[] chars) {

        if (chars.length == 0) return this;

        int utfLength = UtfOps.getByteLength(chars);

        makeSpace(utfLength);
        UtfOps.charsToBytes(chars, 0, getBufferBytes(), getBufferLength(),
                            chars.length);
        addSize(utfLength);
        return this;
    }

    /**
     * Writes an unsigned byte (one byte) value to the buffer.
     * Writes values that can be read using {@link
     * TupleInput#readUnsignedByte}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeUnsignedByte(int val) {

        writeFast(val);
        return this;
    }

    /**
     * Writes an unsigned short (two byte) value to the buffer.
     * Writes values that can be read using {@link
     * TupleInput#readUnsignedShort}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeUnsignedShort(int val) {

        writeFast((byte) (val >>> 8));
        writeFast((byte) val);
        return this;
    }

    /**
     * Writes an unsigned int (four byte) value to the buffer.
     * Writes values that can be read using {@link
     * TupleInput#readUnsignedInt}.
     *
     * @param val is the value to write to the buffer.
     *
     * @return this tuple output object.
     */
    public final TupleOutput writeUnsignedInt(long val) {

        writeFast((byte) (val >>> 24));
        writeFast((byte) (val >>> 16));
        writeFast((byte) (val >>> 8));
        writeFast((byte) val);
        return this;
    }

    /**
     * This method is private since an unsigned long cannot be treated as
     * such in Java, nor converted to a BigInteger of the same value.
     */
    private final TupleOutput writeUnsignedLong(long val) {

        writeFast((byte) (val >>> 56));
        writeFast((byte) (val >>> 48));
        writeFast((byte) (val >>> 40));
        writeFast((byte) (val >>> 32));
        writeFast((byte) (val >>> 24));
        writeFast((byte) (val >>> 16));
        writeFast((byte) (val >>> 8));
        writeFast((byte) val);
        return this;
    }
}
