/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: LockRequestMode.java,v 1.2 2004/07/26 17:01:51 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class LockRequestMode {
    public static final LockRequestMode READ =
        new LockRequestMode("READ", DbConstants.DB_LOCK_READ);
    public static final LockRequestMode WRITE =
        new LockRequestMode("WRITE", DbConstants.DB_LOCK_WRITE);
    public static final LockRequestMode IWRITE =
        new LockRequestMode("IWRITE", DbConstants.DB_LOCK_IWRITE);
    public static final LockRequestMode IREAD =
        new LockRequestMode("IREAD", DbConstants.DB_LOCK_IREAD);
    public static final LockRequestMode IWR =
        new LockRequestMode("IWR", DbConstants.DB_LOCK_IWR);

    /* package */
    private final String operationName;
    private final int flag;

    public LockRequestMode(final String operationName, final int flag) {
        this.operationName = operationName;
        this.flag = flag;
    }

    public String toString() {
        return "LockRequestMode." + operationName;
    }

    /* package */
    int getFlag() {
        return flag;
    }
}
