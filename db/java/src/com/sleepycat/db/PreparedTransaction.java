/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 *  $Id: PreparedTransaction.java,v 1.1 2004/04/06 20:43:40 mjc Exp $
 */
package com.sleepycat.db;

import com.sleepycat.db.internal.DbTxn;

public class PreparedTransaction {
    private byte[] gid;
    private Transaction txn;

    PreparedTransaction(final DbTxn txn, final byte[] gid) {
        this.txn = new Transaction(txn);
        this.gid = gid;
    }

    public byte[] getGID() {
        return gid;
    }

    public Transaction getTransaction() {
        return txn;
    }
}
