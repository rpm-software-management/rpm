/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleInput.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.util.UtfOps;
import com.sleepycat.bdb.util.FastInputStream;
import java.io.EOFException;
import java.io.IOException;

/**
 * Used by tuple bindings to read tuple data.
 *
 * <p>This class has many methods that have the same signatures as methods in
 * the {@link java.io.DataInput} interface.  The reason this class does not
 * implement {@link java.io.DataInput} is because it would break the interface
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
public class TupleInput extends FastInputStream {

    /**
     * Creates a tuple input object for reading a byte array of tuple data.  A
     * reference to the byte array will be kept by this object (it will not be
     * copied) and therefore the byte array should not be modified while this
     * object is in use.
     *
     * @param buffer is the byte array to be read and should contain data in
     * tuple format.
     */
    public TupleInput(byte[] buffer) {

        super(buffer);
    }

    /**
     * Creates a tuple input object for reading a byte array of tuple data at
     * a given offset for a given length.  A reference to the byte array will
     * be kept by this object (it will not be copied) and therefore the byte
     * array should not be modified while this object is in use.
     *
     * @param buffer is the byte array to be read and should contain data in
     * tuple format.
     *
     * @param offset is the byte offset at which to begin reading.
     *
     * @param length is the number of bytes to be read.
     */
    public TupleInput(byte[] buffer, int offset, int length) {

        super(buffer, offset, length);
    }

    /**
     * Creates a tuple input object from the data contained in a tuple output
     * object.  A reference to the tuple output's byte array will be kept by
     * this object (it will not be copied) and therefore the tuple output
     * object should not be modified while this object is in use.
     *
     * @param output is the tuple output object containing the data to be read.
     */
    public TupleInput(TupleOutput output) {

        super(output.getBufferBytes(), output.getBufferOffset(),
              output.getBufferLength());
    }

    // --- begin DataInput compatible methods ---

    /**
     * Reads a null-terminated UTF string from the data buffer and converts
     * the data from UTF to Unicode.
     * Reads values that were written using {@link
     * TupleOutput#writeString(String)}.
     *
     * @return the converted string.
     *
     * @throws IOException if no null terminating byte is found in the buffer
     * or malformed UTF data is encountered.
     */
    public final String readString() throws IOException {

        byte[] buf = getBufferBytes();
        int off = getBufferOffset();
        int byteLen = UtfOps.getZeroTerminatedByteLength(buf, off);
        skip(byteLen + 1);
        return UtfOps.bytesToString(buf, off, byteLen);
    }

    /**
     * Reads a char (two byte) unsigned value from the buffer.
     * Reads values that were written using {@link TupleOutput#writeChar}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final char readChar() throws IOException {

        return (char) readUnsignedShort();
    }

    /**
     * Reads a boolean (one byte) unsigned value from the buffer and returns
     * true if it is non-zero and false if it is zero.
     * Reads values that were written using {@link TupleOutput#writeBoolean}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final boolean readBoolean() throws IOException {

        int c = read();
        if (c < 0) {
            throw new EOFException();
        }
        return (c != 0);
    }

    /**
     * Reads a signed byte (one byte) value from the buffer.
     * Reads values that were written using {@link TupleOutput#writeByte}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final byte readByte() throws IOException {

        byte val = (byte) readUnsignedByte();
        if (val < 0)
            val &= (byte) ~0x80;
        else
            val |= (byte) 0x80;
        return val;
    }

    /**
     * Reads a signed short (two byte) value from the buffer.
     * Reads values that were written using {@link TupleOutput#writeShort}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final short readShort() throws IOException {

        short val = (short) readUnsignedShort();
        if (val < 0)
            val &= (short) ~0x8000;
        else
            val |= (short) 0x8000;
        return val;
    }

    /**
     * Reads a signed int (four byte) value from the buffer.
     * Reads values that were written using {@link TupleOutput#writeInt}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final int readInt() throws IOException {

        int val = (int) readUnsignedInt();
        if (val < 0)
            val &= ~0x80000000;
        else
            val |= 0x80000000;
        return val;
    }

    /**
     * Reads a signed long (eight byte) value from the buffer.
     * Reads values that were written using {@link TupleOutput#writeLong}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final long readLong() throws IOException {

        long val = readUnsignedLong();
        if (val < 0)
            val &= ~0x8000000000000000L;
        else
            val |= 0x8000000000000000L;
        return val;
    }

    /**
     * Reads a signed float (four byte) value from the buffer.
     * Reads values that were written using {@link TupleOutput#writeFloat}.
     * <code>Float.intBitsToFloat</code> is used to convert the signed int
     * value.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final float readFloat() throws IOException {

        return Float.intBitsToFloat((int) readUnsignedInt());
    }

    /**
     * Reads a signed double (eight byte) value from the buffer.
     * Reads values that were written using {@link TupleOutput#writeDouble}.
     * <code>Double.longBitsToDouble</code> is used to convert the signed long
     * value.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final double readDouble() throws IOException {

        return Double.longBitsToDouble(readUnsignedLong());
    }

    /**
     * Reads an unsigned byte (one byte) value from the buffer.
     * Reads values that were written using {@link
     * TupleOutput#writeUnsignedByte}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final int readUnsignedByte() throws IOException {

        int c = read();
        if (c < 0) {
            throw new EOFException();
        }
        return c;
    }

    /**
     * Reads an unsigned short (two byte) value from the buffer.
     * Reads values that were written using {@link
     * TupleOutput#writeUnsignedShort}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final int readUnsignedShort() throws IOException {

        int c1 = read();
        int c2 = read();
        if ((c1 | c2) < 0) {
             throw new EOFException();
        }
        return ((c1 << 8) | c2);
    }

    // --- end DataInput compatible methods ---

    /**
     * Reads an unsigned int (four byte) value from the buffer.
     * Reads values that were written using {@link
     * TupleOutput#writeUnsignedInt}.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final long readUnsignedInt() throws IOException {

        long c1 = read();
        long c2 = read();
        long c3 = read();
        long c4 = read();
        if ((c1 | c2 | c3 | c4) < 0) {
             throw new EOFException();
        }
        return ((c1 << 24) | (c2 << 16) | (c3 << 8) | c4);
    }

    /**
     * This method is private since an unsigned long cannot be treated as
     * such in Java, nor converted to a BigInteger of the same value.
     */
    private final long readUnsignedLong() throws IOException {

        long c1 = read();
        long c2 = read();
        long c3 = read();
        long c4 = read();
        long c5 = read();
        long c6 = read();
        long c7 = read();
        long c8 = read();
        if ((c1 | c2 | c3 | c4 | c5 | c6 | c7 | c8) < 0) {
             throw new EOFException();
        }
        return ((c1 << 56) | (c2 << 48) | (c3 << 40) | (c4 << 32) |
                (c5 << 24) | (c6 << 16) | (c7 << 8)  | c8);
    }

    /**
     * Reads the specified number of bytes from the buffer, converting each
     * unsigned byte value to a character of the resulting string.
     * Reads values that were written using {@link TupleOutput#writeBytes}.
     * Only characters with values below 0x100 may be read using this method.
     *
     * @param length is the number of bytes to be read.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final String readBytes(int length) throws IOException {

        StringBuffer buf = new StringBuffer(length);
        for (int i = 0; i < length; i++) {
            int c = read();
            if (c < 0) {
                throw new EOFException();
            }
            buf.append((char) c);
        }
        return buf.toString();
    }

    /**
     * Reads the specified number of characters from the buffer, converting
     * each two byte unsigned value to a character of the resulting string.
     * Reads values that were written using {@link TupleOutput#writeChars}.
     *
     * @param length is the number of characters to be read.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final String readChars(int length) throws IOException {

        StringBuffer buf = new StringBuffer(length);
        for (int i = 0; i < length; i++) {
            buf.append(readChar());
        }
        return buf.toString();
    }

    /**
     * Reads the specified number of bytes from the buffer, converting each
     * unsigned byte value to a character of the resulting array.
     * Reads values that were written using {@link TupleOutput#writeBytes}.
     * Only characters with values below 0x100 may be read using this method.
     *
     * @param chars is the array to receive the data and whose length is used
     * to determine the number of bytes to be read.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final void readBytes(char[] chars) throws IOException {

        for (int i = 0; i < chars.length; i++) {
            int c = read();
            if (c < 0) {
                throw new EOFException();
            }
            chars[i] = (char) c;
        }
    }

    /**
     * Reads the specified number of characters from the buffer, converting
     * each two byte unsigned value to a character of the resulting array.
     * Reads values that were written using {@link TupleOutput#writeChars}.
     *
     * @param chars is the array to receive the data and whose length is used
     * to determine the number of characters to be read.
     *
     * @return the value read from the buffer.
     *
     * @throws IOException if not enough bytes are available in the buffer.
     */
    public final void readChars(char[] chars) throws IOException {

        for (int i = 0; i < chars.length; i++) {
            chars[i] = readChar();
        }
    }

    /**
     * Reads the specified number of UTF characters string from the data
     * buffer and converts the data from UTF to Unicode.
     * Reads values that were written using {@link
     * TupleOutput#writeString(char[])}.
     *
     * @param length is the number of characters to be read.
     *
     * @return the converted string.
     *
     * @throws IOException if not enough bytes are available in the buffer
     * or malformed UTF data is encountered.
     */
    public final String readString(int length) throws IOException {

        char[] chars = new char[length];
        readString(chars);
        return new String(chars);
    }

    /**
     * Reads the specified number of UTF characters string from the data
     * buffer and converts the data from UTF to Unicode.
     * Reads values that were written using {@link
     * TupleOutput#writeString(char[])}.
     *
     * @param chars is the array to receive the data and whose length is used
     * to determine the number of characters to be read.
     *
     * @return the converted string.
     *
     * @throws IOException if not enough bytes are available in the buffer
     * or malformed UTF data is encountered.
     */
    public final void readString(char[] chars) throws IOException {

        byte[] buf = getBufferBytes();
        off = UtfOps.bytesToChars(buf, off, chars, 0, chars.length, false);
    }
}
