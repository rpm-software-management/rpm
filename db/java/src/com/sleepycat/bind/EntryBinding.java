/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: EntryBinding.java,v 12.5 2007/05/04 00:28:24 mark Exp $
 */

package com.sleepycat.bind;

import com.sleepycat.db.DatabaseEntry;

/**
 * A binding between a key or data entry and a key or data object.
 *
 * @author Mark Hayes
 */
public interface EntryBinding {

    /**
     * Converts a entry buffer into an Object.
     *
     * @param entry is the source entry buffer.
     *
     * @return the resulting Object.
     */
    Object entryToObject(DatabaseEntry entry);

    /**
     * Converts an Object into a entry buffer.
     *
     * @param object is the source Object.
     *
     * @param entry is the destination entry buffer.
     */
    void objectToEntry(Object object, DatabaseEntry entry);
}
