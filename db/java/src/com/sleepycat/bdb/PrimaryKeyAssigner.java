/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: PrimaryKeyAssigner.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.db.DbException;
import com.sleepycat.bdb.collection.StoredList;
import com.sleepycat.bdb.collection.StoredMap;
import java.io.IOException;

/**
 * An interface implemented to assign new primary key values.
 * An implementation of this interface is passed to the {@link DataStore}
 * constructor to assign primary keys for that store. Key assignment occurs
 * when {@link StoredMap#append} or {@link StoredList#append} is called.
 *
 * @author Mark Hayes
 */
public interface PrimaryKeyAssigner {

    /**
     * Assigns a new primary key value into the given data buffer.
     */
    void assignKey(DataBuffer keyData)
        throws DbException, IOException;
}
