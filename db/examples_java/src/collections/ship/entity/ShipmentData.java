/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: ShipmentData.java,v 12.6 2007/05/17 15:15:33 bostic Exp $
 */

package collections.ship.entity;

import java.io.Serializable;

/**
 * A ShipmentData serves as the value in the key/value pair for a shipment
 * entity.
 *
 * <p> In this sample, ShipmentData is used only as the storage data for the
 * value, while the Shipment object is used as the value's object
 * representation.  Because it is used directly as storage data using
 * serial format, it must be Serializable. </p>
 *
 * @author Mark Hayes
 */
public class ShipmentData implements Serializable {

    private int quantity;

    public ShipmentData(int quantity) {

        this.quantity = quantity;
    }

    public final int getQuantity() {

        return quantity;
    }

    public String toString() {

        return "[ShipmentData: quantity=" + quantity + ']';
    }
}
