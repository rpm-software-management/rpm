/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: DatabaseType.java,v 1.2 2004/04/21 01:09:09 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class DatabaseType {
    public static final DatabaseType BTREE =
        new DatabaseType("BTREE", DbConstants.DB_BTREE);

    public static final DatabaseType HASH =
        new DatabaseType("HASH", DbConstants.DB_HASH);

    public static final DatabaseType QUEUE =
        new DatabaseType("QUEUE", DbConstants.DB_QUEUE);

    public static final DatabaseType RECNO =
        new DatabaseType("RECNO", DbConstants.DB_RECNO);

    public static final DatabaseType UNKNOWN =
        new DatabaseType("UNKNOWN", DbConstants.DB_UNKNOWN);

    /* package */
    static DatabaseType fromInt(int type) {
        switch(type) {
        case DbConstants.DB_BTREE:
            return BTREE;
        case DbConstants.DB_HASH:
            return HASH;
        case DbConstants.DB_QUEUE:
            return QUEUE;
        case DbConstants.DB_RECNO:
            return DatabaseType.RECNO;
        case DbConstants.DB_UNKNOWN:
            return DatabaseType.UNKNOWN;
        default:
            throw new IllegalArgumentException(
                "Unknown database type: " + type);
        }
    }

    private String statusName;
    private int id;

    private DatabaseType(final String statusName, final int id) {
        this.statusName = statusName;
        this.id = id;
    }

    /* package */
    int getId() {
        return id;
    }

    public String toString() {
        return "DatabaseType." + statusName;
    }
}
