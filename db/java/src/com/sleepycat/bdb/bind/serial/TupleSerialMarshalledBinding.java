/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleSerialMarshalledBinding.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.bind.tuple.MarshalledTupleKeyEntity;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * A concrete entity binding that uses the {@link MarshalledTupleKeyEntity}
 * interface.  It works by calling the methods of the {@link
 * MarshalledTupleKeyEntity} interface, which must be implemented by the entity
 * class, to convert between the key/value data and entity object.
 *
 * <p> The binding is "tricky" in that it uses the entity class for both the
 * stored data value and the combined entity object.  To do this, the entity's
 * key field(s) are transient and are set by the binding after the data object
 * has been deserialized. This avoids the use of a "value" class completely.
 * </p>
 *
 * @author Mark Hayes
 * @see MarshalledTupleKeyEntity
 */
public class TupleSerialMarshalledBinding extends TupleSerialBinding {

    /**
     * Creates a tuple-serial marshalled binding object.
     *
     * @param keyFormat is the key data format.
     *
     * @param valueFormat is the value data format.
     */
    public TupleSerialMarshalledBinding(TupleFormat keyFormat,
                                        SerialFormat valueFormat) {

        super(keyFormat, valueFormat);
    }

    // javadoc is inherited
    public Object dataToObject(TupleInput tupleInput, Object javaInput)
        throws IOException {

        // Creates the entity by combining the stored key and value.
        // This "tricky" binding returns the stored value as the entity, but
        // first it sets the transient key fields from the stored key.
        MarshalledTupleKeyEntity entity = (MarshalledTupleKeyEntity) javaInput;

        if (tupleInput != null) { // may be null if not used by key extractor
            entity.unmarshalPrimaryKey(tupleInput);
        }
        return entity;
    }

    // javadoc is inherited
    public void objectToKey(Object object, TupleOutput output)
        throws IOException {

        // Creates the stored key from the entity.
        MarshalledTupleKeyEntity entity = (MarshalledTupleKeyEntity) object;
        entity.marshalPrimaryKey(output);
    }

    // javadoc is inherited
    public Object objectToValue(Object object)
        throws IOException {

        // Returns the entity as the stored value.  There is nothing to do here
        // since the entity's key fields are transient.
        return object;
    }
}
