/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: ByteArrayBinding.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind;

import java.io.IOException;

/**
 * A transparent binding where the data byte array is used as the object.
 * The Object of the binding is of type <code>byte[]</code> and the data is
 * nothing more than the byte array itself.
 *
 * @author Mark Hayes
 */
public class ByteArrayBinding implements DataBinding {

    private ByteArrayFormat format;

    /**
     * Creates a byte array binding.
     *
     * @param format is the format of the new binding.
     */
    public ByteArrayBinding(ByteArrayFormat format) {

        this.format = format;
    }

    // javadoc is inherited
    public Object dataToObject(DataBuffer data)
        throws IOException {

        byte[] bytes = (byte[]) data.getDataFormation();
        if (bytes == null) {
            bytes = new byte[data.getDataLength()];
            System.arraycopy(data.getDataBytes(), data.getDataOffset(),
                             bytes, 0, bytes.length);
            data.setDataFormation(bytes);
        }
        return bytes;
    }

    // javadoc is inherited
    public void objectToData(Object object, DataBuffer data)
        throws IOException {

        byte[] bytes = (byte[]) object;
        data.setData(bytes, 0, bytes.length);
    }

    // javadoc is inherited
    public DataFormat getDataFormat() {

        return format;
    }
}
