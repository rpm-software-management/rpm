/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: ReplicationHostAddress.java,v 12.7 2007/07/06 00:22:54 mjc Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public class ReplicationHostAddress
{
    public int port;
    public String host;
    public int eid;
    private int status;
    public boolean isPeer;

    public ReplicationHostAddress()
    {
        this("localhost", 0, false, false);
    }

    public ReplicationHostAddress(String host, int port)
    {
        this(host, port, false, false);
    }

    public ReplicationHostAddress(String host, int port, boolean isPeer)
    {
        this(host, port, isPeer, false);
    }

    public ReplicationHostAddress(String host, int port,
                                  boolean isPeer, boolean isConnected)
    {
        this.host = host;
        this.port = port;
        this.isPeer = isPeer;
        this.status = isConnected ? DbConstants.DB_REPMGR_CONNECTED : 0;
    }

    public boolean isConnected() {
        return ((this.status & DbConstants.DB_REPMGR_CONNECTED) != 0);
    }
}
