/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: LogCursor.java,v 12.7 2007/06/28 14:23:36 mjc Exp $
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

    public int version()
        throws DatabaseException {

        return logc.version(0);
    }
}
