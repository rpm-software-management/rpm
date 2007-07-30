/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: BtreePrefixCalculator.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

public interface BtreePrefixCalculator {
    int prefix(Database db, DatabaseEntry dbt1, DatabaseEntry dbt2);
}
