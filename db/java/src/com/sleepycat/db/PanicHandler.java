/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: PanicHandler.java,v 12.3 2006/08/24 14:46:08 bostic Exp $
 */
package com.sleepycat.db;

public interface PanicHandler {
    void panic(Environment dbenv, DatabaseException e);
}
