/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: RangeCursor.java,v 1.4 2004/11/05 01:08:31 mjc Exp $
 */

package com.sleepycat.collections;

import com.sleepycat.compat.DbCompat;
import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.Cursor;
import com.sleepycat.db.LockMode;
import com.sleepycat.db.OperationStatus;
import com.sleepycat.db.SecondaryCursor;

/**
 * A cursor-like interface that enforces a key range.  The method signatures
 * are actually those of SecondaryCursor, but the pKey parameter may be null.
 * It was done this way to avoid doubling the number of methods.
 *
 * <p>This is not a general implementation of a range cursor and should not
 * be used outside this package; however, it may evolve into a generally useful
 * range cursor some day.</p>
 *
 * @author Mark Hayes
 */
class RangeCursor implements Cloneable {

    /**
     * The cursor and secondary cursor are the same object.  The secCursor is
     * null if the database is not a secondary database.
     */
    private Cursor cursor;
    private SecondaryCursor secCursor;
    private CurrentTransaction currentTxn;
    private boolean writeAllowed;

    /**
     * The range is always non-null, but may be unbounded meaning that it is
     * open and not used.
     */
    private KeyRange range;

    /**
     * If the database is a RECNO or QUEUE database, we know its keys are
     * record numbers.  We treat a non-positive record number as out of bounds,
     * that is, we return NOTFOUND rather than throwing
     * IllegalArgumentException as would happen if we passed a non-positive
     * record number into the DB cursor.  This behavior is required by the
     * collections interface.
     */
    private boolean isRecnoOrQueue;

    /**
     * The privXxx entries are used only when the range is bounded.  We read
     * into these private entries to avoid modifying the caller's entry
     * parameters in the case where we read sucessfully but the key is out of
     * range.  In that case we return NOTFOUND and we want to leave the entry
     * parameters unchanged.
     */
    private DatabaseEntry privKey;
    private DatabaseEntry privPKey;
    private DatabaseEntry privData;

    /**
     * The initialized flag is set to true whenever we sucessfully position the
     * cursor.  It is used to implement the getNext/Prev logic for doing a
     * getFirst/Last when the cursor is not initialized.  We can't rely on
     * Cursor to do that for us, since if we position the underlying cursor
     * sucessfully but the key is out of range, we have no way to set the
     * underlying cursor to uninitialized.  A range cursor always starts in
     * the uninitialized state.
     */
    private boolean initialized;

    /**
     * Create a range cursor.
     */
    RangeCursor(DataView view, KeyRange range, boolean writeAllowed)
        throws DatabaseException {

        this.range = range;
        this.writeAllowed = writeAllowed;
        this.currentTxn = view.currentTxn;
        isRecnoOrQueue = view.recNumAllowed && !view.btreeRecNumDb;

        cursor = currentTxn.openCursor(view.db, writeAllowed,
                                       view.useTransaction());
        init();
    }

    /**
     * Create a cloned range cursor.  The caller must clone the underlying
     * cursor before using this constructor, because cursor open/close is
     * handled specially for CDS cursors outside this class.
     */
    RangeCursor dup(boolean samePosition)
        throws DatabaseException {

        try {
            RangeCursor c = (RangeCursor) super.clone();
            c.cursor = currentTxn.dupCursor(cursor, writeAllowed,
                                            samePosition);
            c.init();
            return c;
        } catch (CloneNotSupportedException neverHappens) {
            return null;
        }
    }

    /**
     * Used for opening and duping (cloning).
     */
    private void init() {

        if (cursor instanceof SecondaryCursor) {
            secCursor = (SecondaryCursor) cursor;
        } else {
            secCursor = null;
        }

        if (range.hasBound()) {
            privKey = new DatabaseEntry();
            privPKey = new DatabaseEntry();
            privData = new DatabaseEntry();
        } else {
            privKey = null;
            privPKey = null;
            privData = null;
        }
    }

    /**
     * Returns the underlying cursor.  Used for cloning.
     */
    Cursor getCursor() {
        return cursor;
    }

    /**
     * When an unbounded range is used, this method is called to use the
     * callers entry parameters directly, to avoid the extra step of copying
     * between the private entries and the caller's entries.
     */
    private void setParams(DatabaseEntry key, DatabaseEntry pKey,
                           DatabaseEntry data) {
        privKey = key;
        privPKey = pKey;
        privData = data;
    }

    /**
     * Dups the cursor, sets the cursor and secCursor fields to the duped
     * cursor, and returns the old cursor.  Always call endOperation in a
     * finally clause after calling beginOperation.
     *
     * <p>If the returned cursor == the cursor field, the cursor is
     * uninitialized and was not duped; this case is handled correctly by
     * endOperation.</p>
     */
    private Cursor beginOperation()
        throws DatabaseException {

        Cursor oldCursor = cursor;
        if (initialized) {
            cursor = currentTxn.dupCursor(cursor, writeAllowed, true);
            if (secCursor != null) {
                secCursor = (SecondaryCursor) cursor;
            }
        } else {
            return cursor;
        }
        return oldCursor;
    }

    /**
     * If the operation succeded, leaves the duped cursor in place and closes
     * the oldCursor.  If the operation failed, moves the oldCursor back in
     * place and closes the duped cursor.  oldCursor may be null if
     * beginOperation was not called, in cases where we don't need to dup
     * the cursor.  Always call endOperation when a successful operation ends,
     * in order to set the initialized field.
     */
    private void endOperation(Cursor oldCursor, OperationStatus status,
                              DatabaseEntry key, DatabaseEntry pKey,
                              DatabaseEntry data)
        throws DatabaseException {

        if (status == OperationStatus.SUCCESS) {
            if (oldCursor != null && oldCursor != cursor) {
                currentTxn.closeCursor(oldCursor);
            }
            if (key != null) {
                swapData(key, privKey);
            }
            if (pKey != null && secCursor != null) {
                swapData(pKey, privPKey);
            }
            if (data != null) {
                swapData(data, privData);
            }
            initialized = true;
        } else {
            if (oldCursor != null && oldCursor != cursor) {
                currentTxn.closeCursor(cursor);
                cursor = oldCursor;
                if (secCursor != null) {
                    secCursor = (SecondaryCursor) cursor;
                }
            }
        }
    }

    /**
     * Swaps the contents of the two entries.  Used to return entry data to
     * the caller when the operation was successful.
     */
    static private void swapData(DatabaseEntry e1, DatabaseEntry e2) {

        byte[] d1 = e1.getData();
        int o1 = e1.getOffset();
        int s1 = e1.getSize();

        e1.setData(e2.getData(), e2.getOffset(), e2.getSize());
        e2.setData(d1, o1, s1);
    }

    /**
     * Shares the same byte array, offset and size between two entries.
     * Used when copying the entry data is not necessary because it is known
     * that the underlying operation will not modify the entry, for example,
     * with getSearchKey.
     */
    static private void shareData(DatabaseEntry from, DatabaseEntry to) {

        if (from != null) {
            to.setData(from.getData(), from.getOffset(), from.getSize());
        }
    }

    public OperationStatus getFirst(DatabaseEntry key,
                                    DatabaseEntry pKey,
                                    DatabaseEntry data,
                                    LockMode lockMode)
        throws DatabaseException {

        OperationStatus status;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetFirst(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        if (range.singleKey) {
            KeyRange.copy(range.beginKey, privKey);
            status = doGetSearchKey(lockMode);
            endOperation(null, status, key, pKey, data);
        } else {
            status = OperationStatus.NOTFOUND;
            Cursor oldCursor = beginOperation();
            try {
                if (range.beginKey == null) {
                    status = doGetFirst(lockMode);
                } else {
                    KeyRange.copy(range.beginKey, privKey);
                    status = doGetSearchKeyRange(lockMode);
                    if (status == OperationStatus.SUCCESS &&
                        !range.beginInclusive &&
                        range.compare(privKey, range.beginKey) == 0) {
                        status = doGetNext(lockMode);
                    }
                }
                if (status == OperationStatus.SUCCESS &&
                    !range.check(privKey)) {
                    status = OperationStatus.NOTFOUND;
                }
            } finally {
                endOperation(oldCursor, status, key, pKey, data);
            }
        }
        return status;
    }

    public OperationStatus getLast(DatabaseEntry key,
                                   DatabaseEntry pKey,
                                   DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        OperationStatus status = OperationStatus.NOTFOUND;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetLast(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        Cursor oldCursor = beginOperation();
        try {
            if (range.endKey == null) {
                status = doGetLast(lockMode);
            } else {
                KeyRange.copy(range.endKey, privKey);
                status = doGetSearchKeyRange(lockMode);
                if (status == OperationStatus.SUCCESS) {
                    if (range.endInclusive &&
                        range.compare(range.endKey, privKey) == 0) {
                        status = doGetNextNoDup(lockMode);
                        if (status == OperationStatus.SUCCESS) {
                            status = doGetPrev(lockMode);
                        } else {
                            status = doGetLast(lockMode);
                        }
                    } else {
                        status = doGetPrev(lockMode);
                    }
                } else {
                    status = doGetLast(lockMode);
                }
            }
            if (status == OperationStatus.SUCCESS &&
                !range.checkBegin(privKey, true)) {
                status = OperationStatus.NOTFOUND;
            }
        } finally {
            endOperation(oldCursor, status, key, pKey, data);
        }
        return status;
    }

    public OperationStatus getNext(DatabaseEntry key,
                                   DatabaseEntry pKey,
                                   DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        OperationStatus status;
        if (!initialized) {
            return getFirst(key, pKey, data, lockMode);
        }
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetNext(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        if (range.singleKey) {
            status = doGetNextDup(lockMode);
            endOperation(null, status, key, pKey, data);
        } else {
            status = OperationStatus.NOTFOUND;
            Cursor oldCursor = beginOperation();
            try {
                status = doGetNext(lockMode);
                if (status == OperationStatus.SUCCESS &&
                    !range.check(privKey)) {
                    status = OperationStatus.NOTFOUND;
                }
            } finally {
                endOperation(oldCursor, status, key, pKey, data);
            }
        }
        return status;
    }

    public OperationStatus getNextNoDup(DatabaseEntry key,
                                        DatabaseEntry pKey,
                                        DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        OperationStatus status;
        if (!initialized) {
            return getFirst(key, pKey, data, lockMode);
        }
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetNextNoDup(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        if (range.singleKey) {
            status = OperationStatus.NOTFOUND;
        } else {
            status = OperationStatus.NOTFOUND;
            Cursor oldCursor = beginOperation();
            try {
                status = doGetNextNoDup(lockMode);
                if (status == OperationStatus.SUCCESS &&
                    !range.check(privKey)) {
                    status = OperationStatus.NOTFOUND;
                }
            } finally {
                endOperation(oldCursor, status, key, pKey, data);
            }
        }
        return status;
    }

    public OperationStatus getPrev(DatabaseEntry key,
                                   DatabaseEntry pKey,
                                   DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        OperationStatus status;
        if (!initialized) {
            return getLast(key, pKey, data, lockMode);
        }
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetPrev(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        if (range.singleKey) {
            status = doGetPrevDup(lockMode);
            endOperation(null, status, key, pKey, data);
        } else {
            status = OperationStatus.NOTFOUND;
            Cursor oldCursor = beginOperation();
            try {
                status = doGetPrev(lockMode);
                if (status == OperationStatus.SUCCESS &&
                    !range.check(privKey)) {
                    status = OperationStatus.NOTFOUND;
                }
            } finally {
                endOperation(oldCursor, status, key, pKey, data);
            }
        }
        return status;
    }

    public OperationStatus getPrevNoDup(DatabaseEntry key,
                                        DatabaseEntry pKey,
                                        DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        OperationStatus status;
        if (!initialized) {
            return getLast(key, pKey, data, lockMode);
        }
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetPrevNoDup(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        if (range.singleKey) {
            status = OperationStatus.NOTFOUND;
        } else {
            status = OperationStatus.NOTFOUND;
            Cursor oldCursor = beginOperation();
            try {
                status = doGetPrevNoDup(lockMode);
                if (status == OperationStatus.SUCCESS &&
                    !range.check(privKey)) {
                    status = OperationStatus.NOTFOUND;
                }
            } finally {
                endOperation(oldCursor, status, key, pKey, data);
            }
        }
        return status;
    }

    public OperationStatus getSearchKey(DatabaseEntry key,
                                        DatabaseEntry pKey,
                                        DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        OperationStatus status;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetSearchKey(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        if (!range.check(key)) {
            status = OperationStatus.NOTFOUND;
        } else {
            shareData(key, privKey);
            status = doGetSearchKey(lockMode);
            endOperation(null, status, key, pKey, data);
        }
        return status;
    }

    public OperationStatus getSearchBoth(DatabaseEntry key,
                                         DatabaseEntry pKey,
                                         DatabaseEntry data,
                                         LockMode lockMode)
        throws DatabaseException {

        OperationStatus status;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetSearchBoth(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        if (!range.check(key)) {
            status = OperationStatus.NOTFOUND;
        } else {
            shareData(key, privKey);
            if (secCursor != null) {
                shareData(pKey, privPKey);
            } else {
                shareData(data, privData);
            }
            status = doGetSearchBoth(lockMode);
            endOperation(null, status, key, pKey, data);
        }
        return status;
    }

    public OperationStatus getSearchKeyRange(DatabaseEntry key,
                                             DatabaseEntry pKey,
                                             DatabaseEntry data,
                                             LockMode lockMode)
        throws DatabaseException {

        OperationStatus status = OperationStatus.NOTFOUND;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetSearchKeyRange(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        Cursor oldCursor = beginOperation();
        try {
            shareData(key, privKey);
            status = doGetSearchKeyRange(lockMode);
            if (status == OperationStatus.SUCCESS &&
                !range.check(privKey)) {
                status = OperationStatus.NOTFOUND;
            }
        } finally {
            endOperation(oldCursor, status, key, pKey, data);
        }
        return status;
    }

    public OperationStatus getSearchBothRange(DatabaseEntry key,
                                              DatabaseEntry pKey,
                                              DatabaseEntry data,
                                              LockMode lockMode)
        throws DatabaseException {

        OperationStatus status = OperationStatus.NOTFOUND;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetSearchBothRange(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        Cursor oldCursor = beginOperation();
        try {
            shareData(key, privKey);
            if (secCursor != null) {
                shareData(pKey, privPKey);
            } else {
                shareData(data, privData);
            }
            status = doGetSearchBothRange(lockMode);
            if (status == OperationStatus.SUCCESS &&
                !range.check(privKey)) {
                status = OperationStatus.NOTFOUND;
            }
        } finally {
            endOperation(oldCursor, status, key, pKey, data);
        }
        return status;
    }

    public OperationStatus getSearchRecordNumber(DatabaseEntry key,
                                                 DatabaseEntry pKey,
                                                 DatabaseEntry data,
                                                 LockMode lockMode)
        throws DatabaseException {

        OperationStatus status;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetSearchRecordNumber(lockMode);
            endOperation(null, status, null, null, null);
            return status;
        }
        if (!range.check(key)) {
            status = OperationStatus.NOTFOUND;
        } else {
            shareData(key, privKey);
            status = doGetSearchRecordNumber(lockMode);
            endOperation(null, status, key, pKey, data);
        }
        return status;
    }

    public OperationStatus getNextDup(DatabaseEntry key,
                                      DatabaseEntry pKey,
                                      DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

        if (!initialized) {
            throw new DatabaseException("Cursor not initialized");
        }
        OperationStatus status;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetNextDup(lockMode);
            endOperation(null, status, null, null, null);
        } else {
            status = doGetNextDup(lockMode);
            endOperation(null, status, key, pKey, data);
        }
        return status;
    }

    public OperationStatus getPrevDup(DatabaseEntry key,
                                      DatabaseEntry pKey,
                                      DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

        if (!initialized) {
            throw new DatabaseException("Cursor not initialized");
        }
        OperationStatus status;
        if (!range.hasBound()) {
            setParams(key, pKey, data);
            status = doGetPrevDup(lockMode);
            endOperation(null, status, null, null, null);
        } else {
            status = doGetPrevDup(lockMode);
            endOperation(null, status, key, pKey, data);
        }
        return status;
    }

    public OperationStatus getCurrent(DatabaseEntry key,
                                      DatabaseEntry pKey,
                                      DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

        if (!initialized) {
            throw new DatabaseException("Cursor not initialized");
        }
        if (secCursor != null && pKey != null) {
            return secCursor.getCurrent(key, pKey, data, lockMode);
        } else {
            return cursor.getCurrent(key, data, lockMode);
        }
    }

    /*
     * Pass-thru methods.
     */

    public void close()
        throws DatabaseException {

        currentTxn.closeCursor(cursor);
    }

    public int count()
        throws DatabaseException {

	return cursor.count();
    }

    public OperationStatus delete()
        throws DatabaseException {

	return cursor.delete();
    }

    public OperationStatus put(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {

        return cursor.put(key, data);
    }

    public OperationStatus putNoOverwrite(DatabaseEntry key,
                                          DatabaseEntry data)
        throws DatabaseException {

        return cursor.putNoOverwrite(key, data);
    }

    public OperationStatus putNoDupData(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {

        return cursor.putNoDupData(key, data);
    }

    public OperationStatus putCurrent(DatabaseEntry data)
        throws DatabaseException {

        return cursor.putCurrent(data);
    }

    public OperationStatus putAfter(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {

        return DbCompat.putAfter(cursor, key, data);
    }

    public OperationStatus putBefore(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {

        return DbCompat.putBefore(cursor, key, data);
    }

    private OperationStatus doGetFirst(LockMode lockMode)
        throws DatabaseException {

        if (secCursor != null && privPKey != null) {
            return secCursor.getFirst(privKey, privPKey, privData, lockMode);
        } else {
            return cursor.getFirst(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetLast(LockMode lockMode)
        throws DatabaseException {

        if (secCursor != null && privPKey != null) {
            return secCursor.getLast(privKey, privPKey, privData, lockMode);
        } else {
            return cursor.getLast(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetNext(LockMode lockMode)
        throws DatabaseException {

        if (secCursor != null && privPKey != null) {
            return secCursor.getNext(privKey, privPKey, privData, lockMode);
        } else {
            return cursor.getNext(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetNextDup(LockMode lockMode)
        throws DatabaseException {

        if (secCursor != null && privPKey != null) {
            return secCursor.getNextDup(privKey, privPKey, privData, lockMode);
        } else {
            return cursor.getNextDup(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetNextNoDup(LockMode lockMode)
        throws DatabaseException {

        if (secCursor != null && privPKey != null) {
            return secCursor.getNextNoDup(privKey, privPKey, privData,
                                          lockMode);
        } else {
            return cursor.getNextNoDup(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetPrev(LockMode lockMode)
        throws DatabaseException {

        if (secCursor != null && privPKey != null) {
            return secCursor.getPrev(privKey, privPKey, privData, lockMode);
        } else {
            return cursor.getPrev(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetPrevDup(LockMode lockMode)
        throws DatabaseException {

        if (secCursor != null && privPKey != null) {
            return secCursor.getPrevDup(privKey, privPKey, privData, lockMode);
        } else {
            return cursor.getPrevDup(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetPrevNoDup(LockMode lockMode)
        throws DatabaseException {

        if (secCursor != null && privPKey != null) {
            return secCursor.getPrevNoDup(privKey, privPKey, privData,
                                          lockMode);
        } else {
            return cursor.getPrevNoDup(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetSearchKey(LockMode lockMode)
        throws DatabaseException {

        if (isRecnoOrQueue && DbCompat.getRecordNumber(privKey) <= 0) {
            return OperationStatus.NOTFOUND;
        }
        if (secCursor != null && privPKey != null) {
            return secCursor.getSearchKey(privKey, privPKey, privData,
                                          lockMode);
        } else {
            return cursor.getSearchKey(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetSearchKeyRange(LockMode lockMode)
        throws DatabaseException {

        if (isRecnoOrQueue && DbCompat.getRecordNumber(privKey) <= 0) {
            return OperationStatus.NOTFOUND;
        }
        if (secCursor != null && privPKey != null) {
            return secCursor.getSearchKeyRange(privKey, privPKey, privData,
                                               lockMode);
        } else {
            return cursor.getSearchKeyRange(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetSearchBoth(LockMode lockMode)
        throws DatabaseException {

        if (isRecnoOrQueue && DbCompat.getRecordNumber(privKey) <= 0) {
            return OperationStatus.NOTFOUND;
        }
        if (secCursor != null && privPKey != null) {
            return secCursor.getSearchBoth(privKey, privPKey, privData,
                                           lockMode);
        } else {
            return cursor.getSearchBoth(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetSearchBothRange(LockMode lockMode)
        throws DatabaseException {

        if (isRecnoOrQueue && DbCompat.getRecordNumber(privKey) <= 0) {
            return OperationStatus.NOTFOUND;
        }
        if (secCursor != null && privPKey != null) {
            return secCursor.getSearchBothRange(privKey, privPKey, privData,
                                                lockMode);
        } else {
            return cursor.getSearchBothRange(privKey, privData, lockMode);
        }
    }

    private OperationStatus doGetSearchRecordNumber(LockMode lockMode)
        throws DatabaseException {

        if (DbCompat.getRecordNumber(privKey) <= 0) {
            return OperationStatus.NOTFOUND;
        }
        if (secCursor != null && privPKey != null) {
            return DbCompat.getSearchRecordNumber(secCursor, privKey, privPKey,
                                                  privData, lockMode);
        } else {
            return DbCompat.getSearchRecordNumber(cursor, privKey, privData,
                                                  lockMode);
        }
    }
}
