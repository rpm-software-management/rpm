/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2001
 *      Sleepycat Software.  All rights reserved.
 *
 * Id: DbLockRequest.java,v 11.2 2001/10/05 02:36:06 bostic Exp 
 */

package com.sleepycat.db;

public class DbLockRequest
{
    public DbLockRequest(int op, int mode, Dbt obj, DbLock lock)
    {
        this.op = op;
        this.mode = mode;
        this.obj = obj;
        this.lock = lock;
    }

    public int get_op()
    {
        return op;
    }

    public void set_op(int op)
    {
        this.op = op;
    }

    public int get_mode()
    {
        return mode;
    }

    public void set_mode(int mode)
    {
        this.mode = mode;
    }

    public Dbt get_obj()
    {
        return obj;
    }

    public void set_obj(Dbt obj)
    {
        this.obj = obj;
    }

    public DbLock get_lock()
    {
        return lock;
    }

    public void set_lock(DbLock lock)
    {
        this.lock = lock;
    }

    private int op;
    private int mode;
    private Dbt obj;
    private DbLock lock;
}
