/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SampleDatabase.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.basic;

import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.StoredClassCatalog;
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

    private DbEnv env;
    private DataStore supplierStore;
    private DataStore partStore;
    private DataStore shipmentStore;
    private StoredClassCatalog javaCatalog;
    private SerialFormat partKeyFormat;
    private SerialFormat partValueFormat;
    private SerialFormat supplierKeyFormat;
    private SerialFormat supplierValueFormat;
    private SerialFormat shipmentKeyFormat;
    private SerialFormat shipmentValueFormat;

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

        // Set the Berkeley DB flags for opening all stores and indices.
        //
        int flags = Db.DB_CREATE | Db.DB_AUTO_COMMIT;

        // Create the Serial class catalog.  This holds the serialized class
        // format for all database records of format SerialFormat.
        //
        javaCatalog = new StoredClassCatalog(env, CLASS_CATALOG, null, flags);

        // Create the data formats.  In this example, all formats are
        // SerialFormat.
        //
        partKeyFormat = new SerialFormat(javaCatalog, PartKey.class);
        partValueFormat = new SerialFormat(javaCatalog, PartValue.class);
        supplierKeyFormat = new SerialFormat(javaCatalog, SupplierKey.class);
        supplierValueFormat =
                new SerialFormat(javaCatalog, SupplierValue.class);
        shipmentKeyFormat = new SerialFormat(javaCatalog, ShipmentKey.class);
        shipmentValueFormat =
            new SerialFormat(javaCatalog, ShipmentValue.class);

        // Open the Berkeley DB database, along with the associated
        // DataStore, for the part, supplier and shipment stores.
        // In this sample, the stores are opened with the DB_BTREE access
        // method and no duplicate keys allowed.  Although the DB_BTREE method
        // provides ordered keys, the ordering of serialized key objects
        // is not very useful. Duplicate keys are not allowed for any entity
        // with indexes or foreign key relationships.
        //
        Db partDb = new Db(env, 0);
        partDb.open(null, PART_STORE, null, Db.DB_BTREE, flags, 0);
        partStore = new DataStore(partDb, partKeyFormat, partValueFormat,
                                  null);

        Db supplierDb = new Db(env, 0);
        supplierDb.open(null, SUPPLIER_STORE, null, Db.DB_BTREE, flags, 0);
        supplierStore = new DataStore(supplierDb, supplierKeyFormat,
                                      supplierValueFormat, null);

        Db shipmentDb = new Db(env, 0);
        shipmentDb.open(null, SHIPMENT_STORE, null, Db.DB_BTREE, flags, 0);
        shipmentStore = new DataStore(shipmentDb, shipmentKeyFormat,
                                      shipmentValueFormat, null);
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
    public final SerialFormat getPartKeyFormat() {

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
    public final SerialFormat getSupplierKeyFormat() {

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
    public final SerialFormat getShipmentKeyFormat() {

        return shipmentKeyFormat;
    }

    /**
     * Return the DataFormat of the shipment value.
     */
    public final SerialFormat getShipmentValueFormat() {

        return shipmentValueFormat;
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
}
