/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: OperationStatus.java,v 1.2 2004/04/21 01:09:09 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;

public final class OperationStatus {
    public static final OperationStatus SUCCESS =
        new OperationStatus("SUCCESS", 0);
    public static final OperationStatus KEYEXIST =
        new OperationStatus("KEYEXIST", DbConstants.DB_KEYEXIST);
    public static final OperationStatus KEYEMPTY =
        new OperationStatus("KEYEMPTY", DbConstants.DB_KEYEMPTY);
    public static final OperationStatus NOTFOUND =
        new OperationStatus("NOTFOUND", DbConstants.DB_NOTFOUND);

    /* package */
    static OperationStatus fromInt(final int errCode) {
        switch(errCode) {
        case 0:
            return SUCCESS;
        case DbConstants.DB_KEYEXIST:
            return KEYEXIST;
        case DbConstants.DB_KEYEMPTY:
            return KEYEMPTY;
        case DbConstants.DB_NOTFOUND:
            return NOTFOUND;
        default:
            throw new IllegalArgumentException(
                "Unknown error code: " + DbEnv.strerror(errCode));
        }
    }

    /* For toString */
    private String statusName;
    private int errCode;

    private OperationStatus(final String statusName, int errCode) {
        this.statusName = statusName;
        this.errCode = errCode;
    }

    public String toString() {
        return "OperationStatus." + statusName;
    }
}
