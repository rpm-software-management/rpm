/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 *  $Id: RecordNumberAppender.java,v 1.1 2004/04/06 20:43:40 mjc Exp $
 */
package com.sleepycat.db;

public interface RecordNumberAppender {
    void appendRecordNumber(Database db, DatabaseEntry data, int recno)
        throws DatabaseException;
}
