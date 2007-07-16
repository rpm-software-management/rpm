/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: SecondaryKeyCreator.java,v 12.3 2006/08/24 14:46:09 bostic Exp $
 */

package com.sleepycat.db;

public interface SecondaryKeyCreator {
    boolean createSecondaryKey(SecondaryDatabase secondary,
                                      DatabaseEntry key,
                                      DatabaseEntry data,
                                      DatabaseEntry result)
        throws DatabaseException;
}
