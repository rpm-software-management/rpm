/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataBuffer.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind;

/**
 * The interface used in bindings to access the data as a byte array.
 *
 * <p>Each data buffer may contain a formation Object that is associated with
 * the data. The formation may be used by bindings to cache an internal
 * representation of the data that is specific to the format but can be used by
 * all bindings.  The formation must be cleared whenever the data in the buffer
 * is changed.</p>
 *
 * @author Mark Hayes
 */
public interface DataBuffer {

    /**
     * Sets the data in this buffer to the given value.  The byte array given
     * will be owned by this buffer object after this method is called and
     * must not be modified thereafter.  This method must clear the data
     * formation for this buffer.
     *
     * @param data is the data array and must not be modified after this
     *  method is called.
     *
     * @param offset is the byte offset of the data in the array.
     *
     * @param length is the byte length of the data in the array.
     */
    void setData(byte[] data, int offset, int length);

    /**
     * Returns the byte array of the data buffer.  This array is owned by
     * the buffer object and should not be modified.
     *
     * @return the byte array of the data buffer.
     */
    byte[] getDataBytes();

    /**
     * Returns the byte offset of the data in the array.
     *
     * @return the byte offset of the data in the array.
     */
    int getDataOffset();

    /**
     * Returns the byte length of the data in the array.
     *
     * @return the byte length of the data in the array.
     */
    int getDataLength();

    /**
     * Sets the formation associated with the data in this buffer.
     *
     * @param formation is Object to set.
     */
    void setDataFormation(Object formation);

    /**
     * Returns the formation associated with the data in this buffer.
     *
     * @return formation Object.
     */
    Object getDataFormation();

    /**
     * Sets the formation associated with the data in this buffer to null.
     */
    void clearDataFormation();
}
