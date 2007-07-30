/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: ReplicationConfig.java,v 12.7 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class ReplicationConfig implements Cloneable {
    public static final ReplicationConfig BULK =
        new ReplicationConfig("BULK", DbConstants.DB_REP_CONF_BULK);

    public static final ReplicationConfig DELAYCLIENT =
      new ReplicationConfig("DELAYCLIENT", DbConstants.DB_REP_CONF_DELAYCLIENT);

    public static final ReplicationConfig NOAUTOINIT =
        new ReplicationConfig("NOAUTOINIT", DbConstants.DB_REP_CONF_NOAUTOINIT);

    public static final ReplicationConfig NOWAIT =
        new ReplicationConfig("NOWAIT", DbConstants.DB_REP_CONF_NOWAIT);

    /* package */
    static ReplicationConfig fromInt(int which) {
        switch(which) {
        case DbConstants.DB_REP_CONF_BULK:
            return BULK;
        case DbConstants.DB_REP_CONF_DELAYCLIENT:
            return DELAYCLIENT;
        case DbConstants.DB_REP_CONF_NOAUTOINIT:
            return NOAUTOINIT;
        case DbConstants.DB_REP_CONF_NOWAIT:
            return NOWAIT;
        default:
            throw new IllegalArgumentException(
                "Unknown replication config: " + which);
        }
    }

    private String configName;
    private int flag;

    private ReplicationConfig(final String configName, final int flag) {
        this.configName = configName;
        this.flag = flag;
    }

    /* package */
    int getFlag() {
        return flag;
    }

    public String toString() {
        return "ReplicationConfig." + configName;
    }
}
