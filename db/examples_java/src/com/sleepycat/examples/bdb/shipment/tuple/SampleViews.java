/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SampleViews.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.tuple;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.bdb.bind.serial.TupleSerialBinding;
import com.sleepycat.bdb.bind.tuple.TupleBinding;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import com.sleepycat.bdb.collection.StoredSortedValueSet;
import com.sleepycat.bdb.collection.StoredSortedMap;
import java.io.IOException;

/**
 * SampleViews defines the data bindings and collection views for the sample
 * database.
 *
 * @author Mark Hayes
 */
public class SampleViews {

    private StoredSortedMap partMap;
    private StoredSortedMap supplierMap;
    private StoredSortedMap shipmentMap;
    private StoredSortedMap shipmentByPartMap;
    private StoredSortedMap shipmentBySupplierMap;
    private StoredSortedMap supplierByCityMap;

    /**
     * Create the data bindings and collection views.
     */
    public SampleViews(SampleDatabase db) {

        // Create the data bindings.
        // In this sample, EntityBinding classes are used to bind the stored
        // key/value data pair to a combined value object.  For keys, a
        // one-to-one binding is implemented with DataBinding classes to bind
        // the stored tuple data to a key Object.
        //
        DataBinding partKeyBinding =
            new PartKeyBinding(db.getPartKeyFormat());
        EntityBinding partValueBinding =
            new PartBinding(db.getPartKeyFormat(), db.getPartValueFormat());
        DataBinding supplierKeyBinding =
            new SupplierKeyBinding(db.getSupplierKeyFormat());
        EntityBinding supplierValueBinding =
            new SupplierBinding(db.getSupplierKeyFormat(),
                                db.getSupplierValueFormat());
        DataBinding shipmentKeyBinding =
            new ShipmentKeyBinding(db.getShipmentKeyFormat());
        EntityBinding shipmentValueBinding =
            new ShipmentBinding(db.getShipmentKeyFormat(),
                                db.getShipmentValueFormat());
        DataBinding cityKeyBinding =
            TupleBinding.getPrimitiveBinding(String.class,
                                             db.getCityKeyFormat());

        // Create map views for all stores and indices.
        // StoredSortedMap is used since the stores and indices are ordered
        // (they use the DB_BTREE access method).
        //
        partMap =
            new StoredSortedMap(db.getPartStore(),
                          partKeyBinding, partValueBinding, true);
        supplierMap =
            new StoredSortedMap(db.getSupplierStore(),
                          supplierKeyBinding, supplierValueBinding, true);
        shipmentMap =
            new StoredSortedMap(db.getShipmentStore(),
                          shipmentKeyBinding, shipmentValueBinding, true);
        shipmentByPartMap =
            new StoredSortedMap(db.getShipmentByPartIndex(),
                          partKeyBinding, shipmentValueBinding, true);
        shipmentBySupplierMap =
            new StoredSortedMap(db.getShipmentBySupplierIndex(),
                          supplierKeyBinding, shipmentValueBinding, true);
        supplierByCityMap =
            new StoredSortedMap(db.getSupplierByCityIndex(),
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
    public StoredSortedMap getPartMap() {

        return partMap;
    }

    /**
     * Return a map view of the supplier storage container.
     */
    public StoredSortedMap getSupplierMap() {

        return supplierMap;
    }

    /**
     * Return a map view of the shipment storage container.
     */
    public StoredSortedMap getShipmentMap() {

        return shipmentMap;
    }

    /**
     * Return an entity set view of the part storage container.
     */
    public StoredSortedValueSet getPartSet() {

        return (StoredSortedValueSet) partMap.values();
    }

    /**
     * Return an entity set view of the supplier storage container.
     */
    public StoredSortedValueSet getSupplierSet() {

        return (StoredSortedValueSet) supplierMap.values();
    }

    /**
     * Return an entity set view of the shipment storage container.
     */
    public StoredSortedValueSet getShipmentSet() {

        return (StoredSortedValueSet) shipmentMap.values();
    }

    /**
     * Return a map view of the shipment-by-part index.
     */
    public StoredSortedMap getShipmentByPartMap() {

        return shipmentByPartMap;
    }

    /**
     * Return a map view of the shipment-by-supplier index.
     */
    public StoredSortedMap getShipmentBySupplierMap() {

        return shipmentBySupplierMap;
    }

    /**
     * Return a map view of the supplier-by-city index.
     */
    public final StoredSortedMap getSupplierByCityMap() {

        return supplierByCityMap;
    }

    /**
     * PartKeyBinding is used to bind the stored key tuple data for a part to
     * a key object representation.
     */
    private static class PartKeyBinding extends TupleBinding {

        /**
         * Construct the binding object with the key tuple data format.
         */
        private PartKeyBinding(TupleFormat format) {

            super(format);
        }

        /**
         * Create the key object from the stored key tuple data.
         */
        public Object dataToObject(TupleInput input)
            throws IOException {

            String number = input.readString();
            return new PartKey(number);
        }

        /**
         * Create the stored key tuple data from the key object.
         */
        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            PartKey key = (PartKey) object;
            output.writeString(key.getNumber());
        }
    }

    /**
     * PartBinding is used to bind the stored key/value data pair for a part
     * to a combined value object (entity).
     */
    private static class PartBinding extends TupleSerialBinding {

        /**
         * Construct the binding object.
         * @param keyFormat is the stored data key format.
         * @param valueFormat is the stored data value format.
         */
        private PartBinding(TupleFormat keyFormat,
                            SerialFormat valueFormat) {

            super(keyFormat, valueFormat);
        }

        /**
         * Create the entity by combining the stored key and value.
         */
        public Object dataToObject(TupleInput keyInput, Object valueInput)
            throws IOException {

            String number = keyInput.readString();
            PartValue value = (PartValue) valueInput;
            return new Part(number, value.getName(), value.getColor(),
                            value.getWeight(), value.getCity());
        }

        /**
         * Create the stored key from the entity.
         */
        public void objectToKey(Object object, TupleOutput output)
            throws IOException {

            Part part = (Part) object;
            output.writeString(part.getNumber());
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
     * SupplierKeyBinding is used to bind the stored key tuple data for a
     * supplier to a key object representation.
     */
    private static class SupplierKeyBinding extends TupleBinding {

        /**
         * Construct the binding object with the key tuple data format.
         */
        private SupplierKeyBinding(TupleFormat format) {

            super(format);
        }

        /**
         * Create the key object from the stored key tuple data.
         */
        public Object dataToObject(TupleInput input)
            throws IOException {

            String number = input.readString();
            return new SupplierKey(number);
        }

        /**
         * Create the stored key tuple data from the key object.
         */
        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            SupplierKey key = (SupplierKey) object;
            output.writeString(key.getNumber());
        }
    }

    /**
     * SupplierBinding is used to bind the stored key/value data pair for a
     * supplier to a combined value object (entity).
     */
    private static class SupplierBinding extends TupleSerialBinding {

        /**
         * Construct the binding object.
         * @param keyFormat is the stored data key format.
         * @param valueFormat is the stored data value format.
         */
        private SupplierBinding(TupleFormat keyFormat,
                                SerialFormat valueFormat) {

            super(keyFormat, valueFormat);
        }

        /**
         * Create the entity by combining the stored key and value.
         */
        public Object dataToObject(TupleInput keyInput, Object valueInput)
            throws IOException {

            String number = keyInput.readString();
            SupplierValue value = (SupplierValue) valueInput;
            return new Supplier(number, value.getName(),
                                value.getStatus(), value.getCity());
        }

        /**
         * Create the stored key from the entity.
         */
        public void objectToKey(Object object, TupleOutput output)
            throws IOException {

            Supplier supplier = (Supplier) object;
            output.writeString(supplier.getNumber());
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
     * ShipmentKeyBinding is used to bind the stored key tuple data for a
     * shipment to a key object representation.
     */
    private static class ShipmentKeyBinding extends TupleBinding {

        /**
         * Construct the binding object with the key tuple data format.
         */
        private ShipmentKeyBinding(TupleFormat format) {

            super(format);
        }

        /**
         * Create the key object from the stored key tuple data.
         */
        public Object dataToObject(TupleInput input)
            throws IOException {

            String partNumber = input.readString();
            String supplierNumber = input.readString();
            return new ShipmentKey(partNumber, supplierNumber);
        }

        /**
         * Create the stored key tuple data from the key object.
         */
        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            ShipmentKey key = (ShipmentKey) object;
            output.writeString(key.getPartNumber());
            output.writeString(key.getSupplierNumber());
        }
    }

    /**
     * ShipmentBinding is used to bind the stored key/value data pair for a
     * shipment to a combined value object (entity).
     */
    private static class ShipmentBinding extends TupleSerialBinding {

        /**
         * Construct the binding object.
         * @param keyFormat is the stored data key format.
         * @param valueFormat is the stored data value format.
         */
        private ShipmentBinding(TupleFormat keyFormat,
                                SerialFormat valueFormat) {

            super(keyFormat, valueFormat);
        }

        /**
         * Create the entity by combining the stored key and value.
         */
        public Object dataToObject(TupleInput keyInput, Object valueInput)
            throws IOException {

            String partNumber = keyInput.readString();
            String supplierNumber = keyInput.readString();
            ShipmentValue value = (ShipmentValue) valueInput;
            return new Shipment(partNumber, supplierNumber,
                                value.getQuantity());
        }

        /**
         * Create the stored key from the entity.
         */
        public void objectToKey(Object object, TupleOutput output)
            throws IOException {

            Shipment shipment = (Shipment) object;
            output.writeString(shipment.getPartNumber());
            output.writeString(shipment.getSupplierNumber());
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
