/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbFeedbackHandler.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */
package com.sleepycat.db;

/**
 *  The DbFeedbackHandler interface is used by the Db.setFeedback
 *  method. This interface defines the application-specific function
 *  to be called to to report Berkeley DB operation progress.
 */
public interface DbFeedbackHandler {
    /**
     *  The DbFeedbackHandler interface is used by the Db.setFeedback
     *  method. This interface defines the application-specific
     *  function to be called to to report Berkeley DB operation
     *  progress. <p>
     *
     *  </p>
     *
     * @param  db       a reference to the enclosing database.
     * @param  opcode   an operation code. The <b>opcode</b> parameter
     *      may take on any of the following values:
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_UPGRADE
     *        Db.DB_UPGRADE}<p>
     *
     *        The underlying database is being upgraded. </li>
     *      </ul>
     *
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_VERIFY Db.DB_VERIFY}
     *        <p>
     *
     *        The underlying database is being verified. </li>
     *      </ul>
     *
     * @param  percent  the percent of the operation that has been
     *      completed, specified as an integer value between 0 and
     *      100.
     */
    public abstract void feedback(Db db, int opcode, int percent);
}
