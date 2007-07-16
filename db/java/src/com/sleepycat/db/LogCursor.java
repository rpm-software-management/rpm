/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: LogCursor.java,v 12.4 2006/08/24 14:46:08 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbLogc;

public class LogCursor {
    /* package */ DbLogc logc;

    /* package */ LogCursor(final DbLogc logc) {
        this.logc = logc;
    }

    /* package */
    static LogCursor wrap(DbLogc logc) {
        return (logc == null) ? null : new LogCursor(logc);
    }

    public synchronized void close()
        throws DatabaseException {

        logc.close(0);
    }

    public OperationStatus getCurrent(final LogSequenceNumber lsn,
                                      final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            logc.get(lsn, data, DbConstants.DB_CURRENT));
    }

    public OperationStatus getNext(final LogSequenceNumber lsn,
                                   final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            logc.get(lsn, data, DbConstants.DB_NEXT));
    }

    public OperationStatus getFirst(final LogSequenceNumber lsn,
                                    final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            logc.get(lsn, data, DbConstants.DB_FIRST));
    }

    public OperationStatus getLast(final LogSequenceNumber lsn,
                                   final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            logc.get(lsn, data, DbConstants.DB_LAST));
    }

    public OperationStatus getPrev(final LogSequenceNumber lsn,
                                   final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            logc.get(lsn, data, DbConstants.DB_PREV));
    }

    public OperationStatus set(final LogSequenceNumber lsn,
                               final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            logc.get(lsn, data, DbConstants.DB_SET));
    }
}
