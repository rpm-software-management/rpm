/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1999-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbMemoryException.java,v 11.28 2003/11/28 18:35:45 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  This information describes the DbMemoryException class and how it
 *  is used by the various Db* classes.</p> <p>
 *
 *  A DbMemoryException is thrown when there is insufficient memory to
 *  complete an operation, and there is the possibility of recovering.
 *  An example is during a {@link com.sleepycat.db.Db#get Db.get} or
 *  {@link com.sleepycat.db.Dbc#get Dbc.get} operation with the {@link
 *  com.sleepycat.db.Dbt Dbt} flags set to {@link
 *  com.sleepycat.db.Db#DB_DBT_USERMEM Db.DB_DBT_USERMEM}.</p> <p>
 *
 *  In a Java Virtual Machine, there are usually separate heaps for
 *  memory allocated by native code and for objects allocated in Java
 *  code. If the Java heap is exhausted, the JVM will throw an
 *  OutOfMemoryError, so you may see that exception rather than
 *  DbMemoryException.</p>
 */
public class DbMemoryException extends DbException {

    Dbt dbt = null;
    String message;


    /**
     *  Constructor for the DbMemoryException object
     *
     */
    protected DbMemoryException(String s, Dbt dbt, int errno, DbEnv dbenv) {
        super(s, errno, dbenv);
        this.message = s;
        this.dbt = dbt;
    }


    /**
     *  The <b>getDbt</b> method returns the {@link
     *  com.sleepycat.db.Dbt Dbt} with insufficient memory to complete
     *  the operation, causing the DbMemoryException to be thrown.
     *  </p>
     *
     * @return    The <b>getDbt</b> method returns the {@link
     *      com.sleepycat.db.Dbt Dbt} with insufficient memory to
     *      complete the operation, causing the DbMemoryException to
     *      be thrown.</p>
     */
    public Dbt getDbt() {
        return dbt;
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getDbt()}
     */
    public Dbt get_dbt() {
        return getDbt();
    }


    /**
     *  Override of DbException.toString(): the extra verbage that
     *  comes from DbEnv.strerror(ENOMEM) is not helpful.
     *
     * @return    Description of the Return Value
     */
    public String toString() {
        return message;
    }


    /**
     *  The updateDbt method is called from native code to set the
     *  Dbt. If it is called multiple times, only the first Dbt is
     *  saved.
     *
     * @param  dbt  The Dbt object for which this exception was thrown
     */
    void updateDbt(Dbt dbt) {
        if (this.dbt == null) {
            this.message = "Dbt not large enough for available data";
            this.dbt = dbt;
        }
    }


    /**
     * @param  dbt    The Dbt object for which this exception was
     *      thrown
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #updateDbt(Dbt)}
     */
    void update_dbt(Dbt dbt) {
        updateDbt(dbt);
    }
}

// end of DbMemoryException.java
