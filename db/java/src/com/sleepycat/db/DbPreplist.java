/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2001
 *	Sleepycat Software.  All rights reserved.
 *
 * Id: DbPreplist.java,v 11.1 2001/04/15 15:36:30 dda Exp 
 */

package com.sleepycat.db;

/*
 * This is filled in and returned by the
 * DbEnv.txn_recover() method.
 */
public class DbPreplist
{
    public DbTxn txn;
    public byte gid[];
}

// end of DbPreplist.java
