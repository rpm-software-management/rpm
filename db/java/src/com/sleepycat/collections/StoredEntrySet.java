/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredEntrySet.java,v 1.3 2004/06/04 18:24:50 mark Exp $
 */

package com.sleepycat.collections;

import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.OperationStatus;

/**
 * The Set returned by Map.entrySet().  This class may not be instantiated
 * directly.  Contrary to what is stated by {@link Map#entrySet} this class
 * does support the {@link #add} and {@link #addAll} methods.
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
 * @author Mark Hayes
 */
public class StoredEntrySet extends StoredCollection implements Set {

    StoredEntrySet(DataView mapView) {

        super(mapView);
    }

    /**
     * Adds the specified element to this set if it is not already present
     * (optional operation).
     * This method conforms to the {@link Set#add} interface.
     *
     * @param mapEntry must be a {@link java.util.Map.Entry} instance.
     *
     * @return true if the key-value pair was added to the set (and was not
     * previously present).
     *
     * @throws UnsupportedOperationException if the collection is read-only.
     *
     * @throws ClassCastException if the mapEntry is not a {@link
     * java.util.Map.Entry} instance.
     *
     * @throws RuntimeExceptionWrapper if a {@link DatabaseException} is thrown.
     */
    public boolean add(Object mapEntry) {

        Map.Entry entry = (Map.Entry) mapEntry; // allow ClassCastException
        return add(entry.getKey(), entry.getValue());
    }

    /**
     * Removes the specified element from this set if it is present (optional
     * operation).
     * This method conforms to the {@link Set#remove} interface.
     *
     * @param mapEntry is a {@link java.util.Map.Entry} instance to be removed.
     *
     * @return true if the key-value pair was removed from the set, or false if
     * the mapEntry is not a {@link java.util.Map.Entry} instance or is not
     * present in the set.
     *
     * @throws UnsupportedOperationException if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DatabaseException} is thrown.
     */
    public boolean remove(Object mapEntry) {

        if (!(mapEntry instanceof Map.Entry)) {
            return false;
        }
        Map.Entry entry = (Map.Entry) mapEntry;
        DataCursor cursor = null;
        boolean doAutoCommit = beginAutoCommit();
        try {
            cursor = new DataCursor(view, true);
            OperationStatus status =
                cursor.getSearchBoth(entry.getKey(), entry.getValue(), true);
            if (status == OperationStatus.SUCCESS) {
                cursor.delete();
            }
            closeCursor(cursor);
            commitAutoCommit(doAutoCommit);
            return (status == OperationStatus.SUCCESS);
        } catch (Exception e) {
            closeCursor(cursor);
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Returns true if this set contains the specified element.
     * This method conforms to the {@link Set#contains} interface.
     *
     * @param mapEntry is a {@link java.util.Map.Entry} instance to be checked.
     *
     * @return true if the key-value pair is present in the set, or false if
     * the mapEntry is not a {@link java.util.Map.Entry} instance or is not
     * present in the set.
     *
     * @throws RuntimeExceptionWrapper if a {@link DatabaseException} is thrown.
     */
    public boolean contains(Object mapEntry) {

        if (!(mapEntry instanceof Map.Entry)) {
            return false;
        }
        Map.Entry entry = (Map.Entry) mapEntry;
        DataCursor cursor = null;
        try {
            cursor = new DataCursor(view, false);
            OperationStatus status =
                cursor.getSearchBoth(entry.getKey(), entry.getValue(), false);
            return (status == OperationStatus.SUCCESS);
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        } finally {
            closeCursor(cursor);
        }
    }

    // javadoc is inherited
    public String toString() {
	StringBuffer buf = new StringBuffer();
	buf.append("[");
	Iterator i = iterator();
        try {
            while (i.hasNext()) {
                Map.Entry entry = (Map.Entry) i.next();
                if (buf.length() > 1) buf.append(',');
                Object key = entry.getKey();
                Object val = entry.getValue();
                if (key != null) buf.append(key.toString());
                buf.append('=');
                if (val != null) buf.append(val.toString());
            }
            buf.append(']');
            return buf.toString();
        }
        finally {
            StoredIterator.close(i);
        }
    }

    Object makeIteratorData(StoredIterator iterator, DataCursor cursor)
        throws DatabaseException {

        return new StoredMapEntry(cursor.getCurrentKey(),
                                  cursor.getCurrentValue(),
                                  this, iterator);
    }

    boolean hasValues() {

        return true;
    }
}
