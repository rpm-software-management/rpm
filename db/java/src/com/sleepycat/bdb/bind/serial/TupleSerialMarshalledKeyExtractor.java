/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleSerialMarshalledKeyExtractor.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.bind.tuple.MarshalledTupleKeyEntity;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * A concrete key extractor that works in conjunction with a {@link
 * TupleSerialMarshalledBinding}.  This key extractor works by calling the
 * methods of the {@link MarshalledTupleKeyEntity} interface to extract and
 * clear the index key data.
 *
 * @author Mark Hayes
 */
public class TupleSerialMarshalledKeyExtractor
    extends TupleSerialKeyExtractor {

    private TupleSerialMarshalledBinding binding;
    private String keyName;

    /**
     * Creates a tuple-serial marshalled key extractor.
     *
     * @param binding is the binding used for the tuple-serial entity.
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
    public TupleSerialMarshalledKeyExtractor(
                                        TupleSerialMarshalledBinding binding,
                                        TupleFormat indexKeyFormat,
                                        String keyName,
                                        boolean usePrimaryKey,
                                        boolean useValue) {

        super(usePrimaryKey ? ((TupleFormat) binding.getKeyFormat()) : null,
              useValue ? ((SerialFormat) binding.getValueFormat()) : null,
              indexKeyFormat);
        this.binding = binding;
        this.keyName = keyName;

        if (valueFormat == null)
            throw new IllegalArgumentException("valueFormat may not be null");
    }

    // javadoc is inherited
    public void extractIndexKey(TupleInput primaryKeyInput,
                                Object valueInput,
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
    public void clearIndexKey(Object valueInput)
        throws IOException {

        MarshalledTupleKeyEntity entity = (MarshalledTupleKeyEntity)
            binding.dataToObject(null, valueInput);

        entity.clearIndexKey(keyName);
    }
}
