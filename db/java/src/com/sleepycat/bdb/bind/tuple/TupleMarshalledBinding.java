/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleMarshalledBinding.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.util.IOExceptionWrapper;
import java.io.IOException;

/**
 * A concrete key or value binding that uses the {@link MarshalledTupleData}
 * interface.  It works by calling the methods of the {@link
 * MarshalledTupleData} interface, which must be implemented by the key or
 * value class, to convert between the key or value data and the object.
 * use the {@link TupleInput#TupleInput(TupleOutput)} constructor.
 *
 * @author Mark Hayes
 */
public class TupleMarshalledBinding extends TupleBinding {

    private Class cls;

    /**
     * Creates a tuple marshalled binding object.
     *
     * <p>The given class is used to instantiate key or value objects using
     * {@link Class#forName}, and therefore must be a public class and have a
     * public no-arguments constructor.  It must also implement the {@link
     * MarshalledTupleData} interface.</p>
     *
     * @param format is the format of the new binding.
     *
     * @param cls is the class of the key or value objects.
     */
    public TupleMarshalledBinding(TupleFormat format, Class cls) {

        super(format);
        this.cls = cls;

        // The class will be used to instantiate the object.
        //
        if (!MarshalledTupleData.class.isAssignableFrom(cls)) {
            throw new IllegalArgumentException(cls.toString() +
                        " does not implement MarshalledTupleData");
        }
    }

    // javadoc is inherited
    public Object dataToObject(TupleInput input)
        throws IOException {

        try {
            MarshalledTupleData obj =
                (MarshalledTupleData) cls.newInstance();
            obj.unmarshalData(input);
            return obj;
        } catch (IllegalAccessException e) {
            throw new IOExceptionWrapper(e);
        } catch (InstantiationException e) {
            throw new IOExceptionWrapper(e);
        }
    }

    // javadoc is inherited
    public void objectToData(Object object, TupleOutput output)
        throws IOException {

        MarshalledTupleData obj = (MarshalledTupleData) object;
        obj.marshalData(output);
    }
}
