/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2001-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbRepTransport.java,v 1.3 2003/12/15 21:44:12 jbj Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a replication transmit function, which
 *  sends information to other members of the replication group.</p>
 */
public interface DbRepTransport {
    /**
     *  The DbRepTransport interface is used by the
     *  DbEnv.setReplicationTransport method. This interface defines
     *  the application-specific function to be called during
     *  transaction abort and recovery.</p> It may sometimes be useful
     *  to pass application-specific data to the <b>send</b> function;
     *  see <a href="{@docRoot}/../ref/env/faq.html">Environment FAQ
     *  </a> for a discussion on how to do this.</p>
     *
     * @param  dbenv         the enclosing database environment
     *      handle.
     * @param  control       the first of the two data elements to be
     *      transmitted by the <b>send</b> function.
     * @param  rec           the second of the two data elements to be
     *      transmitted by the <b>send</b> function.
     * @param  lsn           If the type of message to be sent has an
     *      LSN associated with it, then the <b>lsn</b> parameter
     *      contains the LSN of the record being sent. This LSN can be
     *      used to determine that certain records have been processed
     *      successfully by clients.
     * @param  envid         a positive integer identifier that
     *      specifies the replication environment to which the message
     *      should be sent (see <a href="{@docRoot}/../ref/rep/id.html">
     *      Replication environment IDs</a> for more information).
     *
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_EID_BROADCAST
     *        Db.DB_EID_BROADCAST}<p>
     *
     *        The special identifier <code>Db.DB_EID_BROADCAST</code>
     *        indicates that a message should be broadcast to every
     *        environment in the replication group. The application
     *        may use a true broadcast protocol or may send the
     *        message in sequence to each machine with which it is in
     *        communication. In both cases, the sending site should
     *        not be asked to process the message.</p> </li>
     *      </ul>
     *
     * @param  flags         must be set to 0 or by bitwise
     *      inclusively <b>OR</b> 'ing together one or more of the
     *      following values:
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_REP_NOBUFFER
     *        Db.DB_REP_NOBUFFER}<p>
     *
     *        The record being sent should be transmitted immediately
     *        and not buffered or delayed. </li>
     *      </ul>
     *
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_REP_PERMANENT
     *        Db.DB_REP_PERMANENT}<p>
     *
     *        The record being sent is critical for maintaining
     *        database integrity (for example, the message includes a
     *        transaction commit). The application should take
     *        appropriate action to enforce the reliability guarantees
     *        it has chosen, such as waiting for acknowledgement from
     *        one or more clients. </li>
     *      </ul>
     *
     * @throws  DbException  Signals that an exception of some sort
     *      has occurred.
     * @return               The <b>send</b> function should not call
     *      back down into Berkeley DB. The <b>send</b> function must
     *      return 0 on success and non-zero on failure. If the <b>
     *      send</b> function fails, the message being sent is
     *      necessary to maintain database integrity, and the local
     *      log is not configured for synchronous flushing, the local
     *      log will be flushed; otherwise, any error from the <b>send
     *      </b> function will be ignored.</p>
     */
    public int send(DbEnv dbenv, Dbt control, Dbt rec, DbLsn lsn, int flags,
            int envid)
             throws DbException;
}
