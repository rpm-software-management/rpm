/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleTupleMarshalledKeyExtractor.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.util.IOExceptionWrapper;
import java.io.IOException;

/**
 * A concrete key extractor that works in conjunction with a {@link
 * TupleTupleMarshalledBinding}.  This key extractor works by calling the
 * methods of the {@link MarshalledTupleKeyEntity} interface to extract and
 * clear the index key data.
 *
 * <p>Note that a marshalled tuple key extractor is somewhat less efficient
 * than a non-marshalled key tuple extractor because more conversions are
 * needed.  A marshalled key extractor must convert the data to an object in
 * order to extract the key data, while an unmarshalled key extractor does
 * not.</p>
 *
 * @author Mark Hayes
 */
public class TupleTupleMarshalledKeyExtractor extends TupleTupleKeyExtractor {

    private String keyName;
    private TupleTupleMarshalledBinding binding;

    /**
     * Creates a tuple-tuple marshalled key extractor.
     *
     * @param binding is the binding used for the tuple-tuple entity.
     *
     * @param indexKeyFormat is the index key format.
     *
     * @param keyName is the key name passed to the {@link
     * MarshalledTupleKeyEntity#marshalIndexKey} method to identify the index
     * key.
     *
     * @param usePrimaryKey is true if the primary key data is used to
     * construct the index key.
     *
     * @param useValue is true if the value data is used to construct the index
     * key.
     */
    public TupleTupleMarshalledKeyExtractor(
                                    TupleTupleMarshalledBinding binding,
                                    TupleFormat indexKeyFormat,
                                    String keyName,
                                    boolean usePrimaryKey,
                                    boolean useValue) {

        super(usePrimaryKey ? ((TupleFormat) binding.getKeyFormat()) : null,
              useValue ? ((TupleFormat) binding.getValueFormat()) : null,
              indexKeyFormat);
        this.binding = binding;
        this.keyName = keyName;
    }

    // javadoc is inherited
    public void extractIndexKey(TupleInput primaryKeyInput,
                                TupleInput valueInput,
                                TupleOutput indexKeyOutput)
        throws IOException {

        MarshalledTupleKeyEntity entity = (MarshalledTupleKeyEntity)
            binding.dataToObject(primaryKeyInput, valueInput);

        // the primary key is unmarshalled before marshalling the index key, to
        // account for cases where the index key includes data taken from the
        // primary key

        entity.marshalIndexKey(keyName, indexKeyOutput);
    }

    // javadoc is inherited
    public void clearIndexKey(TupleInput valueInput, TupleOutput valueOutput)
        throws IOException {

        MarshalledTupleKeyEntity entity = (MarshalledTupleKeyEntity)
            binding.dataToObject(null, valueInput);
        entity.clearIndexKey(keyName);
        binding.objectToValue(entity, valueOutput);
    }
}
