/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: MarshalledTupleData.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import java.io.IOException;

/**
 * A marshalling interface implemented by key, value or entity classes that
 * have tuple data.  Key classes implement this interface to marshal their key
 * data.  Value or entity classes implement this interface to marshal their
 * value data.  Implementations of this interface must have a public no
 * arguments contructor so that they can be instantiated by a binding,
 * prior to calling the {@link #unmarshalData} method.
 *
 * <p>Note that implementing this interface is not necessary when the data is a
 * Java simple type, for example: String, Integer, etc. These types can be
 * used with built-in bindings returned by {@link
 * TupleBinding#getPrimitiveBinding}.</p>
 *
 * @author Mark Hayes
 * @see TupleTupleMarshalledBinding
 * @see TupleTupleMarshalledKeyExtractor
 */
public interface MarshalledTupleData {

    /**
     * Construct the key or value tuple data from the key or value object.
     *
     * @param dataOutput is the output tuple.
     */
    void marshalData(TupleOutput dataOutput)
        throws IOException;

    /**
     * Construct the key or value object from the key or value tuple data.
     *
     * @param dataInput is the input tuple.
     */
    void unmarshalData(TupleInput dataInput)
        throws IOException;
}
