/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SampleDatabase.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.factory;

import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.bdb.ForeignKeyIndex;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.StoredClassCatalog;
import com.sleepycat.bdb.factory.TupleSerialDbFactory;
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
    private TupleSerialDbFactory factory;

    /**
     * Open all storage containers, indices, and catalogs.
     */
    public SampleDatabase(String homeDirectory, boolean runRecovery)
        throws Exception {

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

        // Use the TupleSerialDbFactory for a Serial/Tuple-based database
        // where marshalling interfaces are used.  This opens the Serial
        // class catalog automatically.
        //
        factory = new TupleSerialDbFactory(javaCatalog);

        // Open the Berkeley DB database, along with the associated
        // DataStore, for the part, supplier and shipment stores.
        // In this sample, the stores are opened as sorted and no duplicate
        // keys allowed.  Duplicate keys are not allowed for any entity
        // with indexes or foreign key relationships.
        //
        Db partDb = new Db(env, 0);
        partDb.open(null, PART_STORE, null, Db.DB_BTREE, flags, 0);
        partStore = factory.newDataStore(partDb, Part.class, null);

        Db supplierDb = new Db(env, 0);
        supplierDb.open(null, SUPPLIER_STORE, null, Db.DB_BTREE, flags, 0);
        supplierStore = factory.newDataStore(supplierDb, Supplier.class, null);

        Db shipmentDb = new Db(env, 0);
        shipmentDb.open(null, SHIPMENT_STORE, null, Db.DB_BTREE, flags, 0);
        shipmentStore = factory.newDataStore(shipmentDb, Shipment.class, null);

        // Open the ForeignKeyIndex, along with the associated the Berkeley
        // DB database, for the part and supplier indices of the shipment
        // store.
        // In this sample, the indices are opened with sorted duplicate
        // keys.  Duplicate keys are allowed since more than one shipment
        // may exist for the same supplier or part.
        //
        Db cityIndexDb = new Db(env, 0);
        cityIndexDb.setFlags(Db.DB_DUPSORT);
        cityIndexDb.open(null, SUPPLIER_CITY_INDEX, null, Db.DB_BTREE,
                         flags, 0);
        supplierByCityIndex = factory.newDataIndex(supplierStore,
                                            cityIndexDb, Supplier.CITY_KEY,
                                            false, true);

        Db partIndexDb = new Db(env, 0);
        partIndexDb.setFlags(Db.DB_DUPSORT);
        partIndexDb.open(null, SHIPMENT_PART_INDEX, null, Db.DB_BTREE,
                         flags, 0);
        shipmentByPartIndex = factory.newForeignKeyIndex(shipmentStore,
                                            partIndexDb, Shipment.PART_KEY,
                                            false, true, partStore,
                                            ForeignKeyIndex.ON_DELETE_CASCADE);

        Db supplierIndexDb = new Db(env, 0);
        supplierIndexDb.setFlags(Db.DB_DUPSORT);
        supplierIndexDb.open(null, SHIPMENT_SUPPLIER_INDEX, null, Db.DB_BTREE,
                             flags, 0);
        shipmentBySupplierIndex = factory.newForeignKeyIndex(shipmentStore,
                                            supplierIndexDb,
                                            Shipment.SUPPLIER_KEY,
                                            false, true, supplierStore,
                                            ForeignKeyIndex.ON_DELETE_CASCADE);
    }

    /**
     * Return the simple db factory.
     */
    public final TupleSerialDbFactory getFactory() {

        return factory;
    }

    /**
     * Return the storage environment for the database.
     */
    public final DbEnv getEnvironment() {

        return env;
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
        throws Exception {

        partStore.close();
        supplierStore.close();
        shipmentStore.close();
        // And don't forget to close the catalog and the environment.
        javaCatalog.close();
        env.close(0);
    }
}
