/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: MarshalledEntity.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.marshal;

import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * MarshalledEntity is implemented by entity (combined key/value) objects and
 * called by {@link SampleViews.MarshalledEntityBinding}.  In this sample,
 * MarshalledEntity is implemented by {@link Part}, {@link Supplier}, and
 * {@link Shipment}.  This interface is package-protected rather than public
 * to hide the marshalling interface from other users of the data objects.
 * Note that a MarshalledEntity must also have a no arguments contructor so
 * that it can be instantiated by the binding.
 *
 * @author Mark Hayes
 */
interface MarshalledEntity {

    /**
     * Extracts the entity's primary key and writes it to the key output.
     */
    void marshalPrimaryKey(TupleOutput keyOutput)
        throws IOException;

    /**
     * Completes construction of the entity by setting its primary key from the
     * stored primary key.
     */
    void unmarshalPrimaryKey(TupleInput keyInput)
        throws IOException;

    /**
     * Extracts the entity's index key and writes it to the key output.
     */
    void marshalIndexKey(String keyName, TupleOutput keyOutput)
        throws IOException;

    /**
     * Clears the entity's index key value for the given key name.
     *
     * <p> This method is called when the entity for this foreign key is
     * deleted, if ON_DELETE_CLEAR was specified when creating the index. </p>
     */
    void clearIndexKey(String keyName)
        throws IOException;
}
