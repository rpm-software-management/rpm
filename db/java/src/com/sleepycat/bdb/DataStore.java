/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataStore.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;

/**
 * Represents a Berkeley DB database in the role of a primary data store.  A
 * store may be used by itself or along with one or more {@link DataIndex}
 * objects.  A store is typically accessed by passing it to the constructor of
 * one of the collection classes in the {@link com.sleepycat.bdb.collection}
 * package.  For example:
 *
 * <pre>
 * Db db = new Db(env, 0);
 * db.open(null, "store.db", null, Db.DB_BTREE, dbOpenFlags, 0);
 * DataStore store = new DataStore(db, keyFormat, valueFormat, keyAssigner);
 * StoredMap map = new StoredMap(store, keyBinding, valueBinding, writeAllowed);
 * </pre>
 *
 * <p>All access methods may be used with BDB.  However, some access methods
 * may only be used with certain types of collection views, and some access
 * methods impose restrictions on the way collection views are used.</p>
 *
 * <p>A store is always associated with the environment of its underlying
 * database, which is the first parameter to the {Db#Db} constructor.  There
 * are three types of environments in Berkeley DB.</p>
 * <p>
 * <table border="1">
 * <tr>
 *   <th>Environment</th>
 *   <th>Access Mode</th>
 *   <th>Berkeley DB Flags</th>
 * </tr>
 * <tr>
 *   <td>Data Store</td>
 *   <td>single-threaded access</td>
 *   <td>Db.DB_INIT_MPOOL</td>
 * </tr>
 * <tr>
 *   <td>Concurrent Data Store</td>
 *   <td>single-writer multiple-reader access</td>
 *   <td>Db.DB_INIT_CDB | Db.DB_INIT_MPOOL</td>
 * </tr>
 * <tr>
 *   <td>Transactional Data Store</td>
 *   <td>transactional access for any number of readers and writers</td>
 *   <td>Db.DB_INIT_TXN | Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL</td>
 * </tr>
 * </table>
 *
 * <p>The flags shown are the minimum required for creating the Berkeley DB
 * environment. Many other Berkeley DB options are also available.  For details
 * on creating and configuring the environment see the Berkeley DB
 * documentation.</p>
 *
 * <p>All three environments may be used within BDB.  However, the Concurrent
 * Data Store Environment imposes the restriction that only one writable cursor
 * may be open at a time.  This means that if you have a writable iterator for
 * a data store open, then you cannot obtain another writable iterator for the
 * same data store and you cannot perform other write operations through a
 * collection for that data store (since this also uses a write cursor).</p>
 *
 * @author Mark Hayes
 */
public class DataStore {

    DataDb db;
    DataFormat keyFormat;
    DataFormat valueFormat;
    PrimaryKeyAssigner keyAssigner;
    ArrayList indices;
    ArrayList inverseIndices;

    /**
     * Creates a store from a previously opened Db object.
     *
     * @param db the previously opened Db object.
     *
     * @param keyFormat the data format for keys.
     *
     * @param valueFormat the data format for values.
     *
     * @param keyAssigner an object for assigning keys or null if no automatic
     * key assignment is used.
     */
    public DataStore(Db db, DataFormat keyFormat, DataFormat valueFormat,
                     PrimaryKeyAssigner keyAssigner) {

        this.db = new DataDb(db);
        this.keyFormat = keyFormat;
        this.valueFormat = valueFormat;
        this.keyAssigner = keyAssigner;

        if (keyFormat instanceof RecordNumberFormat &&
            !this.db.hasRecNumAccess()) {
            throw new IllegalArgumentException(this.db.toString() +
                " RecordNumberFormat is only allowed when the" +
                " access method has record number keys");
        }

        if (valueFormat instanceof RecordNumberFormat) {
            throw new IllegalArgumentException(this.db.toString() +
                " RecordNumberFormat is only allowed for keys");
        }
    }

    /**
     * Closes the store and all associated indices.
     */
    public void close()
        throws DbException {

        db.close();

        if (indices != null) {
            for (int i = 0; i < indices.size(); i++) {
                DataIndex index = (DataIndex) indices.get(i);
                index.db.close();
            }
        }
    }

    /**
     * Returns the environment associated with this store.
     */
    public final DbEnv getEnv() {

        return db.env.getEnv();
    }

    /**
     * Returns the key format associated with this store.
     */
    public final DataFormat getKeyFormat() {

        return keyFormat;
    }

    /**
     * Returns the key assigner associated with this store.
     */
    public final PrimaryKeyAssigner getKeyAssigner() {

        return keyAssigner;
    }

    /**
     * Returns the value format associated with this store.
     */
    public final DataFormat getValueFormat() {

        return valueFormat;
    }

    /**
     * Returns the indices associated with this store.  Indices are associated
     * with a store when they are constructed.  All objects returned by the
     * iterator will be of class {@link DataIndex} and may also be of class
     * {@link ForeignKeyIndex}.
     *
     * @return an iterator of associated indices or null if there are none.
     */
    public final Iterator getIndices() {

        return (indices != null) ?  indices.iterator() : null;
    }

    /**
     * Returns a printable string identifying the filename and datbase name
     * of the store.
     */
    public String toString() {

        return db.toString();
    }

    final void addIndex(DataIndex index) {

        if (db.areDuplicatesAllowed()) {
            throw new IllegalArgumentException(
                "The primary store of an index must now allow duplicates");
        }
        if (indices == null) {
            indices = new ArrayList();
        }
        indices.add(index);
    }

    final void addInverseIndex(ForeignKeyIndex inverseIndex) {

        if (inverseIndices == null) {
            inverseIndices = new ArrayList();
        }
        inverseIndices.add(inverseIndex);
    }

    void applyChange(DataThang keyThang, DataThang oldValueThang,
                     DataThang newValueThang)
        throws DbException, IOException {

        if (newValueThang == null && inverseIndices != null) {
            for (int i = 0; i < inverseIndices.size(); i += 1) {
                ForeignKeyIndex index =
                    (ForeignKeyIndex) inverseIndices.get(i);
                index.applyForeignDelete(keyThang);
            }
        }

        if (indices != null) {
            for (int i = 0; i < indices.size(); i++) {
                DataIndex index = (DataIndex) indices.get(i);
                index.applyChange(keyThang, oldValueThang, newValueThang);
            }
        }
    }
}
