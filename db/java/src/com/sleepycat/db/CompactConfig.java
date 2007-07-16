/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: CompactConfig.java,v 12.4 2006/08/24 14:46:07 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public class CompactConfig implements Cloneable {
    public static final CompactConfig DEFAULT = new CompactConfig();

    /* package */
    static CompactConfig checkNull(CompactConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    private int fillpercent = 0;
    private boolean freeListOnly = false;
    private boolean freeSpace = false;
    private int maxPages = 0;
    private int timeout = 0;

    public CompactConfig() {
    }

    public void setFillPercent(final int fillpercent) {
        this.fillpercent = fillpercent;
    }

    public int getFillPercent() {
        return fillpercent;
    }

    public void setFreeListOnly(boolean freeListOnly) {
        this.freeListOnly = freeListOnly;
    }

    public boolean getFreeListOnly() {
        return freeListOnly;
    }

    public void setFreeSpace(boolean freeSpace) {
        this.freeSpace = freeSpace;
    }

    public boolean getFreeSpace() {
        return freeSpace;
    }

    public void setMaxPages(final int maxPages) {
        this.maxPages = maxPages;
    }

    public int getMaxPages() {
        return maxPages;
    }

    public void setTimeout(final int timeout) {
        this.timeout = timeout;
    }

    public int getTimeout() {
        return timeout;
    }

    /* package */
    int getFlags() {
        int flags = 0;
        flags |= freeListOnly ? DbConstants.DB_FREELIST_ONLY : 0;
        flags |= freeSpace ? DbConstants.DB_FREE_SPACE : 0;

        return flags;
    }
}
