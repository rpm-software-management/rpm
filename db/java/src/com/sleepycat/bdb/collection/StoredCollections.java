/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredCollections.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import com.sleepycat.db.DbEnv;
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
 * and iterators derieved from that collection will inherit the property.  For
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
 * (and allowed) for a container, {@link StoredContainer#isDirtyReadEnabled}
 * will return true.  Dirty-read is disabled by default for a container.</p>
 *
 * <p><b>Auto-Commit</b>  Methods names beginning with autoCommit create a new
 * auto-commit container from a given stored container.  If auto-commit is
 * enabled for a container (or for its {@link DbEnv}), a transaction will be
 * started and committed automatically for each write operation when no
 * transaction is already active.  Auto-commit only applies to container
 * methods. It does not apply to iterator methods and these always require an
 * active transaction.  For a non-transactional container (where {@link
 * StoredContainer#isTransactional} returns false) auto-commit has no effect.
 * If auto-commit is enabled for a transactional container or its environment,
 * {@link StoredContainer#isAutoCommit} will return true.  Auto-commit is
 * disabled by default for a container but may be enabled for an entire
 * environment using {@link DbEnv#setFlags}.</p>
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
     * Creates a auto-commit collection from a given stored collection.
     *
     * @param storedCollection the base collection.
     *
     * @return the auto-commit collection.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Collection autoCommitCollection(Collection
                                                  storedCollection) {
        return (Collection)
            ((StoredContainer) storedCollection).autoCommitClone();
    }

    /**
     * Creates a auto-commit list from a given stored list.
     *
     * @param storedList the base list.
     *
     * @return the auto-commit list.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static List autoCommitList(List storedList) {

        return (List) ((StoredContainer) storedList).autoCommitClone();
    }

    /**
     * Creates a auto-commit map from a given stored map.
     *
     * @param storedMap the base map.
     *
     * @return the auto-commit map.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Map autoCommitMap(Map storedMap) {

        return (Map) ((StoredContainer) storedMap).autoCommitClone();
    }

    /**
     * Creates a auto-commit set from a given stored set.
     *
     * @param storedSet the base set.
     *
     * @return the auto-commit set.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Set autoCommitSet(Set storedSet) {

        return (Set) ((StoredContainer) storedSet).autoCommitClone();
    }

    /**
     * Creates a auto-commit sorted map from a given stored sorted map.
     *
     * @param storedSortedMap the base map.
     *
     * @return the auto-commit map.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static SortedMap autoCommitSortedMap(SortedMap storedSortedMap) {

        return (SortedMap)
            ((StoredContainer) storedSortedMap).autoCommitClone();
    }

    /**
     * Creates a auto-commit sorted set from a given stored sorted set.
     *
     * @param storedSortedSet the base set.
     *
     * @return the auto-commit set.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static SortedSet autoCommitSortedSet(SortedSet storedSortedSet) {

        return (SortedSet)
            ((StoredContainer) storedSortedSet).autoCommitClone();
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
