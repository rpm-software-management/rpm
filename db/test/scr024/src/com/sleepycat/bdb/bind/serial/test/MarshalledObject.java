/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: MarshalledObject.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial.test;

import com.sleepycat.bdb.bind.tuple.MarshalledTupleData;
import com.sleepycat.bdb.bind.tuple.MarshalledTupleKeyEntity;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;
import java.io.Serializable;

/**
 * @author Mark Hayes
 */
public class MarshalledObject
    implements Serializable, MarshalledTupleKeyEntity {

    private String data;
    private transient String primaryKey;
    private String indexKey1;
    private String indexKey2;

    public MarshalledObject(String data, String primaryKey,
                            String indexKey1, String indexKey2) {
        this.data = data;
        this.primaryKey = primaryKey;
        this.indexKey1 = indexKey1;
        this.indexKey2 = indexKey2;
    }

    public boolean equals(Object o) {

        try {
            MarshalledObject other = (MarshalledObject) o;

            return this.data.equals(other.data) &&
                   this.primaryKey.equals(other.primaryKey) &&
                   this.indexKey1.equals(other.indexKey1) &&
                   this.indexKey2.equals(other.indexKey2);
        } catch (Exception e) {
            return false;
        }
    }

    public String getData() {

        return data;
    }

    public String getPrimaryKey() {

        return primaryKey;
    }

    public String getIndexKey1() {

        return indexKey1;
    }

    public String getIndexKey2() {

        return indexKey2;
    }

    public int expectedKeyLength() {

        return primaryKey.length() + 1;
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
            if (indexKey2.length() > 0)
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

