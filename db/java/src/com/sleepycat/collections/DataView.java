/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataView.java,v 1.4 2004/08/02 18:52:05 mjc Exp $
 */

package com.sleepycat.collections;

import com.sleepycat.bind.EntityBinding;
import com.sleepycat.bind.EntryBinding;
import com.sleepycat.compat.DbCompat;
import com.sleepycat.db.Database;
import com.sleepycat.db.DatabaseConfig;
import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.Environment;
import com.sleepycat.db.JoinConfig;
import com.sleepycat.db.OperationStatus;
import com.sleepycat.db.SecondaryConfig;
import com.sleepycat.db.SecondaryDatabase;
import com.sleepycat.db.SecondaryKeyCreator;
import com.sleepycat.db.Transaction;
import com.sleepycat.util.RuntimeExceptionWrapper;

/**
 * Represents a Berkeley DB database and adds support for indices, bindings and
 * key ranges.
 *
 * <p>This class defines a view and takes care of reading and updating indices,
 * calling bindings, constraining access to a key range, etc.</p>
 *
 * @author Mark Hayes
 */
final class DataView implements Cloneable {

    Database db;
    SecondaryDatabase secDb;
    CurrentTransaction currentTxn;
    KeyRange range;
    EntryBinding keyBinding;
    EntryBinding valueBinding;
    EntityBinding entityBinding;
    PrimaryKeyAssigner keyAssigner;
    SecondaryKeyCreator secKeyCreator;
    boolean writeAllowed;       // Read-write view
    boolean ordered;            // Not a HASH Db
    boolean recNumAllowed;      // QUEUE, RECNO, or BTREE-RECNUM Db
    boolean recNumAccess;       // recNumAllowed && using a rec num binding
    boolean btreeRecNumDb;      // BTREE-RECNUM Db
    boolean btreeRecNumAccess;  // recNumAccess && BTREE-RECNUM Db
    boolean recNumRenumber;     // RECNO-RENUM Db
    boolean keysRenumbered;     // recNumRenumber || btreeRecNumAccess
    boolean dupsAllowed;        // Dups configured
    boolean dupsOrdered;        // Sorted dups configured
    boolean transactional;      // Db is transactional
    boolean dirtyReadAllowed;   // Dirty-read is optional in DB-CORE
    boolean dirtyReadEnabled;   // This view is a dirty-ready view

    /**
     * Creates a view for a given database and bindings.  The initial key range
     * of the view will be open.
     */
    DataView(Database database, EntryBinding keyBinding,
             EntryBinding valueBinding, EntityBinding entityBinding,
             boolean writeAllowed, PrimaryKeyAssigner keyAssigner)
        throws IllegalArgumentException {

        if (database == null) {
            throw new IllegalArgumentException("database is null");
        }
        db = database;
        try {
            currentTxn =
                CurrentTransaction.getInstanceInternal(db.getEnvironment());
            DatabaseConfig dbConfig;
            if (db instanceof SecondaryDatabase) {
                secDb = (SecondaryDatabase) database;
                SecondaryConfig secConfig = secDb.getSecondaryConfig();
                secKeyCreator = secConfig.getKeyCreator();
                dbConfig = secConfig;
            } else {
                dbConfig = db.getConfig();
            }
            ordered = !DbCompat.isTypeHash(dbConfig);
            recNumAllowed = DbCompat.isTypeQueue(dbConfig) ||
                            DbCompat.isTypeRecno(dbConfig) ||
                            DbCompat.getBtreeRecordNumbers(dbConfig);
            recNumRenumber = DbCompat.getRenumbering(dbConfig);
            dupsAllowed = DbCompat.getSortedDuplicates(dbConfig) ||
                          DbCompat.getUnsortedDuplicates(dbConfig);
            dupsOrdered = DbCompat.getSortedDuplicates(dbConfig);
            transactional = currentTxn.isTxnMode() &&
                            dbConfig.getTransactional();
            dirtyReadAllowed = DbCompat.getDirtyRead(dbConfig);
            btreeRecNumDb = recNumAllowed && DbCompat.isTypeBtree(dbConfig);
            range = new KeyRange(dbConfig.getBtreeComparator());
        } catch (DatabaseException e) {
            throw new RuntimeExceptionWrapper(e);
        }
        this.writeAllowed = writeAllowed;
        this.keyBinding = keyBinding;
        this.valueBinding = valueBinding;
        this.entityBinding = entityBinding;
        this.keyAssigner = keyAssigner;

        if (valueBinding != null && entityBinding != null)
            throw new IllegalArgumentException(
                "both valueBinding and entityBinding are non-null");

        if (keyBinding instanceof com.sleepycat.bind.RecordNumberBinding) {
            if (!recNumAllowed) {
                throw new IllegalArgumentException(
                    "RecordNumberBinding requires DB_BTREE/DB_RECNUM, " +
                    "DB_RECNO, or DB_QUEUE");
            }
            recNumAccess = true;
            if (btreeRecNumDb) {
                btreeRecNumAccess = true;
            }
        }
        keysRenumbered = recNumRenumber || btreeRecNumAccess;
    }

    /**
     * Clones the view.
     */
    private DataView cloneView() {

        try {
            return (DataView) super.clone();
        } catch (CloneNotSupportedException willNeverOccur) {
            throw new IllegalStateException();
        }
    }

    /**
     * Return a new key-set view derived from this view by setting the
     * entity and value binding to null.
     *
     * @return the derived view.
     */
    DataView keySetView() {

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
    DataView valueSetView() {

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
     * @throws DatabaseException if a database problem occurs.
     *
     * @throws KeyRangeException if the specified range is not within the
     * current range.
     */
    DataView valueSetView(Object singleKey)
        throws DatabaseException, KeyRangeException {

        /*
         * Must do subRange before valueSetView since the latter clears the
         * key binding needed for the former.
         */
        KeyRange singleKeyRange = subRange(singleKey);
        DataView view = valueSetView();
        view.range = singleKeyRange;
        return view;
    }

    /**
     * Return a new value-set view for key range, optionally changing
     * the key binding.
     */
    DataView subView(Object beginKey, boolean beginInclusive,
                     Object endKey, boolean endInclusive,
                     EntryBinding keyBinding)
        throws DatabaseException, KeyRangeException {

        DataView view = cloneView();
        view.setRange(beginKey, beginInclusive, endKey, endInclusive);
        if (keyBinding != null) view.keyBinding = keyBinding;
        return view;
    }

    /**
     * Returns a new view with a specified dirtyRead setting.
     */
    DataView dirtyReadView(boolean enable) {

        if (!dirtyReadAllowed)
            return this;
        DataView view = cloneView();
        view.dirtyReadEnabled = enable;
        return view;
    }

    /**
     * Returns the current transaction for the view or null if the environment
     * is non-transactional.
     */
    CurrentTransaction getCurrentTxn() {

        return transactional ? currentTxn : null;
    }

    /**
     * Sets this view's range to a subrange with the given parameters.
     */
    private void setRange(Object beginKey, boolean beginInclusive,
                          Object endKey, boolean endInclusive)
        throws DatabaseException, KeyRangeException {

        range = subRange(beginKey, beginInclusive, endKey, endInclusive);
    }

    /**
     * Returns the key thang for a single key range, or null if a single key
     * range is not used.
     */
    DatabaseEntry getSingleKeyThang() {

        return range.getSingleKey();
    }

    /**
     * Returns the environment for the database.
     */
    final Environment getEnv() {

        return currentTxn.getEnvironment();
    }

    /**
     * Returns whether this is a view on a secondary database rather
     * than directly on a primary database.
     */
    final boolean isSecondary() {

        return (secDb != null);
    }

    /**
     * Returns whether no records are present in the view.
     */
    boolean isEmpty()
        throws DatabaseException {

        DataCursor cursor = new DataCursor(this, false);
        try {
            return cursor.getFirst(false) != OperationStatus.SUCCESS;
        } finally {
            cursor.close();
        }
    }

    /**
     * Appends a value and returns the new key.  If a key assigner is used
     * it assigns the key, otherwise a QUEUE or RECNO database is required.
     */
    OperationStatus append(Object value, Object[] retPrimaryKey,
                           Object[] retValue)
        throws DatabaseException {

        /*
         * Flags will be NOOVERWRITE if used with assigner, or APPEND
         * otherwise.
         * Requires: if value param, value or entity binding
         * Requires: if retPrimaryKey, primary key binding (no index).
         * Requires: if retValue, value or entity binding
         */
        DatabaseEntry keyThang = new DatabaseEntry();
        DatabaseEntry valueThang = new DatabaseEntry();
        useValue(value, valueThang, null);
        OperationStatus status;
        if (keyAssigner != null) {
            keyAssigner.assignKey(keyThang);
            if (!range.check(keyThang)) {
                throw new IllegalArgumentException(
                    "assigned key out of range");
            }
            DataCursor cursor = new DataCursor(this, true);
            try {
                status = cursor.getCursor().putNoOverwrite(keyThang,
                                                           valueThang);
            } finally {
                cursor.close();
            }
        } else {
            /* Assume QUEUE/RECNO access method. */
            if (currentTxn.isCDBCursorOpen(db)) {
                throw new IllegalStateException(
                  "cannot open CDB write cursor when read cursor is open");
            }
            status = DbCompat.append(db, useTransaction(),
                                     keyThang, valueThang);
            if (status == OperationStatus.SUCCESS && !range.check(keyThang)) {
                db.delete(useTransaction(), keyThang);
                throw new IllegalArgumentException(
                    "appended record number out of range");
            }
        }
        if (status == OperationStatus.SUCCESS) {
            returnPrimaryKeyAndValue(keyThang, valueThang,
                                     retPrimaryKey, retValue);
        }
        return status;
    }

    /**
     * Returns the current transaction if the database is transaction, or null
     * if the database is not transactional or there is no current transaction.
     */
    Transaction useTransaction() {
        return transactional ?  currentTxn.getTransaction() : null;
    }

    /**
     * Deletes all records in the current range.
     */
    void clear()
        throws DatabaseException {

        DataCursor cursor = new DataCursor(this, true);
        try {
            OperationStatus status = OperationStatus.SUCCESS;
            while (status == OperationStatus.SUCCESS) {
                if (keysRenumbered) {
                    status = cursor.getFirst(true);
                } else {
                    status = cursor.getNext(true);
                }
                if (status == OperationStatus.SUCCESS) {
                    cursor.delete();
                }
            }
        } finally {
            cursor.close();
        }
    }

    /**
     * Returns a cursor for this view that reads only records having the
     * specified index key values.
     */
    DataCursor join(DataView[] indexViews, Object[] indexKeys,
                    JoinConfig joinConfig)
        throws DatabaseException {

        DataCursor joinCursor = null;
        DataCursor[] indexCursors = new DataCursor[indexViews.length];
        try {
            for (int i = 0; i < indexViews.length; i += 1) {
                indexCursors[i] = new DataCursor(indexViews[i], false);
                indexCursors[i].getSearchKey(indexKeys[i], null, false);
            }
            joinCursor = new DataCursor(this, indexCursors, joinConfig, true);
            return joinCursor;
        } finally {
            if (joinCursor == null) {
                // An exception is being thrown, so close cursors we opened.
                for (int i = 0; i < indexCursors.length; i += 1) {
                    if (indexCursors[i] != null) {
                        try { indexCursors[i].close(); }
                        catch (Exception e) {
			    /* FindBugs, this is ok. */
			}
                    }
                }
            }
        }
    }

    /**
     * Returns a cursor for this view that reads only records having the
     * index key values at the specified cursors.
     */
    DataCursor join(DataCursor[] indexCursors, JoinConfig joinConfig)
        throws DatabaseException {

        return new DataCursor(this, indexCursors, joinConfig, false);
    }

    /**
     * Returns primary key and value if return parameters are non-null.
     */
    private void returnPrimaryKeyAndValue(DatabaseEntry keyThang,
                                          DatabaseEntry valueThang,
                                          Object[] retPrimaryKey,
                                          Object[] retValue)
        throws DatabaseException {

        // Requires: if retPrimaryKey, primary key binding (no index).
        // Requires: if retValue, value or entity binding

        if (retPrimaryKey != null) {
            if (keyBinding == null) {
                throw new IllegalArgumentException(
                    "returning key requires primary key binding");
            } else if (isSecondary()) {
                throw new IllegalArgumentException(
                    "returning key requires unindexed view");
            } else {
                retPrimaryKey[0] = keyBinding.entryToObject(keyThang);
            }
        }
        if (retValue != null) {
            retValue[0] = makeValue(keyThang, valueThang);
        }
    }

    /**
     * Populates the key entry and returns whether the key is within range.
     */
    boolean useKey(Object key, Object value, DatabaseEntry keyThang,
                   KeyRange checkRange)
        throws DatabaseException {

        if (key != null) {
            if (keyBinding == null) {
                throw new IllegalArgumentException(
                    "non-null key with null key binding");
            }
            keyBinding.objectToEntry(key, keyThang);
        } else {
            if (value == null) {
                throw new IllegalArgumentException(
                    "null key and null value");
            }
            if (entityBinding == null) {
                throw new IllegalStateException(
                    "EntityBinding required to derive key from value");
            }
            if (isSecondary()) {
                DatabaseEntry primaryKeyThang = new DatabaseEntry();
                entityBinding.objectToKey(value, primaryKeyThang);
                DatabaseEntry valueThang = new DatabaseEntry();
                entityBinding.objectToData(value, valueThang);
                secKeyCreator.createSecondaryKey(secDb, primaryKeyThang,
                                                 valueThang, keyThang);
            } else {
                entityBinding.objectToKey(value, keyThang);
            }
        }
        if (recNumAccess && DbCompat.getRecordNumber(keyThang) <= 0) {
            return false;
        }
        if (checkRange != null && !checkRange.check(keyThang)) {
            return false;
        }
        return true;
    }

    /**
     * Returns whether data keys can be derived from the value/entity binding
     * of this view, which determines whether a value/entity object alone is
     * sufficient for operations that require keys.
     */
    final boolean canDeriveKeyFromValue() {

        return (entityBinding != null);
    }

    /**
     * Populates the value entry and throws an exception if the primary key
     * would be changed via an entity binding.
     */
    void useValue(Object value, DatabaseEntry valueThang,
                  DatabaseEntry checkKeyThang)
        throws DatabaseException {

        if (value != null) {
            if (valueBinding != null) {
                valueBinding.objectToEntry(value, valueThang);
            } else if (entityBinding != null) {
                entityBinding.objectToData(value, valueThang);
                if (checkKeyThang != null) {
                    DatabaseEntry thang = new DatabaseEntry();
                    entityBinding.objectToKey(value, thang);
                    if (!KeyRange.equalBytes(thang, checkKeyThang)) {
                        throw new IllegalArgumentException(
                            "cannot change primary key");
                    }
                }
            } else {
                throw new IllegalArgumentException(
                    "non-null value with null value/entity binding");
            }
        } else {
            valueThang.setData(new byte[0]);
            valueThang.setOffset(0);
            valueThang.setSize(0);
        }
    }

    /**
     * Converts a key entry to a key object.
     */
    Object makeKey(DatabaseEntry keyThang)
        throws DatabaseException {

        if (keyThang.getSize() == 0) return null;
        return keyBinding.entryToObject(keyThang);
    }

    /**
     * Converts a key-value entry pair to a value object.
     */
    Object makeValue(DatabaseEntry primaryKeyThang, DatabaseEntry valueThang)
        throws DatabaseException {

        Object value;
        if (valueBinding != null) {
            value = valueBinding.entryToObject(valueThang);
        } else if (entityBinding != null) {
            value = entityBinding.entryToObject(primaryKeyThang,
                                                    valueThang);
        } else {
            throw new UnsupportedOperationException(
                "requires valueBinding or entityBinding");
        }
        return value;
    }

    /**
     * Intersects the given key and the current range.
     */
    KeyRange subRange(Object singleKey)
        throws DatabaseException, KeyRangeException {

        return range.subRange(makeRangeKey(singleKey));
    }

    /**
     * Intersects the given range and the current range.
     */
    KeyRange subRange(Object beginKey, boolean beginInclusive,
                      Object endKey, boolean endInclusive)
        throws DatabaseException, KeyRangeException {

        if (beginKey == endKey && beginInclusive && endInclusive) {
            return subRange(beginKey);
        }
        if (!ordered) {
            throw new UnsupportedOperationException(
                    "Cannot use key ranges on an unsorted database");
        }
        DatabaseEntry beginThang =
            (beginKey != null) ? makeRangeKey(beginKey) : null;
        DatabaseEntry endThang =
            (endKey != null) ? makeRangeKey(endKey) : null;

        return range.subRange(beginThang, beginInclusive,
                              endThang, endInclusive);
    }

    /**
     * Given a key object, make a key entry that can be used in a range.
     */
    private DatabaseEntry makeRangeKey(Object key)
        throws DatabaseException {

        DatabaseEntry thang = new DatabaseEntry();
        if (keyBinding != null) {
            useKey(key, null, thang, null);
        } else {
            useKey(null, key, thang, null);
        }
        return thang;
    }
}
