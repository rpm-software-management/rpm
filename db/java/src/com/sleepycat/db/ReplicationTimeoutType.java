/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: ReplicationTimeoutType.java,v 12.5 2007/06/28 14:23:36 mjc Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class ReplicationTimeoutType {

    public static final ReplicationTimeoutType ACK_TIMEOUT =
        new ReplicationTimeoutType("ACK_TIMEOUT", DbConstants.DB_REP_ACK_TIMEOUT);

    public static final ReplicationTimeoutType CHECKPOINT_DELAY =
        new ReplicationTimeoutType("CHECKPOINT_DELAY", DbConstants.DB_REP_CHECKPOINT_DELAY);

    public static final ReplicationTimeoutType CONNECTION_RETRY =
        new ReplicationTimeoutType("CONNECTION_RETRY", DbConstants.DB_REP_CONNECTION_RETRY);

    public static final ReplicationTimeoutType ELECTION_TIMEOUT =
        new ReplicationTimeoutType("ELECTION_TIMEOUT", DbConstants.DB_REP_ELECTION_TIMEOUT);

    public static final ReplicationTimeoutType ELECTION_RETRY =
        new ReplicationTimeoutType("ELECTION_RETRY", DbConstants.DB_REP_ELECTION_RETRY);

    public static final ReplicationTimeoutType FULL_ELECTION_TIMEOUT =
        new ReplicationTimeoutType("FULL_ELECTION_TIMEOUT", DbConstants.DB_REP_FULL_ELECTION_TIMEOUT);

    /* package */
    static ReplicationTimeoutType fromInt(int type) {
        switch(type) {
        case DbConstants.DB_REP_ACK_TIMEOUT:
            return ACK_TIMEOUT;
        case DbConstants.DB_REP_ELECTION_TIMEOUT:
            return ELECTION_TIMEOUT;
        case DbConstants.DB_REP_ELECTION_RETRY:
            return ELECTION_RETRY;
        case DbConstants.DB_REP_CONNECTION_RETRY:
            return CONNECTION_RETRY;
        default:
            throw new IllegalArgumentException(
                "Unknown timeout type: " + type);
        }
    }

    private String statusName;
    private int id;

    private ReplicationTimeoutType(final String statusName, final int id) {
        this.statusName = statusName;
        this.id = id;
    }

    /* package */
    int getId() {
        return id;
    }

    public String toString() {
        return "ReplicationTimeoutType." + statusName;
    }
}
