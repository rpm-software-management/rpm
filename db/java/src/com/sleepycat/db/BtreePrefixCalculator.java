/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: BtreePrefixCalculator.java,v 12.3 2006/08/24 14:46:06 bostic Exp $
 */

package com.sleepycat.db;

public interface BtreePrefixCalculator {
    int prefix(Database db, DatabaseEntry dbt1, DatabaseEntry dbt2);
}
