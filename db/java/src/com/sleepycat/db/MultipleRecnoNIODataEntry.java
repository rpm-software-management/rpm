/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: MultipleRecnoNIODataEntry.java,v 1.5 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbUtil;

import java.nio.ByteBuffer;

public class MultipleRecnoNIODataEntry extends MultipleEntry {
    public MultipleRecnoNIODataEntry() {
        super(null);
    }

    public MultipleRecnoNIODataEntry(final ByteBuffer data) {
        super(data);
    }

    /**
     * Return the bulk retrieval flag and reset the entry position so that the
     * next set of key/data can be returned.
     */
    /* package */
    int getMultiFlag() {
        pos = 0;
        return DbConstants.DB_MULTIPLE_KEY;
    }

    public boolean next(final DatabaseEntry recno, final DatabaseEntry data) {
        byte[] intarr;
        int saveoffset;
        if (pos == 0)
            pos = ulen - INT32SZ;

        // pull the offsets out of the ByteBuffer.
        if(this.data_nio.capacity() < 12)
            return false;
        intarr = new byte[12];
        saveoffset = this.data_nio.position();
        this.data_nio.position(pos - INT32SZ*2);
        this.data_nio.get(intarr, 0, 12);
        this.data_nio.position(saveoffset);

        final int keyoff = DbUtil.array2int(intarr, 8);

        // crack out the key offset and the data offset and length.
        if (keyoff < 0)
            return false;

        final int dataoff = DbUtil.array2int(intarr, 4);
        final int datasz = DbUtil.array2int(intarr, 0);

        // move the position to one before the last offset read.
        pos -= INT32SZ*3;

        recno.setDataNIO(this.data_nio);
        recno.setOffset(keyoff);
        recno.setSize(INT32SZ);

        data.setDataNIO(this.data_nio);
        data.setOffset(dataoff);
        data.setSize(datasz);

        return true;
    }
}
