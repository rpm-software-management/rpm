/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Supplier.java,v 1.2 2004/09/22 16:17:13 mark Exp $
 */

package com.sleepycat.examples.collections.ship.tuple;

/**
 * A Supplier represents the combined key/data pair for a supplier entity.
 *
 * <p> In this sample, Supplier is created from the stored key/data entry
 * using a SerialSerialBinding.  See {@link SampleViews.SupplierBinding} for
 * details.  Since this class is not used directly for data storage, it does
 * not need to be Serializable. </p>
 *
 * @author Mark Hayes
 */
public class Supplier {

    private String number;
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
}
