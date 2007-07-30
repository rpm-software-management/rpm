/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class ReplicationManagerStats {
    // no public constructor
    /* package */ ReplicationManagerStats() {}

    private int st_perm_failed;
    public int getPermFailed() {
        return st_perm_failed;
    }

    private int st_msgs_queued;
    public int getMsgsQueued() {
        return st_msgs_queued;
    }

    private int st_msgs_dropped;
    public int getMsgsDropped() {
        return st_msgs_dropped;
    }

    private int st_connection_drop;
    public int getConnectionDrop() {
        return st_connection_drop;
    }

    private int st_connect_fail;
    public int getConnectFail() {
        return st_connect_fail;
    }

    public String toString() {
        return "ReplicationManagerStats:"
            + "\n  st_perm_failed=" + st_perm_failed
            + "\n  st_msgs_queued=" + st_msgs_queued
            + "\n  st_msgs_dropped=" + st_msgs_dropped
            + "\n  st_connection_drop=" + st_connection_drop
            + "\n  st_connect_fail=" + st_connect_fail
            ;
    }
}
