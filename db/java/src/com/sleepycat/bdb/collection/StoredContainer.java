/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredContainer.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import com.sleepycat.bdb.CurrentTransaction;
import com.sleepycat.bdb.DataCursor;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.DataView;
import com.sleepycat.bdb.util.RuntimeExceptionWrapper;
import java.io.IOException;
import java.util.Collection;
import java.util.Iterator;
import java.util.Map;

/**
 * A abstract base class for all stored collections and maps.  This class
 * provides implementations of methods that are common to the {@link
 * Collection} and the {@link Map} interfaces, namely {@link #clear}, {@link
 * #isEmpty} and {@link #size}.
 *
 * <p>In addition, this class provides the following methods for stored
 * collections only.  Note that the use of these methods is not compatible with
 * the standard Java collections interface.</p>
 * <ul>
 * <li>{@link #isWriteAllowed()}</li>
 * <li>{@link #isIndexed()}</li>
 * <li>{@link #isOrdered()}</li>
 * <li>{@link #areDuplicatesAllowed()}</li>
 * <li>{@link #areDuplicatesOrdered()}</li>
 * <li>{@link #areKeysRenumbered()}</li>
 * <li>{@link #isDirtyReadAllowed()}</li>
 * <li>{@link #isDirtyReadEnabled()}</li>
 * <li>{@link #isAutoCommit()}</li>
 * <li>{@link #isTransactional()}</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class StoredContainer implements Cloneable {

    DataView view;

    StoredContainer(DataView view) {

        this.view = view;
    }

    /**
     * Returns true if this is a read-write container or false if this is a
     * read-only container.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether write is allowed.
     */
    public final boolean isWriteAllowed() {

        return view.isWriteAllowed();
    }

    /**
     * Returns whether dirty-read is allowed for this container.
     * Dirty-read is allowed if Db.DB_DIRTY_READ was specified when opening the
     * database of the underlying Store and Index (if any) for this container.
     * Even when dirty-read is allowed it must specifically be enabled by
     * calling one of the {@link StoredCollections} methods.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether dirty-read is allowed.
     */
    public final boolean isDirtyReadAllowed() {

        return view.isDirtyReadAllowed();
    }

    /**
     * Returns whether dirty-read is enabled for this container.
     * If dirty-read is enabled, data will be read that is modified but not
     * committed.
     * Dirty-read is disabled by default.
     * This method always returns false if {@link #isDirtyReadAllowed} returns
     * false.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether dirty-read is enabled.
     */
    public final boolean isDirtyReadEnabled() {

        return view.isDirtyReadEnabled();
    }

    /**
     * Returns whether auto-commit is enabled for this container or for its
     * associated {@link DbEnv}.
     * If auto-commit is enabled, a transaction will be started and committed
     * automatically for each write operation when no transaction is active.
     * Auto-commit only applies to container methods.  It does not apply to
     * iterator methods and these always require an explict transaction.
     * Auto-commit is disabled by default for a container but may be enabled
     * for an entire environment using {@link DbEnv#setFlags}.
     * This method always returns false if {@link #isTransactional} returns
     * false.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether auto-commit is enabled.
     */
    public final boolean isAutoCommit() {

        return view.isAutoCommit();
    }

    /**
     * Returns whether the databases underlying this container are
     * transactional.
     * Even in a transactional environment, a database will be transactional
     * only if it was opened within a transaction or if the auto-commit option
     * was specified when it was opened.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether the database is transactional.
     */
    public final boolean isTransactional() {

        return view.isTransactional();
    }

    final StoredContainer dirtyReadClone() {

        if (!isDirtyReadAllowed())
            return this;
        try {
            StoredContainer cont = (StoredContainer) clone();
            cont.view = cont.view.dirtyReadView(true);
            return cont;
        } catch (CloneNotSupportedException willNeverOccur) { return null; }
    }

    final StoredContainer autoCommitClone() {

        if (!isTransactional())
            return this;
        try {
            StoredContainer cont = (StoredContainer) clone();
            cont.view = cont.view.autoCommitView(true);
            return cont;
        } catch (CloneNotSupportedException willNeverOccur) { return null; }
    }

    /**
     * Returns whether duplicate keys are allowed in this container.
     * Duplicates are optionally allowed for HASH and BTREE databases.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether duplicates are allowed.
     */
    public final boolean areDuplicatesAllowed() {

        return view.areDuplicatesAllowed();
    }

    /**
     * Returns whether duplicate keys are allowed and sorted by element value.
     * Duplicates are optionally sorted for HASH and BTREE databases.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether duplicates are ordered.
     */
    public final boolean areDuplicatesOrdered() {

        return view.areDuplicatesOrdered();
    }

    /**
     * Returns whether keys are renumbered when insertions and deletions occur.
     * Keys are optionally renumbered for RECNO databases.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether keys are renumbered.
     */
    public final boolean areKeysRenumbered() {

        return view.areKeysRenumbered();
    }

    /**
     * Returns whether keys are ordered in this container.
     * Keys are ordered for BTREE, RECNO and QUEUE database.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether keys are ordered.
     */
    public final boolean isOrdered() {

        return view.isOrdered();
    }

    /**
     * Returns whether this container is a view on a {@link DataIndex} rather
     * than directly on a {@link DataStore}.
     * This method does not exist in the standard {@link Map} or {@link
     * Collection} interfaces.
     *
     * @return whether the view is indexed.
     */
    public final boolean isIndexed() {

        return (view.getIndex() != null);
    }

    /**
     * Always throws UnsupportedOperationException.  The size of a database
     * cannot be obtained reliably or inexpensively.
     * This method therefore violates the {@link Collection#size} and {@link
     * Map#size} interfaces.
     *
     * @return always throws an exception.
     *
     * @throws UnsupportedOperationException unconditionally.
     */
    public int size() {

        throw new UnsupportedOperationException(
            "collection size not available");
    }

    /**
     * Returns true if this map or collection contains no mappings or elements.
     * This method conforms to the {@link Collection#isEmpty} and {@link
     * Map#isEmpty} interfaces.
     *
     * @return whether the container is empty.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean isEmpty() {

        try {
            return view.isEmpty();
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Removes all mappings or elements from this map or collection (optional
     * operation).
     * This method conforms to the {@link Collection#clear} and {@link
     * Map#clear} interfaces.
     *
     * @throws UnsupportedOperationException if the container is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public void clear() {

        boolean doAutoCommit = beginAutoCommit();
        try {
            view.clear(null);
            commitAutoCommit(doAutoCommit);
        } catch (Exception e) {
            throw handleException(e, doAutoCommit);
        }
    }

    Object get(Object key) {

        try {
            Object[] ret = new Object[1];
            int err = view.get(key, null, Db.DB_SET, false, ret);
            return (err == 0) ? ret[0] : null;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    Object put(final Object key, final Object value) {

        boolean doAutoCommit = beginAutoCommit();
        try {
            Object[] oldValue = new Object[1];
            view.put(key, value, 0, oldValue);
            commitAutoCommit(doAutoCommit);
            return oldValue[0];
        } catch (Exception e) {
            throw handleException(e, doAutoCommit);
        }
    }

    final boolean removeKey(final Object key, final Object[] oldVal) {

        DataCursor cursor = null;
        boolean doAutoCommit = beginAutoCommit();
        try {
            cursor = new DataCursor(view, true);
            boolean found = false;
            int err = cursor.get(key, null, Db.DB_SET, true);
            while (err == 0) {
                cursor.delete();
                found = true;
                if (oldVal != null && oldVal[0] == null)
                    oldVal[0] = cursor.getCurrentValue();
                err = cursor.get(null, null, Db.DB_NEXT_DUP, true);
            }
            closeCursor(cursor);
            commitAutoCommit(doAutoCommit);
            return found;
        } catch (Exception e) {
            closeCursor(cursor);
            throw handleException(e, doAutoCommit);
        }
    }

    boolean containsKey(Object key) {

        DataCursor cursor = null;
        try {
            cursor = new DataCursor(view, false);
            int err = cursor.get(key, null, Db.DB_SET, false);
            return (err == 0);
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        } finally {
            closeCursor(cursor);
        }
    }

    final boolean removeValue(Object value) {

        DataCursor cursor = null;
        boolean doAutoCommit = beginAutoCommit();
        try {
            cursor = new DataCursor(view, true);
            int err = cursor.find(value, true);
            if (err == 0) {
                cursor.delete();
            }
            closeCursor(cursor);
            commitAutoCommit(doAutoCommit);
            return (err == 0);
        } catch (Exception e) {
            closeCursor(cursor);
            throw handleException(e, doAutoCommit);
        }
    }

    boolean containsValue(Object value) {

        DataCursor cursor = null;
        try {
            cursor = new DataCursor(view, false);
            int err = cursor.find(value, true);
            return (err == 0);
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        } finally {
            closeCursor(cursor);
        }
    }

    final void closeCursor(DataCursor cursor) {

        if (cursor != null) {
            try {
                cursor.close();
            } catch (Exception e) {
                throw StoredContainer.convertException(e);
            }
        }
    }

    final boolean beginAutoCommit() {

        if (isAutoCommit()) {
            try {
                CurrentTransaction currentTxn = view.getCurrentTxn();
                if (currentTxn.getTxn() == null) {
                    currentTxn.beginTxn();
                    return true;
                }
            } catch (DbException e) {
                throw new RuntimeExceptionWrapper(e);
            }
        }
        return false;
    }

    final void commitAutoCommit(boolean doAutoCommit)
        throws DbException {

        if (doAutoCommit) view.getCurrentTxn().commitTxn();
    }

    final RuntimeException handleException(Exception e, boolean doAutoCommit) {

        if (doAutoCommit) {
            try {
                view.getCurrentTxn().abortTxn();
            } catch (DbException ignored) {
            }
        }
        return StoredContainer.convertException(e);
    }

    static RuntimeException convertException(Exception e) {

        if (e instanceof RuntimeException) {
            return (RuntimeException) e;
        } else {
            return new RuntimeExceptionWrapper(e);
        }
    }
}
