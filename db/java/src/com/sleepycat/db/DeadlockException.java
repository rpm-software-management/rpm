/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: DeadlockException.java,v 12.4 2006/08/24 14:46:07 bostic Exp $
 */
package com.sleepycat.db;

import com.sleepycat.db.internal.DbEnv;

public class DeadlockException extends DatabaseException {
    /* package */ DeadlockException(final String s,
                                final int errno,
                                final DbEnv dbenv) {
        super(s, errno, dbenv);
    }
}
