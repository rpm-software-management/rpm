/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: SecondaryCursor.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.Dbc;

public class SecondaryCursor extends Cursor {
    /* package */
    SecondaryCursor(final SecondaryDatabase database,
                    final Dbc dbc,
                    final CursorConfig config)
        throws DatabaseException {

        super(database, dbc, config);
    }

    public SecondaryDatabase getSecondaryDatabase() {
        return (SecondaryDatabase)super.getDatabase();
    }

    public Cursor dup(final boolean samePosition)
        throws DatabaseException {

        return dupSecondary(samePosition);
    }

    public SecondaryCursor dupSecondary(final boolean samePosition)
        throws DatabaseException {

        return new SecondaryCursor(getSecondaryDatabase(),
            dbc.dup(samePosition ? DbConstants.DB_POSITION : 0), config);
    }

    public OperationStatus getCurrent(final DatabaseEntry key,
                                      final DatabaseEntry pKey,
                                      final DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_CURRENT | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getFirst(final DatabaseEntry key,
                                    final DatabaseEntry pKey,
                                    final DatabaseEntry data,
                                    LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_FIRST | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getLast(final DatabaseEntry key,
                                   final DatabaseEntry pKey,
                                   final DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_LAST | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getNext(final DatabaseEntry key,
                                   final DatabaseEntry pKey,
                                   final DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_NEXT | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getNextDup(final DatabaseEntry key,
                                      final DatabaseEntry pKey,
                                      final DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

       return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_NEXT_DUP | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getNextNoDup(final DatabaseEntry key,
                                        final DatabaseEntry pKey,
                                        final DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_NEXT_NODUP | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getPrev(final DatabaseEntry key,
                                   final DatabaseEntry pKey,
                                   final DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_PREV | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getPrevDup(final DatabaseEntry key,
                                      final DatabaseEntry pKey,
                                      final DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {

        /*
         * "Get the previous duplicate" isn't directly supported by the C API,
         * so here's how to get it: dup the cursor and call getPrev, then dup
         * the result and call getNextDup.  If both succeed then there was a
         * previous duplicate and the first dup is sitting on it.  Keep that,
         * and call getCurrent to fill in the user's buffers.
         */
        Dbc dup1 = dbc.dup(DbConstants.DB_POSITION);
        try {
            int errCode = dup1.get(DatabaseEntry.IGNORE, DatabaseEntry.IGNORE,
                DbConstants.DB_PREV | LockMode.getFlag(lockMode));
            if (errCode == 0) {
                Dbc dup2 = dup1.dup(DbConstants.DB_POSITION);
                try {
                    errCode = dup2.get(DatabaseEntry.IGNORE,
                        DatabaseEntry.IGNORE,
                        DbConstants.DB_NEXT_DUP | LockMode.getFlag(lockMode));
                } finally {
                    dup2.close();
                }
            }
            if (errCode == 0)
                errCode = dup1.pget(key, pKey, data,
                    DbConstants.DB_CURRENT | LockMode.getFlag(lockMode) |
                    ((data == null) ? 0 : data.getMultiFlag()));
            if (errCode == 0) {
                Dbc tdbc = dbc;
                dbc = dup1;
                dup1 = tdbc;
            }
            return OperationStatus.fromInt(errCode);
        } finally {
            dup1.close();
        }
    }

    public OperationStatus getPrevNoDup(final DatabaseEntry key,
                                        final DatabaseEntry pKey,
                                        final DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_PREV_NODUP | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getRecordNumber(final DatabaseEntry secondaryRecno,
                                           final DatabaseEntry primaryRecno,
                                           LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(DatabaseEntry.IGNORE, secondaryRecno, primaryRecno,
                DbConstants.DB_GET_RECNO | LockMode.getFlag(lockMode)));
    }

    public OperationStatus getSearchKey(final DatabaseEntry key,
                                        final DatabaseEntry pKey,
                                        final DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_SET | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchKeyRange(final DatabaseEntry key,
                                             final DatabaseEntry pKey,
                                             final DatabaseEntry data,
                                             LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_SET_RANGE | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchBoth(final DatabaseEntry key,
                                         final DatabaseEntry pKey,
                                         final DatabaseEntry data,
                                         LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_GET_BOTH | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchBothRange(final DatabaseEntry key,
                                              final DatabaseEntry pKey,
                                              final DatabaseEntry data,
                                              LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(key, pKey, data,
                DbConstants.DB_GET_BOTH_RANGE | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchRecordNumber(
            final DatabaseEntry secondaryRecno,
            final DatabaseEntry pKey,
            final DatabaseEntry data,
            LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.pget(secondaryRecno, pKey, data,
                DbConstants.DB_SET_RECNO | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }
}
