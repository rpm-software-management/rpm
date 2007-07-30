/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: StoredSortedValueSet.java,v 12.7 2007/05/04 00:28:25 mark Exp $
 */

package com.sleepycat.collections;

import java.util.Comparator;
import java.util.SortedSet;

import com.sleepycat.bind.EntityBinding;
import com.sleepycat.db.Database;

/**
 * The SortedSet returned by Map.values() and which can also be constructed
 * directly if a Map is not needed.
 * Although this collection is a set it may contain duplicate values.  Only if
 * an entity value binding is used are all elements guaranteed to be unique.
 *
 * <p>In addition to the standard SortedSet methods, this class provides the
 * following methods for stored sorted value sets only.  Note that the use of
 * these methods is not compatible with the standard Java collections
 * interface.</p>
 * <ul>
 * <li>{@link #headSet(Object, boolean)}</li>
 * <li>{@link #tailSet(Object, boolean)}</li>
 * <li>{@link #subSet(Object, boolean, Object, boolean)}</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public class StoredSortedValueSet extends StoredValueSet implements SortedSet {

    /*
     * No valueBinding ctor is possible since key cannot be derived.
     */

    /**
     * Creates a sorted value set entity view of a {@link Database}.
     *
     * @param database is the Database underlying the new collection.
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
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public StoredSortedValueSet(Database database,
                                EntityBinding valueEntityBinding,
                                boolean writeAllowed) {

        super(new DataView(database, null, null, valueEntityBinding,
                           writeAllowed, null));
        checkKeyDerivation();
    }

    StoredSortedValueSet(DataView valueSetView) {

        super(valueSetView);
        checkKeyDerivation();
    }

    private void checkKeyDerivation() {

        if (!view.canDeriveKeyFromValue()) {
            throw new IllegalArgumentException("Cannot derive key from value");
        }
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
     * strictly less than toValue.
     * This method conforms to the {@link SortedSet#headSet} interface.
     *
     * <p>Note that the return value is a StoredCollection and must be treated
     * as such; for example, its iterators must be explicitly closed.</p>
     *
     * @param toValue the upper bound.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet headSet(Object toValue) {

        return subSet(null, false, toValue, false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * strictly less than toValue, optionally including toValue.
     * This method does not exist in the standard {@link SortedSet} interface.
     *
     * <p>Note that the return value is a StoredCollection and must be treated
     * as such; for example, its iterators must be explicitly closed.</p>
     *
     * @param toValue is the upper bound.
     *
     * @param toInclusive is true to include toValue.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet headSet(Object toValue, boolean toInclusive) {

        return subSet(null, false, toValue, toInclusive);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * greater than or equal to fromValue.
     * This method conforms to the {@link SortedSet#tailSet} interface.
     *
     * <p>Note that the return value is a StoredCollection and must be treated
     * as such; for example, its iterators must be explicitly closed.</p>
     *
     * @param fromValue is the lower bound.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet tailSet(Object fromValue) {

        return subSet(fromValue, true, null, false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * strictly greater than fromValue, optionally including fromValue.
     * This method does not exist in the standard {@link SortedSet} interface.
     *
     * <p>Note that the return value is a StoredCollection and must be treated
     * as such; for example, its iterators must be explicitly closed.</p>
     *
     * @param fromValue is the lower bound.
     *
     * @param fromInclusive is true to include fromValue.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet tailSet(Object fromValue, boolean fromInclusive) {

        return subSet(fromValue, fromInclusive, null, false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements range
     * from fromValue, inclusive, to toValue, exclusive.
     * This method conforms to the {@link SortedSet#subSet} interface.
     *
     * <p>Note that the return value is a StoredCollection and must be treated
     * as such; for example, its iterators must be explicitly closed.</p>
     *
     * @param fromValue is the lower bound.
     *
     * @param toValue is the upper bound.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet subSet(Object fromValue, Object toValue) {

        return subSet(fromValue, true, toValue, false);
    }

    /**
     * Returns a view of the portion of this sorted set whose elements are
     * strictly greater than fromValue and strictly less than toValue,
     * optionally including fromValue and toValue.
     * This method does not exist in the standard {@link SortedSet} interface.
     *
     * <p>Note that the return value is a StoredCollection and must be treated
     * as such; for example, its iterators must be explicitly closed.</p>
     *
     * @param fromValue is the lower bound.
     *
     * @param fromInclusive is true to include fromValue.
     *
     * @param toValue is the upper bound.
     *
     * @param toInclusive is true to include toValue.
     *
     * @return the subset.
     *
     * @throws RuntimeExceptionWrapper if a {@link
     * com.sleepycat.db.DatabaseException} is thrown.
     */
    public SortedSet subSet(Object fromValue, boolean fromInclusive,
                            Object toValue, boolean toInclusive) {

        try {
            return new StoredSortedValueSet(
               view.subView(fromValue, fromInclusive, toValue, toInclusive,
                            null));
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }
}
