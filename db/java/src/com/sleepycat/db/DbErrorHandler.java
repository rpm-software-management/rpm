/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbErrorHandler.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a application-specific error reporting
 *  function.</p>
 */
public interface DbErrorHandler {
    /**
     *  In some cases, when an error occurs, Berkeley DB will call the
     *  DbErrorHandler interface with additional error information. It
     *  is up to this interface to display the error message in an
     *  appropriate manner.</p> <p>
     *
     *  </p>
     *
     * @param  errpfx  the prefix string (as previously set by {@link
     *      com.sleepycat.db.Db#setErrorPrefix Db.setErrorPrefix} or
     *      {@link com.sleepycat.db.DbEnv#setErrorPrefix
     *      DbEnv.setErrorPrefix}).
     * @param  msg     the error message string.
     * @param  errpfx  the prefix string (as previously set by {@link
     *      com.sleepycat.db.Db#setErrorPrefix Db.setErrorPrefix} or
     *      {@link com.sleepycat.db.DbEnv#setErrorPrefix
     *      DbEnv.setErrorPrefix}).
     * @param  msg     the error message string.
     */
    public abstract void error(String errpfx, String msg);
}
