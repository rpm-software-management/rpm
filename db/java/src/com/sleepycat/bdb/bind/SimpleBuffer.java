/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: SimpleBuffer.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind;

/**
 * A simple data buffer implementation that allows using bindings for arbitrary
 * data outside the context of a database.
 *
 * @author Mark Hayes
 */
public class SimpleBuffer implements DataBuffer {

    private byte[] data;
    private int offset;
    private int length;
    private Object formation;

    /**
     * Creates a simple buffer with null data, an offset of zero and a length
     * of zero.
     */
    public SimpleBuffer() {
    }

    /**
     * Creates a simple buffer with the given data with an offset of zero and a
     * length equal to the length of the data array.
     *
     * @param data is the data array and must not be modified after this
     *  method is called.
     */
    public SimpleBuffer(byte[] data) {

        setData(data, 0, data.length);
    }

    /**
     * Creates a simple buffer with the given data, offset and length.
     *
     * @param data is the data array and must not be modified after this
     *  method is called.
     *
     * @param offset is the byte offset of the data in the array.
     *
     * @param length is the byte length of the data in the array.
     */
    public SimpleBuffer(byte[] data, int offset, int length) {

        setData(data, offset, length);
    }

    // javadoc is inherited
    public void setData(byte[] data, int offset, int length) {

        formation = null;
        this.data = data;
        this.offset = offset;
        this.length = length;
    }

    // javadoc is inherited
    public byte[] getDataBytes() {

        return data;
    }

    // javadoc is inherited
    public int getDataOffset() {

        return offset;
    }

    // javadoc is inherited
    public int getDataLength() {

        return length;
    }

    // javadoc is inherited
    public void setDataFormation(Object formation) {

        this.formation = formation;
    }

    // javadoc is inherited
    public Object getDataFormation() {

        return formation;
    }

    // javadoc is inherited
    public void clearDataFormation() {

        formation = null;
    }
}
