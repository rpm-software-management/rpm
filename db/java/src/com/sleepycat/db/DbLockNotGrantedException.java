/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbLockNotGrantedException.java,v 1.3 2003/12/15 21:44:12 jbj Exp $
 */
package com.sleepycat.db;

/**
 *  This information describes the DbLockNotGrantedException class and
 *  how it is used by the various Db* classes.</p> <p>
 *
 *  A DbLockNotGrantedException is thrown when a lock, requested using
 *  the {@link com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} or {@link
 *  com.sleepycat.db.DbEnv#lockVector DbEnv.lockVector} methods, where
 *  the {@link com.sleepycat.db.Db#DB_LOCK_NOWAIT Db.DB_LOCK_NOWAIT}
 *  option was specified, is unable to be granted immediately.</p>
 */
public class DbLockNotGrantedException extends DbException {
    private int index;
    private DbLock lock;
    private int mode;
    private Dbt obj;

    private int op;


    /**
     *  Constructor for the DbLockNotGrantedException object
     *
     * @param  message  A description of the error
     * @param  op       The operation of the failed lock request
     * @param  mode     The mode of the failed lock request
     * @param  obj      The object of the failed lock request
     * @param  lock     The lock of the failed lock request
     *      (lockVector only)
     * @param  index    The index of the failed lock request
     *      (lockVector only)
     * @param  dbenv    The database environment where the exception
     *      occurred.
     */
    protected DbLockNotGrantedException(String message, int op, int mode, Dbt obj, DbLock lock, int index, DbEnv dbenv) {
        super(message, Db.DB_LOCK_NOTGRANTED, dbenv);
        this.op = op;
        this.mode = mode;
        this.obj = obj;
        this.lock = lock;
        this.index = index;
    }


    /**
     *  The <b>getIndex</b> method returns -1 when {@link
     *  com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was called, and
     *  returns the index of the failed DbLockRequest when {@link
     *  com.sleepycat.db.DbEnv#lockVector DbEnv.lockVector} was
     *  called.</p>
     *
     * @return    The <b>getIndex</b> method returns -1 when {@link
     *      com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was called,
     *      and returns the index of the failed DbLockRequest when
     *      {@link com.sleepycat.db.DbEnv#lockVector DbEnv.lockVector}
     *      was called.</p>
     */
    public int getIndex() {
        return index;
    }


    /**
     *  The <b>getLock</b> method returns null when {@link
     *  com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was called, and
     *  returns the <b>lock</b> in the failed DbLockRequest when
     *  {@link com.sleepycat.db.DbEnv#lockVector DbEnv.lockVector} was
     *  called.</p>
     *
     * @return    The <b>getLock</b> method returns null when {@link
     *      com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was called,
     *      and returns the <b>lock</b> in the failed DbLockRequest
     *      when {@link com.sleepycat.db.DbEnv#lockVector
     *      DbEnv.lockVector} was called.</p>
     */
    public DbLock getLock() {
        return lock;
    }


    /**
     *  The <b>getMode</b> method returns the <b>mode</b> parameter
     *  when {@link com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was
     *  called, and returns the <b>mode</b> for the failed
     *  DbLockRequest when {@link com.sleepycat.db.DbEnv#lockVector
     *  DbEnv.lockVector} was called.</p>
     *
     * @return    The <b>getMode</b> method returns the <b>mode</b>
     *      parameter when {@link com.sleepycat.db.DbEnv#lockGet
     *      DbEnv.lockGet} was called, and returns the <b>mode</b> for
     *      the failed DbLockRequest when {@link
     *      com.sleepycat.db.DbEnv#lockVector DbEnv.lockVector} was
     *      called.</p>
     */
    public int getMode() {
        return mode;
    }


    /**
     *  The <b>getObj</b> method returns the <b>mode</b> parameter
     *  when returns the <b>object</b> parameter when {@link
     *  com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was called, and
     *  returns the <b>object</b> for the failed DbLockRequest when
     *  {@link com.sleepycat.db.DbEnv#lockVector DbEnv.lockVector} was
     *  called. </p>
     *
     * @return    The <b>getObj</b> method returns the <b>mode</b>
     *      parameter when returns the <b>object</b> parameter when
     *      {@link com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was
     *      called, and returns the <b>object</b> for the failed
     *      DbLockRequest when {@link com.sleepycat.db.DbEnv#lockVector
     *      DbEnv.lockVector} was called.</p>
     */
    public Dbt getObj() {

        return obj;
    }


    /**
     *  The <b>getOp</b> method returns 0 when {@link
     *  com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was called, and
     *  returns the <b>op</b> for the failed DbLockRequest when {@link
     *  com.sleepycat.db.DbEnv#lockVector DbEnv.lockVector} was
     *  called.</p>
     *
     * @return    The <b>getOp</b> method returns 0 when {@link
     *      com.sleepycat.db.DbEnv#lockGet DbEnv.lockGet} was called,
     *      and returns the <b>op</b> for the failed DbLockRequest
     *      when {@link com.sleepycat.db.DbEnv#lockVector
     *      DbEnv.lockVector} was called.</p>
     */
    public int getOp() {
        return op;
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getIndex()}
     */
    public int get_index() {
        return getIndex();
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getLock()}
     */
    public DbLock get_lock() {
        return getLock();
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getMode()}
     */
    public int get_mode() {
        return getMode();
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getObj()}
     */
    public Dbt get_obj() {
        return getObj();
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getOp()}
     */
    public int get_op() {
        return getOp();
    }

}

