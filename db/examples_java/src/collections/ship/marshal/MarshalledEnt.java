/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: MarshalledEnt.java,v 12.4 2006/08/24 14:45:58 bostic Exp $
 */

package collections.ship.marshal;

import com.sleepycat.bind.tuple.TupleInput;
import com.sleepycat.bind.tuple.TupleOutput;

/**
 * MarshalledEnt is implemented by entity (combined key/data) objects and
 * called by {@link SampleViews.MarshalledEntityBinding}.  In this sample,
 * MarshalledEnt is implemented by {@link Part}, {@link Supplier}, and
 * {@link Shipment}.  This interface is package-protected rather than public
 * to hide the marshalling interface from other users of the data objects.
 * Note that a MarshalledEnt must also have a no arguments constructor so
 * that it can be instantiated by the binding.
 *
 * @author Mark Hayes
 */
interface MarshalledEnt {

    /**
     * Extracts the entity's primary key and writes it to the key output.
     */
    void marshalPrimaryKey(TupleOutput keyOutput);

    /**
     * Completes construction of the entity by setting its primary key from the
     * stored primary key.
     */
    void unmarshalPrimaryKey(TupleInput keyInput);

    /**
     * Extracts the entity's index key and writes it to the key output.
     */
    boolean marshalSecondaryKey(String keyName, TupleOutput keyOutput);
}
