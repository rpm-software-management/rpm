/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: FeedbackHandler.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */
package com.sleepycat.db;

public interface FeedbackHandler {
    void recoveryFeedback(Environment dbenv, int percent);
    void upgradeFeedback(Database db, int percent);
    void verifyFeedback(Database db, int percent);
}
