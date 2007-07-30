/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: LockOperation.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class LockOperation {
    public static final LockOperation GET =
        new LockOperation("GET", DbConstants.DB_LOCK_GET);
    public static final LockOperation GET_TIMEOUT =
        new LockOperation("GET_TIMEOUT", DbConstants.DB_LOCK_GET_TIMEOUT);
    public static final LockOperation PUT =
        new LockOperation("PUT", DbConstants.DB_LOCK_PUT);
    public static final LockOperation PUT_ALL =
        new LockOperation("PUT_ALL", DbConstants.DB_LOCK_PUT_ALL);
    public static final LockOperation PUT_OBJ =
        new LockOperation("PUT_OBJ", DbConstants.DB_LOCK_PUT_OBJ);
    public static final LockOperation TIMEOUT =
        new LockOperation("TIMEOUT", DbConstants.DB_LOCK_TIMEOUT);

    /* package */
    static LockOperation fromFlag(int flag) {
        switch (flag) {
        case DbConstants.DB_LOCK_GET:
            return GET;
        case DbConstants.DB_LOCK_GET_TIMEOUT:
            return GET_TIMEOUT;
        case DbConstants.DB_LOCK_PUT:
            return PUT;
        case DbConstants.DB_LOCK_PUT_ALL:
            return PUT_ALL;
        case DbConstants.DB_LOCK_PUT_OBJ:
            return PUT_OBJ;
        case DbConstants.DB_LOCK_TIMEOUT:
            return TIMEOUT;
        default:
            throw new IllegalArgumentException(
                "Unknown lock operation: " + flag);
        }
    }

    private final String operationName;
    private final int flag;

    private LockOperation(final String operationName, final int flag) {
        this.operationName = operationName;
        this.flag = flag;
    }

    public String toString() {
        return "LockOperation." + operationName;
    }

    /* package */
    int getFlag() {
        return flag;
    }
}
