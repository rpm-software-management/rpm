/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SampleViews.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.index;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.serial.SerialBinding;
import com.sleepycat.bdb.collection.StoredEntrySet;
import com.sleepycat.bdb.collection.StoredMap;

/**
 * SampleViews defines the data bindings and collection views for the sample
 * database.
 *
 * @author Mark Hayes
 */
public class SampleViews {

    private StoredMap partMap;
    private StoredMap supplierMap;
    private StoredMap shipmentMap;
    private StoredMap shipmentByPartMap;
    private StoredMap shipmentBySupplierMap;
    private StoredMap supplierByCityMap;

    /**
     * Create the data bindings and collection views.
     */
    public SampleViews(SampleDatabase db) {

        // Create the data bindings.
        // In this sample, the stored keys and values are used directly rather
        // than mapping them to separate objects. Therefore, no binding classes
        // are defined here and the SerialBinding class is used.
        //
        DataBinding partKeyBinding =
            new SerialBinding(db.getPartKeyFormat());
        DataBinding partValueBinding =
            new SerialBinding(db.getPartValueFormat());
        DataBinding supplierKeyBinding =
            new SerialBinding(db.getSupplierKeyFormat());
        DataBinding supplierValueBinding =
            new SerialBinding(db.getSupplierValueFormat());
        DataBinding shipmentKeyBinding =
            new SerialBinding(db.getShipmentKeyFormat());
        DataBinding shipmentValueBinding =
            new SerialBinding(db.getShipmentValueFormat());
        DataBinding cityKeyBinding =
            new SerialBinding(db.getCityKeyFormat());

        // Create map views for all stores and indices.
        // StoredSortedMap is not used since the stores and indices are
        // ordered by serialized key objects, which do not provide a very
        // useful ordering.
        //
        partMap =
            new StoredMap(db.getPartStore(),
                          partKeyBinding, partValueBinding, true);
        supplierMap =
            new StoredMap(db.getSupplierStore(),
                          supplierKeyBinding, supplierValueBinding, true);
        shipmentMap =
            new StoredMap(db.getShipmentStore(),
                          shipmentKeyBinding, shipmentValueBinding, true);
        shipmentByPartMap =
            new StoredMap(db.getShipmentByPartIndex(),
                          partKeyBinding, shipmentValueBinding, true);
        shipmentBySupplierMap =
            new StoredMap(db.getShipmentBySupplierIndex(),
                          supplierKeyBinding, shipmentValueBinding, true);
        supplierByCityMap =
            new StoredMap(db.getSupplierByCityIndex(),
                          cityKeyBinding, supplierValueBinding, true);
    }

    // The views returned below can be accessed using the java.util.Map or
    // java.util.Set interfaces, or using the StoredMap and StoredEntrySet
    // classes, which provide additional methods.  The entry sets could be
    // obtained directly from the Map.entrySet() method, but convenience
    // methods are provided here to return them in order to avoid down-casting
    // elsewhere.

    /**
     * Return a map view of the part storage container.
     */
    public final StoredMap getPartMap() {

        return partMap;
    }

    /**
     * Return a map view of the supplier storage container.
     */
    public final StoredMap getSupplierMap() {

        return supplierMap;
    }

    /**
     * Return a map view of the shipment storage container.
     */
    public final StoredMap getShipmentMap() {

        return shipmentMap;
    }

    /**
     * Return an entry set view of the part storage container.
     */
    public final StoredEntrySet getPartEntrySet() {

        return (StoredEntrySet) partMap.entrySet();
    }

    /**
     * Return an entry set view of the supplier storage container.
     */
    public final StoredEntrySet getSupplierEntrySet() {

        return (StoredEntrySet) supplierMap.entrySet();
    }

    /**
     * Return an entry set view of the shipment storage container.
     */
    public final StoredEntrySet getShipmentEntrySet() {

        return (StoredEntrySet) shipmentMap.entrySet();
    }

    /**
     * Return a map view of the shipment-by-part index.
     */
    public final StoredMap getShipmentByPartMap() {

        return shipmentByPartMap;
    }

    /**
     * Return a map view of the shipment-by-supplier index.
     */
    public final StoredMap getShipmentBySupplierMap() {

        return shipmentBySupplierMap;
    }

    /**
     * Return a map view of the supplier-by-city index.
     */
    public final StoredMap getSupplierByCityMap() {

        return supplierByCityMap;
    }

    /**
     * Return an entry set view of the shipment-by-part index.
     */
    public final StoredEntrySet getShipmentByPartEntrySet() {

        return (StoredEntrySet) shipmentByPartMap.entrySet();
    }

    /**
     * Return an entry set view of the shipment-by-supplier index.
     */
    public final StoredEntrySet getShipmentBySupplierEntrySet() {

        return (StoredEntrySet) shipmentBySupplierMap.entrySet();
    }

    /**
     * Return entry set view of the supplier-by-city index.
     */
    public final StoredEntrySet getSupplierByCityEntrySet() {

        return (StoredEntrySet) supplierByCityMap.entrySet();
    }
}
