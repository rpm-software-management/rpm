/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: RecordNumberBinding.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import java.io.IOException;

/**
 * A concrete binding for record number keys.  Record numbers are returned
 * as Long objects, although on input any Number object may be used.
 *
 * @author Mark Hayes
 */
public class RecordNumberBinding implements DataBinding {

    private RecordNumberFormat format;

    /**
     * Creates a byte array binding.
     *
     * @param format is the format of the new binding.
     */
    public RecordNumberBinding(RecordNumberFormat format) {

        this.format = format;
    }

    // javadoc is inherited
    public DataFormat getDataFormat() {

        return format;
    }

    // javadoc is inherited
    public Object dataToObject(DataBuffer data)
        throws IOException {

        return new Long(format.dataToRecordNumber(data));
    }

    // javadoc is inherited
    public void objectToData(Object object, DataBuffer data)
        throws IOException {

        format.recordNumberToData(((Number) object).longValue(), data);
    }
}
