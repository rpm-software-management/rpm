/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: CurrentTransaction.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
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
 * Provides access to the current transaction for the current thread within the
 * context of a Berkeley DB environment.  This class provides explicit
 * transaction control beyond that provided by the {@link TransactionRunner}
 * class.  However, both methods of transaction control manage per-thread
 * transactions.
 *
 * @author Mark Hayes
 */
public class CurrentTransaction {

    private DbEnv dbEnv;
    private ThreadLocal currentTrans = new ThreadLocal();

    /**
     * Gets the CurrentTransaction accessor for a specified Berkeley DB
     * environment.  This method always returns the same reference when called
     * more than once with the same environment parameter.
     *
     * @param env is an open Berkeley DB environment.
     *
     * @return the CurrentTransaction accessor for the given environment, or
     * null if the environment is not transactional.
     */
    public static CurrentTransaction getInstance(DbEnv env) {

        DataEnvironment currentTxn = DataEnvironment.getEnvironment(env);
        return currentTxn.isTxnMode() ? currentTxn : null;
    }

    CurrentTransaction(DbEnv dbEnv) {

        this.dbEnv = dbEnv;
    }

    /**
     * Returns the underlying Berkeley DB environment.
     */
    public final DbEnv getEnv() {

        return dbEnv;
    }

    /**
     * Returns whether AUTO_COMMIT will be used for all non-cursor write
     * operations when no transaction is active.
     */
    public final boolean isAutoCommit() {

        try {
            return (dbEnv.getFlags() & Db.DB_AUTO_COMMIT) != 0;
        } catch (DbException e) {
            throw new RuntimeExceptionWrapper(e);
        }
    }

    /**
     * Returns whether dirty-read is used for the current transaction.
     */
    public final boolean isDirtyRead() {

        Trans trans = (Trans) currentTrans.get();
        return (trans != null) ? trans.dirtyRead : false;
    }

    /**
     * Returns whether no-wait is used for the current transaction.
     */
    public final boolean isNoWait() {

        Trans trans = (Trans) currentTrans.get();
        return (trans != null) ? trans.noWait : false;
    }

    /**
     * Returns the transaction associated with the current thread for this
     * environment, or null if no transaction is active.
     */
    public final DbTxn getTxn() {

        Trans trans = (Trans) currentTrans.get();
        return (trans != null) ? trans.txn : null;
    }

    /**
     * Begins a new transaction for this environment and associates it with
     * the current thread.  If a transaction is already active for this
     * environment and thread, a nested transaction will be created.
     *
     * @return the new transaction.
     *
     * @throws DbException if the transaction cannot be started, in which case
     * any existing transaction is not affected.
     */
    public final DbTxn beginTxn()
        throws DbException {

        return beginTxn(false, false);
    }

    /**
     * Begins a new transaction for this environment and associates it with
     * the current thread.  If a transaction is already active for this
     * environment and thread, a nested transaction will be created.
     *
     * @param dirtyRead true if this transaction should read data that is
     * modified by another transaction but not committed.
     *
     * @param noWait true if this transaction should throw
     * DbLockNotGrantedException instead of blocking when trying to access data
     * that is locked by another transaction.
     *
     * @return the new transaction.
     *
     * @throws DbException if the transaction cannot be started, in which case
     * any existing transaction is not affected.
     */
    public final DbTxn beginTxn(boolean dirtyRead, boolean noWait)
        throws DbException {

        int flags = 0;
        if (dirtyRead) flags |= Db.DB_DIRTY_READ;
        if (noWait) flags |= Db.DB_TXN_NOWAIT;

        Trans trans = (Trans) currentTrans.get();
        if (trans != null) {
            if (trans.txn != null) {
                DbTxn parentTxn = trans.txn;
                trans = new Trans(trans, dirtyRead, noWait);
                trans.txn = dbEnv.txnBegin(parentTxn, flags);
                currentTrans.set(trans);
            } else {
                trans.txn = dbEnv.txnBegin(null, flags);
            }
        } else {
            trans = new Trans(null, dirtyRead, noWait);
            trans.txn = dbEnv.txnBegin(null, flags);
            currentTrans.set(trans);
        }
        return trans.txn;
    }

    /**
     * Commits the transaction that is active for the current thread for this
     * environment and makes the parent transaction (if any) the current
     * transaction.
     *
     * @return the parent transaction or null if the committed transaction was
     * not nested.
     *
     * @throws DbException if an error occurs commiting the transaction.  The
     * transaction will still be closed and the parent transaction will become
     * the current transaction.
     *
     * @throws IllegalStateException if no transaction is active for the
     * current thread for this environement.
     */
    public final DbTxn commitTxn()
        throws DbException, IllegalStateException {

        Trans trans = (Trans) currentTrans.get();
        if (trans != null && trans.txn != null) {
            DbTxn parent = closeTxn(trans);
            trans.txn.commit(0);
            return parent;
        } else {
            throw new IllegalStateException("No transaction is active");
        }
    }

    /**
     * Aborts the transaction that is active for the current thread for this
     * environment and makes the parent transaction (if any) the current
     * transaction.
     *
     * @return the parent transaction or null if the aborted transaction was
     * not nested.
     *
     * @throws DbException if an error occurs aborting the transaction.  The
     * transaction will still be closed and the parent transaction will become
     * the current transaction.
     *
     * @throws IllegalStateException if no transaction is active for the
     * current thread for this environement.
     */
    public final DbTxn abortTxn()
        throws DbException, IllegalStateException {

        Trans trans = (Trans) currentTrans.get();
        if (trans != null && trans.txn != null) {
            DbTxn parent = closeTxn(trans);
            trans.txn.abort();
            return parent;
        } else {
            throw new IllegalStateException("No transaction is active");
        }
    }

    private DbTxn closeTxn(Trans trans) {

        currentTrans.set(trans.parent);
        return (trans.parent != null) ? trans.parent.txn : null;
    }

    private static class Trans {

        private DbTxn txn;
        private Trans parent;
        private boolean dirtyRead;
        private boolean noWait;

        private Trans(Trans parent, boolean dirtyRead, boolean noWait) {

            this.parent = parent;
            this.dirtyRead = dirtyRead;
            this.noWait = noWait;
        }
    }
}
