/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: CursorConfig.java,v 1.4 2004/09/28 19:30:36 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.Db;
import com.sleepycat.db.internal.Dbc;
import com.sleepycat.db.internal.DbTxn;

public class CursorConfig implements Cloneable {
    public static final CursorConfig DEFAULT = new CursorConfig();

    public static final CursorConfig DIRTY_READ = new CursorConfig();
    static { DIRTY_READ.setDirtyRead(true); }

    public static final CursorConfig DEGREE_2 = new CursorConfig();
    static { DEGREE_2.setDegree2(true); }

    public static final CursorConfig WRITECURSOR = new CursorConfig();
    static { WRITECURSOR.setWriteCursor(true); }


    private boolean dirtyRead = false;
    private boolean degree2 = false;
    private boolean writeCursor = false;

    public CursorConfig() {
    }

    /* package */
    static CursorConfig checkNull(CursorConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    public void setDegree2(final boolean degree2) {
        this.degree2 = degree2;
    }

    public boolean getDegree2() {
        return degree2;
    }

    public void setDirtyRead(final boolean dirtyRead) {
        this.dirtyRead = dirtyRead;
    }

    public boolean getDirtyRead() {
        return dirtyRead;
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
        flags |= dirtyRead ? DbConstants.DB_DIRTY_READ : 0;
        flags |= degree2 ? DbConstants.DB_DEGREE_2 : 0;
        flags |= writeCursor ? DbConstants.DB_WRITECURSOR : 0;
        return db.cursor(txn, flags);
    }
}
