/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: MarshalledObject.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple.test;

import com.sleepycat.bdb.bind.tuple.MarshalledTupleData;
import com.sleepycat.bdb.bind.tuple.MarshalledTupleKeyEntity;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * @author Mark Hayes
 */
public class MarshalledObject
    implements MarshalledTupleData, MarshalledTupleKeyEntity {

    private String data;
    private String primaryKey;
    private String indexKey1;
    private String indexKey2;

    public MarshalledObject() {
    }

    MarshalledObject(String data, String primaryKey,
                     String indexKey1, String indexKey2) {

        this.data = data;
        this.primaryKey = primaryKey;
        this.indexKey1 = indexKey1;
        this.indexKey2 = indexKey2;
    }

    String getData() {

        return data;
    }

    String getPrimaryKey() {

        return primaryKey;
    }

    String getIndexKey1() {

        return indexKey1;
    }

    String getIndexKey2() {

        return indexKey2;
    }

    int expectedDataLength() {

        return data.length() + 1 +
               indexKey1.length() + 1 +
               indexKey2.length() + 1;
    }

    int expectedKeyLength() {

        return primaryKey.length() + 1;
    }

    public void marshalData(TupleOutput dataOutput)
        throws IOException {

        dataOutput.writeString(data);
        dataOutput.writeString(indexKey1);
        dataOutput.writeString(indexKey2);
    }

    public void unmarshalData(TupleInput dataInput)
        throws IOException {

        data = dataInput.readString();
        indexKey1 = dataInput.readString();
        indexKey2 = dataInput.readString();
    }

    public void marshalPrimaryKey(TupleOutput keyOutput)
        throws IOException {

        keyOutput.writeString(primaryKey);
    }

    public void unmarshalPrimaryKey(TupleInput keyInput)
        throws IOException {

        primaryKey = keyInput.readString();
    }

    public void marshalIndexKey(String keyName, TupleOutput keyOutput)
        throws IOException {

        if ("1".equals(keyName)) {
            if (indexKey1.length() > 0)
                keyOutput.writeString(indexKey1);
        } else if ("2".equals(keyName)) {
            if (indexKey1.length() > 0)
                keyOutput.writeString(indexKey2);
        } else {
            throw new IllegalArgumentException("Unknown keyName: " + keyName);
        }
    }

    public void clearIndexKey(String keyName)
        throws IOException {

        if ("1".equals(keyName)) {
            indexKey1 = "";
        } else if ("2".equals(keyName)) {
            indexKey2 = "";
        } else {
            throw new IllegalArgumentException("Unknown keyName: " + keyName);
        }
    }
}

