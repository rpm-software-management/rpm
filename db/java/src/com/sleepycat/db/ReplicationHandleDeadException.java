/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 *  $Id: ReplicationHandleDeadException.java,v 1.1 2004/09/23 17:56:39 mjc Exp $
 */
package com.sleepycat.db;

import com.sleepycat.db.internal.DbEnv;

public class ReplicationHandleDeadException extends DatabaseException {
    protected ReplicationHandleDeadException(final String s,
                                   final int errno,
                                   final DbEnv dbenv) {
        super(s, errno, dbenv);
    }
}
