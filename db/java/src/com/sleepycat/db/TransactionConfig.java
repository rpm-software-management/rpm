/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TransactionConfig.java,v 1.3 2004/09/28 19:30:37 mjc Exp $
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

    private boolean dirtyRead = false;
    private boolean degree2 = false;
    private boolean noSync = false;
    private boolean noWait = false;
    private boolean sync = false;

    public TransactionConfig() {
    }

    public void setDegree2(final boolean degree2) {
        this.degree2 = degree2;
    }

    public boolean getDegree2() {
        return degree2;
    }

    public void setDirtyRead(final boolean dirtyRead) {
        this.dirtyRead = dirtyRead;
    }

    public boolean getDirtyRead() {
        return dirtyRead;
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

    public void setSync(final boolean sync) {
        this.sync = sync;
    }

    public boolean getSync() {
        return sync;
    }

    DbTxn beginTransaction(final DbEnv dbenv, final DbTxn parent)
        throws DatabaseException {

        int flags = 0;
        flags |= degree2 ? DbConstants.DB_DEGREE_2 : 0;
        flags |= dirtyRead ? DbConstants.DB_DIRTY_READ : 0;
        flags |= noSync ? DbConstants.DB_TXN_NOSYNC : 0;
        flags |= noWait ? DbConstants.DB_TXN_NOWAIT : 0;
        flags |= sync ? DbConstants.DB_TXN_SYNC : 0;

        return dbenv.txn_begin(parent, flags);
    }
}
