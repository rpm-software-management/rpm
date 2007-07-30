/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: LockMode.java,v 12.6 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class LockMode {
    private String lockModeName;
    private int flag;

    private LockMode(String lockModeName, int flag) {
        this.lockModeName = lockModeName;
        this.flag = flag;
    }

    public static final LockMode DEFAULT =
        new LockMode("DEFAULT", 0);
    public static final LockMode READ_UNCOMMITTED =
        new LockMode("READ_UNCOMMITTED", DbConstants.DB_READ_UNCOMMITTED);
    public static final LockMode READ_COMMITTED =
        new LockMode("READ_COMMITTED", DbConstants.DB_READ_COMMITTED);
    public static final LockMode RMW =
        new LockMode("RMW", DbConstants.DB_RMW);

    /** @deprecated */
    public static final LockMode DIRTY_READ = READ_UNCOMMITTED;
    /** @deprecated */
    public static final LockMode DEGREE_2 = READ_COMMITTED;

    public String toString() {
        return "LockMode." + lockModeName;
    }

    /* package */
    static int getFlag(LockMode mode) {
        return ((mode == null) ? DEFAULT : mode).flag;
    }
}
