/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: EventHandlerAdapter.java,v 1.4 2007/07/06 00:22:54 mjc Exp $
 */
package com.sleepycat.db;
import com.sleepycat.db.EventHandler;

/*
 * An abstract implementation of the EventHandler class can be extended by
 * the application to implement customized handling for any event generated
 * by Berkeley DB.
 */

public abstract class EventHandlerAdapter implements EventHandler {
    public void handlePanicEvent() {}
    public void handleRepClientEvent() {}
    public void handleRepElectedEvent() {}
    public void handleRepMasterEvent() {}
    public void handleRepNewMasterEvent(int envId) {}
    public void handleRepPermFailedEvent() {}
    public void handleRepStartupDoneEvent() {}
    public void handleWriteFailedEvent(int errorCode) {}
}
