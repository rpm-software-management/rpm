/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: JoinCursorAdapter.java,v 1.1 2004/04/06 20:43:41 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;

class JoinCursorAdapter extends Cursor {
    JoinCursor jc;

    JoinCursorAdapter(Database database, JoinCursor jc)
        throws DatabaseException {
        this.database = database;
        this.config = new CursorConfig();
        this.jc = jc;
    }

    public synchronized void close()
        throws DatabaseException {
        jc.close();
    }

    public Cursor dup(boolean samePosition)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public int count()
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus delete()
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getCurrent(DatabaseEntry key,
                                      DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getFirst(DatabaseEntry key,
                                    DatabaseEntry data,
                                    LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getLast(DatabaseEntry key,
                                   DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getNext(DatabaseEntry key,
                                   DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getNextDup(DatabaseEntry key,
                                      DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getNextNoDup(DatabaseEntry key,
                                        DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getPrev(DatabaseEntry key,
                                   DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getPrevDup(DatabaseEntry key,
                                      DatabaseEntry data,
                                      LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getPrevNoDup(DatabaseEntry key,
                                        DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getRecordNumber(DatabaseEntry recno,
                                           LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getSearchKey(DatabaseEntry key,
                                        DatabaseEntry data,
                                        LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getSearchKeyRange(DatabaseEntry key,
                                             DatabaseEntry data,
                                             LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getSearchBoth(DatabaseEntry key,
                                         DatabaseEntry data,
                                         LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus getSearchBothRange(DatabaseEntry key,
                                              DatabaseEntry data,
                                              LockMode lockMode)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus put(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus putNoOverwrite(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus putKeyFirst(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus putKeyLast(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus putNoDupData(DatabaseEntry key, DatabaseEntry data)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }

    public OperationStatus putCurrent(DatabaseEntry data)
        throws DatabaseException {
        throw new UnsupportedOperationException("not supported on join cursors");
    }
}
