/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: Transaction.java,v 1.2 2004/04/21 01:09:10 mjc Exp $
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

    public void discard()
        throws DatabaseException {

        txn.discard(0);
    }

    public int getId()
        throws DatabaseException {

        return txn.id();
    }

    public void prepare(final byte[] gid)
        throws DatabaseException {

        txn.prepare(gid);
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
