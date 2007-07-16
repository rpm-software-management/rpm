/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: EventHandler.java,v 1.2 2006/08/24 14:46:08 bostic Exp $
 */
package com.sleepycat.db;
import com.sleepycat.db.EventType;

public interface EventHandler {
    int handleEvent(EventType event);
}
