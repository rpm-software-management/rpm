/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: MultipleRecnoDataEntry.java,v 1.1 2004/04/06 20:43:40 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbUtil;

public class MultipleRecnoDataEntry extends MultipleEntry {
    public MultipleRecnoDataEntry() {
        super(null, 0, 0);
    }

    public MultipleRecnoDataEntry(final byte[] data) {
        super(data, 0, (data == null) ? 0 : data.length);
    }

    public MultipleRecnoDataEntry(final byte[] data,
                                  final int offset,
                                  final int size) {
        super(data, offset, size);
    }

    /* package */
    int getMultiFlag() {
        return DbConstants.DB_MULTIPLE_KEY;
    }

    public boolean next(final DatabaseEntry recno, final DatabaseEntry data) {
        if (pos == 0)
            pos = ulen - INT32SZ;

        final int keyoff = DbUtil.array2int(this.data, pos);

        // crack out the key offset and the data offset and length.
        if (keyoff < 0)
            return false;

        pos -= INT32SZ;
        final int dataoff = DbUtil.array2int(this.data, pos);
        pos -= INT32SZ;
        final int datasz = DbUtil.array2int(this.data, pos);
        pos -= INT32SZ;

        recno.setData(this.data);
        recno.setOffset(keyoff);
        recno.setSize(INT32SZ);

        data.setData(this.data);
        data.setOffset(dataoff);
        data.setSize(datasz);

        return true;
    }
}
