/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: PartKey.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.factory;

import com.sleepycat.bdb.bind.tuple.MarshalledTupleData;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * A PartKey serves as the key in the key/value pair for a part entity.
 *
 * <p> In this sample, PartKey is bound to the stored key tuple data by
 * implementing the MarshalledTupleData interface, which is called by {@link
 * SampleViews.MarshalledKeyBinding}. </p>
 *
 * @author Mark Hayes
 */
public class PartKey implements MarshalledTupleData {

    private String number;

    public PartKey(String number) {

        this.number = number;
    }

    public final String getNumber() {

        return number;
    }

    public String toString() {

        return "[PartKey: number=" + number + ']';
    }

    // --- MarshalledTupleData implementation ---

    public PartKey() {

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
