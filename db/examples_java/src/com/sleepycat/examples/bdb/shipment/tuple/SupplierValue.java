/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SupplierValue.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.tuple;

import java.io.Serializable;

/**
 * A SupplierValue serves as the value in the key/value pair for a supplier
 * entity.
 *
 * <p> In this sample, SupplierValue is used only as the storage data for the
 * value, while the Supplier object is used as the value's object
 * representation.  Because it is used directly as storage data using
 * SerialFormat, it must be Serializable. </p>
 *
 * @author Mark Hayes
 */
public class SupplierValue implements Serializable {

    private String name;
    private int status;
    private String city;

    public SupplierValue(String name, int status, String city) {

        this.name = name;
        this.status = status;
        this.city = city;
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

        return "[SupplierValue: name=" + name +
               " status=" + status +
               " city=" + city + ']';
    }
}
