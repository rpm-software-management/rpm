/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Supplier.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.factory;

import com.sleepycat.bdb.bind.tuple.MarshalledTupleKeyEntity;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;
import java.io.Serializable;

/**
 * A Supplier represents the combined key/value pair for a supplier entity.
 *
 * <p> In this sample, Supplier is bound to the stored key/value data by
 * implementing the MarshalledTupleKeyEntity interface. </p>
 *
 * <p> The binding is "tricky" in that it uses this class for both the stored
 * data value and the combined entity object.  To do this, the key field(s) are
 * transient and are set by the binding after the data object has been
 * deserialized. This avoids the use of a SupplierValue class completely. </p>
 *
 * <p> Since this class is used directly for data storage, it must be
 * Serializable. </p>
 *
 * @author Mark Hayes
 */
public class Supplier implements Serializable, MarshalledTupleKeyEntity {

    static final String CITY_KEY = "city";

    private transient String number;
    private String name;
    private int status;
    private String city;

    public Supplier(String number, String name, int status, String city) {

        this.number = number;
        this.name = name;
        this.status = status;
        this.city = city;
    }

    public final String getNumber() {

        return number;
    }

    public final String getName() {

        return name;
    }

    public final int getStatus() {

        return status;
    }

    public final String getCity() {

        return city;
    }

    public String toString() {

        return "[Supplier: number=" + number +
               " name=" + name +
               " status=" + status +
               " city=" + city + ']';
    }

    // --- MarshalledTupleKeyEntity implementation ---

    public void marshalPrimaryKey(TupleOutput keyOutput)
        throws IOException {

        keyOutput.writeString(this.number);
    }

    public void unmarshalPrimaryKey(TupleInput keyInput)
        throws IOException {

        this.number = keyInput.readString();
    }

    public void marshalIndexKey(String keyName, TupleOutput keyOutput)
        throws IOException {

        if (keyName.equals(CITY_KEY)) {
            keyOutput.writeString(this.city);
        } else {
            throw new UnsupportedOperationException(keyName);
        }
    }

    public void clearIndexKey(String keyName)
        throws IOException {

        if (keyName.equals(CITY_KEY)) {
            this.city = null;
        } else {
            throw new UnsupportedOperationException(keyName);
        }
    }
}
