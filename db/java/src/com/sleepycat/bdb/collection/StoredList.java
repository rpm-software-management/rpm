/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredList.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.RecordNumberFormat;
import com.sleepycat.bdb.DataCursor;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.DataView;
import com.sleepycat.bdb.KeyRangeException;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbException;
import java.io.IOException;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;

/**
 * A List view of a {@link DataStore} or {@link DataIndex}.
 *
 * <p>For all stored lists the keys of the underlying DataStore or DataIndex
 * must have record number format, and therefore the store or index must be a
 * RECNO, RECNO-RENUMBER, QUEUE, or BTREE-RECNUM database.  Only RECNO-RENUMBER
 * allows true list behavior where record numbers are renumbered following the
 * position of an element that is added or removed.  For the other access
 * methods (RECNO, QUEUE, and BTREE-RECNUM), stored Lists are most useful as
 * read-only collections where record numbers are not required to be
 * sequential.</p>
 *
 * <p>In addition to the standard List methods, this class provides the
 * following methods for stored lists only.  Note that the use of these methods
 * is not compatible with the standard Java collections interface.</p>
 * <ul>
 * <li>{@link #append(Object)}</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public class StoredList extends StoredCollection implements List {

    private static final DataBinding DEFAULT_KEY_BINDING =
        new IndexKeyBinding(1);

    private int baseIndex = 1;
    private boolean isSubList;

    /**
     * Creates a list view of a {@link DataStore}.
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
    public StoredList(DataStore store, DataBinding valueBinding,
                      boolean writeAllowed) {

        super(new DataView(store, null, DEFAULT_KEY_BINDING, valueBinding,
                           null, writeAllowed));
    }

    /**
     * Creates a list entity view of a {@link DataStore}.
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
    public StoredList(DataStore store, EntityBinding valueEntityBinding,
                      boolean writeAllowed) {

        super(new DataView(store, null, DEFAULT_KEY_BINDING, null,
                           valueEntityBinding, writeAllowed));
    }

    /**
     * Creates a list view of a {@link DataIndex}.
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
    public StoredList(DataIndex index, DataBinding valueBinding,
                      boolean writeAllowed) {

        super(new DataView(null, index, DEFAULT_KEY_BINDING, valueBinding,
                           null, writeAllowed));
    }

    /**
     * Creates a list entity view of a {@link DataIndex}.
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
    public StoredList(DataIndex index, EntityBinding valueEntityBinding,
                      boolean writeAllowed) {

        super(new DataView(null, index, DEFAULT_KEY_BINDING, null,
                           valueEntityBinding, writeAllowed));
    }

    private StoredList(DataView view, int baseIndex) {

        super(view);
        this.baseIndex = baseIndex;
        this.isSubList = true;
    }

    /**
     * Inserts the specified element at the specified position in this list
     * (optional operation).
     * This method conforms to the {@link List#add(int, Object)} interface.
     *
     * @throws UnsupportedOperationException if the collection is a sublist, or
     * if the collection is indexed, or if the collection is read-only, or if
     * the RECNO-RENUMBER access method was not used.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public void add(int index, Object value) {

        checkIterAddAllowed();
        DataCursor cursor = null;
        boolean doAutoCommit = beginAutoCommit();
        try {
            cursor = new DataCursor(view, true);
            int err = cursor.get(new Long(index), null, Db.DB_SET, false);
            if (err == 0) {
                cursor.put(null, value, Db.DB_BEFORE, null);
                closeCursor(cursor);
            } else {
                closeCursor(cursor);
                cursor = null;
                view.append(value, null, null);
            }
            commitAutoCommit(doAutoCommit);
        } catch (Exception e) {
            closeCursor(cursor);
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Appends the specified element to the end of this list (optional
     * operation).
     * This method conforms to the {@link List#add(Object)} interface.
     *
     * @throws UnsupportedOperationException if the collection is a sublist, or
     * if the collection is indexed, or if the collection is read-only, or if
     * the RECNO-RENUMBER access method was not used.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean add(Object value) {

        checkIterAddAllowed();
        boolean doAutoCommit = beginAutoCommit();
        try {
            view.append(value, null, null);
            commitAutoCommit(doAutoCommit);
            return true;
        } catch (Exception e) {
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Appends a given value returning the newly assigned index.
     * If a {@link com.sleepycat.bdb.PrimaryKeyAssigner} is associated with
     * Store for this list, it will be used to assigned the returned index.
     * Otherwise the Store must be a QUEUE or RECNO database and the next
     * available record number is assigned as the index.
     * This method does not exist in the standard {@link List} interface.
     *
     * @param value the value to be appended.
     *
     * @return the assigned index.
     *
     * @throws UnsupportedOperationException if the collection is indexed, or
     * if the collection is read-only, or if the Store has no {@link
     * com.sleepycat.bdb.PrimaryKeyAssigner} and is not a QUEUE or RECNO
     * database.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public int append(Object value) {

        boolean doAutoCommit = beginAutoCommit();
        try {
            Object[] key = new Object[1];
            view.append(value, key, null);
            commitAutoCommit(doAutoCommit);
            return ((Number) key[0]).intValue();
        } catch (Exception e) {
            throw handleException(e, doAutoCommit);
        }
    }

    void checkIterAddAllowed()
        throws UnsupportedOperationException {

        if (isSubList) {
            throw new UnsupportedOperationException("cannot add to subList");
        }
        if (!view.getDb().areKeysRenumbered()) { // checks for RECNO-RENUM
            throw new UnsupportedOperationException(
                "requires renumbered keys");
        }
    }

    /**
     * Inserts all of the elements in the specified collection into this list
     * at the specified position (optional operation).
     * This method conforms to the {@link List#addAll(int, Collection)}
     * interface.
     *
     * @throws UnsupportedOperationException if the collection is a sublist, or
     * if the collection is indexed, or if the collection is read-only, or if
     * the RECNO-RENUMBER access method was not used.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean addAll(int index, Collection coll) {

        checkIterAddAllowed();
        DataCursor cursor = null;
	Iterator i = null;
        boolean doAutoCommit = beginAutoCommit();
        try {
            i = coll.iterator();
            if (!i.hasNext()) {
                return false;
            }
            cursor = new DataCursor(view, true);
            int err = cursor.get(new Long(index), null, Db.DB_SET, false);
            if (err == 0) {
                while (i.hasNext()) {
                    cursor.put(null, i.next(), Db.DB_BEFORE, null);
                }
                closeCursor(cursor);
            } else {
                closeCursor(cursor);
                cursor = null;
                while (i.hasNext()) {
                    view.append(i.next(), null, null);
                }
            }
            StoredIterator.close(i);
            commitAutoCommit(doAutoCommit);
            return true;
        } catch (Exception e) {
            closeCursor(cursor);
            StoredIterator.close(i);
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Returns true if this list contains the specified element.
     * This method conforms to the {@link List#contains} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean contains(Object value) {

        return containsValue(value);
    }

    /**
     * Returns the element at the specified position in this list.
     * This method conforms to the {@link List#get} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object get(int index) {

        return super.get(new Long(index));
    }

    /**
     * Returns the index in this list of the first occurrence of the specified
     * element, or -1 if this list does not contain this element.
     * This method conforms to the {@link List#indexOf} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public int indexOf(Object value) {

        return indexOf(value, true);
    }

    /**
     * Returns the index in this list of the last occurrence of the specified
     * element, or -1 if this list does not contain this element.
     * This method conforms to the {@link List#lastIndexOf} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public int lastIndexOf(Object value) {

        return indexOf(value, false);
    }

    private int indexOf(Object value, boolean findFirst) {

        DataCursor cursor = null;
        try {
            cursor = new DataCursor(view, false);
            int err = cursor.find(value, findFirst);
            return (err == 0)
                    ? (cursor.getCurrentRecordNumber() - baseIndex)
                    : (-1);
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        } finally {
            closeCursor(cursor);
        }
    }

    int getIndexOffset() {

        return baseIndex;
    }

    /**
     * Returns a list iterator of the elements in this list (in proper
     * sequence).
     * The iterator will be read-only if the collection is read-only.
     * This method conforms to the {@link List#listIterator()} interface.
     *
     * @return a {@link StoredIterator} for this collection.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     *
     * @see #isWriteAllowed
     */
    public ListIterator listIterator() {

        return iterator(isWriteAllowed());
    }

    /**
     * Returns a list iterator of the elements in this list (in proper
     * sequence), starting at the specified position in this list.
     * The iterator will be read-only if the collection is read-only.
     * This method conforms to the {@link List#listIterator(int)} interface.
     *
     * @return a {@link StoredIterator} for this collection.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     *
     * @see #isWriteAllowed
     */
    public ListIterator listIterator(int index) {

        StoredIterator i = iterator(isWriteAllowed());
        if (i.moveToIndex(index)) {
            return i;
        } else {
            i.close();
            throw new IndexOutOfBoundsException(String.valueOf(index));
        }
    }

    /**
     * Removes the element at the specified position in this list (optional
     * operation).
     * This method conforms to the {@link List#remove(int)} interface.
     *
     * @throws UnsupportedOperationException if the collection is a sublist, or
     * if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object remove(int index) {

        try {
            Object[] oldVal = new Object[1];
            removeKey(new Long(index), oldVal);
            return oldVal[0];
        } catch (IllegalArgumentException e) {
            throw new IndexOutOfBoundsException(e.getMessage());
        }
    }

    /**
     * Removes the first occurrence in this list of the specified element
     * (optional operation).
     * This method conforms to the {@link List#remove(Object)} interface.
     *
     * @throws UnsupportedOperationException if the collection is a sublist, or
     * if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean remove(Object value) {

        return removeValue(value);
    }

    /**
     * Replaces the element at the specified position in this list with the
     * specified element (optional operation).
     * This method conforms to the {@link List#set} interface.
     *
     * @throws UnsupportedOperationException if the collection is indexed, or
     * if the collection is read-only.
     *
     * @throws IllegalArgumentException if an entity value binding is used and
     * the primary key of the value given is different than the existing stored
     * primary key.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object set(int index, Object value) {

        try {
            return put(new Long(index), value);
        } catch (IllegalArgumentException e) {
            throw new IndexOutOfBoundsException(e.getMessage());
        }
    }

    /**
     * Returns a view of the portion of this list between the specified
     * fromIndex, inclusive, and toIndex, exclusive.
     * Note that add() and remove() may not be called for the returned sublist.
     * This method conforms to the {@link List#subList} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public List subList(int fromIndex, int toIndex) {

        if (fromIndex < 0 || fromIndex > toIndex) {
            throw new IndexOutOfBoundsException(String.valueOf(fromIndex));
        }
        try {
            int newBaseIndex = baseIndex + fromIndex;
            return new StoredList(
                view.subView(new Long(fromIndex), true,
                             new Long(toIndex), false,
                             new IndexKeyBinding(newBaseIndex)),
                newBaseIndex);
        } catch (KeyRangeException e) {
            throw new IndexOutOfBoundsException(e.getMessage());
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Compares the specified object with this list for equality.
     * A value comparison is performed by this method and the stored values
     * are compared rather than calling the equals() method of each element.
     * This method conforms to the {@link List#equals} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean equals(Object other) {

        if (!(other instanceof List)) return false;
        List otherList = (List) other;
        ListIterator i1 = null;
        ListIterator i2 = null;
        try {
            i1 = listIterator();
            i2 = otherList.listIterator();
            while (i1.hasNext()) {
                if (!i2.hasNext()) return false;
                if (i1.nextIndex() != i2.nextIndex()) return false;
                Object o1 = i1.next();
                Object o2 = i2.next();
                if (o1 == null) {
                    if (o2 != null) return false;
                } else {
                    if (!o1.equals(o2)) return false;
                }
            }
            if (i2.hasNext()) return false;
            return true;
        } finally {
            StoredIterator.close(i1);
            StoredIterator.close(i2);
        }
    }

    Object makeIteratorData(StoredIterator iterator, DataCursor cursor)
        throws DbException, IOException {

        return cursor.getCurrentValue();
    }

    boolean hasValues() {

        return true;
    }

    private static class IndexKeyBinding implements DataBinding {

        private static RecordNumberFormat format = new RecordNumberFormat();
        private int baseIndex;

        private IndexKeyBinding(int baseIndex) {

            this.baseIndex = baseIndex;
        }

        public DataFormat getDataFormat() {

            return format;
        }

        public Object dataToObject(DataBuffer data)
            throws IOException {

            return new Long(format.dataToRecordNumber(data) - baseIndex);
        }

        public void objectToData(Object object, DataBuffer data)
            throws IOException {

            format.recordNumberToData(((Number) object).intValue() + baseIndex,
                                        data);
        }
    }
}
