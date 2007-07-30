/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: StoredCollections.java,v 12.6 2007/05/04 00:28:25 mark Exp $
 */

package com.sleepycat.collections;

import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.SortedMap;
import java.util.SortedSet;

import com.sleepycat.db.CursorConfig;

/**
 * Static methods operating on collections and maps.
 *
 * <p>This class consists exclusively of static methods that operate on or
 * return stored collections and maps, jointly called containers. It contains
 * methods for changing certain properties of a container.  Because container
 * properties are immutable, these methods always return a new container
 * instance.  This allows stored container instances to be used safely by
 * multiple threads.  Creating the new container instance is not expensive and
 * creates only two new objects.</p>
 *
 * <p>When a container is created with a particular property, all containers
 * and iterators derived from that container will inherit the property.  For
 * example, if a read-uncommitted Map is created then calls to its subMap(),
 * values(), entrySet(), and keySet() methods will create read-uncommitted
 * containers also.</p>
 *
 * <p>Method names beginning with "configured" create a new container with a
 * specified {@link CursorConfig} from a given stored container.  This allows
 * configuring a container for read-committed isolation, read-uncommitted
 * isolation, or any other property supported by <code>CursorConfig</code>.
 * All operations performed with the resulting container will be performed with
 * the specified cursor configuration.</p>
 */
public class StoredCollections {

    private StoredCollections() {}

    /**
     * Creates a configured collection from a given stored collection.
     *
     * @param storedCollection the base collection.
     *
     * @param config is the cursor configuration to be used for all operations
     * performed via the new collection instance; null may be specified to use
     * the default configuration.
     *
     * @return the configured collection.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Collection configuredCollection(Collection storedCollection,
                                                  CursorConfig config) {

        return (Collection)
            ((StoredContainer) storedCollection).configuredClone(config);
    }

    /**
     * Creates a configured list from a given stored list.
     *
     * <p>Note that this method may not be called in the JE product, since the
     * StoredList class is not supported.</p>
     *
     * @param storedList the base list.
     *
     * @param config is the cursor configuration to be used for all operations
     * performed via the new list instance; null may be specified to use the
     * default configuration.
     *
     * @return the configured list.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static List configuredList(List storedList, CursorConfig config) {

        return (List) ((StoredContainer) storedList).configuredClone(config);
    }

    /**
     * Creates a configured map from a given stored map.
     *
     * @param storedMap the base map.
     *
     * @param config is the cursor configuration to be used for all operations
     * performed via the new map instance; null may be specified to use the
     * default configuration.
     *
     * @return the configured map.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Map configuredMap(Map storedMap, CursorConfig config) {

        return (Map) ((StoredContainer) storedMap).configuredClone(config);
    }

    /**
     * Creates a configured set from a given stored set.
     *
     * @param storedSet the base set.
     *
     * @param config is the cursor configuration to be used for all operations
     * performed via the new set instance; null may be specified to use the
     * default configuration.
     *
     * @return the configured set.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static Set configuredSet(Set storedSet, CursorConfig config) {

        return (Set) ((StoredContainer) storedSet).configuredClone(config);
    }

    /**
     * Creates a configured sorted map from a given stored sorted map.
     *
     * @param storedSortedMap the base map.
     *
     * @param config is the cursor configuration to be used for all operations
     * performed via the new map instance; null may be specified to use the
     * default configuration.
     *
     * @return the configured map.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static SortedMap configuredSortedMap(SortedMap storedSortedMap,
                                                CursorConfig config) {

        return (SortedMap)
            ((StoredContainer) storedSortedMap).configuredClone(config);
    }

    /**
     * Creates a configured sorted set from a given stored sorted set.
     *
     * @param storedSortedSet the base set.
     *
     * @param config is the cursor configuration to be used for all operations
     * performed via the new set instance; null may be specified to use the
     * default configuration.
     *
     * @return the configured set.
     *
     * @throws ClassCastException if the given container is not a
     * StoredContainer.
     */
    public static SortedSet configuredSortedSet(SortedSet storedSortedSet,
                                                CursorConfig config) {

        return (SortedSet)
            ((StoredContainer) storedSortedSet).configuredClone(config);
    }

    /**
     * @deprecated This method has been replaced by {@link
     * #configuredCollection} in order to conform to ANSI database isolation
     * terminology.  To obtain a dirty-read collection, pass
     * <code>CursorConfig.READ_UNCOMMITTED</code>
     */
    public static Collection dirtyReadCollection(Collection storedCollection) {

        /* We can't use READ_UNCOMMITTED until is is added to DB core. */
        return configuredCollection
            (storedCollection, CursorConfig.DIRTY_READ);
    }

    /**
     * @deprecated This method has been replaced by {@link #configuredList} in
     * order to conform to ANSI database isolation terminology.  To obtain a
     * dirty-read list, pass <code>CursorConfig.READ_UNCOMMITTED</code>
     */
    public static List dirtyReadList(List storedList) {

        /* We can't use READ_UNCOMMITTED until is is added to DB core. */
        return configuredList(storedList, CursorConfig.DIRTY_READ);
    }

    /**
     * @deprecated This method has been replaced by {@link #configuredMap} in
     * order to conform to ANSI database isolation terminology.  To obtain a
     * dirty-read map, pass <code>CursorConfig.READ_UNCOMMITTED</code>
     */
    public static Map dirtyReadMap(Map storedMap) {

        /* We can't use READ_UNCOMMITTED until is is added to DB core. */
        return configuredMap(storedMap, CursorConfig.DIRTY_READ);
    }

    /**
     * @deprecated This method has been replaced by {@link #configuredSet} in
     * order to conform to ANSI database isolation terminology.  To obtain a
     * dirty-read set, pass <code>CursorConfig.READ_UNCOMMITTED</code>
     */
    public static Set dirtyReadSet(Set storedSet) {

        /* We can't use READ_UNCOMMITTED until is is added to DB core. */
        return configuredSet(storedSet, CursorConfig.DIRTY_READ);
    }

    /**
     * @deprecated This method has been replaced by {@link
     * #configuredSortedMap} in order to conform to ANSI database isolation
     * terminology.  To obtain a dirty-read map, pass
     * <code>CursorConfig.READ_UNCOMMITTED</code>
     */
    public static SortedMap dirtyReadSortedMap(SortedMap storedSortedMap) {

        /* We can't use READ_UNCOMMITTED until is is added to DB core. */
        return configuredSortedMap
            (storedSortedMap, CursorConfig.DIRTY_READ);
    }

    /**
     * @deprecated This method has been replaced by {@link
     * #configuredSortedSet} in order to conform to ANSI database isolation
     * terminology.  To obtain a dirty-read set, pass
     * <code>CursorConfig.READ_UNCOMMITTED</code>
     */
    public static SortedSet dirtyReadSortedSet(SortedSet storedSortedSet) {

        /* We can't use READ_UNCOMMITTED until is is added to DB core. */
        return configuredSortedSet
            (storedSortedSet, CursorConfig.DIRTY_READ);
    }

    /**
     * Clones an iterator preserving its current position.
     *
     * @param iter an iterator to clone.
     *
     * @return a new {@code Iterator} having the same position as the given
     * iterator.
     *
     * @throws ClassCastException if the given iterator was not obtained via a
     * {@link StoredCollection} method.
     */
    public static Iterator iterator(Iterator iter) {

        return ((BaseIterator) iter).dup();
    }
}
