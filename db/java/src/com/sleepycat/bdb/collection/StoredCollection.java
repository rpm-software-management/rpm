/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredCollection.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import com.sleepycat.bdb.DataCursor;
import com.sleepycat.bdb.DataView;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbException;
import com.sleepycat.bdb.util.RuntimeExceptionWrapper;
import java.io.IOException;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

/**
 * A abstract base class for all stored collections.  This class, and its
 * base class {@link StoredContainer}, provide implementations of most methods
 * in the {@link Collection} interface.  Other methods, such as {@link #add}
 * and {@link #remove}, are provided by concrete classes that extend this
 * class.
 *
 * <p>In addition, this class provides the following methods for stored
 * collections only.  Note that the use of these methods is not compatible with
 * the standard Java collections interface.</p>
 * <ul>
 * <li>{@link #iterator(boolean)}</li>
 * <li>{@link #join(StoredContainer[], Object[])}</li>
 * <li>{@link #join(StoredContainer[], Object[], boolean)}</li>
 * <li>{@link #toList()}</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class StoredCollection extends StoredContainer
    implements Collection {

    StoredCollection(DataView view) {

        super(view);
    }

    final boolean add(Object key, Object value) {

        boolean doAutoCommit = beginAutoCommit();
        try {
            int err = view.put(key, value, Db.DB_NODUPDATA, null);
            commitAutoCommit(doAutoCommit);
            return (err == 0);
        } catch (Exception e) {
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Returns an iterator over the elements in this collection.
     * The iterator will be read-only if the collection is read-only.
     * This method conforms to the {@link Collection#iterator} interface.
     *
     * @return a {@link StoredIterator} for this collection.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     *
     * @see #isWriteAllowed
     */
    public Iterator iterator() {

        return iterator(isWriteAllowed());
    }

    /**
     * Returns a read or read-write iterator over the elements in this
     * collection.
     * This method does not exist in the standard {@link Collection} interface.
     *
     * @param writeAllowed is true to open a read-write iterator or false to
     * open a read-only iterator.  If the collection is read-only the iterator
     * will always be read-only.
     *
     * @return a {@link StoredIterator} for this collection.
     *
     * @throws IllegalStateException if writeAllowed is true but the collection
     * is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     *
     * @see #isWriteAllowed
     */
    public StoredIterator iterator(boolean writeAllowed) {

        try {
            return new StoredIterator(this, writeAllowed && isWriteAllowed(),
                                      null);
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Returns an array of all the elements in this collection.
     * This method conforms to the {@link Collection#toArray()} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object[] toArray() {

        ArrayList list = new ArrayList();
        Iterator i = iterator();
        try {
            while (i.hasNext()) {
                list.add(i.next());
            }
        } finally {
            StoredIterator.close(i);
        }
        return list.toArray();
    }

    /**
     * Returns an array of all the elements in this collection whose runtime
     * type is that of the specified array.
     * This method conforms to the {@link Collection#toArray(Object[])}
     * interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object[] toArray(Object[] a) {

        int j = 0;
        Iterator i = iterator();
        try {
            while (j < a.length && i.hasNext()) {
                a[j++] = i.next();
            }
            if (j < a.length) {
                a[j] = null;
            } else if (i.hasNext()) {
                ArrayList list = new ArrayList(Arrays.asList(a));
                while (i.hasNext()) {
                    list.add(i.next());
                }
                a = list.toArray(a);
            }
        } finally {
            StoredIterator.close(i);
        }
        return a;
    }

    /**
     * Returns true if this collection contains all of the elements in the
     * specified collection.
     * This method conforms to the {@link Collection#containsAll} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean containsAll(Collection coll) {
	Iterator i = coll.iterator();
        try {
            while (i.hasNext()) {
                if (!contains(i.next())) {
                    return false;
                }
            }
        } finally {
            StoredIterator.close(i);
        }
	return true;
    }

    /**
     * Adds all of the elements in the specified collection to this collection
     * (optional operation).
     * This method calls the {@link #add(Object)} method of the concrete
     * collection class, which may or may not be supported.
     * This method conforms to the {@link Collection#addAll} interface.
     *
     * @throws UnsupportedOperationException if the collection is read-only, or
     * if the collection is indexed, or if the add method is not supported by
     * the concrete collection.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean addAll(Collection coll) {
	Iterator i = null;
        boolean doAutoCommit = beginAutoCommit();
        try {
            i = coll.iterator();
            boolean changed = false;
            while (i.hasNext()) {
                if (add(i.next())) {
                    changed = true;
                }
            }
            StoredIterator.close(i);
            commitAutoCommit(doAutoCommit);
            return changed;
        } catch (Exception e) {
            StoredIterator.close(i);
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Removes all this collection's elements that are also contained in the
     * specified collection (optional operation).
     * This method conforms to the {@link Collection#removeAll} interface.
     *
     * @throws UnsupportedOperationException if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean removeAll(Collection coll) {

        return removeAll(coll, true);
    }

    /**
     * Retains only the elements in this collection that are contained in the
     * specified collection (optional operation).
     * This method conforms to the {@link Collection#removeAll} interface.
     *
     * @throws UnsupportedOperationException if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean retainAll(Collection coll) {

        return removeAll(coll, false);
    }

    private boolean removeAll(Collection coll, boolean ifExistsInColl) {
	Iterator i = null;
        boolean doAutoCommit = beginAutoCommit();
        try {
            boolean changed = false;
            i = iterator();
            while (i.hasNext()) {
                if (ifExistsInColl == coll.contains(i.next())) {
                    i.remove();
                    changed = true;
                }
            }
            StoredIterator.close(i);
            commitAutoCommit(doAutoCommit);
            return changed;
        } catch (Exception e) {
            StoredIterator.close(i);
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Compares the specified object with this collection for equality.
     * A value comparison is performed by this method and the stored values
     * are compared rather than calling the equals() method of each element.
     * This method conforms to the {@link Collection#equals} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean equals(Object other) {

        if (other instanceof Collection) {
            Collection otherColl = StoredCollection.copyCollection(other);
            Iterator i = iterator();
            try {
                while (i.hasNext()) {
                    if (!otherColl.remove(i.next())) {
                        return false;
                    }
                }
                return otherColl.isEmpty();
            } finally {
                StoredIterator.close(i);
            }
        } else {
            return false;
        }
    }

    /**
     * Returns a copy of this collection as an ArrayList.  This is the same as
     * {@link #toArray()} but returns a collection instead of an array.
     *
     * @return an {@link ArrayList} containing a copy of all elements in this
     * collection.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public List toList() {

        ArrayList list = new ArrayList();
        Iterator i = iterator();
        try {
            while (i.hasNext()) list.add(i.next());
            return list;
        } finally {
            StoredIterator.close(i);
        }
    }

    /**
     * Converts the collection to a string representation for debugging.
     * WARNING: All elements will be converted to strings and returned and
     * therefore the returned string may be very large.
     *
     * @return the string representation.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public String toString() {
	StringBuffer buf = new StringBuffer();
	buf.append("[");
	Iterator i = iterator();
        try {
            while (i.hasNext()) {
                if (buf.length() > 1) buf.append(',');
                buf.append(i.next().toString());
            }
            buf.append(']');
            return buf.toString();
        } finally {
            StoredIterator.close(i);
        }
    }

    /**
     * Returns an iterator representing an equality join of the indices and
     * index key values specified.
     * The indices will be sorted by least number of references, which is
     * commonly the best optimization.
     * This method does not exist in the standard {@link Collection} interface.
     *
     * <p>The returned iterator supports only the two methods: hasNext() and
     * next().  All other methods will throw UnsupportedOperationException.</p>
     *
     * @param indices is an array of indices with elements corresponding to
     * those in the indexKeys array.
     *
     * @param indexKeys is an array of index key values identifying the
     * elements to be selected.
     *
     * @return an iterator over the elements in this collection that match
     * all specified index key values.
     *
     * @throws IllegalArgumentException if this collection is indexed or if a
     * given index does not have the same store as this collection.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredIterator join(StoredContainer[] indices, Object[] indexKeys) {

        return join(indices, indexKeys, false);
    }

    /**
     * Returns an iterator representing an equality join of the indices and
     * index key values specified.
     * The indices may be presorted to allow custom optimizations.
     * This method does not exist in the standard {@link Collection} interface.
     *
     * <p>The returned iterator supports only the two methods: hasNext() and
     * next().  All other methods will throw UnsupportedOperationException.</p>
     *
     * @param indices is an array of indices with elements corresponding to
     * those in the indexKeys array.
     *
     * @param indexKeys is an array of index key values identifying the
     * elements to be selected.
     *
     * @param presorted is true if the index order should not be changed, or
     * false to use the default sorting by least number of references.
     *
     * @return an iterator over the elements in this collection that match
     * all specified index key values.
     *
     * @throws IllegalArgumentException if this collection is indexed or if a
     * given index does not have the same store as this collection.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredIterator join(StoredContainer[] indices, Object[] indexKeys,
                               boolean presorted) {

        try {
            DataView[] indexViews = new DataView[indices.length];
            for (int i = 0; i < indices.length; i += 1) {
                indexViews[i] = indices[i].view;
            }
            DataCursor cursor = view.join(indexViews, indexKeys, presorted);
            return new StoredIterator(this, false, cursor);
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    final Object getFirstOrLast(boolean doGetFirst) {

        DataCursor cursor = null;
        try {
            cursor = new DataCursor(view, false);
            int err = cursor.get(null, null,
                                 doGetFirst ? Db.DB_FIRST : Db.DB_LAST,
                                 false);
            return (err == 0) ? makeIteratorData(null, cursor) : null;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        } finally {
            closeCursor(cursor);
        }
    }

    abstract Object makeIteratorData(StoredIterator iterator,
                                     DataCursor cursor)
        throws DbException, IOException;

    abstract boolean hasValues();

    boolean iterateDuplicates() {

        return true;
    }

    void checkIterAddAllowed()
        throws UnsupportedOperationException {

        if (!areDuplicatesAllowed()) {
            throw new UnsupportedOperationException("duplicates required");
        }
    }

    int getIndexOffset() {

        return 0;
    }

    private static Collection copyCollection(Object other) {

        if (other instanceof StoredCollection) {
            return ((StoredCollection) other).toList();
        } else {
            return new ArrayList((Collection) other);
        }
    }
}
