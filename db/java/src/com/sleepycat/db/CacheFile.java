/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: CacheFile.java,v 12.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbMpoolFile;

public class CacheFile {
    private DbMpoolFile mpf;

    /* package */
    CacheFile(final DbMpoolFile mpf) {
        this.mpf = mpf;
    }

    public CacheFilePriority getPriority()
        throws DatabaseException {

        return CacheFilePriority.fromFlag(mpf.get_priority());
    }

    public void setPriority(final CacheFilePriority priority)
        throws DatabaseException {

        mpf.set_priority(priority.getFlag());
    }

    public long getMaximumSize()
        throws DatabaseException {

        return mpf.get_maxsize();
    }

    public void setMaximumSize(final long bytes)
        throws DatabaseException {

        mpf.set_maxsize(bytes);
    }

    public boolean getNoFile()
        throws DatabaseException {

        return (mpf.get_flags() & DbConstants.DB_MPOOL_NOFILE) != 0;
    }

    public void setNoFile(final boolean onoff)
        throws DatabaseException {

        mpf.set_flags(DbConstants.DB_MPOOL_NOFILE, onoff);
    }

    public boolean getUnlink()
        throws DatabaseException {

        return (mpf.get_flags() & DbConstants.DB_MPOOL_UNLINK) != 0;
    }

    public void setUnlink(boolean onoff)
        throws DatabaseException {

        mpf.set_flags(DbConstants.DB_MPOOL_UNLINK, onoff);
    }
}
