/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: BaseIterator.java,v 12.4 2007/05/04 00:28:25 mark Exp $
 */

package com.sleepycat.collections;

import java.util.ListIterator;

/**
 * Common interface for BlockIterator and StoredIterator.
 */
interface BaseIterator extends ListIterator {

    /**
     * Duplicate a cursor.  Called by StoredCollections.iterator.
     */
    ListIterator dup();

    /**
     * Returns whether the given data is the current iterator data.  Called by
     * StoredMapEntry.setValue.
     */
    boolean isCurrentData(Object currentData);

    /**
     * Initializes a list iterator at the given index.  Called by
     * StoredList.iterator(int).
     */
    boolean moveToIndex(int index);
}
