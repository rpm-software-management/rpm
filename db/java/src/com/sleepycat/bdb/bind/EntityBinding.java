/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: EntityBinding.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind;

import java.io.IOException;

/**
 * The interface implemented by all entity or key/data-to-object bindings.
 *
 * @author Mark Hayes
 */
public interface EntityBinding {

    /**
     * Converts key and value data buffers into an entity Object.
     *
     * @param key is the source key data.
     *
     * @param value is the source value data.
     *
     * @return the resulting Object.
     */
    Object dataToObject(DataBuffer key, DataBuffer value)
        throws IOException;

    /**
     * Extracts the key data from an entity Object.
     *
     * @param object is the source Object.
     *
     * @param key is the destination data buffer.
     */
    void objectToKey(Object object, DataBuffer key)
        throws IOException;

    /**
     * Extracts the value data from an entity Object.
     *
     * @param object is the source Object.
     *
     * @param value is the destination data buffer.
     */
    void objectToValue(Object object, DataBuffer value)
        throws IOException;

    /**
     * Returns the format used for the key data of this binding.
     *
     * @return the key data format.
     */
    DataFormat getKeyFormat();

    /**
     * Returns the format used for the value data of this binding.
     *
     * @return the value data format.
     */
    DataFormat getValueFormat();
}
