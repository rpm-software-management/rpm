/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: ReplicationManagerStartPolicy.java,v 12.4 2006/09/08 20:32:14 bostic Exp $
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

    public static final ReplicationManagerStartPolicy REP_FULL_ELECTION =
        new ReplicationManagerStartPolicy(
        "REP_FULL_ELECTION", DbConstants.DB_REP_FULL_ELECTION);

    /* package */
    static ReplicationManagerStartPolicy fromInt(int type) {
        switch(type) {
        case DbConstants.DB_REP_MASTER:
            return REP_MASTER;
        case DbConstants.DB_REP_CLIENT:
            return REP_CLIENT;
        case DbConstants.DB_REP_ELECTION:
            return REP_ELECTION;
        case DbConstants.DB_REP_FULL_ELECTION:
            return REP_FULL_ELECTION;
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

