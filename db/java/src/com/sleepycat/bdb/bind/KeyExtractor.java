/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: KeyExtractor.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind;

import java.io.IOException;

/**
 * The interface implemented for extracting the index key from primary key
 * and/or value buffers, and for clearing the index key in a value buffer.  The
 * implementation of this interface defines a specific index key for use in a
 * database, and that is independent of any bindings that may be used.
 *
 * @author Mark Hayes
 */
public interface KeyExtractor {

    /**
     * Extracts the index key data from primary key and value buffers.
     * The index key is extracted when saving the data record identified by the
     * primary key and value buffers, in order to add or remove an index
     * entry in the database for that data record.
     *
     * @param primaryKeyData is the source primary key data, or null if no
     * primary key data is used to construct the index key, in which case
     * {@link #getPrimaryKeyFormat} should also return null.
     *
     * @param valueData is the source value data, or null if no value data is
     * used to construct the index key, in which case {@link #getValueFormat}
     * should also return null.
     *
     * @param indexKeyData is the destination index key buffer.  For index keys
     * which are optionally present, the buffer's length should be set to zero
     * to indicate that the key is not present or null.
     */
    void extractIndexKey(DataBuffer primaryKeyData, DataBuffer valueData,
                         DataBuffer indexKeyData)
        throws IOException;

    /**
     * Clears the index key in a value buffer.  The index key is cleared when
     * the index is for a foreign key identifying a record that has been
     * deleted.  This method is called only if the {@link
     * com.sleepycat.bdb.ForeignKeyIndex} is configured with {@link
     * com.sleepycat.bdb.ForeignKeyIndex#ON_DELETE_CLEAR}.  It is never called
     * for index keys that are derived from primary key data, since in this
     * case {@link com.sleepycat.bdb.ForeignKeyIndex#ON_DELETE_CLEAR} is not
     * allowed.
     *
     * @param valueData is the source and destination value data.  On entry
     * this contains the index key to be cleared.  It should be changed by this
     * method such that {@link #extractIndexKey} will extract a null key (set
     * the buffer length to zero).  Other data in the buffer should remain
     * unchanged.
     */
    void clearIndexKey(DataBuffer valueData)
        throws IOException;

    /**
     * Returns the format of the primary key data or null if the index key data
     * is not derived from the primary key data.  If this method returns null,
     * then null will be passed for the <code>primaryKeyData</code> parameter
     * of {@link #extractIndexKey}.
     *
     * @return the format of the primary key data or null.
     */
    DataFormat getPrimaryKeyFormat();

    /**
     * Returns the format of the value data or null if the index key data is
     * not derived from the value data.  If this method returns null, then null
     * will be passed for the <code>valueData</code> parameter of {@link
     * #extractIndexKey}.
     *
     * @return the format of the value data or null.
     */
    DataFormat getValueFormat();

    /**
     * Returns the format of the index key data.
     *
     * @return the format of the index key data.
     */
    DataFormat getIndexKeyFormat();
}
