/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: RecordNumberFormat.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.db.Dbt;

/**
 * The data format for record number keys.  This class must be used whenever a
 * record number is used with a store, index, or binding.  It is used to
 * identify Berkeley DB record numbers as such and perform special processing
 * required by Berkeley DB.  Namely, the byte order of record numbers is not
 * the same as the byte order for integers in Java, and is also platform
 * dependent.
 *
 * @author Mark Hayes
 */
public class RecordNumberFormat implements DataFormat {

    /**
     * Creates a record number format.
     */
    public RecordNumberFormat() {
    }

    /**
     * Utility method for use by bindings to translate a data buffer to an
     * record number integer.
     *
     * @param data the data buffer.
     *
     * @return the record number.
     */
    public final long dataToRecordNumber(DataBuffer data) {

        return ((Dbt) data).get_recno_key_data() & 0xFFFFFFFFL;
    }

    /**
     * Utility method for use by bindings to translate a record number integer
     * to a data buffer.
     *
     * @param recordNumber the record number.
     *
     * @param data the data buffer to hold the record number.
     */
    public final void recordNumberToData(long recordNumber, DataBuffer data) {

        data.setData(new byte[4], 0, 4);
        ((Dbt) data).set_recno_key_data((int) recordNumber);
    }

    /**
     * Test for equality.
     *
     * @param o the object to check.
     *
     * @return true if the given object is a RecordNumberFormat instance,
     * since all RecordNumberFormat instances are equivalent.
     */
    public boolean equals(Object o) {

        return (o instanceof RecordNumberFormat);
    }
}
