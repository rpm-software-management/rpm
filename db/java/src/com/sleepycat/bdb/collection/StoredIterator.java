/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredIterator.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import com.sleepycat.bdb.DataCursor;
import com.sleepycat.bdb.util.ExceptionWrapper;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbDeadlockException;
import com.sleepycat.db.DbException;
import com.sleepycat.db.DbMemoryException;
import com.sleepycat.db.DbRunRecoveryException;
import java.io.IOException;
import java.util.Iterator;
import java.util.ListIterator;
import java.util.Map;
import java.util.NoSuchElementException;

/**
 * The Iterator returned by all stored collections.
 *
 * <p>While in general this class conforms to the {@link Iterator} interface,
 * it is important to note that all iterators for stored collections must be
 * explicitly closed with {@link #close()}.  The static method {@link
 * #close(java.util.Iterator)} allows calling close for all iterators without
 * harm to iterators that are not from stored collections, and also avoids
 * casting.  If a stored iterator is not closed, unpredictable behavior
 * including process death may result.</p>
 *
 * <p>This class implements the {@link Iterator} interface for all stored
 * iterators.  It also implements {@link ListIterator} because some list
 * iterator methods apply to all stored iterators, for example, {@link
 * #previous} and {@link #hasPrevious}.  Other list iterator methods are always
 * supports for lists, but for other types of collections are only supported
 * under certain conditions.  For example, {@link #nextIndex} and {@link
 * #previousIndex} are only supported when record number keys are used, while
 * {@link #add} and {@link #set} are supported only under certain other
 * conditions.  See the individual method descriptions for more
 * information.</p>
 *
 * <p>In addition, this class provides the following methods for stored
 * collection iterators only.  Note that the use of these methods is not
 * compatible with the standard Java collections interface.</p>
 * <ul>
 * <li>{@link #close()}</li>
 * <li>{@link #close(Iterator)}</li>
 * <li>{@link #getCollection()}</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public class StoredIterator implements ListIterator, Cloneable {

    /**
     * Closes the given iterator using {@link #close()} if it is a {@link
     * StoredIterator}.  If the given iterator is not a {@link StoredIterator},
     * this method does nothing.
     *
     * @param i is the iterator to close.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public static void close(Iterator i) {

        if (i instanceof StoredIterator) {
            ((StoredIterator) i).close();
        }
    }

    private boolean lockForWrite;
    private StoredCollection coll;
    private DataCursor cursor;
    private int toNext;
    private int toPrevious;
    private int toCurrent;
    private boolean writeAllowed;
    private boolean setAndRemoveAllowed;
    private Object currentData;
    private final int NEXT_FLAG;
    private final int PREV_FLAG;
    private final boolean recNumAccess;

    StoredIterator(StoredCollection coll, boolean writeAllowed,
                   DataCursor joinCursor) {

        try {
            this.coll = coll;
            this.writeAllowed = writeAllowed;
            if (joinCursor == null)
                this.cursor = new DataCursor(coll.view, writeAllowed);
            else
                this.cursor = joinCursor;
            this.recNumAccess = cursor.hasRecNumAccess();
            if (coll.iterateDuplicates()) {
                this.NEXT_FLAG = Db.DB_NEXT;
                this.PREV_FLAG = Db.DB_PREV;
            } else {
                this.NEXT_FLAG = Db.DB_NEXT_NODUP;
                this.PREV_FLAG = Db.DB_PREV_NODUP;
            }
            reset();
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Clones this iterator preserving its current position.
     *
     * @return a new {@link StoredIterator} having the same position as this
     * iterator.
     */
    protected Object clone() {

        try {
            StoredIterator o = (StoredIterator) super.clone();
            o.cursor = new DataCursor(cursor);
            return o;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Returns whether write-locks will be obtained when reading with this
     * cursor.
     * Obtaining write-locks can prevent deadlocks when reading and then
     * modifying data.
     *
     * @return the write-lock setting.
     */
    public final boolean getLockForWrite() {

        return lockForWrite;
    }

    /**
     * Changes whether write-locks will be obtained when reading with this
     * cursor.
     * Obtaining write-locks can prevent deadlocks when reading and then
     * modifying data.
     *
     * @param lockForWrite the write-lock setting.
     */
    public void setLockForWrite(boolean lockForWrite) {

        this.lockForWrite = lockForWrite;
    }

    // --- begin Iterator/ListIterator methods ---

    /**
     * Returns true if this iterator has more elements when traversing in the
     * forward direction.  False is returned if the iterator has been closed.
     * This method conforms to the {@link Iterator#hasNext} interface.
     *
     * @return whether {@link #next()} will succeed.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean hasNext() {

        if (cursor == null) {
            return false;
        }
        try {
            if (toNext != 0) {
                int err = cursor.get(null, null, toNext, lockForWrite);
                if (err == 0) {
                    toNext = 0;
                    toPrevious = PREV_FLAG;
                    toCurrent = PREV_FLAG;
                }
            }
            return (toNext == 0);
        }
        catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Returns true if this iterator has more elements when traversing in the
     * reverse direction.  It returns false if the iterator has been closed.
     * This method conforms to the {@link ListIterator#hasPrevious} interface.
     *
     * @return whether {@link #previous()} will succeed.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean hasPrevious() {

        if (cursor == null) {
            return false;
        }
        try {
            if (toPrevious != 0) {
                int err = cursor.get(null, null, toPrevious, lockForWrite);
                if (err == 0) {
                    toPrevious = 0;
                    toNext = NEXT_FLAG;
                    toCurrent = NEXT_FLAG;
                }
            }
            return (toPrevious == 0);
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Returns the next element in the interation.
     * This method conforms to the {@link Iterator#next} interface.
     *
     * @return the next element.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object next() {

        try {
            if (toNext != 0) {
                int err = cursor.get(null, null, toNext, lockForWrite);
                if (err == 0) {
                    toNext = 0;
                }
            }
            if (toNext == 0) {
                currentData = coll.makeIteratorData(this, cursor);
                toNext = NEXT_FLAG;
                toPrevious = 0;
                toCurrent = 0;
                setAndRemoveAllowed = true;
                return currentData;
            }
            // else throw NoSuchElementException below
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
        throw new NoSuchElementException();
    }

    /**
     * Returns the next element in the interation.
     * This method conforms to the {@link ListIterator#previous} interface.
     *
     * @return the previous element.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object previous() {

        try {
            if (toPrevious != 0) {
                int err = cursor.get(null, null, toPrevious, lockForWrite);
                if (err == 0) {
                    toPrevious = 0;
                }
            }
            if (toPrevious == 0) {
                currentData = coll.makeIteratorData(this, cursor);
                toPrevious = PREV_FLAG;
                toNext = 0;
                toCurrent = 0;
                setAndRemoveAllowed = true;
                return currentData;
            }
            // else throw NoSuchElementException below
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
        throw new NoSuchElementException();
    }

    /**
     * Returns the index of the element that would be returned by a subsequent
     * call to next.
     * This method conforms to the {@link ListIterator#nextIndex} interface
     * except that it returns Integer.MAX_VALUE for stored lists when
     * positioned at the end of the list, rather than returning the list size
     * as specified by the ListIterator interface. This is because the database
     * size is not available.
     *
     * @return the next index.
     *
     * @throws UnsupportedOperationException if this iterator's collection does
     * not use record number keys.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public int nextIndex() {

        if (!recNumAccess) {
            throw new UnsupportedOperationException(
                "Record number access not supported");
        }
        try {
            return hasNext() ? (cursor.getCurrentRecordNumber() -
                                coll.getIndexOffset())
                             : Integer.MAX_VALUE;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Returns the index of the element that would be returned by a subsequent
     * call to previous.
     * This method conforms to the {@link ListIterator#previousIndex}
     * interface.
     *
     * @return the previous index.
     *
     * @throws UnsupportedOperationException if this iterator's collection does
     * not use record number keys.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public int previousIndex() {

        if (!recNumAccess) {
            throw new UnsupportedOperationException(
                "Record number access not supported");
        }
        try {
            return hasPrevious() ? (cursor.getCurrentRecordNumber() -
                                    coll.getIndexOffset())
                                 : -1;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Replaces the last element returned by next or previous with the
     * specified element (optional operation).
     * This method conforms to the {@link ListIterator#set} interface.
     *
     * @param value the new value.
     *
     * @throws UnsupportedOperationException if the collection is a {@link
     * StoredKeySet} (the set returned by {@link Map#keySet}), or if duplicates
     * are sorted since this would change the iterator position, or if
     * the collection is indexed, or if the collection is read-only.
     *
     * @throws IllegalArgumentException if an entity value binding is used and
     * the primary key of the value given is different than the existing stored
     * primary key.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public void set(Object value) {

        if (!coll.hasValues()) throw new UnsupportedOperationException();
        if (!setAndRemoveAllowed) throw new IllegalStateException();
        try {
            moveToCurrent();
            cursor.put(null, value, Db.DB_CURRENT, null);
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Removes the last element that was returned by next or previous (optional
     * operation).
     * This method conforms to the {@link ListIterator#remove} interface except
     * that when the collection is a list and the RECNO-RENUMBER access method
     * is not used, list indices will not be renumbered.
     *
     * @throws UnsupportedOperationException if the collection is a sublist, or
     * if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public void remove() {

        if (!setAndRemoveAllowed) throw new IllegalStateException();
        try {
            moveToCurrent();
            cursor.delete();
            setAndRemoveAllowed = false;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Inserts the specified element into the list or inserts a duplicate into
     * other types of collections (optional operation).
     * This method conforms to the {@link ListIterator#add} interface when
     * the collection is a list and the RECNO-RENUMBER access method is used.
     * Otherwise, this method may only be called when duplicates are allowed.
     * If duplicates are unsorted, the new value will be inserted in the same
     * manner as list elements.
     * If duplicates are sorted, the new value will be inserted in sort order.
     *
     * @param value the new value.
     *
     * @throws UnsupportedOperationException if the collection is a sublist, or
     * if the collection is indexed, or if the collection is read-only, or if
     * the collection is a list and the RECNO-RENUMBER access method was not
     * used, or if the collection is not a list and duplicates are not allowed.
     *
     * @throws IllegalStateException if the collection is empty and is not a
     * list with RECNO-RENUMBER access.
     *
     * @throws IllegalArgumentException if a duplicate value is being added
     * that already exists and duplicates are sorted.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public void add(Object value) {

        coll.checkIterAddAllowed();
        try {
            int err =  0;
            if (toNext != 0 && toPrevious != 0) { // database is empty
                if (coll.view.areKeysRenumbered()) { // recno-renumber database
                    // close cursor during append and then reopen to support
                    // CDB restriction that append may not be called with a
                    // cursor open; note the append will still fail if the
                    // application has another cursor open
                    close();
                    err = coll.view.append(value, null, null);
                    cursor = new DataCursor(coll.view, writeAllowed);
                    reset();
                    next(); // move past new record
                } else { // hash/btree with duplicates
                    throw new IllegalStateException(
                        "Collection is empty, cannot add() duplicate");
                }
            } else { // database is not empty
                int flags;
                if (coll.view.areKeysRenumbered()) { // recno-renumber database
                    moveToCurrent();
                    flags = hasNext() ? Db.DB_BEFORE : Db.DB_AFTER;
                } else { // hash/btree with duplicates
                    flags = coll.areDuplicatesOrdered()
                                ? Db.DB_NODUPDATA
                                : (toNext == 0) ? Db.DB_BEFORE : Db.DB_AFTER;
                }
                err = cursor.put(null, value, flags, null, true);
                if (flags == Db.DB_BEFORE) {
                    toPrevious = 0;
                    toNext = NEXT_FLAG;
                }
            }
            if (err == Db.DB_KEYEXIST)
                throw new IllegalArgumentException("Duplicate value");
            else if (err != 0)
                throw new IllegalArgumentException("Could not insert: " + err);
            setAndRemoveAllowed = false;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    // --- end Iterator/ListIterator methods ---

    // hide for now
    private void reset() {

        toNext = Db.DB_FIRST;
        toPrevious = PREV_FLAG;
        toCurrent = 0;
        currentData = null;
        // initialize cursor at beginning to avoid "initial previous == last"
        // behavior when cursor is uninitialized
        hasNext();
    }

    /**
     * Returns the number of elements having the same key value as the key
     * value of the element last returned by next() or previous().  If no
     * duplicates are allowed, 1 is always returned.
     *
     * @return the number of duplicates.
     *
     * @throws IllegalStateException if next() or previous() has not been
     * called for this iterator, or if remove() or add() were called after
     * the last call to next() or previous().
     */
    public int count() {

        if (!setAndRemoveAllowed) throw new IllegalStateException();
        try {
            moveToCurrent();
            return cursor.count();
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Closes this iterator.
     * This method does not exist in the standard {@link Iterator} or {@link
     * ListIterator} interfaces.
     *
     * <p>After being closed, only the {@link #hasNext} and {@link
     * #hasPrevious} methods may be called and these will return false.  {@link
     * #close()} may also be called again and will do nothing.  If other
     * methods are called a <code>NullPointerException</code> will generally be
     * thrown.</p>
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public void close() {

        if (cursor != null) {
            coll.closeCursor(cursor);
            cursor = null;
        }
    }

    /**
     * Returns the collection associated with this iterator.
     * This method does not exist in the standard {@link Iterator} or {@link
     * ListIterator} interfaces.
     *
     * @return the collection associated with this iterator.
     */
    public final StoredCollection getCollection() {

        return coll;
    }

    /* hide for now
    private final Object current() {

        return currentData;
    }
    */

    /* hide for now
    private int currentIndex()
        throws IllegalStateException {

        if (!recNumAccess) {
            throw new UnsupportedOperationException(
                "Record number access not supported");
        }
        try {
            // TODO: don't we need to moveToCurrent?
            return cursor.getCurrentRecordNumber() - coll.getIndexOffset();
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }
    */

    /* hide for now
    final DataCursor getCursor() {

        return cursor;
    }
    */

    final boolean isCurrentData(Object currentData) {

        return (this.currentData == currentData);
    }

    final boolean moveToIndex(int index) {

        try {
            int err = cursor.get(new Integer(index), null, Db.DB_SET,
                                 lockForWrite);
            setAndRemoveAllowed = (err == 0);
            return setAndRemoveAllowed;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    private void moveToCurrent()
        throws DbException, IOException {

        if (toCurrent != 0) {
            cursor.get(null, null, toCurrent, lockForWrite);
            toCurrent = 0;
        }
    }
}
