/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleTupleMarshalledBinding.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.util.IOExceptionWrapper;
import java.io.IOException;

/**
 * A concrete entity binding that uses the {@link MarshalledTupleData} and the
 * {@link MarshalledTupleKeyEntity} interfaces.  It calls the methods of the
 * {@link MarshalledTupleData} interface to convert between the value data and
 * entity object.  It calls the methods of the {@link MarshalledTupleKeyEntity}
 * interface to convert between the key data and the entity object.  These two
 * interfaces must both be implemented by the entity class
 *
 * @author Mark Hayes
 */
public class TupleTupleMarshalledBinding extends TupleTupleBinding {

    private Class cls;

    /**
     * Creates a tuple-tuple marshalled binding object.
     *
     * <p>The given class is used to instantiate entity objects using
     * {@link Class#forName}, and therefore must be a public class and have a
     * public no-arguments constructor.  It must also implement the {@link
     * MarshalledTupleData} and {@link MarshalledTupleKeyEntity}
     * interfaces.</p>
     *
     * @param keyFormat is the stored data key format.
     *
     * @param valueFormat is the stored data value format.
     *
     * @param cls is the class of the entity objects.
     */
    public TupleTupleMarshalledBinding(TupleFormat keyFormat,
                                       TupleFormat valueFormat,
                                       Class cls) {

        super(keyFormat, valueFormat);
        this.cls = cls;

        // The entity class will be used to instantiate the entity object.
        //
        if (!MarshalledTupleKeyEntity.class.isAssignableFrom(cls)) {
            throw new IllegalArgumentException(cls.toString() +
                        " does not implement MarshalledTupleKeyEntity");
        }
        if (!MarshalledTupleData.class.isAssignableFrom(cls)) {
            throw new IllegalArgumentException(cls.toString() +
                        " does not implement MarshalledTupleData");
        }
    }

    // javadoc is inherited
    public Object dataToObject(TupleInput keyInput, TupleInput valueInput)
        throws IOException {

        // This "tricky" binding returns the stored value as the entity, but
        // first it sets the transient key fields from the stored key.
        MarshalledTupleData obj;
        try {
            obj = (MarshalledTupleData) cls.newInstance();
        } catch (IllegalAccessException e) {
            throw new IOExceptionWrapper(e);
        } catch (InstantiationException e) {
            throw new IOExceptionWrapper(e);
        }
        if (valueInput != null) { // may be null if used by key extractor
            obj.unmarshalData(valueInput);
        }
        MarshalledTupleKeyEntity entity = (MarshalledTupleKeyEntity) obj;
        if (keyInput != null) { // may be null if used by key extractor
            entity.unmarshalPrimaryKey(keyInput);
        }
        return entity;
    }

    // javadoc is inherited
    public void objectToKey(Object object, TupleOutput output)
        throws IOException {

        MarshalledTupleKeyEntity entity = (MarshalledTupleKeyEntity) object;
        entity.marshalPrimaryKey(output);
    }

    // javadoc is inherited
    public void objectToValue(Object object, TupleOutput output)
        throws IOException {

        MarshalledTupleData entity = (MarshalledTupleData) object;
        entity.marshalData(output);
    }
}
