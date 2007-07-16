/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2006
 *      Oracle Corporation.  All rights reserved.
 *
 * $Id: EntityBinding.java,v 12.3 2006/08/31 18:14:05 bostic Exp $
 */

package com.sleepycat.bind;

import com.sleepycat.db.DatabaseEntry;

/**
 * A binding between a key-value entry pair and an entity object.
 *
 * @author Mark Hayes
 */
public interface EntityBinding {

    /**
     * Converts key and data entry buffers into an entity Object.
     *
     * @param key is the source key entry.
     *
     * @param data is the source data entry.
     *
     * @return the resulting Object.
     */
    Object entryToObject(DatabaseEntry key, DatabaseEntry data);

    /**
     * Extracts the key entry from an entity Object.
     *
     * @param object is the source Object.
     *
     * @param key is the destination entry buffer.
     */
    void objectToKey(Object object, DatabaseEntry key);

    /**
     * Extracts the data entry from an entity Object.
     *
     * @param object is the source Object.
     *
     * @param data is the destination entry buffer.
     */
    void objectToData(Object object, DatabaseEntry data);
}
