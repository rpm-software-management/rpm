/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleTupleKeyExtractor.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.KeyExtractor;
import java.io.IOException;

/**
 * An abstract key extractor that uses a tuple key and a tuple value. This
 * class takes care of converting the key and value data to/from {@link
 * TupleInput} and {@link TupleOutput} objects.  Its two abstract methods must
 * be implemented by a concrete subclass to extract and clear the index key
 * using these objects.
 * <ul>
 * <li> {@link #extractIndexKey(TupleInput,TupleInput,TupleOutput)} </li>
 * <li> {@link #clearIndexKey(TupleInput,TupleOutput)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class TupleTupleKeyExtractor implements KeyExtractor {

    protected TupleFormat primaryKeyFormat;
    protected TupleFormat valueFormat;
    protected TupleFormat indexKeyFormat;

    /**
     * Creates a tuple-tuple key extractor.
     *
     * @param primaryKeyFormat is the primary key format, or null if no
     * primary key data is used to construct the index key.
     *
     * @param valueFormat is the value format, or null if no value data is
     * used to construct the index key.
     *
     * @param indexKeyFormat is the index key format.
     */
    public TupleTupleKeyExtractor(TupleFormat primaryKeyFormat,
                                  TupleFormat valueFormat,
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
        TupleInput valueInput = ((valueFormat != null)
                                ? valueFormat.dataToInput(valueData)
                                : null);
        extractIndexKey(primaryKeyInput, valueInput, output);
        indexKeyFormat.outputToData(output, indexKeyData);
    }

    // javadoc is inherited
    public void clearIndexKey(DataBuffer valueData)
        throws IOException {

        TupleOutput output = valueFormat.newOutput();
        clearIndexKey(valueFormat.dataToInput(valueData), output);
        valueFormat.outputToData(output, valueData);
    }

    /**
     * Extracts the index key data from primary
     * key tuple and value tuple data.
     *
     * @param primaryKeyInput is the {@link TupleInput} for the primary key
     * data, or null if no primary key data is used to construct the index key.
     *
     * @param valueInput is the {@link TupleInput} for the value data, or null
     * if no value data is used to construct the index key.
     *
     * @param indexKeyOutput is the destination index key tuple.  For index
     * keys which are optionally present, no tuple data should be output to
     * indicate that the key is not present or null.
     */
    public abstract void extractIndexKey(TupleInput primaryKeyInput,
                                         TupleInput valueInput,
                                         TupleOutput indexKeyOutput)
        throws IOException;

    /**
     * Clears the index key in the tuple value data.  The valueInput should be
     * read and then written to the valueOutput, clearing the index key in the
     *
     * @param valueInput is the {@link TupleInput} for the value data.
     *
     * @param valueOutput is the destination {@link TupleOutput}.
     */
    public abstract void clearIndexKey(TupleInput valueInput,
                                       TupleOutput valueOutput)
        throws IOException;
}
