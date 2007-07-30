/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: CursorConfig.java,v 12.7 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.Db;
import com.sleepycat.db.internal.Dbc;
import com.sleepycat.db.internal.DbTxn;

public class CursorConfig implements Cloneable {
    public static final CursorConfig DEFAULT = new CursorConfig();

    public static final CursorConfig READ_UNCOMMITTED = new CursorConfig();
    static { READ_UNCOMMITTED.setReadUncommitted(true); }

    public static final CursorConfig READ_COMMITTED = new CursorConfig();
    static { READ_COMMITTED.setReadCommitted(true); }

    public static final CursorConfig WRITECURSOR = new CursorConfig();
    static { WRITECURSOR.setWriteCursor(true); }

    /** @deprecated */
    public static final CursorConfig DIRTY_READ = READ_UNCOMMITTED;
    /** @deprecated */
    public static final CursorConfig DEGREE_2 = READ_COMMITTED;

    private boolean readUncommitted = false;
    private boolean readCommitted = false;
    private boolean writeCursor = false;

    public CursorConfig() {
    }

    /* package */
    static CursorConfig checkNull(CursorConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    public void setReadCommitted(final boolean readCommitted) {
        this.readCommitted = readCommitted;
    }

    public boolean getReadCommitted() {
        return readCommitted;
    }

    /** @deprecated */
    public void setDegree2(final boolean degree2) {
        setReadCommitted(degree2);
    }

    /** @deprecated */
    public boolean getDegree2() {
        return getReadCommitted();
    }

    public void setReadUncommitted(final boolean readUncommitted) {
        this.readUncommitted = readUncommitted;
    }

    public boolean getReadUncommitted() {
        return readUncommitted;
    }

    /** @deprecated */
    public void setDirtyRead(final boolean dirtyRead) {
        setReadUncommitted(dirtyRead);
    }

    /** @deprecated */
    public boolean getDirtyRead() {
        return getReadUncommitted();
    }

    public void setWriteCursor(final boolean writeCursor) {
        this.writeCursor = writeCursor;
    }

    public boolean getWriteCursor() {
        return writeCursor;
    }

    /* package */
    Dbc openCursor(final Db db, final DbTxn txn)
        throws DatabaseException {

        int flags = 0;
        flags |= readUncommitted ? DbConstants.DB_READ_UNCOMMITTED : 0;
        flags |= readCommitted ? DbConstants.DB_READ_COMMITTED : 0;
        flags |= writeCursor ? DbConstants.DB_WRITECURSOR : 0;
        return db.cursor(txn, flags);
    }
}
