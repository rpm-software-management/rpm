/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: MultipleDataEntry.java,v 1.2 2004/04/09 15:08:38 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbUtil;

public class MultipleDataEntry extends MultipleEntry {
    public MultipleDataEntry() {
        super(null, 0, 0);
    }

    public MultipleDataEntry(final byte[] data) {
        super(data, 0, (data == null) ? 0 : data.length);
    }

    public MultipleDataEntry(final byte[] data,
                             final int offset,
                             final int size) {
        super(data, offset, size);
    }

    /* package */
    int getMultiFlag() {
        return DbConstants.DB_MULTIPLE;
    }

    public boolean next(final DatabaseEntry data) {
        if (pos == 0)
            pos = ulen - INT32SZ;

        final int dataoff = DbUtil.array2int(this.data, pos);

        // crack out the data offset and length.
        if (dataoff < 0) {
            return (false);
        }

        pos -= INT32SZ;
        final int datasz = DbUtil.array2int(this.data, pos);

        pos -= INT32SZ;

        data.setData(this.data);
        data.setSize(datasz);
        data.setOffset(dataoff);

        return (true);
    }
}
