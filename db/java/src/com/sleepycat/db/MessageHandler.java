/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 *  $Id: MessageHandler.java,v 1.2 2004/04/20 20:45:11 mjc Exp $
 */
package com.sleepycat.db;

public interface MessageHandler {
    void message(Environment dbenv, String message);
}
