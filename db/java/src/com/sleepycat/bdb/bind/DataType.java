/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataType.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind;

/**
 * Primitive data type constants.
 *
 * @author Mark Hayes
 */
public interface DataType {

    /** Undefined data type. */
    public static final int NONE = 0;

    /** <code>String</code> data type. */
    public static final int STRING = 1;

    /** <code>byte[]</code> data type. */
    public static final int BINARY = 2;

    /** <code>Integer</code> data type. */
    public static final int INT = 3;

    /** <code>Long</code> data type. */
    public static final int LONG = 4;

    /** <code>Float</code> data type. */
    public static final int FLOAT = 5;

    /** <code>Double</code> data type. */
    public static final int DOUBLE = 6;

    /** <code>Date</code> data type. */
    public static final int DATETIME = 7;
}
