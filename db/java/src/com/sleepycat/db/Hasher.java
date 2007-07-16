/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: Hasher.java,v 12.3 2006/08/24 14:46:08 bostic Exp $
 */
package com.sleepycat.db;

public interface Hasher {
    int hash(Database db, byte[] data, int len);
}
