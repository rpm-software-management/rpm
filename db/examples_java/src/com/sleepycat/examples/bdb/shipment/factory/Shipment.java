/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Shipment.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.factory;

import com.sleepycat.bdb.bind.tuple.MarshalledTupleKeyEntity;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;
import java.io.Serializable;

/**
 * A Shipment represents the combined key/value pair for a shipment entity.
 *
 * <p> In this sample, Shipment is bound to the stored key/value data by
 * implementing the MarshalledTupleKeyEntity interface. </p>
 *
 * <p> The binding is "tricky" in that it uses this class for both the stored
 * data value and the combined entity object.  To do this, the key field(s) are
 * transient and are set by the binding after the data object has been
 * deserialized. This avoids the use of a ShipmentValue class completely. </p>
 *
 * <p> Since this class is used directly for data storage, it must be
 * Serializable. </p>
 *
 * @author Mark Hayes
 */
public class Shipment implements Serializable, MarshalledTupleKeyEntity {

    static final String PART_KEY = "part";
    static final String SUPPLIER_KEY = "supplier";

    private transient String partNumber;
    private transient String supplierNumber;
    private int quantity;

    public Shipment(String partNumber, String supplierNumber, int quantity) {

        this.partNumber = partNumber;
        this.supplierNumber = supplierNumber;
        this.quantity = quantity;
    }

    public final String getPartNumber() {

        return partNumber;
    }

    public final String getSupplierNumber() {

        return supplierNumber;
    }

    public final int getQuantity() {

        return quantity;
    }

    public String toString() {

        return "[Shipment: part=" + partNumber +
                " supplier=" + supplierNumber +
                " quantity=" + quantity + ']';
    }

    // --- MarshalledTupleKeyEntity implementation ---

    public void marshalPrimaryKey(TupleOutput keyOutput)
        throws IOException {

        keyOutput.writeString(this.partNumber);
        keyOutput.writeString(this.supplierNumber);
    }

    public void unmarshalPrimaryKey(TupleInput keyInput)
        throws IOException {

        this.partNumber = keyInput.readString();
        this.supplierNumber = keyInput.readString();
    }

    public void marshalIndexKey(String keyName, TupleOutput keyOutput)
        throws IOException {

        if (keyName.equals(PART_KEY)) {
            keyOutput.writeString(this.partNumber);
        } else if (keyName.equals(SUPPLIER_KEY)) {
            keyOutput.writeString(this.supplierNumber);
        } else {
            throw new UnsupportedOperationException(keyName);
        }
    }

    public void clearIndexKey(String keyName)
        throws IOException {

        if (keyName.equals(PART_KEY)) {
            this.partNumber = null;
        } else if (keyName.equals(SUPPLIER_KEY)) {
            this.supplierNumber = null;
        } else {
            throw new UnsupportedOperationException(keyName);
        }
    }
}
