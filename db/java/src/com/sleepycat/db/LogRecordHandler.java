/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 *  $Id: LogRecordHandler.java,v 1.2 2004/04/21 01:09:09 mjc Exp $
 */
package com.sleepycat.db;

public interface LogRecordHandler {
    int handleLogRecord(Environment dbenv,
                        DatabaseEntry logRecord,
                        LogSequenceNumber lsn,
                        RecoveryOperation operation);
}
