/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SampleViews.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.marshal;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.serial.TupleSerialBinding;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import com.sleepycat.bdb.bind.tuple.TupleBinding;
import com.sleepycat.bdb.collection.StoredSortedValueSet;
import com.sleepycat.bdb.collection.StoredSortedMap;
import com.sleepycat.bdb.util.IOExceptionWrapper;
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
        // key/value data pair to a combined value object; a "tricky" binding
        // that uses transient fields is used--see PartBinding, etc, for
        // details.  For keys, a one-to-one binding is implemented with
        // DataBinding classes to bind the stored tuple data to a key Object.
        //
        DataBinding partKeyBinding =
            new MarshalledKeyBinding(db.getPartKeyFormat(), PartKey.class);
        EntityBinding partValueBinding =
            new MarshalledEntityBinding(db.getPartKeyFormat(),
                                        db.getPartValueFormat(),
                                        Part.class);
        DataBinding supplierKeyBinding =
            new MarshalledKeyBinding(db.getSupplierKeyFormat(),
                                     SupplierKey.class);
        EntityBinding supplierValueBinding =
            new MarshalledEntityBinding(db.getSupplierKeyFormat(),
                                        db.getSupplierValueFormat(),
                                        Supplier.class);
        DataBinding shipmentKeyBinding =
            new MarshalledKeyBinding(db.getShipmentKeyFormat(),
                                     ShipmentKey.class);
        EntityBinding shipmentValueBinding =
            new MarshalledEntityBinding(db.getShipmentKeyFormat(),
                                        db.getShipmentValueFormat(),
                                        Shipment.class);
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
     * MarshalledKeyBinding is used to bind the stored key tuple data to a key
     * object representation.  To do this, it calls the MarshalledKey interface
     * implemented by the key class.
     */
    private static class MarshalledKeyBinding extends TupleBinding {

        private Class keyClass;

        /**
         * Construct the binding object with the key tuple data format.
         */
        private MarshalledKeyBinding(TupleFormat format, Class keyClass) {

            super(format);

            // The key class will be used to instantiate the key object.
            //
            if (!MarshalledKey.class.isAssignableFrom(keyClass)) {
                throw new IllegalArgumentException(keyClass.toString() +
                            " does not implement MarshalledKey");
            }
            this.keyClass = keyClass;
        }

        /**
         * Create the key object from the stored key tuple data.
         */
        public Object dataToObject(TupleInput input)
            throws IOException {

            try {
                MarshalledKey key = (MarshalledKey) keyClass.newInstance();
                key.unmarshalKey(input);
                return key;
            } catch (IllegalAccessException e) {
                throw new IOExceptionWrapper(e);
            } catch (InstantiationException e) {
                throw new IOExceptionWrapper(e);
            }
        }

        /**
         * Create the stored key tuple data from the key object.
         */
        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            MarshalledKey key = (MarshalledKey) object;
            key.marshalKey(output);
        }
    }

    /**
     * MarshalledEntityBinding is used to bind the stored key/value data pair
     * to a combined to an entity object representation.  To do this, it calls
     * the MarshalledEntity interface implemented by the entity class.
     *
     * <p> The binding is "tricky" in that it uses the entity class for both the
     * stored data value and the combined entity object.  To do this, entity's
     * key field(s) are transient and are set by the binding after the data
     * object has been deserialized. This avoids the use of a "value" class
     * completely. </p>
     */
    private static class MarshalledEntityBinding extends TupleSerialBinding {

        private Class entityClass;

        /**
         * Construct the binding object.
         * @param keyFormat is the stored data key format.
         * @param valueFormat is the stored data value format.
         */
        private MarshalledEntityBinding(TupleFormat keyFormat,
                                        SerialFormat valueFormat,
                                        Class entityClass) {

            super(keyFormat, valueFormat);

            // The entity class will be used to instantiate the entity object.
            //
            if (!MarshalledEntity.class.isAssignableFrom(entityClass)) {
                throw new IllegalArgumentException(entityClass.toString() +
                            " does not implement MarshalledEntity");
            }
            this.entityClass = entityClass;
        }

        /**
         * Create the entity by combining the stored key and value.
         * This "tricky" binding returns the stored value as the entity, but
         * first it sets the transient key fields from the stored key.
         */
        public Object dataToObject(TupleInput tupleInput, Object javaInput)
            throws IOException {

            MarshalledEntity entity = (MarshalledEntity) javaInput;
            entity.unmarshalPrimaryKey(tupleInput);
            return entity;
        }

        /**
         * Create the stored key from the entity.
         */
        public void objectToKey(Object object, TupleOutput output)
            throws IOException {

            MarshalledEntity entity = (MarshalledEntity) object;
            entity.marshalPrimaryKey(output);
        }

        /**
         * Return the entity as the stored value.  There is nothing to do here
         * since the entity's key fields are transient.
         */
        public Object objectToValue(Object object)
            throws IOException {

            return object;
        }
    }
}
