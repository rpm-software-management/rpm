/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleSerialBinding.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * A abstract entity binding that uses a tuple key and a serial value. This
 * class takes care of serializing and deserializing the value data,
 * and converting the key data to/from {@link TupleInput} and {@link
 * TupleOutput} objects.  Its three abstract methods must be implemented by a
 * concrete subclass to convert these objects to/from an entity object.
 * <ul>
 * <li> {@link #dataToObject(TupleInput,Object)} </li>
 * <li> {@link #objectToKey(Object,TupleOutput)} </li>
 * <li> {@link #objectToValue(Object)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class TupleSerialBinding implements EntityBinding {

    protected TupleFormat keyFormat;
    protected SerialFormat valueFormat;

    /**
     * Creates a tuple-serial entity binding.
     *
     * @param keyFormat is the key format.
     *
     * @param valueFormat is the value format.
     */
    public TupleSerialBinding(TupleFormat keyFormat,
                              SerialFormat valueFormat) {

        this.keyFormat = keyFormat;
        this.valueFormat = valueFormat;
    }

    // javadoc is inherited
    public Object dataToObject(DataBuffer key, DataBuffer value)
        throws IOException {

        return dataToObject(keyFormat.dataToInput(key),
                            valueFormat.dataToObject(value));
    }

    // javadoc is inherited
    public void objectToKey(Object object, DataBuffer key)
        throws IOException {

        TupleOutput output = keyFormat.newOutput();
        objectToKey(object, output);
        keyFormat.outputToData(output, key);
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
     * Constructs an entity object from {@link TupleInput} key data and
     * deserialized value data objects.
     *
     * @param keyInput is the {@link TupleInput} key data object.
     *
     * @param valueInput is the deserialized value data object.
     *
     * @return the entity object constructed from the key and value.
     *
     * @throws IOException if data cannot be read or written.
     */
    public abstract Object dataToObject(TupleInput keyInput, Object valueInput)
        throws IOException;

    /**
     * Extracts a key tuple from an entity object.
     *
     * @param object is the entity object.
     *
     * @param output is the {@link TupleOutput} to which the key should be
     * written.
     *
     * @throws IOException if data cannot be read or written.
     */
    public abstract void objectToKey(Object object, TupleOutput output)
        throws IOException;

    /**
     * Extracts a value object from an entity object.
     *
     * @param object is the entity object.
     *
     * @return the deserialized value data object.
     *
     * @throws IOException if data cannot be read or written.
     */
    public abstract Object objectToValue(Object object)
        throws IOException;
}
