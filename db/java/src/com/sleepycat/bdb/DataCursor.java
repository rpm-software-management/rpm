/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataCursor.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.db.Db;
import com.sleepycat.db.Dbc;
import com.sleepycat.db.DbException;
import com.sleepycat.db.Dbt;
import com.sleepycat.db.DbTxn;
import java.io.IOException;

/**
 * (<em>internal</em>) Represents a Berkeley DB cursor and adds support for
 * indices, bindings and key ranges.
 *
 * <p><b>NOTE:</b> This classes is internal and may be changed incompatibly or
 * deleted in the future.  It is public only so it may be used by
 * subpackages.</p>
 *
 * <p>This class operates on a view and takes care of reading and updating
 * indices, calling bindings, constraining access to a key range, etc.</p>
 *
 * @author Mark Hayes
 */
public final class DataCursor {

    private Dbc cursor;
    private DbTxn txn;
    private DataView view;
    private DataEnvironment env;
    private DataDb db;
    private KeyRange range;
    private boolean writeAllowed;
    private boolean dirtyRead;
    private DataThang keyThang;
    private DataThang valueThang;
    private DataThang primaryKeyThang;
    private DataThang otherThang;
    private int cursorDbType;
    private DataCursor[] indexCursorsToClose;
    private boolean closeDirect;
    private boolean isJoinCursor;

    /**
     * Creates a cursor for a given view.
     *
     * @param view the database view traversed by the cursor.
     *
     * @param writeAllowed whether the cursor can be used for writing.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public DataCursor(DataView view, boolean writeAllowed)
        throws DbException, IOException {

        init(view, writeAllowed, null, null);
    }

    /**
     * Creates a cursor for a given view and single key range.
     *
     * @param view the database view traversed by the cursor.
     *
     * @param writeAllowed whether the cursor can be used for writing.
     *
     * @param singleKey the single key value.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public DataCursor(DataView view, boolean writeAllowed, Object singleKey)
        throws DbException, IOException {

        init(view, writeAllowed, view.subRange(singleKey), null);
    }

    /**
     * Creates a cursor for a given view and key range.
     *
     * @param view the database view traversed by the cursor.
     *
     * @param writeAllowed whether the cursor can be used for writing.
     *
     * @param beginKey the lower bound.
     *
     * @param beginInclusive whether the lower bound is inclusive.
     *
     * @param endKey the upper bound.
     *
     * @param endInclusive whether the upper bound is inclusive.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public DataCursor(DataView view, boolean writeAllowed,
                      Object beginKey, boolean beginInclusive,
                      Object endKey, boolean endInclusive)
        throws DbException, IOException {

        init(view, writeAllowed,
             view.subRange(beginKey, beginInclusive, endKey, endInclusive),
             null);
    }

    /**
     * Clones a cursor preserving the current position.
     *
     * @param other the cursor to clone.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public DataCursor(DataCursor other)
        throws DbException, IOException {

        this.view = other.view;
        this.env = other.env;
        this.db  = other.db;
        this.writeAllowed = other.writeAllowed;
        this.dirtyRead = other.dirtyRead;

        initThangs();

        this.keyThang.copy(other.keyThang);
        this.valueThang.copy(other.valueThang);
        if (this.primaryKeyThang != this.keyThang) {
            this.primaryKeyThang.copy(other.primaryKeyThang);
        }
        this.range = other.range;

        this.cursor = db.dupCursor(other.cursor, writeAllowed, Db.DB_POSITION);

        this.txn = other.txn;
        this.closeDirect = false;
    }

    DataCursor(DataView view, DataCursor[] indexCursors,
               boolean presorted, boolean closeIndexCursors)
        throws DbException, IOException {

        this.isJoinCursor = true;

        if (view.index != null) {
            throw new IllegalArgumentException(
                "Primary view in join must not be indexed");
        }
        Dbc[] cursors = new Dbc[indexCursors.length];
        for (int i = 0; i < cursors.length; i += 1) {
            DataIndex index = indexCursors[i].view.index;
            if (index == null || index.store != view.store) {
                throw new IllegalArgumentException(
                    "Join cursor for view " + index +
                    " is not indexed on primary store " + view.store);
            }
            cursors[i] = indexCursors[i].cursor;
        }
        Dbc joinCursor = view.store.db.db.join(cursors,
                                           presorted ? Db.DB_JOIN_NOSORT : 0);
        init(view, false, null, joinCursor);
        if (closeIndexCursors) {
            this.indexCursorsToClose = indexCursors;
        }
    }

    private void init(DataView view, boolean writeAllowed, KeyRange range,
                      Dbc cursor)
        throws DbException, IOException {

        this.view = view;
        this.env = view.store.db.env;
        this.range = (range != null) ? range : new KeyRange(view.range);
        this.writeAllowed = writeAllowed && view.isWriteAllowed();
        this.dirtyRead = view.dirtyRead;

        initThangs();

        if (view.index != null) {
            this.db = view.index.db;
        } else {
            this.db = view.store.db;
        }
        this.cursorDbType = this.db.db.getDbType();

        if (cursor != null) {
            this.cursor = cursor;
            this.closeDirect = true;
        } else {
            this.cursor = this.db.openCursor(this.writeAllowed);
            this.closeDirect = false;
        }
    }

    private void initThangs()
        throws DbException, IOException {

        this.keyThang = new DataThang();
        this.primaryKeyThang = (view.index != null)
                                ? (new DataThang()) : keyThang;
        this.valueThang = new DataThang();
    }

    /**
     * Closes a cursor.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public void close()
        throws DbException, IOException {

        if (cursor != null) {
            Dbc toClose = cursor;
            cursor = null;
            if (closeDirect) {
                toClose.close();
            } else {
                db.closeCursor(toClose);
            }
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
     * Returns the view for this cursor, as specified to the constructor.
     *
     * @return the view.
     */
    public DataView getView() {

        return view;
    }

    KeyRange getRange() {

        return range;
    }

    /**
     * Returns whether write is allowed for this cursor, as specified to the
     * constructor.
     *
     * @return whether write is allowed.
     */
    public boolean isWriteAllowed() {

        return writeAllowed;
    }

    /**
     * Returns the key object for the last record read.
     *
     * @return the current key object.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public Object getCurrentKey()
        throws DbException, IOException {

        if (view.keyBinding == null) {
            throw new UnsupportedOperationException(
                "getCurrentKey requires keyBinding");
        }
        return view.makeKey(keyThang);
    }

    /**
     * Returns the value object for the last record read.
     *
     * @return the current value object.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public Object getCurrentValue()
        throws DbException, IOException {

        return view.makeValue(primaryKeyThang, valueThang);
    }

    /**
     * Returns whether record number access is allowed.
     *
     * @return whether record number access is allowed.
     */
    public boolean hasRecNumAccess() {

        return db.hasRecNumAccess();
    }

    /**
     * Returns the record number for the last record read.
     *
     * @return the last read record number.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int getCurrentRecordNumber()
        throws DbException, IOException {

        if (cursorDbType == Db.DB_BTREE) {
            if (otherThang == null) {
                otherThang = new DataThang();
            }
            Dbt discardThang = DataThang.getDiscardDataThang();
            cursor.get(discardThang, otherThang, Db.DB_GET_RECNO);
            return otherThang.get_recno_key_data();
        } else {
            // Assume QUEUE or RECNO database.
            return keyThang.get_recno_key_data();
        }
    }

    /**
     * Perform a database 'get' using the given key and value.
     *
     * @param key the key or null if none is required by the flag.
     *
     * @param value the value or null if none is required by the flag.
     *
     * @param flag a single flag value appropriate for cursor get.
     *
     * @param lockForWrite whether to set the RMW flag.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int get(Object key, Object value, int flag, boolean lockForWrite)
        throws DbException, IOException {

        int err = 0;
        if (view.btreeRecNumAccess && flag == Db.DB_SET) {
            flag = Db.DB_SET_RECNO;
        }
        int indexCursorFlag = flag;
        int indexPrimaryFlag = 0;
        if (flag == Db.DB_GET_BOTH) {
            view.useValue(value, valueThang, null);
            err = view.useKey(key, value, keyThang, range);
            indexCursorFlag = Db.DB_SET;
            indexPrimaryFlag = flag;
        } else if (flag == Db.DB_SET || flag == Db.DB_SET_RANGE ||
                   flag == Db.DB_SET_RECNO) {
            err = view.useKey(key, value, keyThang, range);
        } else if (flag == Db.DB_GET_RECNO || flag == Db.DB_JOIN_ITEM) {
            throw new IllegalArgumentException("flag not supported: " + flag);
        } else {
            // NEXT,PREV,NEXT_DUP,NEXT_NODUP,PREV_NODUP,CURRENT,FIRST,LAST
            if (key != null || value != null) {
                throw new IllegalArgumentException(
                                                "key and value must be null");
            }
        }
        if (err == 0) {
            err = lowLevelGet(flag, indexCursorFlag, indexPrimaryFlag,
                              lockForWrite);
        }
        return err;
    }

    /**
     * Find the given value, using DB_GET_BOTH if possible, or a sequential
     * search otherwise.
     *
     * @param value the value to search for among duplicates at the current
     * position.
     * 
     * @param findFirst whether to find the first or last value.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int find(Object value, boolean findFirst)
        throws DbException, IOException {

        if (isJoinCursor) throw new UnsupportedOperationException();
        view.useValue(value, valueThang, null);
        int err;
        if (view.entityBinding != null && view.index == null &&
            (findFirst || !view.areDuplicatesAllowed())) {
            err = view.useKey(null, value, keyThang, range);
            if (err == 0) {
                err = lowLevelGet(Db.DB_GET_BOTH, Db.DB_SET,
                                  Db.DB_GET_BOTH, false);
            }
        } else {
            if (otherThang == null) {
                otherThang = new DataThang();
            }
            otherThang.copy(valueThang);
            int flag = (findFirst ? Db.DB_FIRST : Db.DB_LAST);
            err = 0;
            while (err == 0) {
                err = get(null, null, flag, false);
                if (err == 0 && valueThang.compareTo(otherThang) == 0) {
                    break;
                } else {
                    flag = (findFirst ? Db.DB_NEXT : Db.DB_PREV);
                }
            }
        }
        return err;
    }

    /**
     * Return the number of duplicates for the current key.
     *
     * @return the number of duplicates.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int count()
        throws DbException, IOException {

        if (isJoinCursor) throw new UnsupportedOperationException();
        return cursor.count(0);
    }

    /**
     * Perform an arbitrary database 'put' operation, optionally returning
     * the previous value.
     *
     * @param key the key to put.
     *
     * @param value the value to put.
     *
     * @param flag a single flag value appropriate for cursor put.
     *
     * @param oldValue holds the old value, or null if the old value should
     * not be returned.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int put(Object key, Object value, int flag, Object[] oldValue)
        throws DbException, IOException {

        return put(key, value, flag, oldValue, false);
    }

    /**
     * Perform an arbitrary database 'put' operation, optionally using the
     * current key instead of the key parameter.
     *
     * @param key the key to put.
     *
     * @param value the value to put.
     *
     * @param flag a single flag value appropriate for cursor put.
     *
     * @param oldValue holds the old value, or null if the old value should
     * not be returned.
     *
     * @param useCurrentKey is true to use the current key rather than the
     * key parameter.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int put(Object key, Object value, int flag, Object[] oldValue,
                   boolean useCurrentKey)
        throws DbException, IOException {

        if (isJoinCursor) throw new UnsupportedOperationException();
        if (view.index != null) {
            throw new UnsupportedOperationException(
                "put with index not allowed");
        }
        boolean doUpdate = false; // update vs insert
        int err = 0;
        if (flag == Db.DB_CURRENT) {
            doUpdate = true;
        } else if (flag == Db.DB_AFTER || flag == Db.DB_BEFORE) {
            // Do nothing.
        } else {
            if (!useCurrentKey) {
                err = view.useKey(key, value, keyThang, range);
                if (err != 0) {
                    throw new IllegalArgumentException("key out of range");
                }
            }
            if (flag == 0) {
                flag = view.areDuplicatesOrdered() ? Db.DB_NODUPDATA
                                                   : Db.DB_KEYFIRST;
            }
            if (flag == Db.DB_KEYFIRST || flag == Db.DB_KEYLAST) {
                if (!view.areDuplicatesAllowed()) {
                    err = lowLevelGet(Db.DB_SET, 0, 0, true);
                    if (err == 0) {
                        doUpdate = true;
                    }
                }
            } else if (flag != Db.DB_NODUPDATA) {
                throw new IllegalArgumentException("flag unknown: " + flag);
            }
        }
        DataThang oldValueThang;
        if (doUpdate) {
            if (oldValue != null) {
                oldValue[0] = getCurrentValue();
            }
            if (otherThang == null) {
                otherThang = new DataThang();
            }
            otherThang.copy(valueThang);
            oldValueThang = otherThang;
        } else {
            if (oldValue != null) {
                oldValue[0] = null;
            }
            oldValueThang = null;
        }
        DataThang checkKeyThang = (flag == Db.DB_AFTER) ? null : keyThang;
        view.useValue(value, valueThang, checkKeyThang);
        err = db.put(cursor, keyThang, valueThang, flag);
        if (err == 0) {
            view.store.applyChange(primaryKeyThang, oldValueThang, valueThang);
        }
        return err;
    }

    /**
     * Perform an arbitrary database 'delete' operation.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     *
     * @throws IOException if an IO problem occurs.
     */
    public int delete()
        throws DbException, IOException {

        if (isJoinCursor || !writeAllowed) {
            throw new UnsupportedOperationException();
        }
        int err;
        if (view.index != null) {
            err = view.store.db.delete(primaryKeyThang, 0);
        } else {
            err = cursor.delete(0);
        }
        if (err == 0) {
            view.store.applyChange(primaryKeyThang, valueThang, null);
        }
        return err;
    }

    private int lowLevelGet(int flag, int indexCursorFlag,
                            int indexPrimaryFlag, boolean lockForWrite)
        throws DbException {

        int err;
        int otherFlags = 0;
        if (dirtyRead)
            otherFlags |= Db.DB_DIRTY_READ;
        // Don't use RMW with dirty-read, since RWM may cause blocking
        if (lockForWrite && !dirtyRead && !env.isDirtyRead())
            otherFlags |= env.getWriteLockFlag();

        // Always clear the cached data formations to prevent inconsistencies.
        keyThang.clearDataFormation();
        primaryKeyThang.clearDataFormation();
        valueThang.clearDataFormation();

        if (view.index != null) {
            if (isJoinCursor) throw new UnsupportedOperationException();
            if (view.btreeRecNumAccess && indexCursorFlag == Db.DB_SET) {
                indexCursorFlag = Db.DB_SET_RECNO;
            }
            err = range.get(db, cursor, keyThang, primaryKeyThang,
                            indexCursorFlag | otherFlags);
            if (err == 0) {
                err = view.store.db.get(primaryKeyThang, valueThang,
                                        indexPrimaryFlag | otherFlags);
            }
        } else {
            if (isJoinCursor) {
                if (flag != Db.DB_FIRST &&
                    flag != Db.DB_NEXT &&
                    flag != Db.DB_NEXT_NODUP) {
                    throw new UnsupportedOperationException();
                }
                flag = 0;
                otherFlags = 0;
            }
            if (view.btreeRecNumAccess && flag == Db.DB_SET) {
                flag = Db.DB_SET_RECNO;
            }
            err = range.get(db, cursor, keyThang, valueThang,
                            flag | otherFlags);
        }
        return err;
    }
}
