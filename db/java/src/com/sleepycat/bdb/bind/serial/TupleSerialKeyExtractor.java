/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleSerialKeyExtractor.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.KeyExtractor;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * A abstract key extractor that uses a tuple key and a serial value. This
 * class takes care of serializing and deserializing the value data, and
 * converting the key data to/from {@link TupleInput} and {@link TupleOutput}
 * objects.  Its two abstract methods must be implemented by a concrete
 * subclass to extract and clear the index key using these objects.
 * <ul>
 * <li> {@link #extractIndexKey(TupleInput,Object,TupleOutput)} </li>
 * <li> {@link #clearIndexKey(Object)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class TupleSerialKeyExtractor implements KeyExtractor {

    protected TupleFormat primaryKeyFormat;
    protected SerialFormat valueFormat;
    protected TupleFormat indexKeyFormat;

    /**
     * Creates a tuple-serial key extractor.
     *
     * @param primaryKeyFormat is the primary key format, or null if no
     * primary key data is used to construct the index key.
     *
     * @param valueFormat is the value format, or null if no value data is
     * used to construct the index key.
     *
     * @param indexKeyFormat is the index key format.
     */
    public TupleSerialKeyExtractor(TupleFormat primaryKeyFormat,
                                   SerialFormat valueFormat,
                                   TupleFormat indexKeyFormat) {

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

        TupleOutput output = indexKeyFormat.newOutput();
        TupleInput primaryKeyInput = ((primaryKeyFormat != null)
                            ? primaryKeyFormat.dataToInput(primaryKeyData)
                            : null);
        Object valueInput = ((valueFormat != null)
                            ? valueFormat.dataToObject(valueData)
                            : null);
        extractIndexKey(primaryKeyInput, valueInput, output);
        indexKeyFormat.outputToData(output, indexKeyData);
    }

    // javadoc is inherited
    public void clearIndexKey(DataBuffer valueData)
        throws IOException {

        Object value = valueFormat.dataToObject(valueData);
        clearIndexKey(value);
        valueFormat.objectToData(value, valueData);
    }

    /**
     * Extracts the index key data from primary key tuple data and deserialized
     * value data.
     *
     * @param primaryKeyInput is the {@link TupleInput} for the primary key
     * data, or null if no primary key data is used to construct the index key.
     *
     * @param valueInput is the deserialized value data, or null if no value
     * data is used to construct the index key.
     *
     * @param indexKeyOutput is the destination index key tuple.  For index
     * keys which are optionally present, no tuple data should be output to
     * indicate that the key is not present or null.
     */
    public abstract void extractIndexKey(TupleInput primaryKeyInput,
                                         Object valueInput,
                                         TupleOutput indexKeyOutput)
        throws IOException;

    /**
     * Clears the index key in the deserialized value data.
     *
     * @param valueInputOutput is the source and destination deserialized value
     * data.  On entry this contains the index key to be cleared.  It should be
     * changed by this method such that {@link #extractIndexKey} will extract a
     * null key (not output any tuple data).  Other data in the value object
     * should remain unchanged.
     */
    public abstract void clearIndexKey(Object valueInputOutput)
        throws IOException;
}
