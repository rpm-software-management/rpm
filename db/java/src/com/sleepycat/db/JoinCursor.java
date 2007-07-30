/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: JoinCursor.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.Dbc;

public class JoinCursor {
    private Database database;
    private Dbc dbc;
    private JoinConfig config;

    JoinCursor(final Database database,
               final Dbc dbc,
               final JoinConfig config) {
        this.database = database;
        this.dbc = dbc;
        this.config = config;
    }

    public void close()
        throws DatabaseException {

        dbc.close();
    }

    public Database getDatabase() {
        return database;
    }

    public JoinConfig getConfig() {
        return config;
    }

    public OperationStatus getNext(final DatabaseEntry key, LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, DatabaseEntry.IGNORE,
                DbConstants.DB_JOIN_ITEM |
                LockMode.getFlag(lockMode)));
    }

    public OperationStatus getNext(final DatabaseEntry key,
                                   final DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }
}
