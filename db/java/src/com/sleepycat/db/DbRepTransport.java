/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 *      Sleepycat Software.  All rights reserved.
 *
 * Id: DbRepTransport.java,v 11.1 2001/10/04 04:59:15 krinsky Exp 
 */

package com.sleepycat.db;

/*
 * This is used as a callback by DbEnv.set_rep_transport.
 */
public interface DbRepTransport
{
    public int send(DbEnv env, Dbt control, Dbt rec, int flags, int envid)
        throws DbException;
}
