/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: SerialSerialBinding.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import java.io.IOException;

/**
 * An abstract entity binding that uses a serial key and a serial value.  This
 * class takes care of serializing and deserializing the key and value data
 * automatically.  Its three abstract methods must be implemented by a concrete
 * subclass to convert the deserialized objects to/from an entity object.
 * <ul>
 * <li> {@link #dataToObject(Object,Object)} </li>
 * <li> {@link #objectToKey(Object)} </li>
 * <li> {@link #objectToValue(Object)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class SerialSerialBinding implements EntityBinding {

    protected SerialFormat keyFormat;
    protected SerialFormat valueFormat;

    /**
     * Creates a serial-serial entity binding.
     *
     * @param keyFormat is the key format.
     *
     * @param valueFormat is the value format.
     */
    public SerialSerialBinding(SerialFormat keyFormat,
                               SerialFormat valueFormat) {

        this.keyFormat = keyFormat;
        this.valueFormat = valueFormat;
    }

    // javadoc is inherited
    public Object dataToObject(DataBuffer key, DataBuffer value)
        throws IOException {

        return dataToObject(keyFormat.dataToObject(key),
                            valueFormat.dataToObject(value));
    }

    // javadoc is inherited
    public void objectToKey(Object object, DataBuffer key)
        throws IOException {

        object = objectToKey(object);
        keyFormat.objectToData(object, key);
    }

    // javadoc is inherited
    public void objectToValue(Object object, DataBuffer value)
        throws IOException {

        object = objectToValue(object);
        valueFormat.objectToData(object, value);
    }

    // javadoc is inherited
    public DataFormat getKeyFormat() {

        return keyFormat;
    }

    // javadoc is inherited
    public DataFormat getValueFormat() {

        return valueFormat;
    }

    /**
     * Constructs an entity object from deserialized key and value data
     * objects.
     *
     * @param keyInput is the deserialized key data object.
     *
     * @param valueInput is the deserialized value data object.
     *
     * @return the entity object constructed from the key and value.
     */
    public abstract Object dataToObject(Object keyInput, Object valueInput)
        throws IOException;

    /**
     * Extracts a key object from an entity object.
     *
     * @param object is the entity object.
     *
     * @return the deserialized key data object.
     */
    public abstract Object objectToKey(Object object)
        throws IOException;

    /**
     * Extracts a value object from an entity object.
     *
     * @param object is the entity object.
     *
     * @return the deserialized value data object.
     */
    public abstract Object objectToValue(Object object)
        throws IOException;
}
