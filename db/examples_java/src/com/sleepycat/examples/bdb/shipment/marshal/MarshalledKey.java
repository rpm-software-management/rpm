/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: MarshalledKey.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.marshal;

import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * MarshalledKey is implemented by key objects and called by {@link
 * SampleViews.MarshalledKeyBinding}.  In this sample, MarshalledKey is
 * implemented by {@link PartKey}, {@link SupplierKey}, and {@link
 * ShipmentKey}.  This interface is package-protected rather than public to
 * hide the marshalling interface from other users of the data objects.  Note
 * that a MarshalledKey must also have a no arguments contructor so that it can
 * be instantiated by the binding.
 *
 * @author Mark Hayes
 */
interface MarshalledKey {

    /**
     * Construct the key tuple data from the key object.
     */
    void marshalKey(TupleOutput keyOutput)
        throws IOException;

    /**
     * Construct the key object from the key tuple data.
     */
    void unmarshalKey(TupleInput keyInput)
        throws IOException;
}
