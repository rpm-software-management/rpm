/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbLockRequest.java,v 1.3 2003/12/15 21:44:12 jbj Exp $
 */
package com.sleepycat.db;

/**
 *  The DbLockRequest object is used to encapsulate a single lock
 *  request.</p>
 */
public class DbLockRequest {
    private DbLock lock;
    /*
     *  db_lockmode_t
     */
    private int mode;
    private Dbt obj;

    /*
     *  db_lockop_t
     */
    private int op;
    /*
     *  db_timeout_t
     */
    private int timeout;


    /**
     *  The DbLockRequest constructor constructs a DbLockRequest with
     *  the specified operation, mode and lock, for the specified
     *  object.</p>
     *
     * @param  lock  the lock type for the object.
     * @param  mode  the permissions mode for the object.
     * @param  obj   the object being locked.
     * @param  op    The <b>op</b> parameter operation being
     *      performed.
     */
    public DbLockRequest(int op, int mode, Dbt obj, DbLock lock) {

        this(op, mode, obj, lock, 0);
    }


    /**
     *  The DbLockRequest constructor constructs a DbLockRequest with
     *  the specified operation, mode, lock and timeout for the
     *  specified object.</p>
     *
     * @param  lock     the lock type for the object.
     * @param  mode     the permissions mode for the object.
     * @param  obj      the object being locked.
     * @param  op       the operation being performed.
     * @param  timeout  the timeout value for the lock.
     */
    public DbLockRequest(int op, int mode, Dbt obj, DbLock lock, int timeout) {

        this.op = op;
        this.mode = mode;
        this.obj = obj;
        this.lock = lock;
        this.timeout = timeout;
    }


    /**
     *  The DbLockRequest.setLock method sets the lock reference.</p>
     *
     * @param  lock  the lock reference.
     */
    public void setLock(DbLock lock) {
        this.lock = lock;
    }


    /**
     *  The DbLockRequest.setMode method sets the lock mode.</p>
     *
     * @param  mode  the lock mode.
     */
    public void setMode(int mode) {
        this.mode = mode;
    }


    /**
     *  The DbLockRequest.setObj method sets the lock object.</p>
     *
     * @param  obj  the lock object.
     */
    public void setObj(Dbt obj) {
        this.obj = obj;
    }


    /**
     *  The DbLockRequest.setOp method sets the operation.</p>
     *
     * @param  op  the operation.
     */
    public void setOp(int op) {
        this.op = op;
    }


    /**
     *  The DbLockRequest.setTimeout method sets the lock timeout
     *  value.</p>
     *
     * @param  timeout  the lock timeout value.
     */
    public void setTimeout(int timeout) {
        this.timeout = timeout;
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setLock(DbLock)}
     */
    public void set_lock(DbLock lock) {
        setLock(lock);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setMode(int)}
     */
    public void set_mode(int mode) {
        setMode(mode);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setObj(Dbt)}
     */
    public void set_obj(Dbt obj) {
        setObj(obj);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setOp(int)}
     */
    public void set_op(int op) {
        setOp(op);
    }


    /**
     *  The DbLockRequest.getLock method returns the lock reference.
     *  </p>
     *
     * @return    The DbLockRequest.getLock method returns the lock
     *      reference.</p>
     */
    public DbLock getLock() {
        return lock;
    }


    /**
     *  The DbLockRequest.getMode method returns the lock mode.</p>
     *
     * @return    The DbLockRequest.getMode method returns the lock
     *      mode.</p>
     */
    public int getMode() {
        return mode;
    }


    /**
     *  The DbLockRequest.getObj method returns the object protected
     *  by this lock.</p>
     *
     * @return    The DbLockRequest.getObj method returns the object
     *      protected by this lock.</p>
     */
    public Dbt getObj() {
        return obj;
    }


    /**
     *  The DbLockRequest.getOp method returns the operation.</p>
     *
     * @return    The DbLockRequest.getOp method returns the
     *      operation.</p>
     */
    public int getOp() {
        return op;
    }


    /**
     *  The DbLockRequest.getTimeout method returns the lock timeout
     *  value.</p>
     *
     * @return    The DbLockRequest.getTimeout method returns the lock
     *      timeout value.</p>
     */
    public int getTimeout() {
        return timeout;
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
