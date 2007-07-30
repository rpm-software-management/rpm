/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: LockDetectMode.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class LockDetectMode {
    public static final LockDetectMode NONE =
        new LockDetectMode("NONE", 0);

    public static final LockDetectMode DEFAULT =
        new LockDetectMode("DEFAULT", DbConstants.DB_LOCK_DEFAULT);

    public static final LockDetectMode EXPIRE =
        new LockDetectMode("EXPIRE", DbConstants.DB_LOCK_EXPIRE);

    public static final LockDetectMode MAXLOCKS =
        new LockDetectMode("MAXLOCKS", DbConstants.DB_LOCK_MAXLOCKS);

    public static final LockDetectMode MAXWRITE =
        new LockDetectMode("MAXWRITE", DbConstants.DB_LOCK_MAXWRITE);

    public static final LockDetectMode MINLOCKS =
        new LockDetectMode("MINLOCKS", DbConstants.DB_LOCK_MINLOCKS);

    public static final LockDetectMode MINWRITE =
        new LockDetectMode("MINWRITE", DbConstants.DB_LOCK_MINWRITE);

    public static final LockDetectMode OLDEST =
        new LockDetectMode("OLDEST", DbConstants.DB_LOCK_OLDEST);

    public static final LockDetectMode RANDOM =
        new LockDetectMode("RANDOM", DbConstants.DB_LOCK_RANDOM);

    public static final LockDetectMode YOUNGEST =
        new LockDetectMode("YOUNGEST", DbConstants.DB_LOCK_YOUNGEST);

    /* package */
    static LockDetectMode fromFlag(int flag) {
        switch (flag) {
        case 0:
            return NONE;
        case DbConstants.DB_LOCK_DEFAULT:
            return DEFAULT;
        case DbConstants.DB_LOCK_EXPIRE:
            return EXPIRE;
        case DbConstants.DB_LOCK_MAXLOCKS:
            return MAXLOCKS;
        case DbConstants.DB_LOCK_MAXWRITE:
            return MAXWRITE;
        case DbConstants.DB_LOCK_MINLOCKS:
            return MINLOCKS;
        case DbConstants.DB_LOCK_MINWRITE:
            return MINWRITE;
        case DbConstants.DB_LOCK_OLDEST:
            return OLDEST;
        case DbConstants.DB_LOCK_RANDOM:
            return RANDOM;
        case DbConstants.DB_LOCK_YOUNGEST:
            return YOUNGEST;
        default:
            throw new IllegalArgumentException(
                "Unknown lock detect mode: " + flag);
        }
    }

    private String modeName;
    private int flag;

    private LockDetectMode(final String modeName, final int flag) {
        this.modeName = modeName;
        this.flag = flag;
    }

    /* package */
    int getFlag() {
        return flag;
    }

    public String toString() {
        return "LockDetectMode." + modeName;
    }
}
