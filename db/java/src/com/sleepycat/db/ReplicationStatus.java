/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: ReplicationStatus.java,v 12.9 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;

public final class ReplicationStatus {
    static final ReplicationStatus SUCCESS =
        new ReplicationStatus("SUCCESS", 0);

    private int errCode;
    private DatabaseEntry cdata;
    private int envid;
    private LogSequenceNumber lsn;

    /* For toString */
    private String statusName;

    private ReplicationStatus(final String statusName,
                              final int errCode,
                              final DatabaseEntry cdata,
                              final int envid,
                              final LogSequenceNumber lsn) {
        this.statusName = statusName;
        this.errCode = errCode;
        this.cdata = cdata;
        this.envid = envid;
        this.lsn = lsn;
    }

    private ReplicationStatus(final String statusName, final int errCode) {
        this(statusName, errCode, null, 0, null);
    }

    public boolean isSuccess() {
        return errCode == 0;
    }

    public boolean isIgnore() {
        return errCode == DbConstants.DB_REP_IGNORE;
    }

    public boolean isPermanent() {
        return errCode == DbConstants.DB_REP_ISPERM;
    }

    public boolean isNewSite() {
        return errCode == DbConstants.DB_REP_NEWSITE;
    }

    public boolean isNotPermanent() {
        return errCode == DbConstants.DB_REP_NOTPERM;
    }

    public DatabaseEntry getCData() {
        return cdata;
    }

    public int getEnvID() {
        return envid;
    }

    public LogSequenceNumber getLSN() {
        return lsn;
    }

    public String toString() {
        return "ReplicationStatus." + statusName;
    }

    /* package */
    static ReplicationStatus getStatus(final int errCode,
                                       final DatabaseEntry cdata,
                                       final int envid,
                                       final LogSequenceNumber lsn) {
        switch(errCode) {
        case 0:
            return SUCCESS;
        case DbConstants.DB_REP_IGNORE:
            return IGNORE;
        case DbConstants.DB_REP_ISPERM:
            return new ReplicationStatus("ISPERM", errCode, cdata, envid, lsn);
        case DbConstants.DB_REP_NEWSITE:
            return new ReplicationStatus("NEWSITE", errCode, cdata, envid, lsn);
        case DbConstants.DB_REP_NOTPERM:
            return new ReplicationStatus("NOTPERM", errCode, cdata, envid, lsn);
        default:
            throw new IllegalArgumentException(
                "Unknown error code: " + DbEnv.strerror(errCode));
        }
    }

    private static final ReplicationStatus IGNORE =
        new ReplicationStatus("IGNORE", DbConstants.DB_REP_IGNORE);
}
