/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbRunRecoveryException.java,v 11.22 2003/11/28 18:35:46 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  This information describes the DbRunRecoveryException class and
 *  how it is used by the various Berkeley DB classes.</p> <p>
 *
 *  Errors can occur in the Berkeley DB library where the only
 *  solution is to shut down the application and run recovery (for
 *  example, if Berkeley DB is unable to allocate heap memory). When a
 *  fatal error occurs in Berkeley DB, methods will throw a
 *  DbRunRecoveryException, at which point all subsequent database
 *  calls will also fail in the same way. When this occurs, recovery
 *  should be performed.</p>
 */
public class DbRunRecoveryException extends DbException {
    /**
     *  Constructor for the DbRunRecoveryException object
     *
     */
    protected DbRunRecoveryException(String s, int errno, DbEnv dbenv) {
        super(s, errno, dbenv);
    }
}
