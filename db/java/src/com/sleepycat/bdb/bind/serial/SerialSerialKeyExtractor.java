/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: SerialSerialKeyExtractor.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.KeyExtractor;
import java.io.IOException;

/**
 * A abstract key extractor that uses a serial key and a serial value.  This
 * class takes care of serializing and deserializing the key and value data
 * automatically.  Its two abstract methods must be implemented by a concrete
 * subclass to extract/clear the index key from the deserialized data objects.
 * <ul>
 * <li> {@link #extractIndexKey(Object,Object)} </li>
 * <li> {@link #clearIndexKey(Object)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class SerialSerialKeyExtractor implements KeyExtractor {

    protected SerialFormat primaryKeyFormat;
    protected SerialFormat valueFormat;
    protected SerialFormat indexKeyFormat;

    /**
     * Creates a serial-serial entity binding.
     *
     * @param primaryKeyFormat is the primary key format, or null if no
     * primary key data is used to construct the index key.
     *
     * @param valueFormat is the value format, or null if no value data is
     * used to construct the index key.
     *
     * @param indexKeyFormat is the index key format.
     */
    public SerialSerialKeyExtractor(SerialFormat primaryKeyFormat,
                                    SerialFormat valueFormat,
                                    SerialFormat indexKeyFormat) {

        this.primaryKeyFormat = primaryKeyFormat;
        this.valueFormat = valueFormat;
        this.indexKeyFormat = indexKeyFormat;
    }

    // javadoc is inherited
    public DataFormat getPrimaryKeyFormat() {

        return primaryKeyFormat;
    }

    // javadoc is inherited
    public DataFormat getValueFormat() {

        return valueFormat;
    }

    // javadoc is inherited
    public DataFormat getIndexKeyFormat() {

        return indexKeyFormat;
    }

    // javadoc is inherited
    public void extractIndexKey(DataBuffer primaryKeyData,
                                DataBuffer valueData,
                                DataBuffer indexKeyData)
        throws IOException {

        Object primaryKeyInput = ((primaryKeyFormat != null)
                            ? primaryKeyFormat.dataToObject(primaryKeyData)
                            : null);
        Object valueInput = ((valueFormat != null)
                            ? valueFormat.dataToObject(valueData)
                            : null);
        Object indexKey = extractIndexKey(primaryKeyInput, valueInput);
        if (indexKey != null)
            indexKeyFormat.objectToData(indexKey, indexKeyData);
        else
            indexKeyData.setData(null, 0, 0);
    }

    // javadoc is inherited
    public void clearIndexKey(DataBuffer valueData)
        throws IOException {

        Object value = valueFormat.dataToObject(valueData);
        value = clearIndexKey(value);
        if (value != null)
            valueFormat.objectToData(value, valueData);
    }

    /**
     * Extracts the index key data object from primary key and value data
     * objects.
     *
     * @param primaryKeyData is the deserialized source primary key data, or
     * null if no primary key data is used to construct the index key.
     *
     * @param valueData is the deserialized source value data, or null if no
     * value data is used to construct the index key.
     *
     * @return the destination index key data object, or null to indicate that
     * the key is not present.
     */
    public abstract Object extractIndexKey(Object primaryKeyData,
                                           Object valueData)
        throws IOException;

    /**
     * Clears the index key in a value data object.
     *
     * @param valueData is the source and destination value data object.
     *
     * @return the destination value data object, or null to indicate that the
     * key is not present and no change is necessary.  The value returned may
     * be the same object passed as the valueData parameter or a newly created
     * object.
     */
    public abstract Object clearIndexKey(Object valueData)
        throws IOException;
}
