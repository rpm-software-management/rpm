/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: ErrorHandler.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */
package com.sleepycat.db;

public interface ErrorHandler {
    void error(Environment dbenv, String errpfx, String msg);
}
