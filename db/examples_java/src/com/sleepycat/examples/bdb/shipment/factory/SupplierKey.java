/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SupplierKey.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.factory;

import com.sleepycat.bdb.bind.tuple.MarshalledTupleData;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * A SupplierKey serves as the key in the key/value pair for a supplier entity.
 *
 * <p> In this sample, SupplierKey is bound to the stored key tuple data by
 * implementing the MarshalledTupleData interface, which is called by {@link
 * SampleViews.MarshalledKeyBinding}. </p>
 *
 * @author Mark Hayes
 */
public class SupplierKey implements MarshalledTupleData {

    private String number;

    public SupplierKey(String number) {

        this.number = number;
    }

    public final String getNumber() {

        return number;
    }

    public String toString() {

        return "[SupplierKey: number=" + number + ']';
    }

    // --- MarshalledTupleData implementation ---

    public SupplierKey() {

        // A no-argument constructor is necessary only to allow the binding to
        // instantiate objects of this class.
    }

    public void marshalData(TupleOutput keyOutput)
        throws IOException {

        keyOutput.writeString(this.number);
    }

    public void unmarshalData(TupleInput keyInput)
        throws IOException {

        this.number = keyInput.readString();
    }
}
