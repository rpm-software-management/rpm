/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: Cursor.java,v 12.8 2007/06/28 14:23:36 mjc Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.Dbc;

public class Cursor {
    /* package */ Dbc dbc;
    /* package */ Database database;
    /* package */ CursorConfig config;

    // Constructor needed by Java RPC server
    protected Cursor(final Database database, final CursorConfig config) {
        this.database = database;
        this.config = config;
    }

    Cursor(final Database database, final Dbc dbc, final CursorConfig config)
        throws DatabaseException {

        this.database = database;
        this.dbc = dbc;
        this.config = config;
    }

    public synchronized void close()
        throws DatabaseException {

        if (dbc != null) {
            try {
                dbc.close();
            } finally {
                dbc = null;
            }
        }
    }

    public Cursor dup(final boolean samePosition)
        throws DatabaseException {

        return new Cursor(database,
            dbc.dup(samePosition ? DbConstants.DB_POSITION : 0), config);
    }

    public CursorConfig getConfig() {
        return config;
    }

    public Database getDatabase() {
        return database;
    }

    public int count()
        throws DatabaseException {

        return dbc.count(0);
    }

    public OperationStatus delete()
        throws DatabaseException {

        return OperationStatus.fromInt(dbc.del(0));
    }

    public OperationStatus getCurrent(final DatabaseEntry key,
                                      final DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_CURRENT |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getFirst(final DatabaseEntry key,
                                    final DatabaseEntry data,
                                    LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_FIRST |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getLast(final DatabaseEntry key,
                                   final DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_LAST |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getNext(final DatabaseEntry key,
                                   final DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_NEXT |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getNextDup(final DatabaseEntry key,
                                      final DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_NEXT_DUP |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getNextNoDup(final DatabaseEntry key,
                                        final DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_NEXT_NODUP |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getPrev(final DatabaseEntry key,
                                   final DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_PREV |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getPrevDup(final DatabaseEntry key,
                                      final DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_PREV_DUP |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getPrevNoDup(final DatabaseEntry key,
                                        final DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_PREV_NODUP |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getRecordNumber(final DatabaseEntry data,
                                           LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(DatabaseEntry.IGNORE, data,
                DbConstants.DB_GET_RECNO |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchKey(final DatabaseEntry key,
                                        final DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_SET |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchKeyRange(final DatabaseEntry key,
                                             final DatabaseEntry data,
                                             LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_SET_RANGE |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchBoth(final DatabaseEntry key,
                                         final DatabaseEntry data,
                                         LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_GET_BOTH |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchBothRange(final DatabaseEntry key,
                                              final DatabaseEntry data,
                                              LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data,
                DbConstants.DB_GET_BOTH_RANGE |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchRecordNumber(final DatabaseEntry key,
                                                 final DatabaseEntry data,
                                                 LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, DbConstants.DB_SET_RECNO |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus put(final DatabaseEntry key,
                               final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.put(key, data, DbConstants.DB_KEYLAST));
    }

    public OperationStatus putAfter(final DatabaseEntry key,
                                    final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.put(key, data, DbConstants.DB_AFTER));
    }

    public OperationStatus putBefore(final DatabaseEntry key,
                                     final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.put(key, data, DbConstants.DB_BEFORE));
    }

    public OperationStatus putNoOverwrite(final DatabaseEntry key,
                                          final DatabaseEntry data)
        throws DatabaseException {

        /*
         * The tricks here are making sure the cursor doesn't move on error and
         * noticing that if the key exists, that's an error and we don't want
         * to return the data.
         */
        Dbc tempDbc = dbc.dup(0);
        try {
            int errCode = tempDbc.get(key, DatabaseEntry.IGNORE,
                DbConstants.DB_SET | database.rmwFlag);
            if (errCode == 0)
                return OperationStatus.KEYEXIST;
            else if (errCode != DbConstants.DB_NOTFOUND &&
                errCode != DbConstants.DB_KEYEMPTY)
                return OperationStatus.fromInt(errCode);
            else {
                Dbc tdbc = dbc;
                dbc = tempDbc;
                tempDbc = tdbc;

                return OperationStatus.fromInt(
                    dbc.put(key, data, DbConstants.DB_KEYLAST));
            }
        } finally {
            tempDbc.close();
        }
    }

    public OperationStatus putKeyFirst(final DatabaseEntry key,
                                       final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.put(key, data, DbConstants.DB_KEYFIRST));
    }

    public OperationStatus putKeyLast(final DatabaseEntry key,
                                      final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.put(key, data, DbConstants.DB_KEYLAST));
    }

    public OperationStatus putNoDupData(final DatabaseEntry key,
                                        final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.put(key, data, DbConstants.DB_NODUPDATA));
    }

    public OperationStatus putCurrent(final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.put(DatabaseEntry.UNUSED, data, DbConstants.DB_CURRENT));
    }

    public CacheFilePriority getPriority()
        throws DatabaseException {

        return CacheFilePriority.fromFlag(dbc.get_priority());
    }

    public void setPriority(final CacheFilePriority priority)
        throws DatabaseException {

        dbc.set_priority(priority.getFlag());
    }
}
