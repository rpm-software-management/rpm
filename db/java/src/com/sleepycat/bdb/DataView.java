/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataView.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.KeyExtractor;
import com.sleepycat.db.Db;
import com.sleepycat.db.Dbc;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import java.io.IOException;
import java.util.Collection;

/**
 * (<em>internal</em>) Represents a Berkeley DB database and adds support
 * for indices, bindings and key ranges.
 *
 * <p><b>NOTE:</b> This classes is internal and may be changed incompatibly or
 * deleted in the future.  It is public only so it may be used by
 * subpackages.</p>
 *
 * <p>This class defines a view and takes care of reading and updating indices,
 * calling bindings, constraining access to a key range, etc.</p>
 *
 * @author Mark Hayes
 */
public final class DataView implements Cloneable {

    private static final KeyRange NULL_RANGE = new KeyRange();

    DataDb db;
    DataStore store;
    DataIndex index;
    KeyRange range;
    boolean writeAllowed;
    boolean dirtyRead;
    boolean transactional;
    boolean dirtyReadAllowed;
    boolean autoCommit;
    DataBinding keyBinding;
    DataBinding valueBinding;
    EntityBinding entityBinding;
    boolean recNumAccess;
    boolean btreeRecNumAccess;

    /**
     * Creates a view for a given store/index and bindings.  The key range of
     * the view will be open.
     *
     * @param store is the store or is ignored if the index parameter is given.
     *
     * @param index is the index or null if no index is used.
     *
     * @param keyBinding is the key binding or null if keys will not be used.
     *
     * @param valueBinding is the value binding or null if an entityBinding is
     * given or if values will not be used.
     *
     * @param entityBinding is the entity binding or null if an valueBinding
     * is given or if values will not be used.
     *
     * @param writeAllowed is whether writing through this view is allowed.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     */
    public DataView(DataStore store, DataIndex index,
                    DataBinding keyBinding, DataBinding valueBinding,
                    EntityBinding entityBinding, boolean writeAllowed)
        throws IllegalArgumentException {

        if (index != null) {
            this.db = index.db;
            this.index = index;
            this.store = index.store;
        } else {
            if (store == null)
                throw new IllegalArgumentException(
                    "both store and index are null");
            this.db = store.db;
            this.store = store;
        }
        this.writeAllowed = writeAllowed;
        this.range = NULL_RANGE;
        this.keyBinding = keyBinding;
        this.valueBinding = valueBinding;
        this.entityBinding = entityBinding;
        this.transactional = db.isTransactional();
        this.dirtyReadAllowed = transactional &&
                        (store == null || store.db.isDirtyReadAllowed()) &&
                        (index == null || index.db.isDirtyReadAllowed());

        if (valueBinding != null && entityBinding != null)
            throw new IllegalArgumentException(
                "both valueBinding and entityBinding are non-null");

        if (keyBinding != null &&
            keyBinding.getDataFormat() instanceof RecordNumberFormat) {
            if (!db.hasRecNumAccess()) {
                throw new IllegalArgumentException(
                    "RecordNumberFormat requires DB_BTREE/DB_RECNUM, " +
                    "DB_RECNO, or DB_QUEUE");
            }
            recNumAccess = true;
            if (db.type == Db.DB_BTREE) {
                btreeRecNumAccess = true;
            }
        }

        checkBindingFormats();
    }

    /**
     * Clone the view.
     */
    private DataView cloneView() {

        try {
            return (DataView) super.clone();
        }
        catch (CloneNotSupportedException willNeverOccur) { return null; }
    }

    /**
     * Return a new key-set view derived from this view by setting the
     * entity and value binding to null.
     *
     * @return the derived view.
     */
    public DataView keySetView() {

        if (keyBinding == null) {
            throw new UnsupportedOperationException("must have keyBinding");
        }
        DataView view = cloneView();
        view.valueBinding = null;
        view.entityBinding = null;
        return view;
    }

    /**
     * Return a new value-set view derived from this view by setting the
     * key binding to null.
     *
     * @return the derived view.
     */
    public DataView valueSetView() {

        if (valueBinding == null && entityBinding == null) {
            throw new UnsupportedOperationException(
                "must have valueBinding or entityBinding");
        }
        DataView view = cloneView();
        view.keyBinding = null;
        return view;
    }

    /**
     * Return a new value-set view for single key range.
     *
     * @param singleKey the single key value.
     *
     * @return the derived view.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     *
     * @throws KeyRangeException if the specified range is not within the
     * current range.
     */
    public DataView valueSetView(Object singleKey)
        throws DbException, IOException, KeyRangeException {

        // must do subRange before valueSetView since the latter clears the
        // key binding needed for the former
        KeyRange singleKeyRange = subRange(singleKey);
        DataView view = valueSetView();
        view.range = singleKeyRange;
        return view;
    }

    /**
     * Return a new value-set view for key range, optionally changing
     * the key binding.
     *
     * @param beginKey the lower bound.
     *
     * @param beginInclusive whether the lower bound is inclusive.
     *
     * @param endKey the upper bound.
     *
     * @param endInclusive whether the upper bound is inclusive.
     *
     * @param keyBinding a key binding to use, or null to retain the base
     * view's key binding.
     *
     * @return the derived view.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     *
     * @throws KeyRangeException if the specified range is not within the
     * current range.
     */
    public DataView subView(Object beginKey, boolean beginInclusive,
                            Object endKey, boolean endInclusive,
                            DataBinding keyBinding)
        throws DbException, IOException, KeyRangeException {

        DataView view = cloneView();
        view.setRange(beginKey, beginInclusive, endKey, endInclusive);
        if (keyBinding != null) view.keyBinding = keyBinding;
        return view;
    }

    /**
     * Returns a new view with a specified dirtyRead setting.
     *
     * @param enable whether to enable or disable dirty-read.
     *
     * @return the derived view.
     */
    public DataView dirtyReadView(boolean enable) {

        if (!isDirtyReadAllowed())
            return this;
        DataView view = cloneView();
        view.dirtyRead = enable;
        return view;
    }

    /**
     * Returns a new view with a specified autoCommit setting.
     * Note that auto-commit is not implemented by the view, the view only
     * holds the auto-commit property.
     *
     * @param enable whether to enable or disable auto-commit.
     *
     * @return the derived view.
     */
    public DataView autoCommitView(boolean enable) {

        if (!isTransactional())
            return this;
        DataView view = cloneView();
        view.autoCommit = enable;
        return view;
    }

    /**
     * Returns the current transaction for the view or null if the environment
     * is non-transactional.
     *
     * @return the current transaction.
     */
    public CurrentTransaction getCurrentTxn() {

        return isTransactional() ? db.env : null;
    }

    private void setRange(Object beginKey, boolean beginInclusive,
                          Object endKey, boolean endInclusive)
        throws DbException, IOException, KeyRangeException {

        range = subRange(beginKey, beginInclusive, endKey, endInclusive);
    }

    /**
     * Returns the key thang for a single key range, or null if a single key
     * range is not used.
     *
     * @return the key thang or null.
     */
    public DataThang getSingleKeyThang() {

        return range.getSingleKey();
    }

    /**
     * Returns the database for the index, if one is used, or store, if no
     * index is used.
     *
     * @return the database of the index or, if none, the store.
     */
    public DataDb getDb() {

        return db;
    }

    /**
     * Returns the environment for the store and index.
     *
     * @return the environment.
     */
    public final DbEnv getEnv() {

        return db.env.getEnv();
    }

    /**
     * Returns whether auto-commit is set for this view or for the
     * transactional environment of the store and index.
     * Note that auto-commit is not implemented by the view, the view only
     * holds the auto-commit property.
     *
     * @return the auto-commit setting.
     */
    public final boolean isAutoCommit() {

        return autoCommit || db.env.isAutoCommit();
    }

    /**
     * Returns the store, as specified to the constructor.
     *
     * @return the store.
     */
    public final DataStore getStore() {

        return store;
    }

    /**
     * Returns the index, as specified to the constructor.
     *
     * @return the index or null.
     */
    public final DataIndex getIndex() {

        return index;
    }

    /**
     * Returns the key binding that is used.
     *
     * @return the key binding or null.
     */
    public final DataBinding getKeyBinding() {

        return keyBinding;
    }

    /**
     * Returns the value binding that is used.
     *
     * @return the value binding or null.
     */
    public final DataBinding getValueBinding() {

        return valueBinding;
    }

    /**
     * Returns the entity binding that is used.
     *
     * @return the entity binding or null.
     */
    public final EntityBinding getValueEntityBinding() {

        return entityBinding;
    }

    /**
     * Returns whether duplicates are allowed for the index or store.
     *
     * @return whether duplicates are allowed.
     */
    public final boolean areDuplicatesAllowed() {

        return db.areDuplicatesAllowed();
    }

    /**
     * Returns whether duplicates are ordered for the index or store.
     *
     * @return whether duplicates are ordered.
     */
    public final boolean areDuplicatesOrdered() {

        return db.areDuplicatesOrdered();
    }

    /**
     * Returns whether keys (record numbers) are renumbered for the index or
     * store.
     *
     * @return whether keys are renumbered.
     */
    public final boolean areKeysRenumbered() {

        return btreeRecNumAccess || db.areKeysRenumbered();
    }

    /**
     * Returns whether keys are ordered for the index or store.
     *
     * @return whether keys are ordered.
     */
    public final boolean isOrdered() {

        return db.isOrdered();
    }

    /**
     * Returns whether write operations are allowed.
     *
     * @return whether write operations are allowed.
     */
    public final boolean isWriteAllowed() {

        return writeAllowed;
    }

    /**
     * Returns whether DIRTY_READ was specified for both the Store and Index.
     *
     * @return whether dirty-read is allowed.
     */
    public final boolean isDirtyReadAllowed() {

        return dirtyReadAllowed;
    }

    /**
     * Returns whether DIRTY_READ will be used for all read operations.
     *
     * @return whether dirty-read is enabled.
     */
    public final boolean isDirtyReadEnabled() {

        return dirtyRead;
    }

    /**
     * Returns whether the store and index are transactional.
     *
     * @return whether the store and index are transactional.
     */
    public final boolean isTransactional() {

        return transactional;
    }

    /**
     * Returns whether no records are present in the view.
     *
     * @return whether the view is empty.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public boolean isEmpty()
        throws DbException, IOException {

        Dbc cursor = db.openCursor(false);
        try {
            // val is always unused (discarded) but key is needed for bounds
            // checking if the range has a bound
            DataThang val = DataThang.getDiscardDataThang();
            DataThang key = (range.hasBound()) ? (new DataThang()) : val;
            int flags = Db.DB_FIRST;
            if (dirtyRead) flags |= Db.DB_DIRTY_READ;
            int err = range.get(db, cursor, key, val, flags);
            return (err != 0 && err != DataDb.ENOMEM);
        } finally {
            db.closeCursor(cursor);
        }
    }

    /**
     * Performs a general database 'get' operation.
     *
     * @param key used to find the value
     *
     * @param value used to find the value
     *
     * @param flags all flags except DB_SET and DB_GET_BOTH
     * are legal, {@link com.sleepycat.db.Db#get(DbTxn,Dbt,Dbt,int)}.
     *
     * @param lockForWrite if true locks the cursor during the get.
     *
     * @param retValue used to store the result of the query
     *
     * @return 0 if mathing values are found, Db.DB_NOTFOUND if not.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int get(Object key, Object value, int flags, boolean lockForWrite,
                   Object[] retValue)
        throws DbException, IOException {
        
        // Only 0/SET/GET_BOTH flags are allowed.

        if (flags == 0) {
            flags = Db.DB_SET;
        } else if (flags != Db.DB_SET && flags != Db.DB_GET_BOTH) {
            throw new IllegalArgumentException("flag not allowed");
        }
        DataCursor cursor = null;
        try {
            cursor = new DataCursor(this, false);
            int err = cursor.get(key, value, flags, lockForWrite);
            if (err == 0 && retValue != null) {
                retValue[0] = cursor.getCurrentValue();
            }
            return err;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    /**
     * Performs a database 'get and consume' operation.
     *
     * @param flags must be CONSUME or CONSUME_WAIT.
     *
     * @param retPrimaryKey used to store the resulting key.
     *
     * @param retValue used to store the resulting value.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int consume(int flags, Object[] retPrimaryKey, Object[] retValue)
        throws DbException, IOException {

        // Does not respect the range.
        // Requires: write allowed
        // Requires: if retPrimaryKey, primary key binding (no index).
        // Requires: if retValue, value or entity binding
         
        if (!writeAllowed) {
            throw new UnsupportedOperationException("write not allowed");
        }
        if (flags != Db.DB_CONSUME && flags != Db.DB_CONSUME_WAIT) {
            throw new IllegalArgumentException("flag not allowed");
        }
        DataThang keyThang = new DataThang();
        DataThang valueThang = new DataThang();
        int err = store.db.get(keyThang, valueThang, flags);
        if (err == 0) {
            store.applyChange(keyThang, valueThang, null);
            returnPrimaryKeyAndValue(keyThang, valueThang,
                                     retPrimaryKey, retValue);
        }
        return err;
    }

    /**
     * Performs a database 'put' operation, optionally returning the old value.
     *
     * @param primaryKey key of new record.
     *
     * @param value value of new record.
     *
     * @param flags must be 0, NODUPDATA or NOOVERWRITE.
     *
     * @param oldValue used to store the old value, or null if none should be
     * returned.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int put(Object primaryKey, Object value, int flags,
                   Object[] oldValue)
        throws DbException, IOException {
     
        // Returns old value if key already exists and no duplicates.
        // Requires: write allowed
        // Requires: if no index, key or entity binding with key or value
        // Requires: if index, entity binding with value param and null key
        // Requires: if value param, value or entity binding
        // Requires: if oldValue param, value or entity binding

        if (!writeAllowed) {
            throw new UnsupportedOperationException("write not allowed");
        }
        if (flags != 0 && flags != Db.DB_NOOVERWRITE &&
            flags != Db.DB_NODUPDATA) {
            throw new IllegalArgumentException("flags not allowed: " + flags);
        }
        if (index != null) {
            throw new UnsupportedOperationException("cannot put() with index");
        }
        if (oldValue != null) {
            oldValue[0] = null;
        }
        DataThang keyThang = new DataThang();
        DataThang valueThang = new DataThang();
        DataThang oldValueThang = null;
        int err = useKey(primaryKey, value, keyThang, range);
        if (err != 0) {
            throw new IllegalArgumentException("primaryKey out of range " +
                                                keyThang + range);
        }
        if (flags == 0 && !areDuplicatesAllowed()) {
            oldValueThang = new DataThang();
            err = store.db.get(keyThang, oldValueThang,
                               db.env.getWriteLockFlag());
            if (err == 0) {
                if (oldValue != null) {
                    oldValue[0] = makeValue(keyThang, oldValueThang);
                }
            } else {
                oldValueThang = null;
            }
        }
        useValue(value, valueThang, null);
        err = store.db.put(keyThang, valueThang, flags);
        if (err == 0) {
            store.applyChange(keyThang, oldValueThang, valueThang);
        }
        return err;
    }

    /**
     * Adds a duplicate value for a specified key.
     *
     * @param primaryKeyThang key of new record.
     *
     * @param value value of new record.
     *
     * @param flags must be 0 or NODUPDATA or KEYFIRST or KEYLAST.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int addValue(DataThang primaryKeyThang, Object value, int flags)
        throws DbException, IOException {

        if (!writeAllowed) {
            throw new UnsupportedOperationException("write not allowed");
        }
        if (!areDuplicatesAllowed()) {
            throw new UnsupportedOperationException("duplicates required");
        }
        if (flags != 0 && flags != Db.DB_NODUPDATA &&
            flags != Db.DB_KEYFIRST && flags != Db.DB_KEYLAST) {
            throw new IllegalArgumentException("flags not allowed: " + flags);
        }
        DataThang valueThang = new DataThang();
        if (!range.check(primaryKeyThang)) {
            throw new IllegalArgumentException("primaryKey out of range");
        }
        useValue(value, valueThang, null);
        int err = store.db.put(primaryKeyThang, valueThang, flags);
        if (err == 0) {
            store.applyChange(primaryKeyThang, null, valueThang);
        }
        return err;
    }

    /**
     * Appends a value and returns the new key.  If a key assigner is used
     * it assigns the key, otherwise a QUEUE or RECNO database is required.
     *
     * @param value is the value to append.
     *
     * @param retPrimaryKey used to store the assigned key.
     *
     * @param retValue used to store the resulting entity, or null if none
     * should be returned.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int append(Object value, Object[] retPrimaryKey, Object[] retValue)
        throws DbException, IOException {

        // Flags will be NOOVERWRITE if used with assigner, or APPEND
        // otherwise.
        // Requires: if value param, value or entity binding
        // Requires: if retPrimaryKey, primary key binding (no index).
        // Requires: if retValue, value or entity binding

        if (!writeAllowed) {
            throw new UnsupportedOperationException("write not allowed");
        }
        DataThang keyThang = new DataThang();
        DataThang valueThang = new DataThang();
        int flags;
        if (store.keyAssigner != null) {
            store.keyAssigner.assignKey(keyThang);
            if (!range.check(keyThang)) {
                throw new IllegalArgumentException(
                    "assigned key out of range");
            }
            flags = Db.DB_NOOVERWRITE;
        } else {
            if (db.type != Db.DB_QUEUE && db.type != Db.DB_RECNO) {
                throw new UnsupportedOperationException(
                    "DB_QUEUE or DB_RECNO type is required");
            }
            flags = Db.DB_APPEND; // assume RECNO access method
        }
        useValue(value, valueThang, null);
        int err = store.db.put(keyThang, valueThang, flags);
        if (err == 0) {
            if (store.keyAssigner == null && !range.check(keyThang)) {
                store.db.delete(keyThang, 0);
                throw new IllegalArgumentException(
                    "appended record number out of range");
            }
            store.applyChange(keyThang, null, valueThang);
            returnPrimaryKeyAndValue(keyThang, valueThang,
                                     retPrimaryKey, retValue);
        }
        return err;
    }

    /**
     * Deletes all records in the current range, optionally returning the
     * values for the deleted records.
     *
     * @param oldValues is used to store the values that are cleared, or null
     * if the old values should not be returned.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public void clear(Collection oldValues)
        throws DbException, IOException {

        DataCursor cursor = null;
        try {
            cursor = new DataCursor(this, true);
            int op = areKeysRenumbered() ? Db.DB_FIRST : Db.DB_NEXT;
            int err = 0;
            while (err == 0) {
                err = cursor.get(null, null, op, true);
                if (err == 0) {
                    if (oldValues != null) {
                        oldValues.add(cursor.getCurrentValue());
                    }
                    err = cursor.delete();
                    if (err != 0)
                        throw new DbException(
                            "Unexpected error on delete", err);
                }
            }
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    /**
     * Returns a cursor for this view that reads only records having the
     * specified index key values.
     *
     * @param indexViews are the views to be joined.
     *
     * @param indexKeys are the keys to join on for each view.
     *
     * @param presorted is whether the given views are presorted or should be
     * sorted by number of values per key.
     *
     * @return the join cursor.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public DataCursor join(DataView[] indexViews, Object[] indexKeys,
                              boolean presorted)
        throws DbException, IOException {

        DataCursor joinCursor = null;
        DataCursor[] indexCursors = new DataCursor[indexViews.length];
        try {
            for (int i = 0; i < indexViews.length; i += 1) {
                indexCursors[i] = new DataCursor(indexViews[i], false);
                indexCursors[i].get(indexKeys[i], null, Db.DB_SET, false);
            }
            joinCursor = new DataCursor(this, indexCursors, presorted, true);
            return joinCursor;
        } finally {
            if (joinCursor == null) {
                // An exception is being thrown, so close cursors we opened.
                for (int i = 0; i < indexCursors.length; i += 1) {
                    if (indexCursors[i] != null) {
                        try { indexCursors[i].close(); }
                        catch (Exception e) {}
                    }
                }
            }
        }
    }

    /**
     * Returns a cursor for this view that reads only records having the
     * index key values at the specified cursors.
     *
     * @param indexCursor are the cursors to be joined.
     *
     * @param presorted is whether the given cursors are presorted or should be
     * sorted by number of values per key.
     *
     * @return the join cursor.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public DataCursor join(DataCursor[] indexCursors, boolean presorted)
        throws DbException, IOException {

        return new DataCursor(this, indexCursors, presorted, false);
    }

    private void returnPrimaryKeyAndValue(DataThang keyThang,
                                          DataThang valueThang,
                                          Object[] retPrimaryKey,
                                          Object[] retValue)
        throws DbException, IOException {

        // Requires: if retPrimaryKey, primary key binding (no index).
        // Requires: if retValue, value or entity binding

        if (retPrimaryKey != null) {
            if (keyBinding == null) {
                throw new IllegalArgumentException(
                    "returning key requires primary key binding");
            } else if (index != null) {
                throw new IllegalArgumentException(
                    "returning key requires unindexed view");
            } else {
                retPrimaryKey[0] = keyBinding.dataToObject(keyThang);
            }
        }
        if (retValue != null) {
            retValue[0] = makeValue(keyThang, valueThang);
        }
    }

    int useKey(Object key, Object value, DataThang keyThang,
               KeyRange checkRange)
        throws DbException, IOException {

        if (key != null) {
            if (keyBinding == null) {
                throw new IllegalArgumentException(
                    "non-null key with null key binding");
            }
            keyBinding.objectToData(key, keyThang);
        } else if (value == null) {
            throw new IllegalArgumentException(
                "null key and null value");
        } else if (index == null) {
            if (entityBinding == null) {
                throw new UnsupportedOperationException(
                    "null key, null index, and null entity binding");
            }
            entityBinding.objectToKey(value, keyThang);
        } else {
            KeyExtractor extractor = index.getKeyExtractor();
            DataThang primaryKeyThang = null;
            DataThang valueThang = null;
            if (entityBinding != null) {
                if (extractor.getPrimaryKeyFormat() != null) {
                    primaryKeyThang = new DataThang();
                    entityBinding.objectToKey(value, primaryKeyThang);
                }
                if (extractor.getValueFormat() != null) {
                    valueThang = new DataThang();
                    entityBinding.objectToValue(value, valueThang);
                }
            } else {
                if (extractor.getPrimaryKeyFormat() != null) {
                    throw new IllegalStateException(
                        "primary key needed by index extractor");
                }
                if (extractor.getValueFormat() != null) {
                    valueThang = new DataThang();
                    valueBinding.objectToData(value, valueThang);
                }
            }
            extractor.extractIndexKey(primaryKeyThang, valueThang, keyThang);
        }
        if (checkRange != null) {
            return checkRange.check(keyThang) ? 0 : Db.DB_NOTFOUND;
        } else {
            return 0;
        }
    }

    /**
     * Returns whether data keys can be derived from the value/entity binding
     * of this view, which determines whether a value/entity object alone is
     * sufficient for operations that require keys.
     *
     * @return whether data keys can be derived.
     */
    public boolean canDeriveKeyFromValue() {

        if (index == null) {
            return (entityBinding != null);
        } else {
            KeyExtractor extractor = index.getKeyExtractor();
            if (extractor.getPrimaryKeyFormat() != null &&
                entityBinding == null) {
                return false;
            } else if (extractor.getValueFormat() != null &&
                     entityBinding == null && valueBinding == null) {
                return false;
            } else {
                return true;
            }
        }
    }

    void useValue(Object value, DataThang valueThang, DataThang checkKeyThang)
        throws DbException, IOException {

        if (value != null) {
            if (valueBinding != null) {
                valueBinding.objectToData(value, valueThang);
            } else if (entityBinding != null) {
                entityBinding.objectToValue(value, valueThang);
                if (checkKeyThang != null) {
                    DataThang thang = new DataThang();
                    entityBinding.objectToKey(value, thang);
                    if (!thang.equals(checkKeyThang)) {
                        throw new IllegalArgumentException(
                            "cannot change primary key");
                    }
                }
            } else {
                throw new IllegalArgumentException(
                    "non-null value with null value/entity binding");
            }
        } else {
            valueThang.set_data(new byte[0]);
            valueThang.set_offset(0);
            valueThang.set_size(0);
        }
    }

    Object makeKey(DataThang keyThang)
        throws DbException, IOException {

        if (keyThang.get_size() == 0) return null;
        return keyBinding.dataToObject(keyThang);
    }

    Object makeValue(DataThang primaryKeyThang, DataThang valueThang)
        throws DbException, IOException {

        Object value;
        if (valueBinding != null) {
            value = valueBinding.dataToObject(valueThang);
        } else if (entityBinding != null) {
            value = entityBinding.dataToObject(primaryKeyThang,
                                                    valueThang);
        } else {
            throw new UnsupportedOperationException(
                "requires valueBinding or entityBinding");
        }
        return value;
    }

    KeyRange subRange(Object singleKey)
        throws DbException, IOException, KeyRangeException {

        return range.subRange(makeRangeKey(singleKey));
    }

    KeyRange subRange(Object beginKey, boolean beginInclusive,
                      Object endKey, boolean endInclusive)
        throws DbException, IOException, KeyRangeException {

        if (beginKey == endKey && beginInclusive && endInclusive) {
            return subRange(beginKey);
        }
        if (!isOrdered()) {
            throw new UnsupportedOperationException(
                    "Cannot use key ranges on an unsorted database");
        }
        DataThang beginThang =
            (beginKey != null) ? makeRangeKey(beginKey) : null;
        DataThang endThang =
            (endKey != null) ? makeRangeKey(endKey) : null;

        return range.subRange(beginThang, beginInclusive,
                              endThang, endInclusive);
    }

    private void checkBindingFormats() {

        if (keyBinding != null && !recNumAccess) {
            DataFormat keyFormat = (index != null) ? index.keyFormat
                                                   : store.keyFormat;
            if (!keyFormat.equals(keyBinding.getDataFormat())) {
                throw new IllegalArgumentException(
                    db.toString() + " key binding format mismatch");
            }
        }
        if (valueBinding != null) {
            if (!store.valueFormat.equals(valueBinding.getDataFormat())) {
                throw new IllegalArgumentException(
                    store.toString() + " value binding format mismatch");
            }
        }
        if (entityBinding != null) {
            if (!store.keyFormat.equals(entityBinding.getKeyFormat())) {
                throw new IllegalArgumentException(store.toString() +
                    " value entity binding keyFormat mismatch");
            }
            if (!store.valueFormat.equals(entityBinding.getValueFormat())) {
                throw new IllegalArgumentException(store.toString() +
                    " value entity binding valueFormat mismatch");
            }
        }
    }

    private DataThang makeRangeKey(Object key)
        throws DbException, IOException {

        DataThang thang = new DataThang();
        if (keyBinding != null) {
            useKey(key, null, thang, null);
        } else {
            useKey(null, key, thang, null);
        }
        return thang;
    }
}
