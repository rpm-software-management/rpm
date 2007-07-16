/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: LogSequenceNumber.java,v 12.3 2006/08/24 14:46:08 bostic Exp $
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
