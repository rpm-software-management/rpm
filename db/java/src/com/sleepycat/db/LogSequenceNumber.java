/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: LogSequenceNumber.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbEnv;

public class LogSequenceNumber {
    private int file;
    private int offset;

    public LogSequenceNumber(final int file, final int offset) {
        this.file = file;
        this.offset = offset;
    }

    public LogSequenceNumber() {
        this(0, 0);
    }

    public int getFile() {
        return file;
    }

    public int getOffset() {
        return offset;
    }

    public static int compare(LogSequenceNumber lsn1, LogSequenceNumber lsn2) {
        return DbEnv.log_compare(lsn1, lsn2);
    }
}
