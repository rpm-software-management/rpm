/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TransactionRunner.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.db.DbException;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbDeadlockException;
import com.sleepycat.db.DbTxn;
import com.sleepycat.bdb.CurrentTransaction;
import com.sleepycat.bdb.util.ExceptionUnwrapper;

/**
 * Starts a transaction, calls {@link TransactionWorker#doWork}, and handles
 * transaction retry and exceptions.
 *
 * @author Mark Hayes
 */
public class TransactionRunner {

    private static final int DEFAULT_MAX_RETRIES = 10;

    private DbEnv env;
    private CurrentTransaction currentTxn;
    private int maxRetries;
    private boolean dirtyRead;
    private boolean noWait;

    /**
     * Creates a transaction runner for a given Berkeley DB environment.
     * The default maximum number of retries (10) will be used.
     *
     * @param env is the environment for running transactions.
     */
    public TransactionRunner(DbEnv env) {

        this(env, DEFAULT_MAX_RETRIES);
    }

    /**
     * Creates a transaction runner for a given Berkeley DB environment and
     * with a given number of maximum retries.
     *
     * @param env is the environment for running transactions.
     *
     * @param maxRetries is the maximum number of retries that will be performed
     * when deadlocks are detected.
     */
    public TransactionRunner(DbEnv env, int maxRetries) {

        this.env = env;
        this.currentTxn = CurrentTransaction.getInstance(env);
        this.maxRetries = maxRetries;
    }

    /**
     * Returns the maximum number of retries that will be performed when
     * deadlocks are detected.
     */
    public int getMaxRetries() {

        return maxRetries;
    }

    /**
     * Changes the maximum number of retries that will be performed when
     * deadlocks are detected.
     * Calling this method does not impact transactions already running.
     */
    public void setMaxRetries(int maxRetries) {

        this.maxRetries = maxRetries;
    }

    /**
     * Returns whether transactions will read data that is modified by another
     * transaction but not committed.
     */
    public boolean getDirtyRead() {

        return dirtyRead;
    }

    /**
     * Changes whether transactions will read data that is modified by another
     * transaction but not committed.
     * Calling this method does not impact transaction already running.
     */
    public void setDirtyRead(boolean dirtyRead) {

        this.dirtyRead = dirtyRead;
    }

    /**
     * Returns whether transactions will throw DbLockNotGrantedException
     * instead of blocking when trying to access data that is locked by another
     * transaction.
     */
    public boolean getNoWait() {

        return noWait;
    }

    /**
     * Changes whether transactions will throw DbLockNotGrantedException
     * instead of blocking when trying to access data that is locked by another
     * transaction.
     */
    public void setNoWait(boolean noWait) {

        this.noWait = noWait;
    }

    /**
     * Calls the {@link TransactionWorker#doWork} method and, for transactional
     * environments, begins and ends a transaction.  If the environment given
     * is non-transactional, a transaction will not be used but the doWork()
     * method will still be called.
     *
     * <p> In a transactional environment, a new transaction is started before
     * calling doWork().  This will start a nested transaction if one is
     * already active.  If DbDeadlockException is thrown by doWork(), the
     * transaction will be aborted and the process will be repeated up to the
     * maximum number of retries specified.  If another exception is thrown by
     * doWork() or the maximum number of retries has occurred, the transaction
     * will be aborted and the exception will be rethrown by this method.  If
     * no exception is thrown by doWork(), the transaction will be committed.
     * This method will not attempt to commit or abort a transaction if it has
     * already been committed or aborted by doWork(). </p>
     *
     * @throws DbDeadlockException when it is thrown by doWork() and the
     * maximum number of retries has occurred.  The transaction will have been
     * aborted by this method.
     *
     * @throws Exception when any other exception is thrown by doWork().  The
     * exception will first be unwrapped by calling {@link
     * ExceptionUnwrapper#unwrap}.  The transaction will have been aborted by
     * this method.
     */
    public void run(TransactionWorker worker)
        throws DbException, Exception {

        if (currentTxn != null) { // if environment is transactional
            boolean dr = dirtyRead; // these values should not be changed
            boolean nw = noWait;    // until this method returns
            for (int i = 0;; i += 1) {
                DbTxn txn = null;
                try {
                    txn = currentTxn.beginTxn(dr, nw);
                    worker.doWork();
                    if (txn != null && txn == currentTxn.getTxn())
                        currentTxn.commitTxn();
                    return;
                } catch (Exception e) {
                    e = ExceptionUnwrapper.unwrap(e);
                    if (txn != null && txn == currentTxn.getTxn()) {
                        try {
                            currentTxn.abortTxn();
                        } catch (Exception e2) {
                            // If we cannot abort, better to throw e than e2
                            // since it contains more information about the
                            // real problem.
                            System.err.println(e2.toString());
                            throw e;
                        }
                    }
                    if (i >= maxRetries || !(e instanceof DbDeadlockException))
                        throw e;
                }
            }
        } else {
            try {
                worker.doWork();
            } catch (Exception e) {
                throw ExceptionUnwrapper.unwrap(e);
            }
        }
    }
}
