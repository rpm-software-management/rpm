/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbException.java,v 11.32 2003/11/28 18:35:43 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  This information describes the DbException class and how it is
 *  used by the various Berkeley DB classes.</p> <p>
 *
 *  Most methods in the Berkeley DB classes throw an exception when an
 *  error occurs. A DbException object contains an informational
 *  string, an errno, and a reference to the environment from which
 *  the exception was thrown.</p> <p>
 *
 *  Some methods may return non-zero values without issuing an
 *  exception. This occurs in situations that are not normally
 *  considered an error, but when some informational status is
 *  returned. For example, {@link com.sleepycat.db.Db#get Db.get}
 *  returns <a href="{@docRoot}/../ref/program/errorret.html#DB_NOTFOUND">
 *  Db.DB_NOTFOUND</a> when a requested key does not appear in the
 *  database.</p>
 */
public class DbException extends Exception {
    private DbEnv dbenv_;
    private int errno_;


    /**
     *  The DbException constructor returns an instance of the
     *  DbException class containing the string.</p>
     *
     * @param  s  specifies a message describing the exception.
     */
    public DbException(String s) {
        this(s, 0, null);
    }


    /**
     *  The DbException constructor returns an instance of the
     *  DbException class containing the string and the encapsulated
     *  errno.</p>
     *
     * @param  s      specifies a message describing the exception.
     * @param  errno  specifies an error code.
     */
    public DbException(String s, int errno) {
        this(s, errno, null);
    }


    /**
     *  The DbException constructor returns an instance of the
     *  DbException class containing the string, the encapsulated
     *  errno, and the database environment.</p>
     *
     * @param  s      specifies a message describing the exception.
     * @param  errno  specifies an error code.
     * @param  dbenv  the database environment where the exception
     *      occurred.
     */
    public DbException(String s, int errno, DbEnv dbenv) {
        super(s);
        this.errno_ = errno;
        this.dbenv_ = dbenv;
    }


    /**
     *  The DbException.getDbEnv method returns the database
     *  environment.</p>
     *
     * @return    The DbException.getDbEnv method returns the database
     *      environment.</p>
     */
    public DbEnv getDbEnv() {
        return dbenv_;
    }


    /**
     *  The DbException.getErrno method returns the error value.</p>
     *
     * @return    The DbException.getErrno method returns the error
     *      value.</p>
     */
    public int getErrno() {

        return errno_;
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getErrno()}
     */
    public int get_errno() {
        return getErrno();
    }


    /**
     * @return    Description of the Return Value
     */
    public String toString() {
        String s = super.toString();
        if (errno_ == 0) {
            return s;
        } else {
            return s + ": " + DbEnv.strerror(errno_);
        }
    }
}
