/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 *  $Id: ReplicationTransport.java,v 1.3 2004/07/06 15:06:37 mjc Exp $
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
             boolean permanent)
        throws DatabaseException;

    int EID_BROADCAST = DbConstants.DB_EID_BROADCAST;
    int EID_INVALID = DbConstants.DB_EID_INVALID;
}
