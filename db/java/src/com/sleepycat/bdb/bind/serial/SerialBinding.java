/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: SerialBinding.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import java.io.IOException;

/**
 * A concrete serial binding for keys or values.  This binding stores objects
 * in serial data format.  If this class is used "as is" the deserialized
 * objects are returned by the binding, and these objects must be serializable.
 *
 * <p>The class may also be extended to override the {@link
 * #dataToObject(Object)} and {@link #objectToData(Object)} methods in order to
 * map between the deserialized objects and other objects.  In that case the
 * objects returned by the binding do not have to be serializable.  Note that
 * both methods must be overridden.</p>
 *
 * @author Mark Hayes
 */
public class SerialBinding implements DataBinding {

    protected SerialFormat format;

    /**
     * Creates a serial binding.
     *
     * @param format is the format of the new binding.
     */
    public SerialBinding(SerialFormat format) {

        this.format = format;
    }

    // javadoc is inherited
    public Object dataToObject(DataBuffer data)
        throws IOException {

        return dataToObject(format.dataToObject(data));
    }

    // javadoc is inherited
    public void objectToData(Object object, DataBuffer data)
        throws IOException {

        format.objectToData(objectToData(object), data);
    }

    // javadoc is inherited
    public DataFormat getDataFormat() {

        return format;
    }

    /**
     * Can be overridden to convert the deserialized data object to another
     * object.  This method is called by {@link #dataToObject(DataBuffer)}
     * after deserializing the data. The default implemention simply returns
     * the data parameter.
     *
     * @param data is the deserialized data object (will always be
     * serializable).
     *
     * @return the resulting object.
     */
    public Object dataToObject(Object data)
        throws IOException {

        return data;
    }

    /**
     * Can be overridden to convert the object to a deserialized data object.
     * This method is called by {@link #objectToData(Object,DataBuffer)}
     * before serializing the object. The default implemention simply returns
     * the object parameter.
     *
     * @param object is the source object.
     *
     * @return the resulting deserialized object (must be serializable)..
     */
    public Object objectToData(Object object)
        throws IOException {

        return object;
    }
}
