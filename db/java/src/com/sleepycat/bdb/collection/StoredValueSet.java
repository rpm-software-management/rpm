/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredValueSet.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.DataCursor;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.DataView;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbException;
import java.io.IOException;
import java.util.Collection;
import java.util.Set;

/**
 * The Set returned by Map.values() and Map.duplicates(), and which can also be
 * constructed directly if a Map is not needed.
 * Although this collection is a set it may contain duplicate values.  Only if
 * an entity value binding is used are all elements guaranteed to be unique.
 *
 * @author Mark Hayes
 */
public class StoredValueSet extends StoredCollection implements Set {

    // This class is also used internally for the set returned by duplicates().

    private boolean isSingleKey;

    /**
     * Creates a value set view of a {@link DataStore}.
     *
     * @param store is the DataStore underlying the new collection.
     *
     * @param valueBinding is the binding used to translate between value
     * buffers and value objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredValueSet(DataStore store,
                          DataBinding valueBinding,
                          boolean writeAllowed) {

        super(new DataView(store, null, null, valueBinding,
                          null, writeAllowed));
    }

    /**
     * Creates a value set entity view of a {@link DataStore}.
     *
     * @param store is the DataStore underlying the new collection.
     *
     * @param valueEntityBinding is the binding used to translate between
     * key/value buffers and entity value objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredValueSet(DataStore store,
                          EntityBinding valueEntityBinding,
                          boolean writeAllowed) {

        super(new DataView(store, null, null, null,
                           valueEntityBinding, writeAllowed));
    }

    /**
     * Creates a value set view of a {@link DataIndex}.
     *
     * @param index is the DataIndex underlying the new collection.
     *
     * @param valueBinding is the binding used to translate between value
     * buffers and value objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredValueSet(DataIndex index,
                          DataBinding valueBinding,
                          boolean writeAllowed) {

        super(new DataView(null, index, null, valueBinding,
                           null, writeAllowed));
    }

    /**
     * Creates a value set entity view of a {@link DataIndex}.
     *
     * @param index is the DataIndex underlying the new collection.
     *
     * @param valueEntityBinding is the binding used to translate between
     * key/value buffers and entity value objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredValueSet(DataIndex index,
                          EntityBinding valueEntityBinding,
                          boolean writeAllowed) {

        super(new DataView(null, index, null, null,
                           valueEntityBinding, writeAllowed));
    }

    StoredValueSet(DataView valueSetView) {

        super(valueSetView);
    }

    StoredValueSet(DataView valueSetView, boolean isSingleKey) {

        super(valueSetView);
        this.isSingleKey = isSingleKey;
    }

    /**
     * Adds the specified entity to this set if it is not already present
     * (optional operation).
     * This method conforms to the {@link Set#add} interface.
     *
     * @param entity is the entity to be added.
     *
     * @return true if the entity was added, that is the key-value pair
     * represented by the entity was not previously present in the collection.
     *
     * @throws UnsupportedOperationException if the collection is read-only,
     * if the collection is indexed, or if an entity binding is not used.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean add(Object entity) {

        if (view.getIndex() != null) {
            throw new UnsupportedOperationException(
                "add() not allowed with index");
        } else if (isSingleKey) {
            // entity is actually just a value in this case
            boolean doAutoCommit = beginAutoCommit();
            try {
                int err = view.addValue(view.getSingleKeyThang(), entity,
                                        Db.DB_NODUPDATA);
                commitAutoCommit(doAutoCommit);
                return (err == 0);
            } catch (Exception e) {
                throw handleException(e, doAutoCommit);
            }
        } else if (view.getValueEntityBinding() == null) {
            throw new UnsupportedOperationException(
                "add() requires entity binding");
        } else {
            return add(null, entity);
        }
    }

    /* hide for now, add() will suffice
    public Object put(Object value) {

        return super.put(null, value);
    }

    public void putAll(Collection coll) {

        boolean doAutoCommit = beginAutoCommit();
	Iterator i = null;
        try {
            i = coll.iterator();
            while (i.hasNext()) {
                put(i.next());
            }
            StoredIterator.close(i);
            commitAutoCommit(doAutoCommit);
        } catch (Exception e); {
            StoredIterator.close(i);
            throw handleException(e, doAutoCommit);
        }
    }
    */

    /**
     * Returns true if this set contains the specified element.
     * This method conforms to the {@link java.util.Set#contains(Object)}
     * interface.
     *
     * @param value the value to check.
     *
     * @return whether the set contains the given value.
     */
    public boolean contains(Object value) {

        return containsValue(value);
    }

    /**
     * Removes the specified value from this set if it is present (optional
     * operation).
     * If an entity binding is used, the key-value pair represented by the
     * given entity is removed.  If an entity binding is used, the first
     * occurance of a key-value pair with the given value is removed.
     * This method conforms to the {@link Set#remove} interface.
     *
     * @throws UnsupportedOperationException if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean remove(Object value) {

        return removeValue(value);
    }

    // javadoc is inherited
    public int size() {

        if (!isSingleKey) return super.size();
        DataCursor cursor = null;
        try {
            cursor = new DataCursor(view, false);
            int err = cursor.get(null, null, Db.DB_FIRST, false);
            if (err == 0) {
                return cursor.count();
            } else {
                return 0;
            }
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        } finally {
            closeCursor(cursor);
        }
    }

    Object makeIteratorData(StoredIterator iterator, DataCursor cursor)
        throws DbException, IOException {

        return cursor.getCurrentValue();
    }

    boolean hasValues() {

        return true;
    }
}
