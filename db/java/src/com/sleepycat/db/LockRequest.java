/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: LockRequest.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */
package com.sleepycat.db;

import com.sleepycat.db.internal.DbLock;

public class LockRequest {
    private DbLock lock;
    private LockRequestMode mode;
    private int modeFlag;
    private DatabaseEntry obj;
    private int op;
    private int timeout;

    public LockRequest(final LockOperation op,
                       final LockRequestMode mode,
                       final DatabaseEntry obj,
                       final Lock lock) {

        this(op, mode, obj, lock, 0);
    }

    public LockRequest(final LockOperation op,
                       final LockRequestMode mode,
                       final DatabaseEntry obj,
                       final Lock lock,
                       final int timeout) {

        this.setOp(op);
        this.setMode(mode);
        this.setObj(obj);
        this.setLock(lock);
        this.setTimeout(timeout);
    }

    public void setLock(final Lock lock) {
        this.lock = lock.unwrap();
    }

    public void setMode(final LockRequestMode mode) {
        this.mode = mode;
        this.modeFlag = mode.getFlag();
    }

    public void setObj(final DatabaseEntry obj) {
        this.obj = obj;
    }

    public void setOp(final LockOperation op) {
        this.op = op.getFlag();
    }

    public void setTimeout(final int timeout) {
        this.timeout = timeout;
    }

    public Lock getLock() {
        return lock.wrapper;
    }

    public LockRequestMode getMode() {
        return mode;
    }

    public DatabaseEntry getObj() {
        return obj;
    }

    public LockOperation getOp() {
        return LockOperation.fromFlag(op);
    }

    public int getTimeout() {
        return timeout;
    }
}
