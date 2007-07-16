/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: JoinConfig.java,v 12.3 2006/08/24 14:46:08 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public class JoinConfig implements Cloneable {
    public static final JoinConfig DEFAULT = new JoinConfig();

    /* package */
    static JoinConfig checkNull(JoinConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    private boolean noSort;

    public JoinConfig() {
    }

    public void setNoSort(final boolean noSort) {
        this.noSort = noSort;
    }

    public boolean getNoSort() {
        return noSort;
    }

    /* package */
    int getFlags() {
        return noSort ? DbConstants.DB_JOIN_NOSORT : 0;
    }
}
