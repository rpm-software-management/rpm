/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SampleDatabase.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.marshal;

import com.sleepycat.bdb.bind.KeyExtractor;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.bdb.bind.serial.TupleSerialKeyExtractor;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import com.sleepycat.bdb.ForeignKeyIndex;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.StoredClassCatalog;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * SampleDatabase defines the storage containers, indices and foreign keys
 * for the sample database.
 *
 * @author Mark Hayes
 */
public class SampleDatabase {

    private static final String CLASS_CATALOG = "java_class_catalog";
    private static final String SUPPLIER_STORE = "supplier_store";
    private static final String PART_STORE = "part_store";
    private static final String SHIPMENT_STORE = "shipment_store";
    private static final String SHIPMENT_PART_INDEX = "shipment_part_index";
    private static final String SHIPMENT_SUPPLIER_INDEX =
                                    "shipment_supplier_index";
    private static final String SUPPLIER_CITY_INDEX = "supplier_city_index";

    private DbEnv env;
    private DataStore supplierStore;
    private DataStore partStore;
    private DataStore shipmentStore;
    private DataIndex supplierByCityIndex;
    private ForeignKeyIndex shipmentByPartIndex;
    private ForeignKeyIndex shipmentBySupplierIndex;
    private StoredClassCatalog javaCatalog;
    private TupleFormat partKeyFormat;
    private SerialFormat partValueFormat;
    private TupleFormat supplierKeyFormat;
    private SerialFormat supplierValueFormat;
    private TupleFormat shipmentKeyFormat;
    private SerialFormat shipmentValueFormat;
    private TupleFormat cityKeyFormat;

    /**
     * Open all storage containers, indices, and catalogs.
     */
    public SampleDatabase(String homeDirectory, boolean runRecovery)
        throws DbException, FileNotFoundException {

        // Open the Berkeley DB environment in transactional mode.
        //
        int envFlags = Db.DB_INIT_TXN | Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL |
                       Db.DB_CREATE;
        if (runRecovery) envFlags |= Db.DB_RECOVER;
        env = new DbEnv(0);
        System.out.println("Opening environment in: " + homeDirectory);
        env.open(homeDirectory, envFlags, 0);

        // Set the flags for opening all stores and indices.
        //
        int flags = Db.DB_CREATE | Db.DB_AUTO_COMMIT;

        // Create the Serial class catalog.  This holds the serialized class
        // format for all database records of format SerialFormat.
        //
        javaCatalog = new StoredClassCatalog(env, CLASS_CATALOG, null, flags);

        // Create the data formats.  In this example, all key formats are
        // TupleFormat and all value formats are SerialFormat.  Note that
        // entity (combined key/value) classes are used for the value
        // formats--for details see the bindings in the SampleViews class.
        //
        partKeyFormat = new TupleFormat();
        partValueFormat = new SerialFormat(javaCatalog, Part.class);
        supplierKeyFormat = new TupleFormat();
        supplierValueFormat = new SerialFormat(javaCatalog, Supplier.class);
        shipmentKeyFormat = new TupleFormat();
        shipmentValueFormat = new SerialFormat(javaCatalog, Shipment.class);
        cityKeyFormat = new TupleFormat();

        // Open the Berkeley DB database, along with the associated
        // DataStore, for the part, supplier and shipment stores.
        // In this sample, the stores are opened with the DB_BTREE access
        // method and no duplicate keys allowed.  The DB_BTREE method is used
        // to provide ordered keys, since ordering is meaningful when the tuple
        // data format is used.  Duplicate keys are not allowed for any entity
        // with indexes or foreign key relationships.
        //
        Db partDb = new Db(env, 0);
        partDb.open(null, PART_STORE, null, Db.DB_BTREE, flags, 0);
        partStore = new DataStore(partDb, partKeyFormat,
                                  partValueFormat, null);

        Db supplierDb = new Db(env, 0);
        supplierDb.open(null, SUPPLIER_STORE, null, Db.DB_BTREE, flags, 0);
        supplierStore = new DataStore(supplierDb, supplierKeyFormat,
                                      supplierValueFormat, null);

        Db shipmentDb = new Db(env, 0);
        shipmentDb.open(null, SHIPMENT_STORE, null, Db.DB_BTREE, flags, 0);
        shipmentStore = new DataStore(shipmentDb, shipmentKeyFormat,
                                      shipmentValueFormat, null);

        // Create the KeyExtractor objects for the part and supplier
        // indices of the shipment store.  Each key extractor object defines
        // its associated index, since it is responsible for mapping between
        // the indexed value and the index key.
        //
        KeyExtractor cityExtractor = new MarshalledKeyExtractor(
                                                    supplierKeyFormat,
                                                    supplierValueFormat,
                                                    cityKeyFormat,
                                                    Supplier.CITY_KEY);
        KeyExtractor partExtractor = new MarshalledKeyExtractor(
                                                    shipmentKeyFormat,
                                                    shipmentValueFormat,
                                                    partKeyFormat,
                                                    Shipment.PART_KEY);
        KeyExtractor supplierExtractor = new MarshalledKeyExtractor(
                                                    shipmentKeyFormat,
                                                    shipmentValueFormat,
                                                    supplierKeyFormat,
                                                    Shipment.SUPPLIER_KEY);

        // Open the Berkeley DB database, along with the associated
        // ForeignKeyIndex, for the part and supplier indices of the shipment
        // store.
        // In this sample, the indices are opened with the DB_BTREE access
        // method and sorted duplicate keys.  The DB_BTREE method is used to
        // provide ordered keys, since ordering is meaningful when the tuple
        // data format is used.  Duplicate keys are allowed since more than one
        // shipment may exist for the same supplier or part. For indices, if
        // duplicates are allowed they should always be sorted to allow for
        // efficient joins.
        //
        Db cityIndexDb = new Db(env, 0);
        cityIndexDb.setFlags(Db.DB_DUPSORT);
        cityIndexDb.open(null, SUPPLIER_CITY_INDEX, null, Db.DB_BTREE,
                         flags, 0);
        supplierByCityIndex = new DataIndex(supplierStore, cityIndexDb,
                                            cityKeyFormat, cityExtractor);

        Db partIndexDb = new Db(env, 0);
        partIndexDb.setFlags(Db.DB_DUPSORT);
        partIndexDb.open(null, SHIPMENT_PART_INDEX, null, Db.DB_BTREE,
                         flags, 0);
        shipmentByPartIndex = new ForeignKeyIndex(shipmentStore, partIndexDb,
                                            partExtractor, partStore,
                                            ForeignKeyIndex.ON_DELETE_CASCADE);

        Db supplierIndexDb = new Db(env, 0);
        supplierIndexDb.setFlags(Db.DB_DUPSORT);
        supplierIndexDb.open(null, SHIPMENT_SUPPLIER_INDEX, null, Db.DB_BTREE,
                             flags, 0);
        shipmentBySupplierIndex = new ForeignKeyIndex(shipmentStore,
                                        supplierIndexDb,
                                        supplierExtractor, supplierStore,
                                        ForeignKeyIndex.ON_DELETE_CASCADE);
    }

    /**
     * Return the storage environment for the database.
     */
    public final DbEnv getEnvironment() {

        return env;
    }

    /**
     * Return the DataFormat of the part key.
     */
    public final TupleFormat getPartKeyFormat() {

        return partKeyFormat;
    }

    /**
     * Return the DataFormat of the part value.
     */
    public final SerialFormat getPartValueFormat() {

        return partValueFormat;
    }

    /**
     * Return the DataFormat of the supplier key.
     */
    public final TupleFormat getSupplierKeyFormat() {

        return supplierKeyFormat;
    }

    /**
     * Return the DataFormat of the supplier value.
     */
    public final SerialFormat getSupplierValueFormat() {

        return supplierValueFormat;
    }

    /**
     * Return the DataFormat of the shipment key.
     */
    public final TupleFormat getShipmentKeyFormat() {

        return shipmentKeyFormat;
    }

    /**
     * Return the DataFormat of the shipment value.
     */
    public final SerialFormat getShipmentValueFormat() {

        return shipmentValueFormat;
    }

    /**
     * Return the DataFormat of the city key.
     */
    public final TupleFormat getCityKeyFormat() {

        return cityKeyFormat;
    }

    /**
     * Return the part storage container.
     */
    public final DataStore getPartStore() {

        return partStore;
    }

    /**
     * Return the supplier storage container.
     */
    public final DataStore getSupplierStore() {

        return supplierStore;
    }

    /**
     * Return the shipment storage container.
     */
    public final DataStore getShipmentStore() {

        return shipmentStore;
    }

    /**
     * Return the shipment-by-part index.
     */
    public final ForeignKeyIndex getShipmentByPartIndex() {

        return shipmentByPartIndex;
    }

    /**
     * Return the shipment-by-supplier index.
     */
    public final ForeignKeyIndex getShipmentBySupplierIndex() {

        return shipmentBySupplierIndex;
    }

    /**
     * Return the supplier-by-city index.
     */
    public final DataIndex getSupplierByCityIndex() {

        return supplierByCityIndex;
    }

    /**
     * Close all stores (closing a store automatically closes its indices).
     */
    public void close()
        throws DbException, IOException {

        partStore.close();
        supplierStore.close();
        shipmentStore.close();
        // And don't forget to close the catalog and the environment.
        javaCatalog.close();
        env.close(0);
    }

    /**
     * The KeyExtractor for MarshalledEntity objects.  This is an
     * extension of the abstract class TupleSerialKeyExtractor, which
     * implements KeyExtractor for the case where the data keys are of the
     * format TupleFormat and the data values are of the format
     * SerialFormat.
     */
    private static class MarshalledKeyExtractor
        extends TupleSerialKeyExtractor {

        private String keyName;

        /**
         * Construct the part key extractor.
         * @param primaryKeyFormat is the shipment key format.
         * @param valueFormat is the shipment value format.
         * @param indexKeyFormat is the part key format.
         */
        private MarshalledKeyExtractor(TupleFormat primaryKeyFormat,
                                       SerialFormat valueFormat,
                                       TupleFormat indexKeyFormat,
                                       String keyName) {

            super(primaryKeyFormat, valueFormat, indexKeyFormat);
            this.keyName = keyName;
        }

        /**
         * Extract the city key from a supplier key/value pair.  The city key
         * is stored in the supplier value, so the supplier key is not used.
         */
        public void extractIndexKey(TupleInput primaryKeyInput,
                                    Object valueInput,
                                    TupleOutput indexKeyOutput)
            throws IOException {

            // the primary key is unmarshalled before marshalling the index
            // key, to account for cases where the index key is composed of
            // data elements from the primary key
            MarshalledEntity entity = (MarshalledEntity) valueInput;
            entity.unmarshalPrimaryKey(primaryKeyInput);
            entity.marshalIndexKey(keyName, indexKeyOutput);
        }

        /**
         * This method will never be called since ON_DELETE_CLEAR was not
         * specified when creating the index.
         */
        public void clearIndexKey(Object valueInputOutput)
            throws IOException {

            MarshalledEntity entity = (MarshalledEntity) valueInputOutput;
            entity.clearIndexKey(keyName);
        }
    }
}
