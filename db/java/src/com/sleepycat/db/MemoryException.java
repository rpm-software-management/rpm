/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: MemoryException.java,v 12.4 2006/08/24 14:46:08 bostic Exp $
 */
package com.sleepycat.db;

import com.sleepycat.db.internal.DbEnv;

public class MemoryException extends DatabaseException {
    private DatabaseEntry dbt = null;
    private String message;

    /* package */ MemoryException(final String s,
                              final DatabaseEntry dbt,
                              final int errno,
                              final DbEnv dbenv) {
        super(s, errno, dbenv);
        this.message = s;
        this.dbt = dbt;
    }

    public DatabaseEntry getDatabaseEntry() {
        return dbt;
    }

    public String toString() {
        return message;
    }

    void updateDatabaseEntry(final DatabaseEntry newEntry) {
        if (this.dbt == null) {
            this.message = "DatabaseEntry not large enough for available data";
            this.dbt = newEntry;
        }
    }
}
