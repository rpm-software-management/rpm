/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: PartValue.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.basic;

import java.io.Serializable;

/**
 * A PartValue serves as the value in the key/value pair for a part entity.
 *
 * <p> In this sample, PartValue is used both as the storage data for the value
 * as well as the object binding to the value.  Because it is used directly as
 * storage data using SerialFormat, it must be Serializable. </p>
 *
 * @author Mark Hayes
 */
public class PartValue implements Serializable {

    private String name;
    private String color;
    private Weight weight;
    private String city;

    public PartValue(String name, String color, Weight weight, String city) {

        this.name = name;
        this.color = color;
        this.weight = weight;
        this.city = city;
    }

    public final String getName() {

        return name;
    }

    public final String getColor() {

        return color;
    }

    public final Weight getWeight() {

        return weight;
    }

    public final String getCity() {

        return city;
    }

    public String toString() {

        return "[PartValue: name=" + name +
               " color=" + color +
               " weight=" + weight +
               " city=" + city + ']';
    }
}
