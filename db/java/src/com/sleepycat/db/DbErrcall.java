/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbErrcall.java,v 11.16 2003/10/31 15:02:03 gburd Exp $
 */
package com.sleepycat.db;

/**
 * @deprecated    As of Berkeley DB 4.2, replaced by {@link
 *      DbErrorHandler}
 */
public interface DbErrcall {
    /**
     * @deprecated     As of Berkeley DB 4.2, replaced by {@link
     *      DbErrorHandler#error(String,String)}
     */
    public abstract void errcall(String prefix, String buffer);
}
