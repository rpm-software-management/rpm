/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbAppendRecno.java,v 11.22 2003/11/28 18:35:40 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a callback function that modifies stored
 *  data based on a generated key.</p>
 */
public interface DbAppendRecno {
    /**
     *  The DbAppendRecno interface is used by the Db.setAppendRecno
     *  method.</p> The called function may modify the data {@link
     *  com.sleepycat.db.Dbt Dbt}. </p>
     *
     * @param  db            the enclosing database handle.
     * @param  data          the data {@link com.sleepycat.db.Dbt Dbt}
     *      to be stored.
     * @param  recno         the generated record number.
     * @throws  DbException  Signals that an exception of some sort
     *      has occurred.
     */
    public abstract void dbAppendRecno(Db db, Dbt data, int recno)
             throws DbException;
}
