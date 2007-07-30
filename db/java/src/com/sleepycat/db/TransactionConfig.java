/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: TransactionConfig.java,v 12.10 2007/06/28 14:23:36 mjc Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;
import com.sleepycat.db.internal.DbTxn;

public class TransactionConfig implements Cloneable {
    /*
     * For internal use, to allow null as a valid value for
     * the config parameter.
     */
    public static final TransactionConfig DEFAULT = new TransactionConfig();

    /* package */
    static TransactionConfig checkNull(TransactionConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    private boolean readUncommitted = false;
    private boolean readCommitted = false;
    private boolean noSync = false;
    private boolean noWait = false;
    private boolean snapshot = false;
    private boolean sync = false;
    private boolean writeNoSync = false;
    private boolean wait = false;

    public TransactionConfig() {
    }

    public void setReadCommitted(final boolean readCommitted) {
        this.readCommitted = readCommitted;
    }

    public boolean getReadCommitted() {
        return readCommitted;
    }

    /** @deprecated */
    public void setDegree2(final boolean degree2) {
        setReadCommitted(degree2);
    }

    /** @deprecated */
    public boolean getDegree2() {
        return getReadCommitted();
    }

    public void setReadUncommitted(final boolean readUncommitted) {
        this.readUncommitted = readUncommitted;
    }

    public boolean getReadUncommitted() {
        return readUncommitted;
    }

    /** @deprecated */
    public void setDirtyRead(final boolean dirtyRead) {
        setReadUncommitted(dirtyRead);
    }

    /** @deprecated */
    public boolean getDirtyRead() {
        return getReadUncommitted();
    }

    public void setNoSync(final boolean noSync) {
        this.noSync = noSync;
    }

    public boolean getNoSync() {
        return noSync;
    }

    public void setNoWait(final boolean noWait) {
        this.noWait = noWait;
    }

    public boolean getNoWait() {
        return noWait;
    }

    public void setSnapshot(final boolean snapshot) {
        this.snapshot = snapshot;
    }

    public boolean getSnapshot() {
        return snapshot;
    }

    public void setSync(final boolean sync) {
        this.sync = sync;
    }

    public boolean getSync() {
        return sync;
    }

    public void setWait(final boolean wait) {
        this.wait = wait;
    }

    public boolean getWait() {
        return wait;
    }

    public void setWriteNoSync(final boolean writeNoSync) {
        this.writeNoSync = writeNoSync;
    }

    public boolean getWriteNoSync() {
        return writeNoSync;
    }

    DbTxn beginTransaction(final DbEnv dbenv, final DbTxn parent)
        throws DatabaseException {

        int flags = 0;
        flags |= readCommitted ? DbConstants.DB_READ_COMMITTED : 0;
        flags |= readUncommitted ? DbConstants.DB_READ_UNCOMMITTED : 0;
        flags |= noSync ? DbConstants.DB_TXN_NOSYNC : 0;
        flags |= noWait ? DbConstants.DB_TXN_NOWAIT : 0;
        flags |= snapshot ? DbConstants.DB_TXN_SNAPSHOT : 0;
        flags |= sync ? DbConstants.DB_TXN_SYNC : 0;
        flags |= wait ? DbConstants.DB_TXN_WAIT : 0;
        flags |= writeNoSync ? DbConstants.DB_TXN_WRITE_NOSYNC : 0;

        return dbenv.txn_begin(parent, flags);
    }
}
