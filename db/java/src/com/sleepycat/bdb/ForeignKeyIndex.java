/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: ForeignKeyIndex.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.KeyExtractor;
import com.sleepycat.db.Db;
import com.sleepycat.db.Dbc;
import com.sleepycat.db.DbTxn;
import com.sleepycat.db.DbException;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * Represents a Berkeley DB secondary index where the index key is the primary
 * key of another data store.  An index is always attached to a single {@link
 * DataStore} when it is constructed.  An index is typically accessed by
 * passing it to the constructor of one of the collection classes in the {@link
 * com.sleepycat.bdb.collection} package.  For example:
 *
 * <pre>
 * Db db = new Db(env, 0);
 * db.setFlags(Db.DB_DUPSORT);
 * db.open(null, "index.db", null, Db.DB_BTREE, dbOpenFlags, 0);
 * ForeignKeyIndex index = new ForeignKeyIndex(store, db, keyExtractor,
 *                                             foreignStore, deleteAction);
 * StoredMap map = new StoredMap(index, keyBinding, valueBinding, writeAllowed);
 * </pre>
 *
 * <p>All access methods may be used with BDB.  However, some access methods
 * may only be used with certain types of collection views, and some access
 * methods impose restrictions on the way collection views are used.</p>
 *
 * @author Mark Hayes
 */
public class ForeignKeyIndex extends DataIndex {

    /**
     * When the foreign key is deleted, throw an exception.
     */
    public static final int ON_DELETE_ABORT = 0;

    /**
     * When the foreign key is deleted, delete the index key.
     */
    public static final int ON_DELETE_CASCADE = 1;

    /**
     * When the foreign key is deleted, clear the index key.
     */
    public static final int ON_DELETE_CLEAR = 2;

    DataStore foreignStore;
    int deleteAction;

    /**
     * Creates a foreign key index from a previously opened Db object.
     *
     * @param store the store to be indexed and also specifies the
     * environment that was used to create the Db object.
     *
     * @param db the previously opened Db object.
     *
     * @param keyExtractor an object for extracting the index key from primary
     * key and/or value buffers, and for clearing the index key in a value
     * buffer.
     *
     * @param foreignStore is the store in which the index key for this store
     * is a primary key.
     *
     * @param deleteAction determines what action occurs when the foreign key
     * is deleted. It must be one of the ON_DELETE_ constants.
     *
     * @throws IllegalArgumentException if a format mismatch is detected
     * between the index and the store, or if unsorted duplicates were
     * specified for the index Db.
     */
    public ForeignKeyIndex(DataStore store, Db db, KeyExtractor keyExtractor,
                           DataStore foreignStore, int deleteAction) {

        super(store, db, foreignStore.keyFormat, keyExtractor);
        this.foreignStore = foreignStore;
        this.deleteAction = deleteAction;

        foreignStore.addInverseIndex(this);

        if (deleteAction == ON_DELETE_CLEAR &&
            keyExtractor.getPrimaryKeyFormat() != null) {
            throw new IllegalArgumentException(
                "ON_DELETE_CLEAR cannot be used with key extractor that " +
                "requires primary key");
        }
    }

    /**
     * Returns the foreign store which has the primary key which matches the
     * index key of this store.
     */
    public final DataStore getForeignStore() {

        return foreignStore;
    }

    /**
     * Returns a value indicating what action occurs when the foreign key
     * is deleted. It must be one of the ON_DELETE_ constants.
     */
    public final int getDeleteAction() {

        return deleteAction;
    }

    final void applyIndexInsert(DataThang keyThang, DataThang newIndexKey)
        throws DbException, IOException {

        // check relative key (newIndexKey) existence

        int err = foreignStore.db.get(newIndexKey,
                                      DataThang.getDiscardDataThang(), 0);
        if (err != 0) {
            throw new IllegalArgumentException(
                        "Integrity error inserting in " +
                        db + ", index key not found in " +
                        foreignStore.db);
        }

        // must call super method to perform index insert

        super.applyIndexInsert(keyThang, newIndexKey);
    }

    final void applyForeignDelete(DataThang foreignPrimaryKeyThang)
        throws DbException, IOException {

        DataEnvironment env = store.db.env;
        DataThang primaryKeyThang = new DataThang();
        int err = db.get(foreignPrimaryKeyThang, primaryKeyThang, 0);
        if (err == 0) {
            switch (deleteAction) {
                case ON_DELETE_ABORT: {
                    throw new IntegrityConstraintException(
                                    "ON_DELETE_ABORT: deletion not allowed");
                }
                case ON_DELETE_CASCADE: {
                    DataThang tempThang = new DataThang();
                    Dbc cursor = store.db.openCursor(true);
                    try {
                        err = cursor.get(primaryKeyThang, tempThang,
                                         Db.DB_SET | env.getWriteLockFlag());
                        if (err == 0) {
                            cursor.delete(0);
                            store.applyChange(primaryKeyThang, tempThang,
                                              null);
                        } else {
                            throw new IntegrityConstraintException(
                               "ON_DELETE_CASCADE: index entry not found");
                        }
                    } finally {
                        store.db.closeCursor(cursor);
                    }
                    break;
                }
                case ON_DELETE_CLEAR: {
                    DataThang valueThang = new DataThang();
                    Dbc cursor = store.db.openCursor(true);
                    try {
                        err = cursor.get(primaryKeyThang, valueThang,
                                         Db.DB_SET | env.getWriteLockFlag());
                        if (err == 0) {
                            keyExtractor.clearIndexKey(
                                (keyExtractor.getValueFormat() != null)
                                ? valueThang : null);
                            cursor.put(primaryKeyThang, valueThang,
                                       Db.DB_CURRENT);
                        } else {
                            throw new IntegrityConstraintException(
                                "ON_DELETE_CLEAR: index entry not found");
                        }
                    } finally {
                        store.db.closeCursor(cursor);
                    }
                    break;
                }
                default: {
                    throw new IllegalArgumentException(
                            "unknown delete action " + deleteAction);
                }
            }
        }
    }
}
