/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestEntityBinding.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.test;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.EntityBinding;
import java.io.IOException;

/**
 * @author Mark Hayes
 */
class TestEntityBinding implements EntityBinding {

    private boolean isRecNum;

    TestEntityBinding(boolean isRecNum) {

        this.isRecNum = isRecNum;
    }

    public Object dataToObject(DataBuffer key, DataBuffer value)
        throws IOException {

        byte keyByte;
        if (isRecNum) {
            if (key.getDataLength() != 4) throw new IllegalStateException();
            keyByte = (byte) TestStore.RECNO_FORMAT.dataToRecordNumber(key);
        } else {
            if (key.getDataLength() != 1) throw new IllegalStateException();
            keyByte = key.getDataBytes()[key.getDataOffset()];
        }
        if (value.getDataLength() != 1) throw new IllegalStateException();
        byte valByte = value.getDataBytes()[value.getDataOffset()];
        return new TestEntity(keyByte, valByte);
    }

    public void objectToKey(Object object, DataBuffer key)
        throws IOException {

        byte val = (byte) ((TestEntity) object).key;
        if (isRecNum) {
            TestStore.RECNO_FORMAT.recordNumberToData(val, key);
        } else {
            key.setData(new byte[] { val }, 0, 1);
        }
    }

    public void objectToValue(Object object, DataBuffer value)
        throws IOException {

        byte val = (byte) ((TestEntity) object).value;
        value.setData(new byte[] { val }, 0, 1);
    }

    public DataFormat getKeyFormat() {

        if (isRecNum) {
            return TestStore.RECNO_FORMAT;
        } else {
            return TestStore.BYTE_FORMAT;
        }
    }

    public DataFormat getValueFormat() {

        return TestStore.BYTE_FORMAT;
    }
}
