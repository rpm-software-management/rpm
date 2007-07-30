/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: RecordNumberBinding.java,v 12.5 2007/05/04 00:28:24 mark Exp $
 */

package com.sleepycat.bind;

import com.sleepycat.compat.DbCompat;
import com.sleepycat.db.DatabaseEntry;

/**
 * An <code>EntryBinding</code> that treats a record number key entry as a
 * <code>Long</code> key object.
 *
 * <p>Record numbers are returned as <code>Long</code> objects, although on
 * input any <code>Number</code> object may be used.</p>
 *
 * @author Mark Hayes
 */
public class RecordNumberBinding implements EntryBinding {

    /**
     * Creates a byte array binding.
     */
    public RecordNumberBinding() {
    }

    // javadoc is inherited
    public Object entryToObject(DatabaseEntry entry) {

        return new Long(entryToRecordNumber(entry));
    }

    // javadoc is inherited
    public void objectToEntry(Object object, DatabaseEntry entry) {

        recordNumberToEntry(((Number) object).longValue(), entry);
    }

    /**
     * Utility method for use by bindings to translate a entry buffer to an
     * record number integer.
     *
     * @param entry the entry buffer.
     *
     * @return the record number.
     */
    public static long entryToRecordNumber(DatabaseEntry entry) {

        return DbCompat.getRecordNumber(entry) & 0xFFFFFFFFL;
    }

    /**
     * Utility method for use by bindings to translate a record number integer
     * to a entry buffer.
     *
     * @param recordNumber the record number.
     *
     * @param entry the entry buffer to hold the record number.
     */
    public static void recordNumberToEntry(long recordNumber,
                                           DatabaseEntry entry) {
        entry.setData(new byte[4], 0, 4);
        DbCompat.setRecordNumber(entry, (int) recordNumber);
    }
}
