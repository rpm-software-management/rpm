/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: SerialFormat.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.util.FastInputStream;
import com.sleepycat.bdb.util.FastOutputStream;
import com.sleepycat.bdb.util.IOExceptionWrapper;
import java.io.IOException;

/**
 * The format for serialized data.  In addition to identifying a serial format
 * this class provides utility methods for use by bindings.
 *
 * @author Mark Hayes
 */
public class SerialFormat implements DataFormat {

    private ClassCatalog classCatalog;
    private Class baseClass;

    /**
     * Creates a serial format.
     *
     * @param classCatalog is the catalog to hold shared class information and
     * for a database should be a {@link
     * com.sleepycat.bdb.StoredClassCatalog}.
     *
     * @param baseClass is the base class for serialized objects stored using
     * this format -- all objects using this format must be an instance of
     * this class.
     */
    public SerialFormat(ClassCatalog classCatalog, Class baseClass) {

        this.classCatalog = classCatalog;
        this.baseClass = baseClass;
    }

    /**
     * Returns the base class for this format.
     *
     * @return the base class for this format.
     */
    public final Class getBaseClass() {

        return baseClass;
    }

    /**
     * Utility method for use by bindings to deserialize an object.  May only
     * be called for data that was serialized using {@link #objectToData},
     * since the fixed serialization header is assumed to not be included in
     * the input data. {@link SerialInput} is used to deserialize the object.
     * If a deserialized object is cached in the buffer's data formation
     * property, it is returned directly.
     *
     * @param data is the input serialized data.
     *
     * @return the output deserialized object.
     */
    public final Object dataToObject(DataBuffer data)
        throws IOException {

        Object object = data.getDataFormation();
        if (object != null) return object;

        int length = data.getDataLength();
        byte[] hdr = SerialOutput.getStreamHeader();
        byte[] bufWithHeader = new byte[length + hdr.length];

        System.arraycopy(hdr, 0, bufWithHeader, 0, hdr.length);
        System.arraycopy(data.getDataBytes(), data.getDataOffset(),
                         bufWithHeader, hdr.length, length);

        SerialInput jin = new SerialInput(
            new FastInputStream(bufWithHeader, 0, bufWithHeader.length),
            classCatalog);

        try {
            object = jin.readObject();
        } catch (ClassNotFoundException e) {
            throw new IOExceptionWrapper(e);
        }

        data.setDataFormation(object);
        return object;
    }

    /**
     * Utility method for use by bindings to serialize an object.  The fixed
     * serialization header is not included in the output data to save space,
     * and therefore to deserialize the data the complementary {@link
     * #dataToObject} method must be used.  {@link SerialOutput} is used to
     * serialize the object.  The deserialized object is cached in the buffer's
     * data formation property.
     *
     * @param object is the input deserialized object.
     *
     * @param data is the output serialized data.
     *
     * @throws IllegalArgumentException if the object is not an instance of the
     * base class for this format.
     */
    public final void objectToData(Object object, DataBuffer data)
        throws IOException {

        if (baseClass != null && !baseClass.isInstance(object)) {
            throw new IllegalArgumentException(
                        "Data object class (" + object.getClass() +
                        ") not an instance of format's base class (" +
                        baseClass + ')');
        }
        FastOutputStream fo = new FastOutputStream();
        SerialOutput jos = new SerialOutput(fo, classCatalog);
        jos.writeObject(object);

        byte[] hdr = SerialOutput.getStreamHeader();
        data.setData(fo.getBufferBytes(), hdr.length,
                     fo.getBufferLength() - hdr.length);
        data.setDataFormation(object);
    }
}
