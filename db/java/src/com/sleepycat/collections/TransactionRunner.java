/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TransactionRunner.java,v 1.2 2004/09/22 18:01:03 bostic Exp $
 */

package com.sleepycat.collections;

import com.sleepycat.compat.DbCompat;
import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.DeadlockException;
import com.sleepycat.db.Environment;
import com.sleepycat.db.Transaction;
import com.sleepycat.db.TransactionConfig;
import com.sleepycat.util.ExceptionUnwrapper;

/**
 * Starts a transaction, calls {@link TransactionWorker#doWork}, and handles
 * transaction retry and exceptions.
 *
 * @author Mark Hayes
 */
public class TransactionRunner {

    /** The default maximum number of retries. */
    public static final int DEFAULT_MAX_RETRIES = 10;

    private Environment env;
    private CurrentTransaction currentTxn;
    private int maxRetries;
    private TransactionConfig config;
    private boolean allowNestedTxn;

    /**
     * Creates a transaction runner for a given Berkeley DB environment.
     * The default maximum number of retries ({@link #DEFAULT_MAX_RETRIES}) and
     * a null (default) {@link TransactionConfig} will be used.
     *
     * @param env is the environment for running transactions.
     */
    public TransactionRunner(Environment env) {

        this(env, DEFAULT_MAX_RETRIES, null);
    }

    /**
     * Creates a transaction runner for a given Berkeley DB environment and
     * with a given number of maximum retries.
     *
     * @param env is the environment for running transactions.
     *
     * @param maxRetries is the maximum number of retries that will be
     * performed when deadlocks are detected.
     *
     * @param config the transaction configuration used for calling
     * {@link Environment#beginTransaction}, or null to use the default
     * configuration.  The configuration object is not cloned, and
     * any modifications to it will impact subsequent transactions.
     */
    public TransactionRunner(Environment env, int maxRetries,
                             TransactionConfig config) {

        this.env = env;
        this.currentTxn = CurrentTransaction.getInstance(env);
        this.maxRetries = maxRetries;
        this.config = config;
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
     * Returns whether nested transactions will be created if
     * <code>run()</code> is called when a transaction is already active for
     * the current thread.
     * By default this property is false.
     */
    public boolean getAllowNestedTransactions() {

        return allowNestedTxn;
    }

    /**
     * Changes whether nested transactions will be created if
     * <code>run()</code> is called when a transaction is already active for
     * the current thread.
     * Calling this method does not impact transactions already running.
     */
    public void setAllowNestedTransactions(boolean allowNestedTxn) {

        if (allowNestedTxn && !DbCompat.NESTED_TRANSACTIONS) {
            throw new UnsupportedOperationException(
                    "Nested transactions are not supported.");
        }
        this.allowNestedTxn = allowNestedTxn;
    }

    /**
     * Returns the transaction configuration used for calling
     * {@link Environment#beginTransaction}.
     *
     * <p>If this property is null, the default configuration is used.  The
     * configuration object is not cloned, and any modifications to it will
     * impact subsequent transactions.</p>
     *
     * @return the transaction configuration.
     */
    public TransactionConfig getTransactionConfig() {

        return config;
    }

    /**
     * Changes the transaction configuration used for calling
     * {@link Environment#beginTransaction}.
     *
     * <p>If this property is null, the default configuration is used.  The
     * configuration object is not cloned, and any modifications to it will
     * impact subsequent transactions.</p>
     *
     * @param config the transaction configuration.
     */
    public void setTransactionConfig(TransactionConfig config) {

        this.config = config;
    }

    /**
     * Calls the {@link TransactionWorker#doWork} method and, for transactional
     * environments, begins and ends a transaction.  If the environment given
     * is non-transactional, a transaction will not be used but the doWork()
     * method will still be called.
     *
     * <p> In a transactional environment, a new transaction is started before
     * calling doWork().  This will start a nested transaction if one is
     * already active.  If DeadlockException is thrown by doWork(), the
     * transaction will be aborted and the process will be repeated up to the
     * maximum number of retries specified.  If another exception is thrown by
     * doWork() or the maximum number of retries has occurred, the transaction
     * will be aborted and the exception will be rethrown by this method.  If
     * no exception is thrown by doWork(), the transaction will be committed.
     * This method will not attempt to commit or abort a transaction if it has
     * already been committed or aborted by doWork(). </p>
     *
     * @throws DeadlockException when it is thrown by doWork() and the
     * maximum number of retries has occurred.  The transaction will have been
     * aborted by this method.
     *
     * @throws Exception when any other exception is thrown by doWork().  The
     * exception will first be unwrapped by calling {@link
     * ExceptionUnwrapper#unwrap}.  The transaction will have been aborted by
     * this method.
     */
    public void run(TransactionWorker worker)
        throws DatabaseException, Exception {

        if (currentTxn != null &&
            (allowNestedTxn || currentTxn.getTransaction() == null)) {
            /*
             * Transactional and (not nested or nested txns allowed).
             */
            for (int i = 0;; i += 1) {
                Transaction txn = null;
                try {
                    txn = currentTxn.beginTransaction(config);
                    worker.doWork();
                    if (txn != null && txn == currentTxn.getTransaction()) {
                        currentTxn.commitTransaction();
                    }
                    return;
                } catch (Exception e) {
                    e = ExceptionUnwrapper.unwrap(e);
                    if (txn != null && txn == currentTxn.getTransaction()) {
                        try {
                            currentTxn.abortTransaction();
                        } catch (Exception e2) {
                            /*
                             * XXX We should really throw a 3rd exception that
                             * wraps both e and e2, to give the user a complete
                             * set of error information.
                             */
                            e2.printStackTrace();
                            throw e;
                        }
                    }
                    if (i >= maxRetries || !(e instanceof DeadlockException)) {
                        throw e;
                    }
                }
            }
        } else {
            /*
             * Non-transactional or (nested and no nested txns allowed).
             */
            try {
                worker.doWork();
            } catch (Exception e) {
                throw ExceptionUnwrapper.unwrap(e);
            }
        }
    }
}
