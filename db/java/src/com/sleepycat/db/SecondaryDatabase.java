/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: SecondaryDatabase.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.Db;
import com.sleepycat.db.internal.DbConstants;

public class SecondaryDatabase extends Database {
    private final Database primaryDatabase;

    /* package */
    SecondaryDatabase(final Db db, final Database primaryDatabase)
        throws DatabaseException {

        super(db);
        this.primaryDatabase = primaryDatabase;
    }

    public SecondaryDatabase(final String fileName,
                             final String databaseName,
                             final Database primaryDatabase,
                             final SecondaryConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        this(SecondaryConfig.checkNull(config).openSecondaryDatabase(
                null, null, fileName, databaseName, primaryDatabase.db),
            primaryDatabase);
    }

    public Cursor openCursor(final Transaction txn, final CursorConfig config)
        throws DatabaseException {

        return openSecondaryCursor(txn, config);
    }

    public SecondaryCursor openSecondaryCursor(final Transaction txn,
                                               final CursorConfig config)
        throws DatabaseException {

        return new SecondaryCursor(this,
            CursorConfig.checkNull(config).openCursor(db,
                (txn == null) ? null : txn.txn), config);
    }

    public Database getPrimaryDatabase() {
        return primaryDatabase;
    }

    public DatabaseConfig getConfig()
        throws DatabaseException {

        return getSecondaryConfig();
    }

    public SecondaryConfig getSecondaryConfig()
        throws DatabaseException {

        return new SecondaryConfig(db);
    }

    public OperationStatus get(final Transaction txn,
                               final DatabaseEntry key,
                               final DatabaseEntry pKey,
                               final DatabaseEntry data,
                               final LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.pget((txn == null) ? null : txn.txn, key, pKey, data,
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchBoth(final Transaction txn,
                                         final DatabaseEntry key,
                                         final DatabaseEntry pKey,
                                         final DatabaseEntry data,
                                         final LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.pget((txn == null) ? null : txn.txn, key, pKey, data,
                DbConstants.DB_GET_BOTH | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchRecordNumber(final Transaction txn,
                                                 final DatabaseEntry key,
                                                 final DatabaseEntry pKey,
                                                 final DatabaseEntry data,
                                                 final LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.pget((txn == null) ? null : txn.txn, key, pKey, data,
                DbConstants.DB_SET_RECNO | LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }
}
