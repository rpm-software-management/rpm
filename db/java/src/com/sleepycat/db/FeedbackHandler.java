/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: FeedbackHandler.java,v 12.3 2006/08/24 14:46:08 bostic Exp $
 */
package com.sleepycat.db;

public interface FeedbackHandler {
    void recoveryFeedback(Environment dbenv, int percent);
    void upgradeFeedback(Database db, int percent);
    void verifyFeedback(Database db, int percent);
}
