/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: Sequence.java,v 12.3 2006/08/24 14:46:09 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbSequence;

public class Sequence {
    private DbSequence seq;
    private int autoCommitFlag;

    /* package */
    Sequence(final DbSequence seq, SequenceConfig config)
        throws DatabaseException {

        this.seq = seq;
        seq.wrapper = this;
        if (seq.get_db().get_transactional())
                this.autoCommitFlag = DbConstants.DB_AUTO_COMMIT |
                    (SequenceConfig.checkNull(config).getAutoCommitNoSync() ?
                    DbConstants.DB_TXN_NOSYNC : 0);
    }

    public void close()
        throws DatabaseException {

        seq.close(0);
    }

    public long get(Transaction txn, int delta)
        throws DatabaseException {

        return seq.get((txn == null) ? null : txn.txn, delta,
            (txn == null) ? autoCommitFlag : 0);
    }

    public Database getDatabase()
        throws DatabaseException {

        return seq.get_db().wrapper;
    }

    public DatabaseEntry getKey()
        throws DatabaseException {

        DatabaseEntry key = new DatabaseEntry();
        seq.get_key(key);
        return key;
    }

    public SequenceStats getStats(StatsConfig config)
        throws DatabaseException {

        return seq.stat(config.getFlags());
    }
}
