/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: HelloDatabaseWorld.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.helloworld;

import com.sleepycat.bdb.bind.serial.ClassCatalog;
import com.sleepycat.bdb.bind.serial.SerialBinding;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.bdb.bind.tuple.TupleBinding;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.TransactionRunner;
import com.sleepycat.bdb.TransactionWorker;
import com.sleepycat.bdb.collection.StoredIterator;
import com.sleepycat.bdb.collection.StoredSortedMap;
import com.sleepycat.bdb.StoredClassCatalog;
import java.util.Iterator;
import java.util.Map;
import java.util.SortedMap;

/**
 * @author Mark Hayes
 */
public class HelloDatabaseWorld implements TransactionWorker {

    private static final String[] INT_NAMES = {
        "Hello", "Database", "World",
    };
    private static boolean create = true;

    private DbEnv env;
    private ClassCatalog catalog;
    private DataStore store;
    private SortedMap map;

    /** Creates the environment and runs a transaction */
    public static void main(String[] argv)
        throws Exception {

        String dir = "./tmp";

        // environment is transactional
        int envFlags = Db.DB_INIT_TXN | Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL;
        if (create)
            envFlags |= Db.DB_CREATE;
        DbEnv env = new DbEnv(0);
        env.open(dir, envFlags, 0);

        // create the application and run a transaction
        HelloDatabaseWorld worker = new HelloDatabaseWorld(env);
        TransactionRunner runner = new TransactionRunner(env);
        try {
            // open and access the database within a transaction
            runner.run(worker);
        } finally {
            // close the database outside the transaction
            worker.close();
        }
    }

    /** Creates the data store for this application */
    private HelloDatabaseWorld(DbEnv env)
        throws Exception {

        this.env = env;
        open();
    }

    /** Performs work within a transaction. */
    public void doWork()
        throws Exception {

        writeAndRead();
    }

    /** Opens the database and creates the Map. */
    private void open()
        throws Exception {

        int dbFlags = Db.DB_AUTO_COMMIT;
        if (create)
            dbFlags |= Db.DB_CREATE;

        // catalog is needed for serial data format (java serialization)
        catalog = new StoredClassCatalog(env, "catalog.db", null, dbFlags);

        // use Integer tuple format and binding for keys
        TupleFormat keyFormat = new TupleFormat();
        TupleBinding keyBinding =
            TupleBinding.getPrimitiveBinding(Integer.class, keyFormat);

        // use String serial format and binding for values
        SerialFormat valueFormat =
            new SerialFormat(catalog, String.class);
        SerialBinding valueBinding = new SerialBinding(valueFormat);

        // open a BTREE (sorted) data store
        Db db = new Db(env, 0);
        db.open(null, "data.db", null, Db.DB_BTREE, dbFlags, 0);
        this.store = new DataStore(db, keyFormat, valueFormat, null);

        // create a map view of the data store
        this.map = new StoredSortedMap(store, keyBinding, valueBinding, true);
    }

    /** Closes the database. */
    private void close()
        throws Exception {

        if (catalog != null) {
            catalog.close();
            catalog = null;
        }
        if (store != null) {
            store.close();
            store = null;
        }
        if (env != null) {
            env.close(0);
            env = null;
        }
    }

    /** Writes and reads the database via the Map. */
    private void writeAndRead() {

        // check for existing data
        Integer key = new Integer(0);
        String val = (String) map.get(key);
        if (val == null) {
            System.out.println("Writing data");
            // write in reverse order to show that keys are sorted
            for (int i = INT_NAMES.length - 1; i >= 0; i -= 1) {
                map.put(new Integer(i), INT_NAMES[i]);
            }
        }
        // get iterator over map entries
        Iterator iter = map.entrySet().iterator();
        try {
            System.out.println("Reading data");
            while (iter.hasNext()) {
                Map.Entry entry = (Map.Entry) iter.next();
                System.out.println(entry.getKey().toString() + ' ' +
                                   entry.getValue());
            }
        } finally {
            // all database iterators must be closed!!
            StoredIterator.close(iter);
        }
    }
}
