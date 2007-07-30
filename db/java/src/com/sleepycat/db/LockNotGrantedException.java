/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: LockNotGrantedException.java,v 12.6 2007/05/17 15:15:41 bostic Exp $
 */
package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;
import com.sleepycat.db.internal.DbLock;

public class LockNotGrantedException extends DeadlockException {
    private int index;
    private Lock lock;
    private int mode;
    private DatabaseEntry obj;
    private int op;

    /* package */ LockNotGrantedException(final String message,
                                      final int op,
                                      final int mode,
                                      final DatabaseEntry obj,
                                      final DbLock lock,
                                      final int index,
                                      final DbEnv dbenv) {
        super(message, DbConstants.DB_LOCK_NOTGRANTED, dbenv);
        this.op = op;
        this.mode = mode;
        this.obj = obj;
        this.lock = (lock == null) ? null : lock.wrapper;
        this.index = index;
    }

    public int getIndex() {
        return index;
    }

    public Lock getLock() {
        return lock;
    }

    public int getMode() {
        return mode;
    }

    public DatabaseEntry getObj() {
        return obj;
    }

    public int getOp() {
        return op;
    }
}
