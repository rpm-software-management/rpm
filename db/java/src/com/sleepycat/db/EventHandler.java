/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: EventHandler.java,v 1.7 2007/07/05 12:26:17 alexg Exp $
 */
package com.sleepycat.db;

/*
 * An interface class with prototype definitions of all event functions that
 * can be called via the Berkeley DB event callback mechanism.
 * A user can choose to implement the EventHandler class, and implement
 * handlers for all of the event types. Or they could extend the
 * EventHandlerAdapter class, and implement only those events relevant to
 * their application.
 */
public interface EventHandler {
    public void handlePanicEvent();
    public void handleRepClientEvent();
    public void handleRepElectedEvent();
    public void handleRepMasterEvent();
    public void handleRepNewMasterEvent(int envId);
    public void handleRepPermFailedEvent();
    public void handleRepStartupDoneEvent();
    public void handleWriteFailedEvent(int errorCode);
}
