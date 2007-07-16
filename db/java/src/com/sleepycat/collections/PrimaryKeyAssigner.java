/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2006
 *      Oracle Corporation.  All rights reserved.
 *
 * $Id: PrimaryKeyAssigner.java,v 12.3 2006/08/31 18:14:08 bostic Exp $
 */

package com.sleepycat.collections;

import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.db.DatabaseException;

/**
 * An interface implemented to assign new primary key values.
 * An implementation of this interface is passed to the {@link StoredMap}
 * or {@link StoredSortedMap} constructor to assign primary keys for that
 * store. Key assignment occurs when <code>StoredMap.append()</code> is called.
 *
 * @author Mark Hayes
 */
public interface PrimaryKeyAssigner {

    /**
     * Assigns a new primary key value into the given data buffer.
     */
    void assignKey(DatabaseEntry keyData)
        throws DatabaseException;
}
