/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: MultipleNIODataEntry.java,v 1.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbUtil;

import java.nio.ByteBuffer;

public class MultipleNIODataEntry extends MultipleEntry {
    public MultipleNIODataEntry() {
        super(null);
    }

    public MultipleNIODataEntry(final ByteBuffer data) {
        super(data);
    }

    /**
     * Return the bulk retrieval flag and reset the entry position so that the
     * next set of key/data can be returned.
     */
    /* package */
    int getMultiFlag() {
        pos = 0;
        return DbConstants.DB_MULTIPLE;
    }

    public boolean next(final DatabaseEntry data) {
        byte[] intarr;
        int saveoffset;
        if (pos == 0)
            pos = ulen - INT32SZ;

        // pull the offsets out of the ByteBuffer.
        if(this.data_nio.capacity() < 8)
            return false;
        intarr = new byte[8];
        saveoffset = this.data_nio.position();
        this.data_nio.position(pos - INT32SZ);
        this.data_nio.get(intarr, 0, 8);
        this.data_nio.position(saveoffset);

        final int dataoff = DbUtil.array2int(intarr, 4);

        // crack out the data offset and length.
        if (dataoff < 0) {
            return (false);
        }

        final int datasz = DbUtil.array2int(intarr, 0);

        // move the position to one before the last offset read.
        pos -= INT32SZ*2;

        data.setDataNIO(this.data_nio);
        data.setSize(datasz);
        data.setOffset(dataoff);

        return (true);
    }
}
