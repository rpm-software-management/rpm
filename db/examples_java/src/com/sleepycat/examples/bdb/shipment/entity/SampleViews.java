/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SampleViews.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.entity;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.serial.SerialBinding;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.bdb.bind.serial.SerialSerialBinding;
import com.sleepycat.bdb.collection.StoredMap;
import com.sleepycat.bdb.collection.StoredValueSet;
import java.io.IOException;

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
        // In this sample, EntityBinding classes are used to bind the stored
        // key/value data pair to a combined value object.  For keys, however,
        // the stored data is used directly via a SerialBinding and no
        // special binding class is needed.
        //
        DataBinding partKeyBinding =
            new SerialBinding(db.getPartKeyFormat());
        EntityBinding partValueBinding =
            new PartBinding(db.getPartKeyFormat(), db.getPartValueFormat());
        DataBinding supplierKeyBinding =
            new SerialBinding(db.getSupplierKeyFormat());
        EntityBinding supplierValueBinding =
            new SupplierBinding(db.getSupplierKeyFormat(),
                                db.getSupplierValueFormat());
        DataBinding shipmentKeyBinding =
            new SerialBinding(db.getShipmentKeyFormat());
        EntityBinding shipmentValueBinding =
            new ShipmentBinding(db.getShipmentKeyFormat(),
                                db.getShipmentValueFormat());
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
    // java.util.Set interfaces, or using the StoredMap and StoredValueSet
    // classes, which provide additional methods.  The entity sets could be
    // obtained directly from the Map.values() method but convenience methods
    // are provided here to return them in order to avoid down-casting
    // elsewhere.

    /**
     * Return a map view of the part storage container.
     */
    public StoredMap getPartMap() {

        return partMap;
    }

    /**
     * Return a map view of the supplier storage container.
     */
    public StoredMap getSupplierMap() {

        return supplierMap;
    }

    /**
     * Return a map view of the shipment storage container.
     */
    public StoredMap getShipmentMap() {

        return shipmentMap;
    }

    /**
     * Return an entity set view of the part storage container.
     */
    public StoredValueSet getPartSet() {

        return (StoredValueSet) partMap.values();
    }

    /**
     * Return an entity set view of the supplier storage container.
     */
    public StoredValueSet getSupplierSet() {

        return (StoredValueSet) supplierMap.values();
    }

    /**
     * Return an entity set view of the shipment storage container.
     */
    public StoredValueSet getShipmentSet() {

        return (StoredValueSet) shipmentMap.values();
    }

    /**
     * Return a map view of the shipment-by-part index.
     */
    public StoredMap getShipmentByPartMap() {

        return shipmentByPartMap;
    }

    /**
     * Return a map view of the shipment-by-supplier index.
     */
    public StoredMap getShipmentBySupplierMap() {

        return shipmentBySupplierMap;
    }

    /**
     * Return a map view of the supplier-by-city index.
     */
    public final StoredMap getSupplierByCityMap() {

        return supplierByCityMap;
    }

    /**
     * PartBinding is used to bind the stored key/value data pair for a part
     * to a combined value object (entity).
     */
    private static class PartBinding extends SerialSerialBinding {

        /**
         * Construct the binding object.
         * @param keyFormat is the stored data key format.
         * @param valueFormat is the stored data value format.
         */
        private PartBinding(SerialFormat keyFormat,
                            SerialFormat valueFormat) {

            super(keyFormat, valueFormat);
        }

        /**
         * Create the entity by combining the stored key and value.
         */
        public Object dataToObject(Object keyInput, Object valueInput)
            throws IOException {

            PartKey key = (PartKey) keyInput;
            PartValue value = (PartValue) valueInput;
            return new Part(key.getNumber(), value.getName(), value.getColor(),
                            value.getWeight(), value.getCity());
        }

        /**
         * Create the stored key from the entity.
         */
        public Object objectToKey(Object object)
            throws IOException {

            Part part = (Part) object;
            return new PartKey(part.getNumber());
        }

        /**
         * Create the stored value from the entity.
         */
        public Object objectToValue(Object object)
            throws IOException {

            Part part = (Part) object;
            return new PartValue(part.getName(), part.getColor(),
                                 part.getWeight(), part.getCity());
        }
    }

    /**
     * PartBinding is used to bind the stored key/value data pair for a part
     * to a combined value object (entity).
     */
    private static class SupplierBinding extends SerialSerialBinding {

        /**
         * Construct the binding object.
         * @param keyFormat is the stored data key format.
         * @param valueFormat is the stored data value format.
         */
        private SupplierBinding(SerialFormat keyFormat,
                                SerialFormat valueFormat) {

            super(keyFormat, valueFormat);
        }

        /**
         * Create the entity by combining the stored key and value.
         */
        public Object dataToObject(Object keyInput, Object valueInput)
            throws IOException {

            SupplierKey key = (SupplierKey) keyInput;
            SupplierValue value = (SupplierValue) valueInput;
            return new Supplier(key.getNumber(), value.getName(),
                                value.getStatus(), value.getCity());
        }

        /**
         * Create the stored key from the entity.
         */
        public Object objectToKey(Object object)
            throws IOException {

            Supplier supplier = (Supplier) object;
            return new SupplierKey(supplier.getNumber());
        }

        /**
         * Create the stored value from the entity.
         */
        public Object objectToValue(Object object)
            throws IOException {

            Supplier supplier = (Supplier) object;
            return new SupplierValue(supplier.getName(), supplier.getStatus(),
                                     supplier.getCity());
        }
    }

    /**
     * PartBinding is used to bind the stored key/value data pair for a part
     * to a combined value object (entity).
     */
    private static class ShipmentBinding extends SerialSerialBinding {

        /**
         * Construct the binding object.
         * @param keyFormat is the stored data key format.
         * @param valueFormat is the stored data value format.
         */
        private ShipmentBinding(SerialFormat keyFormat,
                                SerialFormat valueFormat) {

            super(keyFormat, valueFormat);
        }

        /**
         * Create the entity by combining the stored key and value.
         */
        public Object dataToObject(Object keyInput, Object valueInput)
            throws IOException {

            ShipmentKey key = (ShipmentKey) keyInput;
            ShipmentValue value = (ShipmentValue) valueInput;
            return new Shipment(key.getPartNumber(), key.getSupplierNumber(),
                                value.getQuantity());
        }

        /**
         * Create the stored key from the entity.
         */
        public Object objectToKey(Object object)
            throws IOException {

            Shipment shipment = (Shipment) object;
            return new ShipmentKey(shipment.getPartNumber(),
                                   shipment.getSupplierNumber());
        }

        /**
         * Create the stored value from the entity.
         */
        public Object objectToValue(Object object)
            throws IOException {

            Shipment shipment = (Shipment) object;
            return new ShipmentValue(shipment.getQuantity());
        }
    }
}
