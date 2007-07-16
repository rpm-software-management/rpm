/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: RecordNumberAppender.java,v 12.3 2006/08/24 14:46:08 bostic Exp $
 */
package com.sleepycat.db;

public interface RecordNumberAppender {
    void appendRecordNumber(Database db, DatabaseEntry data, int recno)
        throws DatabaseException;
}
