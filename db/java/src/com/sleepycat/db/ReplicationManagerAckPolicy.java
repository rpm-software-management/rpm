/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: ReplicationManagerAckPolicy.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class ReplicationManagerAckPolicy {

    public static final ReplicationManagerAckPolicy ALL =
        new ReplicationManagerAckPolicy("ALL", DbConstants.DB_REPMGR_ACKS_ALL);

    public static final ReplicationManagerAckPolicy ALL_PEERS =
        new ReplicationManagerAckPolicy(
        "ALL_PEERS", DbConstants.DB_REPMGR_ACKS_ALL_PEERS);

    public static final ReplicationManagerAckPolicy NONE =
        new ReplicationManagerAckPolicy(
        "NONE", DbConstants.DB_REPMGR_ACKS_NONE);

    public static final ReplicationManagerAckPolicy ONE =
        new ReplicationManagerAckPolicy("ONE", DbConstants.DB_REPMGR_ACKS_ONE);

    public static final ReplicationManagerAckPolicy ONE_PEER =
        new ReplicationManagerAckPolicy(
            "ONE_PEER", DbConstants.DB_REPMGR_ACKS_ONE_PEER);

    public static final ReplicationManagerAckPolicy QUORUM =
        new ReplicationManagerAckPolicy(
            "QUORUM", DbConstants.DB_REPMGR_ACKS_QUORUM);

    /* package */
    static ReplicationManagerAckPolicy fromInt(int type) {
        switch(type) {
        case DbConstants.DB_REPMGR_ACKS_ALL:
            return ALL;
        case DbConstants.DB_REPMGR_ACKS_ALL_PEERS:
            return ALL_PEERS;
        case DbConstants.DB_REPMGR_ACKS_NONE:
            return NONE;
        case DbConstants.DB_REPMGR_ACKS_ONE:
            return ONE;
        case DbConstants.DB_REPMGR_ACKS_ONE_PEER:
            return ONE_PEER;
        case DbConstants.DB_REPMGR_ACKS_QUORUM:
            return QUORUM;
        default:
            throw new IllegalArgumentException(
                "Unknown ACK policy: " + type);
        }
    }

    private String statusName;
    private int id;

    private ReplicationManagerAckPolicy(final String statusName, final int id) {
        this.statusName = statusName;
        this.id = id;
    }

    /* package */
    int getId() {
        return id;
    }

    public String toString() {
        return "ReplicationManagerAckPolicy." + statusName;
    }
}

