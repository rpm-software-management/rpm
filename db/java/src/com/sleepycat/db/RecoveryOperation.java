/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: RecoveryOperation.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;

public final class RecoveryOperation {
    public static final RecoveryOperation BACKWARD_ROLL =
        new RecoveryOperation("BACKWARD_ROLL", DbConstants.DB_TXN_BACKWARD_ROLL);
    public static final RecoveryOperation FORWARD_ROLL =
        new RecoveryOperation("FORWARD_ROLL", DbConstants.DB_TXN_FORWARD_ROLL);
    public static final RecoveryOperation ABORT =
        new RecoveryOperation("ABORT", DbConstants.DB_TXN_ABORT);
    public static final RecoveryOperation APPLY =
        new RecoveryOperation("APPLY", DbConstants.DB_TXN_APPLY);
    public static final RecoveryOperation PRINT =
        new RecoveryOperation("PRINT", DbConstants.DB_TXN_PRINT);

    private String operationName;
    private int flag;

    private RecoveryOperation(String operationName, int flag) {
        this.operationName = operationName;
        this.flag = flag;
    }

    public String toString() {
        return "RecoveryOperation." + operationName;
    }

    /* This is public only so it can be called from internal/DbEnv.java. */
    public static RecoveryOperation fromFlag(int flag) {
        switch (flag) {
        case DbConstants.DB_TXN_BACKWARD_ROLL:
            return BACKWARD_ROLL;
        case DbConstants.DB_TXN_FORWARD_ROLL:
            return FORWARD_ROLL;
        case DbConstants.DB_TXN_ABORT:
            return ABORT;
        case DbConstants.DB_TXN_APPLY:
            return APPLY;
        case DbConstants.DB_TXN_PRINT:
            return PRINT;
        default:
            throw new IllegalArgumentException(
                "Unknown recover operation: " + flag);
        }
    }
}
