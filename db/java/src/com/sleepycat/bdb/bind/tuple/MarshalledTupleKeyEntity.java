/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: MarshalledTupleKeyEntity.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import java.io.IOException;

/**
 * A marshalling interface implemented by entity classes that have tuple data
 * keys. Since MarshalledTupleKeyEntity objects are instantiated by Java
 * serialization, no particular contructor is required.
 *
 * <p>Note that a marshalled tuple key extractor is somewhat less efficient
 * than a non-marshalled key tuple extractor because more conversions are
 * needed.  A marshalled key extractor must convert the data to an object in
 * order to extract the key data, while an unmarshalled key extractor does
 * not.</p>
 *
 * @author Mark Hayes
 * @see TupleTupleMarshalledBinding
 * @see TupleTupleMarshalledKeyExtractor
 * @see com.sleepycat.bdb.bind.serial.TupleSerialMarshalledBinding
 * @see com.sleepycat.bdb.bind.serial.TupleSerialMarshalledKeyExtractor
 */
public interface MarshalledTupleKeyEntity {

    /**
     * Extracts the entity's primary key and writes it to the key output.
     *
     * @param keyOutput is the output tuple.
     */
    void marshalPrimaryKey(TupleOutput keyOutput)
        throws IOException;

    /**
     * Completes construction of the entity by setting its primary key from the
     * stored primary key.
     *
     * @param keyInput is the input tuple.
     */
    void unmarshalPrimaryKey(TupleInput keyInput)
        throws IOException;

    /**
     * Extracts the entity's index key and writes it to the key output.
     *
     * @param keyName identifies the index key.
     *
     * @param keyOutput is the output tuple.
     */
    void marshalIndexKey(String keyName, TupleOutput keyOutput)
        throws IOException;

    /**
     * Clears the entity's index key value for the given key name.
     * This method is called when the entity for this foreign key is
     * deleted, if ON_DELETE_CLEAR was specified when creating the index.
     *
     * @param keyName identifies the index key.
     */
    void clearIndexKey(String keyName)
        throws IOException;
}
