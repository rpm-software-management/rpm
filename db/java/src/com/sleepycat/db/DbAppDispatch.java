/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbAppDispatch.java,v 1.2 2003/12/15 21:44:12 jbj Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a recovery function, which recovers
 *  application-specific actions.</p>
 */
public interface DbAppDispatch {
    /**
     *  The DbAppDispatch interface is used by the
     *  DbEnv.setAppDispatch method. This interface defines the
     *  application-specific function to be called during transaction
     *  abort and recovery.</p> The Db.DB_TXN_FORWARD_ROLL and
     *  Db.DB_TXN_APPLY operations frequently imply the same actions,
     *  redoing changes that appear in the log record, although if a
     *  recovery function is to be used on a replication client where
     *  reads may be taking place concurrently with the processing of
     *  incoming messages, Db.DB_TXN_APPLY operations should also
     *  perform appropriate locking. The macro DB_REDO(op) checks that
     *  the operation is one of Db.DB_TXN_FORWARD_ROLL or
     *  Db.DB_TXN_APPLY, and should be used in the recovery code to
     *  refer to the conditions under which operations should be
     *  redone. Similarly, the macro DB_UNDO(op) checks if the
     *  operation is one of Db.DB_TXN_BACKWARD_ROLL or
     *  Db.DB_TXN_ABORT.</p> </p>
     *
     * @param  dbenv    the enclosing database environment handle.
     * @param  log_rec  a log record.
     * @param  lsn      a log sequence number.
     * @param  op       one of the following values:
     * @return          The function must return 0 on success and
     *      either <b>errno</b> or a value outside of the Berkeley DB
     *      error name space on failure.</p>
     */
    public abstract int appDispatch(DbEnv dbenv, Dbt log_rec, DbLsn lsn, int op);
}
