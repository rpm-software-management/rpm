/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: StatsConfig.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public class StatsConfig {
    /*
     * For internal use, to allow null as a valid value for
     * the config parameter.
     */
    public static final StatsConfig DEFAULT = new StatsConfig();

    /* package */
    static StatsConfig checkNull(StatsConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    private boolean clear = false;
    private boolean fast = false;

    public StatsConfig() {
    }

    public void setClear(boolean clear) {
        this.clear = clear;
    }

    public boolean getClear() {
        return clear;
    }

    public void setFast(boolean fast) {
        this.fast = fast;
    }

    public boolean getFast() {
        return fast;
    }

    int getFlags() {
        int flags = 0;
        if (fast)
                flags |= DbConstants.DB_FAST_STAT;
        if (clear)
                flags |= DbConstants.DB_STAT_CLEAR;
        return flags;
    }
}
