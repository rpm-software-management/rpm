/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredCollections.java,v 1.1 2004/04/09 16:34:09 mark Exp $
 */

package com.sleepycat.collections;

import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.SortedMap;
import java.util.SortedSet;

/**
 * This class consists exclusively of static methods that operate on or return
 * stored collections. It contains methods for changing certain properties of a
 * collection.  Because collection properties are immutable, these methods
 * always return a new collection reference.  This allows stored collections to
 * be used safely by multiple threads.  Note that creating the new collection
 * reference is not expensive and creates only two new objects.
 *
 * <p>When a collection is created with a particular property, all collections
 * and iterators derived from that collection will inherit the property.  For
 * example, if a dirty-read Map is created then calls to subMap(), values(),
 * entrySet(), and keySet() will create dirty-read collections also.</p>
 *
 * <p><b>Dirty-Read</b>  Methods names beginning with dirtyRead create a new
 * dirty-read container from a given stored container.  When dirty-read is
 * enabled, data will be read that has been modified by another transaction but
 * not committed.  Using dirty-read can improve concurrency since reading will
 * not wait for other transactions to complete.  For a non-transactional
 * container (when {@link StoredContainer#isTransactional} returns false),
 * dirty-read has no effect.  If {@link StoredContainer#isDirtyReadAllowed}
 * returns false, dirty-read also has no effect.  If dirty-ready is enabled
 * (and allowed) for a container, {@link StoredContainer#isDirtyRead} will
 * return true.  Dirty-read is disabled by default for a container.</p>
 */
public class StoredCollections {

    private StoredCollections() {}

    /**
     * Creates a dirty-read collection from a given stored collection.
     *
     * @param storedCollection the base collection.
     *
     * @return the dirty-read collection.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Collection dirtyReadCollection(Collection storedCollection) {

        return (Collection)
            ((StoredContainer) storedCollection).dirtyReadClone();
    }

    /**
     * Creates a dirty-read list from a given stored list.
     *
     * @param storedList the base list.
     *
     * @return the dirty-read list.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static List dirtyReadList(List storedList) {

        return (List) ((StoredContainer) storedList).dirtyReadClone();
    }

    /**
     * Creates a dirty-read map from a given stored map.
     *
     * @param storedMap the base map.
     *
     * @return the dirty-read map.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Map dirtyReadMap(Map storedMap) {

        return (Map) ((StoredContainer) storedMap).dirtyReadClone();
    }

    /**
     * Creates a dirty-read set from a given stored set.
     *
     * @param storedSet the base set.
     *
     * @return the dirty-read set.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Set dirtyReadSet(Set storedSet) {

        return (Set) ((StoredContainer) storedSet).dirtyReadClone();
    }

    /**
     * Creates a dirty-read sorted map from a given stored sorted map.
     *
     * @param storedSortedMap the base map.
     *
     * @return the dirty-read map.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static SortedMap dirtyReadSortedMap(SortedMap storedSortedMap) {

        return (SortedMap)
            ((StoredContainer) storedSortedMap).dirtyReadClone();
    }

    /**
     * Creates a dirty-read sorted set from a given stored sorted set.
     *
     * @param storedSortedSet the base set.
     *
     * @return the dirty-read set.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static SortedSet dirtyReadSortedSet(SortedSet storedSortedSet) {

        return (SortedSet)
            ((StoredContainer) storedSortedSet).dirtyReadClone();
    }

    /**
     * Clones a stored iterator preserving its current position.
     *
     * @param storedIterator an iterator to clone.
     *
     * @return a new {@link StoredIterator} having the same position as the
     * given iterator.
     *
     * @throws ClassCastException if the given iterator is not a
     * StoredIterator.
     */
    public static Iterator iterator(Iterator storedIterator) {

        return (Iterator) ((StoredIterator) storedIterator).clone();
    }
}
