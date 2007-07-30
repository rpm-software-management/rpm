/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: ReplicationTransport.java,v 12.6 2007/05/17 15:15:41 bostic Exp $
 */
package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public interface ReplicationTransport {
    int send(Environment dbenv,
             DatabaseEntry control,
             DatabaseEntry rec,
             LogSequenceNumber lsn,
             int envid,
             boolean noBuffer,
             boolean permanent,
             boolean anywhere,
             boolean isRetry)
        throws DatabaseException;

    int EID_BROADCAST = DbConstants.DB_EID_BROADCAST;
    int EID_INVALID = DbConstants.DB_EID_INVALID;
}
