/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1999-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbSecondaryKeyCreate.java,v 11.16 2003/11/28 18:35:47 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a function which constructs secondary keys
 *  from primary key and data items.</p>
 */
public interface DbSecondaryKeyCreate {
    /**
     *  The secondaryKeyCreate interface is used by the Db.associate
     *  method. This interface defines the application-specific
     *  function to be called to construct secondary keys from primary
     *  key and data items.</p> </p>
     *
     * @param  secondary     the database handle for the secondary.
     * @param  key           a {@link com.sleepycat.db.Dbt Dbt}
     *      referencing the primary key.
     * @param  data          a {@link com.sleepycat.db.Dbt Dbt}
     *      referencing the primary data item.
     * @param  result        a zeroed {@link com.sleepycat.db.Dbt Dbt}
     *      in which the callback function should fill in <b>data</b>
     *      and <b>size</b> fields that describe the secondary key.
     * @throws  DbException  Signals that an exception of some sort
     *      has occurred.
     * @return
     *      <ul>
     *        <li> {@link com.sleepycat.db.Db#DB_DONOTINDEX
     *        DB_DONOTINDEX}<p>
     *
     *        If any key/data pair in the primary yields a null
     *        secondary key and should be left out of the secondary
     *        index, the callback function may optionally return
     *        <code>Db.DB_DONOTINDEX</code>. Otherwise, the callback
     *        function should return 0 in case of success or an error
     *        outside of the Berkeley DB name space in case of
     *        failure; the error code will be returned from the
     *        Berkeley DB call that initiated the callback.</p> <p>
     *
     *        If the callback function returns <code>Db.DB_DONOTINDEX</code>
     *        for any key/data pairs in the primary database, the
     *        secondary index will not contain any reference to those
     *        key/data pairs, and such operations as cursor iterations
     *        and range queries will reflect only the corresponding
     *        subset of the database. If this is not desirable, the
     *        application should ensure that the callback function is
     *        well-defined for all possible values and never returns
     *        <code>Db.DB_DONOTINDEX</code>.</p> </li>
     *      </ul>
     *
     */
    public int secondaryKeyCreate(Db secondary, Dbt key,
            Dbt data, Dbt result)
             throws DbException;
}
