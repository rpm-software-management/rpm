/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: FastInputStream.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.util;

import java.io.IOException;
import java.io.InputStream;

/**
 * A replacement for ByteArrayInputStream that does not synchronize every
 * byte read.
 *
 * @author Mark Hayes
 */
public class FastInputStream extends InputStream {

    protected int len;
    protected int off;
    protected int mark;
    protected byte[] buf;

    /**
     * Creates an input stream.
     *
     * @param buffer the data to read.
     */
    public FastInputStream(byte[] buffer) {

        buf = buffer;
        len = buffer.length;
    }

    /**
     * Creates an input stream.
     *
     * @param buffer the data to read.
     *
     * @param offset the byte offset at which to begin reading.
     *
     * @param length the number of bytes to read.
     */
    public FastInputStream(byte[] buffer, int offset, int length) {

        buf = buffer;
        off = offset;
        len = length;
    }

    // --- begin ByteArrayInputStream compatible methods ---

    public int available() {

        return len - off;
    }

    public boolean markSupported() {

        return true;
    }

    public void mark(int pos) {

        mark = pos;
    }

    public void reset() {

        off = mark;
    }

    public long skip(long count) {

        int myCount = (int) count;
        if (myCount + off > len) {
            myCount = len - off;
        }
        off += myCount;
        return myCount;
    }

    public int read() throws IOException {

        return (off < len) ? (buf[off++] & 0xff) : (-1);
    }

    public int read(byte[] toBuf) throws IOException {

        return read(toBuf, 0, toBuf.length);
    }

    public int read(byte[] toBuf, int offset, int length) throws IOException {

        int avail = len - off;
        if (avail <= 0) {
            return -1;
        }
        if (length > avail) {
            length = avail;
        }
        for (int i = 0; i < length; i++) {
            toBuf[offset++] = buf[off++];
        }
        return length;
    }

    // --- end ByteArrayInputStream compatible methods ---

    /**
     * Returns the underlying data being read.
     *
     * @return the underlying data.
     */
    public byte[] getBufferBytes() {

        return buf;
    }

    /**
     * Returns the offset at which data is being read from the buffer.
     *
     * @return the offset at which data is being read.
     */
    public int getBufferOffset() {

        return off;
    }

    /**
     * Returns the end of the buffer being read.
     *
     * @return the end of the buffer.
     */
    public int getBufferLength() {

        return len;
    }
}
