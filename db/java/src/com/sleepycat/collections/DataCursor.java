/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataCursor.java,v 1.5 2004/11/05 01:08:31 mjc Exp $
 */

package com.sleepycat.collections;

import com.sleepycat.compat.DbCompat;
import com.sleepycat.db.Cursor;
import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.JoinConfig;
import com.sleepycat.db.JoinCursor;
import com.sleepycat.db.LockMode;
import com.sleepycat.db.OperationStatus;

/**
 * Represents a Berkeley DB cursor and adds support for indices, bindings and
 * key ranges.
 *
 * <p>This class operates on a view and takes care of reading and updating
 * indices, calling bindings, constraining access to a key range, etc.</p>
 *
 * @author Mark Hayes
 */
final class DataCursor implements Cloneable {

    private RangeCursor cursor;
    private JoinCursor joinCursor;
    private DataView view;
    private KeyRange range;
    private boolean writeAllowed;
    private boolean dirtyRead;
    private DatabaseEntry keyThang;
    private DatabaseEntry valueThang;
    private DatabaseEntry primaryKeyThang;
    private DatabaseEntry otherThang;
    private DataCursor[] indexCursorsToClose;

    /**
     * Creates a cursor for a given view.
     */
    DataCursor(DataView view, boolean writeAllowed)
        throws DatabaseException {

        init(view, writeAllowed, null);
    }

    /**
     * Creates a cursor for a given view and single key range.
     */
    DataCursor(DataView view, boolean writeAllowed, Object singleKey)
        throws DatabaseException {

        init(view, writeAllowed, view.subRange(singleKey));
    }

    /**
     * Creates a cursor for a given view and key range.
     */
    DataCursor(DataView view, boolean writeAllowed,
               Object beginKey, boolean beginInclusive,
               Object endKey, boolean endInclusive)
        throws DatabaseException {

        init(view, writeAllowed,
             view.subRange(beginKey, beginInclusive, endKey, endInclusive));
    }

    /**
     * Creates a join cursor.
     */
    DataCursor(DataView view, DataCursor[] indexCursors,
               JoinConfig joinConfig, boolean closeIndexCursors)
        throws DatabaseException {

        if (view.isSecondary()) {
            throw new IllegalArgumentException(
                "The primary collection in a join must not be a secondary " +
                "database");
        }
        Cursor[] cursors = new Cursor[indexCursors.length];
        for (int i = 0; i < cursors.length; i += 1) {
            cursors[i] = indexCursors[i].cursor.getCursor();
        }
        joinCursor = view.db.join(cursors, joinConfig);
        init(view, false, null);
        if (closeIndexCursors) {
            indexCursorsToClose = indexCursors;
        }
    }

    /**
     * Clones a cursor preserving the current position.
     */
    DataCursor cloneCursor()
        throws DatabaseException {

        checkNoJoinCursor();

        DataCursor o;
        try {
            o = (DataCursor) super.clone();
        } catch (CloneNotSupportedException neverHappens) {
            return null;
        }

        o.initThangs();
        KeyRange.copy(keyThang, o.keyThang);
        KeyRange.copy(valueThang, o.valueThang);
        if (primaryKeyThang != keyThang) {
            KeyRange.copy(primaryKeyThang, o.primaryKeyThang);
        }

        o.cursor = cursor.dup(true);
        return o;
    }

    /**
     * Returns the internal range cursor.
     */
    RangeCursor getCursor() {
        return cursor;
    }

    /**
     * Constructor helper.
     */
    private void init(DataView view, boolean writeAllowed, KeyRange range)
        throws DatabaseException {

        this.view = view;
        this.writeAllowed = writeAllowed && view.writeAllowed;
        this.range = (range != null) ? range : view.range;
        dirtyRead = view.dirtyReadEnabled;

        initThangs();

        if (joinCursor == null) {
            cursor = new RangeCursor(view, this.range, this.writeAllowed);
        }
    }

    /**
     * Constructor helper.
     */
    private void initThangs()
        throws DatabaseException {

        keyThang = new DatabaseEntry();
        primaryKeyThang = view.isSecondary() ? (new DatabaseEntry())
                                             : keyThang;
        valueThang = new DatabaseEntry();
    }

    /**
     * Closes the associated cursor.
     */
    void close()
        throws DatabaseException {

        if (joinCursor != null) {
            JoinCursor toClose = joinCursor;
            joinCursor = null;
            toClose.close();
        }
        if (cursor != null) {
            Cursor toClose = cursor.getCursor();
            cursor = null;
            view.currentTxn.closeCursor(toClose );
        }
        if (indexCursorsToClose != null) {
            DataCursor[] toClose = indexCursorsToClose;
            indexCursorsToClose = null;
            for (int i = 0; i < toClose.length; i += 1) {
                toClose[i].close();
            }
        }
    }

    /**
     * Returns the view for this cursor.
     */
    DataView getView() {

        return view;
    }

    /**
     * Returns the range for this cursor.
     */
    KeyRange getRange() {

        return range;
    }

    /**
     * Returns whether write is allowed for this cursor, as specified to the
     * constructor.
     */
    boolean isWriteAllowed() {

        return writeAllowed;
    }

    /**
     * Returns the key object for the last record read.
     */
    Object getCurrentKey()
        throws DatabaseException {

        if (view.keyBinding == null) {
            throw new UnsupportedOperationException();
        }
        return view.makeKey(keyThang);
    }

    /**
     * Returns the value object for the last record read.
     */
    Object getCurrentValue()
        throws DatabaseException {

        return view.makeValue(primaryKeyThang, valueThang);
    }

    /**
     * Returns whether record number access is allowed.
     */
    boolean hasRecNumAccess() {

        return view.recNumAccess;
    }

    /**
     * Returns the record number for the last record read.
     */
    int getCurrentRecordNumber()
        throws DatabaseException {

        if (view.btreeRecNumDb) {
            /* BTREE-RECNO access. */
            if (otherThang == null) {
                otherThang = new DatabaseEntry();
            }
            DbCompat.getCurrentRecordNumber(cursor.getCursor(), otherThang,
                                            getLockMode(false));
            return DbCompat.getRecordNumber(otherThang);
        } else {
            /* QUEUE or RECNO database. */
            return DbCompat.getRecordNumber(keyThang);
        }
    }

    /**
     * Binding version of Cursor.getCurrent(), no join cursor allowed.
     */
    OperationStatus getCurrent(boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        return cursor.getCurrent(keyThang, primaryKeyThang, valueThang,
                                 getLockMode(lockForWrite));
    }

    /**
     * Binding version of Cursor.getFirst(), join cursor is allowed.
     */
    OperationStatus getFirst(boolean lockForWrite)
        throws DatabaseException {

        LockMode lockMode = getLockMode(lockForWrite);
        if (joinCursor != null) {
            return joinCursor.getNext(keyThang, valueThang, lockMode);
        } else {
            return cursor.getFirst(keyThang, primaryKeyThang, valueThang,
                                   lockMode);
        }
    }

    /**
     * Binding version of Cursor.getNext(), join cursor is allowed.
     */
    OperationStatus getNext(boolean lockForWrite)
        throws DatabaseException {

        LockMode lockMode = getLockMode(lockForWrite);
        if (joinCursor != null) {
            return joinCursor.getNext(keyThang, valueThang, lockMode);
        } else {
            return cursor.getNext(keyThang, primaryKeyThang, valueThang,
                                  lockMode);
        }
    }

    /**
     * Binding version of Cursor.getNext(), join cursor is allowed.
     */
    OperationStatus getNextNoDup(boolean lockForWrite)
        throws DatabaseException {

        LockMode lockMode = getLockMode(lockForWrite);
        if (joinCursor != null) {
            return joinCursor.getNext(keyThang, valueThang, lockMode);
        } else {
            return cursor.getNextNoDup(keyThang, primaryKeyThang, valueThang,
                                       lockMode);
        }
    }

    /**
     * Binding version of Cursor.getNextDup(), no join cursor allowed.
     */
    OperationStatus getNextDup(boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        return cursor.getNextDup(keyThang, primaryKeyThang, valueThang,
                                 getLockMode(lockForWrite));
    }

    /**
     * Binding version of Cursor.getLast(), no join cursor allowed.
     */
    OperationStatus getLast(boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        return cursor.getLast(keyThang, primaryKeyThang, valueThang,
                              getLockMode(lockForWrite));
    }

    /**
     * Binding version of Cursor.getPrev(), no join cursor allowed.
     */
    OperationStatus getPrev(boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        return cursor.getPrev(keyThang, primaryKeyThang, valueThang,
                              getLockMode(lockForWrite));
    }

    /**
     * Binding version of Cursor.getPrevNoDup(), no join cursor allowed.
     */
    OperationStatus getPrevNoDup(boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        return cursor.getPrevNoDup(keyThang, primaryKeyThang, valueThang,
                                   getLockMode(lockForWrite));
    }

    /**
     * Binding version of Cursor.getPrevDup(), no join cursor allowed.
     */
    OperationStatus getPrevDup(boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        return cursor.getPrevDup(keyThang, primaryKeyThang, valueThang,
                                 getLockMode(lockForWrite));
    }

    /**
     * Binding version of Cursor.getSearchKey(), no join cursor allowed.
     * Searches by record number in a BTREE-RECNO db with RECNO access.
     */
    OperationStatus getSearchKey(Object key, Object value,
                                 boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        if (view.useKey(key, value, keyThang, range)) {
            return doGetSearchKey(lockForWrite);
        } else {
            return OperationStatus.NOTFOUND;
        }
    }

    /**
     * Pass-thru version of Cursor.getSearchKey().
     * Searches by record number in a BTREE-RECNO db with RECNO access.
     */
    private OperationStatus doGetSearchKey(boolean lockForWrite)
        throws DatabaseException {

        LockMode lockMode = getLockMode(lockForWrite);
        if (view.btreeRecNumAccess) {
            return cursor.getSearchRecordNumber(keyThang, primaryKeyThang,
                                                valueThang, lockMode);
        } else {
            return cursor.getSearchKey(keyThang, primaryKeyThang,
                                       valueThang, lockMode);
        }
    }

    /**
     * Binding version of Cursor.getSearchKeyRange(), no join cursor allowed.
     */
    OperationStatus getSearchKeyRange(Object key, Object value,
                                      boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        if (view.useKey(key, value, keyThang, range)) {
            return cursor.getSearchKeyRange(keyThang, primaryKeyThang,
                                            valueThang,
                                            getLockMode(lockForWrite));
        } else {
            return OperationStatus.NOTFOUND;
        }
    }

    /**
     * Binding version of Cursor.getSearchBoth(), no join cursor allowed.
     * Unlike SecondaryCursor.getSearchBoth, for a secondary this searches for
     * the primary value not the primary key.
     */
    OperationStatus getSearchBoth(Object key, Object value,
                                  boolean lockForWrite)
        throws DatabaseException {

        checkNoJoinCursor();
        LockMode lockMode = getLockMode(lockForWrite);
        view.useValue(value, valueThang, null);
        if (view.useKey(key, value, keyThang, range)) {
            if (view.isSecondary()) {
                if (otherThang == null) {
                    otherThang = new DatabaseEntry();
                }
                OperationStatus status = cursor.getSearchKey(keyThang,
                                                             primaryKeyThang,
                                                             otherThang,
                                                             lockMode);
                while (status == OperationStatus.SUCCESS) {
                    if (KeyRange.equalBytes(otherThang, valueThang)) {
                        break;
                    }
                    status = cursor.getNextDup(keyThang, primaryKeyThang,
                                               otherThang, lockMode);
                }
                /* if status != SUCCESS set range cursor to invalid? */
                return status;
            } else {
                return cursor.getSearchBoth(keyThang, null, valueThang,
                                            lockMode);
            }
        } else {
            return OperationStatus.NOTFOUND;
        }
    }

    /**
     * Find the given value using getSearchBoth if possible or a sequential
     * scan otherwise, no join cursor allowed.
     */
    OperationStatus find(Object value, boolean findFirst)
        throws DatabaseException {

        checkNoJoinCursor();
        LockMode lockMode = getLockMode(false);

        if (view.entityBinding != null && !view.isSecondary() &&
            (findFirst || !view.dupsAllowed)) {
            return getSearchBoth(null, value, false);
        } else {
            if (otherThang == null) {
                otherThang = new DatabaseEntry();
            }
            view.useValue(value, otherThang, null);
            OperationStatus status = findFirst ? getFirst(false)
                                               : getLast(false);
            while (status == OperationStatus.SUCCESS) {
                if (KeyRange.equalBytes(valueThang, otherThang)) {
                    break;
                }
                status = findFirst ? getNext(false) : getPrev(false);
            }
            return status;
        }
    }

    /**
     * Calls Cursor.count(), no join cursor allowed.
     */
    int count()
        throws DatabaseException {

        checkNoJoinCursor();
        return cursor.count();
    }

    /**
     * Binding version of Cursor.putCurrent().
     */
    OperationStatus putCurrent(Object value)
        throws DatabaseException {

        checkWriteAllowed(false);
        view.useValue(value, valueThang, keyThang);

        /*
         * Workaround for a DB core problem: With HASH type a put() with
         * different data is allowed.
         */
        boolean hashWorkaround = (view.dupsOrdered && !view.ordered);
        if (hashWorkaround) {
            if (otherThang == null) {
                otherThang = new DatabaseEntry();
            }
            cursor.getCurrent(keyThang, primaryKeyThang, otherThang,
                              LockMode.DEFAULT);
            if (KeyRange.equalBytes(valueThang, otherThang)) {
                return OperationStatus.SUCCESS;
            } else {
                throw new IllegalArgumentException(
                  "Current data differs from put data with sorted duplicates");
            }
        }

        return cursor.putCurrent(valueThang);
    }

    /**
     * Binding version of Cursor.putAfter().
     */
    OperationStatus putAfter(Object value)
        throws DatabaseException {

        checkWriteAllowed(false);
        view.useValue(value, valueThang, null); /* why no key check? */
        return cursor.putAfter(new DatabaseEntry(), valueThang);
    }

    /**
     * Binding version of Cursor.putBefore().
     */
    OperationStatus putBefore(Object value)
        throws DatabaseException {

        checkWriteAllowed(false);
        view.useValue(value, valueThang, keyThang);
        return cursor.putBefore(new DatabaseEntry(), valueThang);
    }

    /**
     * Binding version of Cursor.put(), optionally returning the old value and
     * optionally using the current key instead of the key parameter.
     */
    OperationStatus put(Object key, Object value, Object[] oldValue,
                        boolean useCurrentKey)
        throws DatabaseException {

        initForPut(key, value, oldValue, useCurrentKey);
        return cursor.put(keyThang, valueThang);
    }

    /**
     * Binding version of Cursor.putNoOverwrite(), optionally using the current
     * key instead of the key parameter.
     */
    OperationStatus putNoOverwrite(Object key, Object value,
                                   boolean useCurrentKey)
        throws DatabaseException {

        initForPut(key, value, null, useCurrentKey);
        return cursor.putNoOverwrite(keyThang, valueThang);
    }

    /**
     * Binding version of Cursor.putNoDupData(), optionally returning the old
     * value and optionally using the current key instead of the key parameter.
     */
    OperationStatus putNoDupData(Object key, Object value, Object[] oldValue,
                                 boolean useCurrentKey)
        throws DatabaseException {

        initForPut(key, value, oldValue, useCurrentKey);
        if (view.dupsOrdered) {
            return cursor.putNoDupData(keyThang, valueThang);
        } else {
            if (view.dupsAllowed) {
                /* Unordered duplicates. */
                OperationStatus status =
                        cursor.getSearchBoth(keyThang, primaryKeyThang,
                                             valueThang,
                                             getLockMode(false));
                if (status == OperationStatus.SUCCESS) {
                    return OperationStatus.KEYEXIST;
                } else {
                    return cursor.put(keyThang, valueThang);
                }
            } else {
                /* No duplicates. */
                return cursor.putNoOverwrite(keyThang, valueThang);
            }
        }
    }

    /**
     * Do setup for a put() operation.
     */
    private void initForPut(Object key, Object value, Object[] oldValue,
                            boolean useCurrentKey)
        throws DatabaseException {

        checkWriteAllowed(false);
        if (!useCurrentKey && !view.useKey(key, value, keyThang, range)) {
            throw new IllegalArgumentException("key out of range");
        }
        if (oldValue != null) {
            oldValue[0] = null;
            if (!view.dupsAllowed) {
                OperationStatus status = doGetSearchKey(true);
                if (status == OperationStatus.SUCCESS) {
                    oldValue[0] = getCurrentValue();
                }
            }
        }
        view.useValue(value, valueThang, keyThang);
    }

    /**
     * Sets the key entry to the begin key of a single key range, so the next
     * time a putXxx() method is called that key will be used.
     */
    void useRangeKey() {
        if (!range.singleKey) {
            throw new IllegalStateException();
        }
        KeyRange.copy(range.beginKey, keyThang);
    }

    /**
     * Perform an arbitrary database 'delete' operation.
     */
    OperationStatus delete()
        throws DatabaseException {

        checkWriteAllowed(true);
        return cursor.delete();
    }

    /**
     * Returns the lock mode to use for a getXxx() operation.
     */
    LockMode getLockMode(boolean lockForWrite) {

        /* Dirty-read takes precedence over write-locking. */

        if (dirtyRead) {
            return LockMode.DIRTY_READ;
        } else if (lockForWrite && !view.currentTxn.isDirtyRead()) {
            return view.currentTxn.getWriteLockMode();
        } else {
            return LockMode.DEFAULT;
        }
    }

    /**
     * Throws an exception if a join cursor is in use.
     */
    private void checkNoJoinCursor() {

        if (joinCursor != null) {
            throw new UnsupportedOperationException
                ("Not allowed with a join cursor");
        }
    }

    /**
     * Throws an exception if write is not allowed or if a join cursor is in
     * use.
     */
    private void checkWriteAllowed(boolean allowSecondary) {

        checkNoJoinCursor();

        if (!writeAllowed || (!allowSecondary && view.isSecondary())) {
            throw new UnsupportedOperationException
                ("Writing is not allowed");
        }
    }
}
