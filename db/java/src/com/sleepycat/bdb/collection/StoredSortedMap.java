/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredSortedMap.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
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
import java.util.Comparator;
import java.util.SortedMap;

/**
 * A SortedMap view of a {@link DataStore} or {@link DataIndex}.
 *
 * <p>In addition to the standard SortedMap methods, this class provides the
 * following methods for stored sorted maps only.  Note that the use of these
 * methods is not compatible with the standard Java collections interface.</p>
 * <ul>
 * <li>{@link #duplicates(Object)}</li>
 * <li>{@link #headMap(Object, boolean)}</li>
 * <li>{@link #tailMap(Object, boolean)}</li>
 * <li>{@link #subMap(Object, boolean, Object, boolean)}</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public class StoredSortedMap extends StoredMap implements SortedMap {

    /**
     * Creates a sorted map view of a {@link DataStore}.
     *
     * @param store is the DataStore underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
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
    public StoredSortedMap(DataStore store, DataBinding keyBinding,
                           DataBinding valueBinding, boolean writeAllowed) {

        super(new DataView(store, null, keyBinding, valueBinding,
                           null, writeAllowed));
    }

    /**
     * Creates a sorted map entity view of a {@link DataStore}.
     *
     * @param store is the DataStore underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
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
    public StoredSortedMap(DataStore store, DataBinding keyBinding,
                           EntityBinding valueEntityBinding,
                           boolean writeAllowed) {

        super(new DataView(store, null, keyBinding, null,
                           valueEntityBinding, writeAllowed));
    }

    /**
     * Creates a sorted map view of a {@link DataIndex}.
     *
     * @param index is the DataIndex underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
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
    public StoredSortedMap(DataIndex index, DataBinding keyBinding,
                           DataBinding valueBinding, boolean writeAllowed) {

        super(new DataView(null, index, keyBinding, valueBinding,
                           null, writeAllowed));
    }

    /**
     * Creates a sorted map entity view of a {@link DataIndex}.
     *
     * @param index is the DataIndex underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
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
    public StoredSortedMap(DataIndex index, DataBinding keyBinding,
                           EntityBinding valueEntityBinding,
                           boolean writeAllowed) {

        super(new DataView(null, index, keyBinding, null,
                           valueEntityBinding, writeAllowed));
    }

    StoredSortedMap(DataView mapView) {

        super(mapView);
    }

    /**
     * Returns null since comparators are not supported.  The natural ordering
     * of a stored collection is data byte order, whether the data classes
     * implement the {@link java.lang.Comparable} interface or not.
     * This method does not conform to the {@link SortedMap#comparator}
     * interface.
     *
     * @return null.
     */
    public Comparator comparator() {

        return null;
    }

    /**
     * Returns the first (lowest) key currently in this sorted map.
     * This method conforms to the {@link SortedMap#firstKey} interface.
     *
     * @return the first key.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object firstKey() {

        return getFirstOrLastKey(true);
    }

    /**
     * Returns the last (highest) element currently in this sorted map.
     * This method conforms to the {@link SortedMap#lastKey} interface.
     *
     * @return the last key.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object lastKey() {

        return getFirstOrLastKey(false);
    }

    private Object getFirstOrLastKey(boolean doGetFirst) {

        DataCursor cursor = null;
        try {
            cursor = new DataCursor(view, false);
            int err = cursor.get(null, null,
                                 doGetFirst ? Db.DB_FIRST : Db.DB_LAST,
                                 false);
            return (err == 0) ? cursor.getCurrentKey() : null;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        } finally {
            closeCursor(cursor);
        }
    }

    /**
     * Returns a view of the portion of this sorted set whose keys are
     * strictly less than toKey.
     * This method conforms to the {@link SortedMap#headMap} interface.
     *
     * @param toKey is the upper bound.
     *
     * @return the submap.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public SortedMap headMap(Object toKey) {

        return subMap(null, false, toKey, false);
    }

    /**
     * Returns a view of the portion of this sorted map whose elements are
     * strictly less than toKey, optionally including toKey.
     * This method does not exist in the standard {@link SortedMap} interface.
     *
     * @param toKey is the upper bound.
     *
     * @param toInclusive is true to include toKey.
     *
     * @return the submap.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public SortedMap headMap(Object toKey, boolean toInclusive) {

        return subMap(null, false, toKey, toInclusive);
    }

    /**
     * Returns a view of the portion of this sorted map whose elements are
     * greater than or equal to fromKey.
     * This method conforms to the {@link SortedMap#tailMap} interface.
     *
     * @param fromKey is the lower bound.
     *
     * @return the submap.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public SortedMap tailMap(Object fromKey) {

        return subMap(fromKey, true, null, false);
    }

    /**
     * Returns a view of the portion of this sorted map whose elements are
     * strictly greater than fromKey, optionally including fromKey.
     * This method does not exist in the standard {@link SortedMap} interface.
     *
     * @param fromKey is the lower bound.
     *
     * @param fromInclusive is true to include fromKey.
     *
     * @return the submap.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public SortedMap tailMap(Object fromKey, boolean fromInclusive) {

        return subMap(fromKey, fromInclusive, null, false);
    }

    /**
     * Returns a view of the portion of this sorted map whose elements range
     * from fromKey, inclusive, to toKey, exclusive.
     * This method conforms to the {@link SortedMap#subMap} interface.
     *
     * @param fromKey is the lower bound.
     *
     * @param toKey is the upper bound.
     *
     * @return the submap.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public SortedMap subMap(Object fromKey, Object toKey) {

        return subMap(fromKey, true, toKey, false);
    }

    /**
     * Returns a view of the portion of this sorted map whose elements are
     * strictly greater than fromKey and strictly less than toKey,
     * optionally including fromKey and toKey.
     * This method does not exist in the standard {@link SortedMap} interface.
     *
     * @param fromKey is the lower bound.
     *
     * @param fromInclusive is true to include fromKey.
     *
     * @param toKey is the upper bound.
     *
     * @param toInclusive is true to include toKey.
     *
     * @return the submap.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public SortedMap subMap(Object fromKey, boolean fromInclusive,
                            Object toKey, boolean toInclusive) {

        try {
            return new StoredSortedMap(
               view.subView(fromKey, fromInclusive, toKey, toInclusive, null));
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }
}
