/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: BtreePrefixCalculator.java,v 1.1 2004/04/06 20:43:36 mjc Exp $
 */

package com.sleepycat.db;

public interface BtreePrefixCalculator {
    int prefix(Database db, DatabaseEntry dbt1, DatabaseEntry dbt2);
}
