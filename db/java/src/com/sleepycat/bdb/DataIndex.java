/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataIndex.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.KeyExtractor;
import com.sleepycat.db.Db;
import com.sleepycat.db.Dbc;
import com.sleepycat.db.DbException;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * Represents a Berkeley DB secondary index.  An index is always attached to a
 * single {@link DataStore} when it is constructed.  An index is typically
 * accessed by passing it to the constructor of one of the collection classes
 * in the {@link com.sleepycat.bdb.collection} package.  For example:

 * <pre>
 * Db db = new Db(env, 0);
 * db.setFlags(Db.DB_DUPSORT);
 * db.open(null, "index.db", null, Db.DB_BTREE, dbOpenFlags, 0);
 * DataIndex index = new DataIndex(store, db, keyFormat, keyExtractor);
 * StoredMap map = new StoredMap(index, keyBinding, valueBinding, writeAllowed);
 * </pre>
 *
 * <p>All access methods may be used with BDB.  However, some access methods
 * may only be used with certain types of collection views, and some access
 * methods impose restrictions on the way collection views are used.</p>
 *
 * @author Mark Hayes
 */
public class DataIndex {

    DataDb db;
    DataStore store;
    DataFormat keyFormat;
    KeyExtractor keyExtractor;

    /**
     * Creates an index from a previously opened Db object.
     *
     * @param store the store to be indexed and also specifies the
     * environment that was used to create the Db object.
     *
     * @param db the previously opened Db object.
     *
     * @param keyFormat the data format for keys.
     *
     * @param keyExtractor an object for extracting the index key from primary
     * key and/or value buffers, and for clearing the index key in a value
     * buffer.
     *
     * @throws IllegalArgumentException if a format mismatch is detected
     * between the index and the store, or if unsorted duplicates were
     * specified for the index Db.
     */
    public DataIndex(DataStore store, Db db,
                     DataFormat keyFormat, KeyExtractor keyExtractor) {

        this.store = store;
        this.keyFormat = keyFormat;
        this.keyExtractor = keyExtractor;

        this.db = new DataDb(db);

        if (store.db.isTransactional() != this.db.isTransactional()) {
            throw new IllegalArgumentException(
                this.db + " and " + store.db +
                " must must both be transactional or non-transactional");
        }

        if (this.db.areDuplicatesAllowed() &&
            !this.db.areDuplicatesOrdered()) {
            throw new IllegalArgumentException(
                this.db + " must use sorted duplicates for index");
        }
        if (!keyFormat.equals(keyExtractor.getIndexKeyFormat())) {
            throw new IllegalArgumentException(
                this.db + " extractor index key format mismatch");
        }
        if (keyExtractor.getPrimaryKeyFormat() != null &&
            !store.keyFormat.equals(keyExtractor.getPrimaryKeyFormat())) {
            throw new IllegalArgumentException(
                this.db + " extractor primary key format mismatch");
        }
        if (keyExtractor.getValueFormat() != null &&
            !store.valueFormat.equals(keyExtractor.getValueFormat())) {
            throw new IllegalArgumentException(
                this.db + " extractor value format mismatch");
        }

        if (keyFormat instanceof RecordNumberFormat &&
            !this.db.hasRecNumAccess()) {
            throw new IllegalArgumentException(
                this.db + " RecordNumberFormat is only allowed when the" +
                       " access method has record number keys");
        }

        store.addIndex(this);
    }

    /**
     * Returns the store associated with this index.
     */
    public final DataStore getStore() {

        return store;
    }

    /**
     * Returns the key format associated with this index.
     */
    public final DataFormat getKeyFormat() {

        return keyFormat;
    }

    /**
     * Returns the key extractor associated with this index.
     */
    public final KeyExtractor getKeyExtractor() {

        return keyExtractor;
    }

    /**
     * Returns a printable string identifying the file and database name
     * of the index.
     */
    public String toString() {

        return db.toString();
    }

    void applyChange(DataThang keyThang,
                     DataThang oldValueThang,
                     DataThang newValueThang)
        throws DbException, IOException {

        DataThang oldIndexKey = null;
        if (oldValueThang != null) {
            oldIndexKey = new DataThang();
            keyExtractor.extractIndexKey(
                            (keyExtractor.getPrimaryKeyFormat() != null)
                                ? keyThang : null,
                            (keyExtractor.getValueFormat() != null)
                                ? oldValueThang : null,
                            oldIndexKey);
            if (oldIndexKey.getDataLength() == 0) oldIndexKey = null;
        }
        DataThang newIndexKey = null;
        if (newValueThang != null) {
            newIndexKey = new DataThang();
            keyExtractor.extractIndexKey(
                            (keyExtractor.getPrimaryKeyFormat() != null)
                                ? keyThang : null,
                            (keyExtractor.getValueFormat() != null)
                                ? newValueThang : null,
                            newIndexKey);
            if (newIndexKey.getDataLength() == 0) newIndexKey = null;
        }
        if (oldIndexKey == null && newIndexKey == null) {
            return;
        }
        if (oldIndexKey != null && newIndexKey != null &&
            oldIndexKey.compareTo(newIndexKey) == 0) {
            return;
        }
        if (oldIndexKey != null) {
            // deleteete old index entry
            applyIndexDelete(keyThang, oldIndexKey);
        }
        if (newIndexKey != null) {
            // insert new index entry
            applyIndexInsert(keyThang, newIndexKey);
        }
    }

    void applyIndexDelete(DataThang keyThang, DataThang oldIndexKey)
        throws DbException, IOException {

        Dbc cursor = db.openCursor(true);
        try {
            int err = cursor.get(oldIndexKey, keyThang,
                                 Db.DB_GET_BOTH |
                                 store.db.env.getWriteLockFlag());
            if (err == 0) {
                cursor.delete(0);
            } else {
                throw new IntegrityConstraintException(
                    "Index entry not found");
            }
        } finally {
            db.closeCursor(cursor);
        }
    }

    void applyIndexInsert(DataThang keyThang, DataThang newIndexKey)
        throws DbException, IOException {

        int err = db.put(newIndexKey, keyThang, Db.DB_NODUPDATA);
        if (err != 0) {
            throw new IntegrityConstraintException(
                "Index entry already exists");
        }
    }
}
