/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestKeyExtractor.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.test;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.KeyExtractor;
import java.io.IOException;

/**
 * @author Mark Hayes
 */
class TestKeyExtractor implements KeyExtractor {

    private boolean isRecNum;

    TestKeyExtractor(boolean isRecNum) {

        this.isRecNum = isRecNum;
    }

    public void extractIndexKey(DataBuffer primaryKeyData,
                                DataBuffer valueData, DataBuffer indexKeyData)
        throws IOException {

        if (valueData.getDataLength() == 0) return;
        if (valueData.getDataLength() != 1) throw new IllegalStateException();
        byte val = valueData.getDataBytes()[valueData.getDataOffset()];
        if (val == 0) return; // fixed-len pad value
        val -= 100;
        if (isRecNum) {
            TestStore.RECNO_FORMAT.recordNumberToData(val, indexKeyData);
        } else {
            indexKeyData.setData(new byte[] { val }, 0, 1);
        }
    }

    public void clearIndexKey(DataBuffer valueData)
        throws IOException {

        throw new IOException("not supported");
    }

    public DataFormat getPrimaryKeyFormat() {

        return null;
    }

    public DataFormat getValueFormat() {

        return TestStore.BYTE_FORMAT;
    }

    public DataFormat getIndexKeyFormat() {

        if (isRecNum) {
            return TestStore.RECNO_FORMAT;
        } else {
            return TestStore.BYTE_FORMAT;
        }
    }
}
