/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredKeySet.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import com.sleepycat.bdb.DataCursor;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.DataView;
import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbException;
import java.io.IOException;
import java.util.Set;

/**
 * The Set returned by Map.keySet() and which can also be constructed directly
 * if a Map is not needed.
 * Since this collection is a set it only contains one element for each key,
 * even when duplicates are allowed.  Key set iterators are therefore
 * particularly useful for enumerating the unique keys of a store or index that
 * allows duplicates.
 *
 * @author Mark Hayes
 */
public class StoredKeySet extends StoredCollection implements Set {

    /**
     * Creates a key set view of a {@link DataStore}.
     *
     * @param store is the DataStore underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredKeySet(DataStore store, DataBinding keyBinding,
                        boolean writeAllowed) {

        super(new DataView(store, null, keyBinding, null,
                           null, writeAllowed));
    }

    /**
     * Creates a key set view of a {@link DataIndex}.
     *
     * @param index is the DataIndex underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredKeySet(DataIndex index, DataBinding keyBinding,
                        boolean writeAllowed) {

        super(new DataView(null, index, keyBinding, null,
                           null, writeAllowed));
    }

    StoredKeySet(DataView keySetView) {

        super(keySetView);
    }

    /**
     * Adds the specified key to this set if it is not already present
     * (optional operation).
     * When a key is added the value in the underlying data store will be
     * empty.
     * This method conforms to the {@link Set#add} interface.
     *
     * @throws UnsupportedOperationException if the collection is indexed, or
     * if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean add(Object key) {

        boolean doAutoCommit = beginAutoCommit();
        try {
            int err = view.put(key, null, Db.DB_NOOVERWRITE, null);
            commitAutoCommit(doAutoCommit);
            return (err == 0);
        } catch (Exception e) {
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Removes the specified key from this set if it is present (optional
     * operation).
     * If duplicates are allowed, this method removes all duplicates for the
     * given key.
     * This method conforms to the {@link Set#remove} interface.
     *
     * @throws UnsupportedOperationException if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean remove(Object key) {

        return removeKey(key, null);
    }

    /**
     * Returns true if this set contains the specified key.
     * This method conforms to the {@link Set#contains} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean contains(Object key) {

        return containsKey(key);
    }

    boolean hasValues() {

        return false;
    }

    Object makeIteratorData(StoredIterator iterator, DataCursor cursor)
        throws DbException, IOException {

        return cursor.getCurrentKey();
    }

    boolean iterateDuplicates() {

        return false;
    }
}
