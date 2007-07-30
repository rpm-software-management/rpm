/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: RecordNumberAppender.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */
package com.sleepycat.db;

public interface RecordNumberAppender {
    void appendRecordNumber(Database db, DatabaseEntry data, int recno)
        throws DatabaseException;
}
