/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredSortedEntrySet.java,v 1.2 2004/06/02 20:59:39 mark Exp $
 */

package com.sleepycat.collections;

import java.util.Comparator;
import java.util.Map;
import java.util.SortedSet;

/**
 * The SortedSet returned by Map.entrySet().  This class may not be
 * instantiated directly.  Contrary to what is stated by {@link Map#entrySet}
 * this class does support the {@link #add} and {@link #addAll} methods.
 *
 * <p>The {@link java.util.Map.Entry#setValue} method of the Map.Entry objects
 * that are returned by this class and its iterators behaves just as the {@link
 * StoredIterator#set} method does.</p>
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
 * <p>In addition to the standard SortedSet methods, this class provides the
 * following methods for stored sorted sets only.  Note that the use of these
 * methods is not compatible with the standard Java collections interface.</p>
 * <ul>
 * <li>{@link #headSet(Object, boolean)}</li>
 * <li>{@link #tailSet(Object, boolean)}</li>
 * <li>{@link #subSet(Object, boolean, Object, boolean)}</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public class StoredSortedEntrySet extends StoredEntrySet implements SortedSet {

    StoredSortedEntrySet(DataView mapView) {

        super(mapView);
    }

    /**
     * Returns null since comparators are not supported.  The natural ordering
     * of a stored collection is data byte order, whether the data classes
     * implement the {@link java.lang.Comparable} interface or not.
     * This method does not conform to the {@link SortedSet#comparator}
     * interface.
     *
     * @return null.
     */
    public Comparator comparator() {

        return null;
    }

    /**
     * Returns the first (lowest) element currently in this sorted set.
     * This method conforms to the {@link SortedSet#first} interface.
     *
     * @return the first element.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public Object first() {

        return getFirstOrLast(true);
    }

    /**
     * Returns the last (highest) element currently in this sorted set.
     * This method conforms to the {@link SortedSet#last} interface.
     *
     * @return the last element.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public Object last() {

        return getFirstOrLast(false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * strictly less than toMapEntry.
     * This method conforms to the {@link SortedSet#headSet} interface.
     *
     * @param toMapEntry the upper bound.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet headSet(Object toMapEntry) {

        return subSet(null, false, toMapEntry, false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * strictly less than toMapEntry, optionally including toMapEntry.
     * This method does not exist in the standard {@link SortedSet} interface.
     *
     * @param toMapEntry is the upper bound.
     *
     * @param toInclusive is true to include toMapEntry.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet headSet(Object toMapEntry, boolean toInclusive) {

        return subSet(null, false, toMapEntry, toInclusive);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * greater than or equal to fromMapEntry.
     * This method conforms to the {@link SortedSet#tailSet} interface.
     *
     * @param fromMapEntry is the lower bound.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet tailSet(Object fromMapEntry) {

        return subSet(fromMapEntry, true, null, false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * strictly greater than fromMapEntry, optionally including fromMapEntry.
     * This method does not exist in the standard {@link SortedSet} interface.
     *
     * @param fromMapEntry is the lower bound.
     *
     * @param fromInclusive is true to include fromMapEntry.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet tailSet(Object fromMapEntry, boolean fromInclusive) {

        return subSet(fromMapEntry, fromInclusive, null, false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements range
     * from fromMapEntry, inclusive, to toMapEntry, exclusive.
     * This method conforms to the {@link SortedSet#subSet} interface.
     *
     * @param fromMapEntry is the lower bound.
     *
     * @param toMapEntry is the upper bound.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet subSet(Object fromMapEntry, Object toMapEntry) {

        return subSet(fromMapEntry, true, toMapEntry, false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * strictly greater than fromMapEntry and strictly less than toMapEntry,
     * optionally including fromMapEntry and toMapEntry.
     * This method does not exist in the standard {@link SortedSet} interface.
     *
     * @param fromMapEntry is the lower bound.
     *
     * @param fromInclusive is true to include fromMapEntry.
     *
     * @param toMapEntry is the upper bound.
     *
     * @param toInclusive is true to include toMapEntry.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet subSet(Object fromMapEntry, boolean fromInclusive,
                            Object toMapEntry, boolean toInclusive) {

        Object fromKey = (fromMapEntry != null) ?
            ((Map.Entry) fromMapEntry).getKey() : null;
        Object toKey = (toMapEntry != null) ?
            ((Map.Entry) toMapEntry).getKey() : null;
        try {
            return new StoredSortedEntrySet(
               view.subView(fromKey, fromInclusive, toKey, toInclusive, null));
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }
}
