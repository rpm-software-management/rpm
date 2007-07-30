/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: ReplicationManagerStartPolicy.java,v 12.7 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class ReplicationManagerStartPolicy {

    public static final ReplicationManagerStartPolicy REP_MASTER =
        new ReplicationManagerStartPolicy(
        "REP_MASTER", DbConstants.DB_REP_MASTER);

    public static final ReplicationManagerStartPolicy REP_CLIENT =
        new ReplicationManagerStartPolicy(
        "REP_CLIENT", DbConstants.DB_REP_CLIENT);

    public static final ReplicationManagerStartPolicy REP_ELECTION =
        new ReplicationManagerStartPolicy(
        "REP_ELECTION", DbConstants.DB_REP_ELECTION);

    /* package */
    static ReplicationManagerStartPolicy fromInt(int type) {
        switch(type) {
        case DbConstants.DB_REP_MASTER:
            return REP_MASTER;
        case DbConstants.DB_REP_CLIENT:
            return REP_CLIENT;
        case DbConstants.DB_REP_ELECTION:
            return REP_ELECTION;
        default:
            throw new IllegalArgumentException(
                "Unknown rep start policy: " + type);
        }
    }

    private String statusName;
    private int id;

    private ReplicationManagerStartPolicy(final String statusName,
        final int id) {

        this.statusName = statusName;
        this.id = id;
    }

    /* package */
    int getId() {
        return id;
    }

    public String toString() {
        return "ReplicationManagerStartPolicy." + statusName;
    }
}

