/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: SecondaryKeyCreator.java,v 1.1 2004/04/06 20:43:40 mjc Exp $
*/

package com.sleepycat.db;

public interface SecondaryKeyCreator {
    boolean createSecondaryKey(SecondaryDatabase secondary,
                                      DatabaseEntry key,
                                      DatabaseEntry data,
                                      DatabaseEntry result)
        throws DatabaseException;
}
