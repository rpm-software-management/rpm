/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbKeyRange.java,v 1.12 2003/10/20 20:12:36 mjc Exp $
 */
package com.sleepycat.db;

/**
 */
public class DbKeyRange {
    /**
     *  A value between 0 and 1, the proportion of keys equal to the
     *  specified key.
     */
    public double equal;
    /**
     *  A value between 0 and 1, the proportion of keys greater than
     *  the specified key.
     */
    public double greater;
    /**
     *  A value between 0 and 1, the proportion of keys less than the
     *  specified key.
     */
    public double less;
}
