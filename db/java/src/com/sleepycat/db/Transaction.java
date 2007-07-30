/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: Transaction.java,v 12.7 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbTxn;

public class Transaction {
    /*package */ final DbTxn txn;

    Transaction(final DbTxn txn) {
        this.txn = txn;
    }

    public void abort()
        throws DatabaseException {

        txn.abort();
    }

    public void commit()
        throws DatabaseException {

        txn.commit(0);
    }

    public void commitSync()
        throws DatabaseException {

        txn.commit(DbConstants.DB_TXN_SYNC);
    }

    public void commitNoSync()
        throws DatabaseException {

        txn.commit(DbConstants.DB_TXN_NOSYNC);
    }

    public void commitWriteNoSync()
        throws DatabaseException {

        txn.commit(DbConstants.DB_TXN_WRITE_NOSYNC);
    }

    public void discard()
        throws DatabaseException {

        txn.discard(0);
    }

    public int getId()
        throws DatabaseException {

        return txn.id();
    }

    public String getName()
        throws DatabaseException {

        return txn.get_name();
    }

    public void prepare(final byte[] gid)
        throws DatabaseException {

        txn.prepare(gid);
    }

    public void setName(final String name)
        throws DatabaseException {

        txn.set_name(name);
    }

    public void setTxnTimeout(final long timeOut)
        throws DatabaseException {

        txn.set_timeout(timeOut, DbConstants.DB_SET_TXN_TIMEOUT);
    }

    public void setLockTimeout(final long timeOut)
        throws DatabaseException {

        txn.set_timeout(timeOut, DbConstants.DB_SET_LOCK_TIMEOUT);
    }
}
