/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleTupleBinding.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * An abstract entity binding that uses a tuple key and a tuple value.
 * This class takes care of converting the data to/from {@link TupleInput} and
 * {@link TupleOutput} objects.  Its three abstract methods must be implemented
 * by a concrete subclass to convert between tuples and entity objects.
 * <ul>
 * <li> {@link #dataToObject(TupleInput,TupleInput)} </li>
 * <li> {@link #objectToKey(Object,TupleOutput)} </li>
 * <li> {@link #objectToValue(Object,TupleOutput)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class TupleTupleBinding implements EntityBinding {

    protected TupleFormat keyFormat;
    protected TupleFormat valueFormat;

    /**
     * Creates a tuple-tuple entity binding.
     *
     * @param keyFormat is the key format.
     *
     * @param valueFormat is the value format.
     */
    public TupleTupleBinding(TupleFormat keyFormat,
                             TupleFormat valueFormat) {

        this.keyFormat = keyFormat;
        this.valueFormat = valueFormat;
    }

    // javadoc is inherited
    public Object dataToObject(DataBuffer key, DataBuffer value)
        throws IOException {

        return dataToObject(keyFormat.dataToInput(key),
                            valueFormat.dataToInput(value));
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

        TupleOutput output = valueFormat.newOutput();
        objectToValue(object, output);
        valueFormat.outputToData(output, value);
    }

    // javadoc is inherited
    public DataFormat getKeyFormat() {

        return keyFormat;
    }

    // javadoc is inherited
    public DataFormat getValueFormat() {

        return valueFormat;
    }

    // abstract methods

    /**
     * Constructs an entity object from {@link TupleInput} key and value data
     * objects.
     *
     * @param keyInput is the {@link TupleInput} key data object.
     *
     * @param valueInput is the {@link TupleInput} value data object.
     *
     * @return the entity object constructed from the key and value.
     */
    public abstract Object dataToObject(TupleInput keyInput,
                                        TupleInput valueInput)
        throws IOException;

    /**
     * Extracts a key tuple from an entity object.
     *
     * @param object is the entity object.
     *
     * @param output is the {@link TupleOutput} to which the key should be
     * written.
     */
    public abstract void objectToKey(Object object, TupleOutput output)
        throws IOException;

    /**
     * Extracts a key tuple from an entity object.
     *
     * @param object is the entity object.
     *
     * @param output is the {@link TupleOutput} to which the value should be
     * written.
     */
    public abstract void objectToValue(Object object, TupleOutput output)
        throws IOException;
}
