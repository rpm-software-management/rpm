/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Supplier.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.entity;

/**
 * A Supplier represents the combined key/value pair for a supplier entity.
 *
 * <p> In this sample, Supplier is created from the stored key/value data using
 * a SerialSerialBinding.  See {@link SampleViews.SupplierBinding} for details.
 * Since this class is not used directly for data storage, it does not need to
 * be Serializable. </p>
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
