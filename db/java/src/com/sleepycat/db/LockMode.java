/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: LockMode.java,v 1.2 2004/04/09 15:08:38 mjc Exp $
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
    public static final LockMode DIRTY_READ =
        new LockMode("DIRTY_READ", DbConstants.DB_DIRTY_READ);
    public static final LockMode DEGREE_2 =
        new LockMode("DEGREE_2", DbConstants.DB_DEGREE_2);
    public static final LockMode RMW =
        new LockMode("RMW", DbConstants.DB_RMW);

    public String toString() {
        return "LockMode." + lockModeName;
    }

    /* package */
    static int getFlag(LockMode mode) {
        return ((mode == null) ? DEFAULT : mode).flag;
    }
}
