/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: FastOutputStream.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.util;

import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;

/**
 * A replacement for ByteArrayOutputStream that does not synchronize every
 * byte read.
 *
 * @author Mark Hayes
 */
public class FastOutputStream extends OutputStream {

    public static final int DEFAULT_INIT_SIZE = 100;
    public static final int DEFAULT_BUMP_SIZE = 100;

    private int len;
    private int bumpLen;
    private byte[] buf;

    /**
     * Creates an output stream with default sizes.
     */
    public FastOutputStream() {

        buf = new byte[DEFAULT_INIT_SIZE];
        bumpLen = DEFAULT_BUMP_SIZE;
    }

    /**
     * Creates an output stream with a default bump size and a given initial
     * size.
     *
     * @param initialSize the initial size of the buffer.
     */
    public FastOutputStream(int initialSize) {

        buf = new byte[initialSize];
        bumpLen = DEFAULT_BUMP_SIZE;
    }

    /**
     * Creates an output stream with a given bump size and initial size.
     *
     * @param initialSize the initial size of the buffer.
     *
     * @param bumpSize the amount to increment the buffer.
     */
    public FastOutputStream(int initialSize, int bumpSize) {

        buf = new byte[initialSize];
        bumpLen = bumpSize;
    }

    /**
     * Creates an output stream with a given initial buffer and a default
     * bump size.
     *
     * @param buffer the initial buffer; will be owned by this object.
     */
    public FastOutputStream(byte[] buffer) {

        buf = buffer;
        bumpLen = DEFAULT_BUMP_SIZE;
    }

    /**
     * Creates an output stream with a given initial buffer and a given
     * bump size.
     *
     * @param buffer the initial buffer; will be owned by this object.
     *
     * @param bumpSize the amount to increment the buffer.
     */
    public FastOutputStream(byte[] buffer, int bumpSize) {

        buf = buffer;
        bumpLen = bumpSize;
    }

    // --- begin ByteArrayOutputStream compatible methods ---

    public int size() {

        return len;
    }

    public void reset() {

        len = 0;
    }

    public void write(int b) throws IOException {

        if (len + 1 > buf.length)
            bump(1);

        buf[len++] = (byte) b;
    }

    public void write(byte[] fromBuf) throws IOException {

        int needed = len + fromBuf.length - buf.length;
        if (needed > 0)
            bump(needed);

        for (int i = 0; i < fromBuf.length; i++)
            buf[len++] = fromBuf[i];
    }

    public void write(byte[] fromBuf, int offset, int length)
        throws IOException {

        int needed = len + length - buf.length;
        if (needed > 0)
            bump(needed);

        int fromLen = offset + length;

        for (int i = offset; i < fromLen; i++)
            buf[len++] = fromBuf[i];
    }

    public synchronized void writeTo(OutputStream out) throws IOException {

        out.write(buf, 0, len);
    }

    public String toString() {

        return new String(buf, 0, len);
    }

    public String toString(String encoding)
        throws UnsupportedEncodingException {

        return new String(buf, 0, len, encoding);
    }

    public byte[] toByteArray() {

        byte[] toBuf = new byte[len];

        for (int i = 0; i < len; i++)
            toBuf[i] = buf[i];

        return toBuf;
    }

    // --- end ByteArrayOutputStream compatible methods ---

    /**
     * Copy the buffered data to the given array.
     *
     * @param toBuf the buffer to hold a copy of the data.
     *
     * @param offset the offset at which to start copying.
     */
    public void toByteArray(byte[] toBuf, int offset) {

        int toLen = (toBuf.length > len) ? len : toBuf.length;

        for (int i = offset; i < toLen; i++)
            toBuf[i] = buf[i];
    }

    /**
     * Returns the buffer owned by this object.
     *
     * @return the buffer.
     */
    public byte[] getBufferBytes() {

        return buf;
    }

    /**
     * Returns the offset of the internal buffer.
     *
     * @return always zero currently.
     */
    public int getBufferOffset() {

        return 0;
    }

    /**
     * Returns the length used in the internal buffer, that is, the offset at
     * which data will be written next.
     *
     * @return the buffer length.
     */
    public int getBufferLength() {

        return len;
    }

    /**
     * Ensure that at least the given number of bytes are available in the
     * internal buffer.
     *
     * @param sizeNeeded the number of bytes desired.
     */
    public void makeSpace(int sizeNeeded) {

        int needed = len + sizeNeeded - buf.length;
        if (needed > 0)
            bump(needed);
    }

    /**
     * Skip the given number of bytes in the buffer.
     *
     * @param sizeAdded number of bytes to skip.
     */
    public void addSize(int sizeAdded) {

        len += sizeAdded;
    }

    private void bump(int needed) {

        byte[] toBuf = new byte[buf.length + needed + bumpLen];

        for (int i = 0; i < len; i++)
            toBuf[i] = buf[i];

        buf = toBuf;
    }
}
