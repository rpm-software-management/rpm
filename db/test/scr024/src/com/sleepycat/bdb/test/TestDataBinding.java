/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestDataBinding.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.test;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import java.io.IOException;

/**
 * @author Mark Hayes
 */
class TestDataBinding implements DataBinding {

    public Object dataToObject(DataBuffer data)
        throws IOException {

        if (data.getDataLength() != 1) throw new IllegalStateException();
        byte val = data.getDataBytes()[data.getDataOffset()];
        return new Long(val);
    }

    public void objectToData(Object object, DataBuffer data)
        throws IOException {

        byte val = ((Number) object).byteValue();
        data.setData(new byte[] { val }, 0, 1);
    }

    public DataFormat getDataFormat() {

        return TestStore.BYTE_FORMAT;
    }
}
