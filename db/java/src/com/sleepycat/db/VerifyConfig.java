/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: VerifyConfig.java,v 12.3 2006/08/24 14:46:09 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public class VerifyConfig {
    public static final VerifyConfig DEFAULT = new VerifyConfig();

    /* package */
    static VerifyConfig checkNull(VerifyConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    private boolean aggressive = false;
    private boolean noOrderCheck = false;
    private boolean orderCheckOnly = false;
    private boolean salvage = false;
    private boolean printable = false;

    public VerifyConfig() {
    }

    public void setAggressive(final boolean aggressive) {
        this.aggressive = aggressive;
    }

    public boolean getAggressive() {
        return aggressive;
    }

    public void setNoOrderCheck(final boolean noOrderCheck) {
        this.noOrderCheck = noOrderCheck;
    }

    public boolean getNoOrderCheck() {
        return printable;
    }

    public void setOrderCheckOnly(final boolean orderCheckOnly) {
        this.orderCheckOnly = orderCheckOnly;
    }

    public boolean getOrderCheckOnly() {
        return orderCheckOnly;
    }

    public void setPrintable(final boolean printable) {
        this.printable = printable;
    }

    public boolean getPrintable() {
        return printable;
    }

    public void setSalvage(final boolean salvage) {
        this.salvage = salvage;
    }

    public boolean getSalvage() {
        return salvage;
    }

    int getFlags() {
        int flags = 0;
        flags |= aggressive ? DbConstants.DB_AGGRESSIVE : 0;
        flags |= noOrderCheck ? DbConstants.DB_NOORDERCHK : 0;
        flags |= orderCheckOnly ? DbConstants.DB_ORDERCHKONLY : 0;
        flags |= salvage ? DbConstants.DB_SALVAGE : 0;
        flags |= printable ? DbConstants.DB_PRINTABLE : 0;

        return flags;
    }
}
