/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataEnvironment.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import com.sleepycat.db.DbTxn;
import com.sleepycat.bdb.util.RuntimeExceptionWrapper;
import java.io.FileNotFoundException;
import java.util.WeakHashMap;

/**
 * The internal parent class of CurrentTransaction which also contains methods
 * for non-transactional environments.  Unlike {@link
 * CurrentTransaction#getInstance}, {@link #getEnvironment} never returns null
 * and is used by other classes in this package to return info for all types of
 * environments.
 *
 * @author Mark Hayes
 */
class DataEnvironment extends CurrentTransaction {

    private static WeakHashMap envMap = new WeakHashMap();

    private int writeLockFlag;
    private boolean cdbMode;
    private boolean txnMode;
    private ThreadLocal currentTrans = new ThreadLocal();

    /**
     * Gets the DataEnvironment accessor for a specified Berkeley DB
     * environment.
     *
     * @param env is an open Berkeley DB environment.
     *
     * @param envFlags are the flags that were passed to env.open().
     */
    public static DataEnvironment getEnvironment(DbEnv env) {

        synchronized (envMap) {
            DataEnvironment myEnv =
                    (DataEnvironment) envMap.get(env);
            if (myEnv == null) {
                myEnv = new DataEnvironment(env);
                envMap.put(env, myEnv);
            }
            return myEnv;
        }
    }

    private DataEnvironment(DbEnv dbEnv) {

        super(dbEnv);
        try {
            this.txnMode = (dbEnv.getOpenFlags() & Db.DB_INIT_TXN) != 0;
            if (this.txnMode || ((dbEnv.getOpenFlags() & Db.DB_INIT_LOCK) != 0))
                this.writeLockFlag = Db.DB_RMW;
            this.cdbMode = (dbEnv.getOpenFlags() & Db.DB_INIT_CDB) != 0;
        } catch (DbException e) {
            throw new RuntimeExceptionWrapper(e);
        }
    }

    /**
     * Returns whether this is a transactional environement.
     */
    public final boolean isTxnMode() {

        return txnMode;
    }

    /**
     * Returns whether this is a Concurrent Data Store environement.
     */
    public final boolean isCdbMode() {

        return cdbMode;
    }

    /**
     * Return the Db.DB_RMW flag or zero, depending on whether locking is
     * enabled.  Db.DB_RMW will cause an error if passed when locking is not
     * enabled.  Locking is enabled if Db.DB_INIT_LOCK or Db.DB_INIT_TXN were
     * specified for this environment.
     */
    public final int getWriteLockFlag() {

        return writeLockFlag;
    }
}
