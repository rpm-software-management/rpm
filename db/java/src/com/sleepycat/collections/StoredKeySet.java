/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredKeySet.java,v 1.3 2004/06/04 18:24:50 mark Exp $
 */

package com.sleepycat.collections;

import java.util.Set;

import com.sleepycat.bind.EntryBinding;
import com.sleepycat.db.Database;
import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.OperationStatus;

/**
 * The Set returned by Map.keySet() and which can also be constructed directly
 * if a Map is not needed.
 * Since this collection is a set it only contains one element for each key,
 * even when duplicates are allowed.  Key set iterators are therefore
 * particularly useful for enumerating the unique keys of a store or index that
 * allows duplicates.
 *
 * <p><em>Note that this class does not conform to the standard Java
 * collections interface in the following ways:</em></p>
 * <ul>
 * <li>The {@link #size} method always throws
 * <code>UnsupportedOperationException</code> because, for performance reasons,
 * databases do not maintain their total record count.</li>
 * <li>All iterators must be explicitly closed using {@link
 * StoredIterator#close()} or {@link StoredIterator#close(java.util.Iterator)}
 * to release the underlying database cursor resources.</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public class StoredKeySet extends StoredCollection implements Set {

    /**
     * Creates a key set view of a {@link Database}.
     *
     * @param database is the Database underlying the new collection.
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
     * @throws RuntimeExceptionWrapper if a {@link DatabaseException} is
     * thrown.
     */
    public StoredKeySet(Database database, EntryBinding keyBinding,
                        boolean writeAllowed) {

        super(new DataView(database, keyBinding, null, null,
                           writeAllowed, null));
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
     * @throws RuntimeExceptionWrapper if a {@link DatabaseException} is
     * thrown.
     */
    public boolean add(Object key) {

        DataCursor cursor = null;
        boolean doAutoCommit = beginAutoCommit();
        try {
            cursor = new DataCursor(view, true);
            OperationStatus status = cursor.putNoOverwrite(key, null, false);
            closeCursor(cursor);
            commitAutoCommit(doAutoCommit);
            return (status == OperationStatus.SUCCESS);
        } catch (Exception e) {
            closeCursor(cursor);
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
     * @throws RuntimeExceptionWrapper if a {@link DatabaseException} is
     * thrown.
     */
    public boolean remove(Object key) {

        return removeKey(key, null);
    }

    /**
     * Returns true if this set contains the specified key.
     * This method conforms to the {@link Set#contains} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DatabaseException} is
     * thrown.
     */
    public boolean contains(Object key) {

        return containsKey(key);
    }

    boolean hasValues() {

        return false;
    }

    Object makeIteratorData(StoredIterator iterator, DataCursor cursor)
        throws DatabaseException {

        return cursor.getCurrentKey();
    }

    boolean iterateDuplicates() {

        return false;
    }
}
