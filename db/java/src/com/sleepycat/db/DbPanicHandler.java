/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbPanicHandler.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a function to handle database environment
 *  panics.</p>
 */
public interface DbPanicHandler {
    /**
     *  The DbPanicHandler interface is used by the
     *  DbEnv.setPanicHandler method. This interface defines the
     *  application-specific function to be called when the database
     *  environment panics.</p> <p>
     *
     *  </p>
     *
     * @param  dbenv  the enclosing database environment handle.
     * @param  e      the DbException that would have been thrown to
     *      the calling method.
     * @param  dbenv  the enclosing database environment handle.
     * @param  e      the DbException that would have been thrown to
     *      the calling method.
     */
    public abstract void panic(DbEnv dbenv, DbException e);
}
