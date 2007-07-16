/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2006
 *      Oracle Corporation.  All rights reserved.
 *
 * $Id: PackedInteger.java,v 12.2 2006/08/31 18:14:09 bostic Exp $
 */

package com.sleepycat.util;

/**
 * Static methods for reading and writing packed integers.
 *
 * <p>Note that packed integers are not sorted naturally for a byte-by-byte
 * comparison because they have a preceding length and are little endian;
 * therefore, they are typically not used for keys.</p>
 *
 * <p>Values in the inclusive range [-119,119] are stored in a single byte.
 * For values outside that range, the first byte stores the sign and the number
 * of additional bytes.  The additional bytes store (abs(value) - 119) as an
 * unsigned little endian integer.</p>
 *
 * <p>To read and write packed integer values, call {@link #readInt} and {@link
 * #writeInt}.  To get the length of a packed integer without reading it, call
 * {@link #getReadIntLength}.  To get the length of an unpacked integer without
 * writing it, call {@link #getWriteIntLength}.</p>
 *
 * <p>Note that the packed integer format is designed to accomodate long
 * integers using up to 9 bytes of storage.  Currently only int values are
 * implemented, but the same format may be used in future for long values.</p>
 */
public class PackedInteger {

    /**
     * The maximum number of bytes needed to store an int value (5).  The fifth
     * byte is only needed for values greater than (Integer.MAX_VALUE - 119) or
     * less than (Integer.MIN_VALUE + 119).
     */
    public static final int MAX_LENGTH = 5;

    /**
     * Reads a packed integer at the given buffer offset and returns it.
     *
     * @param buf the buffer to read from.
     *
     * @param off the offset in the buffer at which to start reading.
     *
     * @return the integer that was read.
     */
    public static int readInt(byte[] buf, int off) {

        boolean negative;
        int byteLen;

        int b1 = buf[off++];
        if (b1 < -119) {
            negative = true;
            byteLen = -b1 - 119;
        } else if (b1 > 119) {
            negative = false;
            byteLen = b1 - 119;
        } else {
            return b1;
        }

        int value = buf[off++] & 0xFF;
        if (byteLen > 1) {
            value |= (buf[off++] & 0xFF) << 8;
            if (byteLen > 2) {
                value |= (buf[off++] & 0xFF) << 16;
                if (byteLen > 3) {
                    value |= (buf[off++] & 0xFF) << 24;
                }
            }
        }

        return negative ? (-value - 119) : (value + 119);
    }

    /**
     * Returns the number of bytes that would be read by {@link #readInt}.
     *
     * @param buf the buffer to read from.
     *
     * @param off the offset in the buffer at which to start reading.
     *
     * @return the number of bytes that would be read.
     */
    public static int getReadIntLength(byte[] buf, int off) {

        int b1 = buf[off];
        if (b1 < -119) {
            return -b1 - 119 + 1;
        } else if (b1 > 119) {
            return b1 - 119 + 1;
        } else {
            return 1;
        }
    }

    /**
     * Writes a packed integer starting at the given buffer offset and returns
     * the next offset to be written.
     *
     * @param buf the buffer to write to.
     *
     * @param offset the offset in the buffer at which to start writing.
     *
     * @param value the integer to be written.
     *
     * @return the offset past the bytes written.
     */
    public static int writeInt(byte[] buf, int offset, int value) {

        int byte1Off = offset;
        boolean negative;

        if (value < -119) {
            negative = true;
            value = -value - 119;
        } else if (value > 119) {
            negative = false;
            value = value - 119;
        } else {
            buf[offset++] = (byte) value;
            return offset;
        }
        offset++;

        buf[offset++] = (byte) value;
        if ((value & 0xFFFFFF00) == 0) {
            buf[byte1Off] = negative ? (byte) -120 : (byte) 120;
            return offset;
        }

        buf[offset++] = (byte) (value >>> 8);
        if ((value & 0xFFFF0000) == 0) {
            buf[byte1Off] = negative ? (byte) -121 : (byte) 121;
            return offset;
        }

        buf[offset++] = (byte) (value >>> 16);
        if ((value & 0xFF000000) == 0) {
            buf[byte1Off] = negative ? (byte) -122 : (byte) 122;
            return offset;
        }

        buf[offset++] = (byte) (value >>> 24);
        buf[byte1Off] = negative ? (byte) -123 : (byte) 123;
        return offset;
    }

    /**
     * Returns the number of bytes that would be written by {@link #writeInt}.
     *
     * @param value the integer to be written.
     *
     * @return the number of bytes that would be used to write the given
     * integer.
     */
    public static int getWriteIntLength(int value) {

        if (value < -119) {
            value = -value - 119;
        } else if (value > 119) {
            value = value - 119;
        } else {
            return 1;
        }

        if ((value & 0xFFFFFF00) == 0) {
            return 2;
        }
        if ((value & 0xFFFF0000) == 0) {
            return 3;
        }
        if ((value & 0xFF000000) == 0) {
            return 4;
        }
        return 5;
    }
}
