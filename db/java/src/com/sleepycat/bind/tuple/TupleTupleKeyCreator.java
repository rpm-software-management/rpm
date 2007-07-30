/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: TupleTupleKeyCreator.java,v 12.7 2007/05/04 00:28:25 mark Exp $
 */

package com.sleepycat.bind.tuple;

import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.SecondaryDatabase;
import com.sleepycat.db.SecondaryKeyCreator;

/**
 * An abstract key creator that uses a tuple key and a tuple data entry. This
 * class takes care of converting the key and data entry to/from {@link
 * TupleInput} and {@link TupleOutput} objects.
 * The following abstract method must be implemented by a concrete subclass
 * to create the index key using these objects
 * <ul>
 * <li> {@link #createSecondaryKey(TupleInput,TupleInput,TupleOutput)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class TupleTupleKeyCreator extends TupleBase
    implements SecondaryKeyCreator {

    /**
     * Creates a tuple-tuple key creator.
     */
    public TupleTupleKeyCreator() {
    }

    // javadoc is inherited
    public boolean createSecondaryKey(SecondaryDatabase db,
                                      DatabaseEntry primaryKeyEntry,
                                      DatabaseEntry dataEntry,
                                      DatabaseEntry indexKeyEntry)
        throws DatabaseException {

        TupleOutput output = getTupleOutput(null);
        TupleInput primaryKeyInput = entryToInput(primaryKeyEntry);
        TupleInput dataInput = entryToInput(dataEntry);
        if (createSecondaryKey(primaryKeyInput, dataInput, output)) {
            outputToEntry(output, indexKeyEntry);
            return true;
        } else {
            return false;
        }
    }

    // javadoc is inherited
    public boolean nullifyForeignKey(SecondaryDatabase db,
                                     DatabaseEntry dataEntry)
        throws DatabaseException {

        TupleOutput output = getTupleOutput(null);
        if (nullifyForeignKey(entryToInput(dataEntry), output)) {
            outputToEntry(output, dataEntry);
            return true;
        } else {
            return false;
        }
    }

    /**
     * Creates the index key from primary key tuple and data tuple.
     *
     * @param primaryKeyInput is the {@link TupleInput} for the primary key
     * entry.
     *
     * @param dataInput is the {@link TupleInput} for the data entry.
     *
     * @param indexKeyOutput is the destination index key tuple.
     *
     * @return true if a key was created, or false to indicate that the key is
     * not present.
     */
    public abstract boolean createSecondaryKey(TupleInput primaryKeyInput,
                                               TupleInput dataInput,
                                               TupleOutput indexKeyOutput);

    /**
     * Clears the index key in the tuple data entry.  The dataInput should be
     * read and then written to the dataOutput, clearing the index key in the
     * process.
     *
     * <p>The secondary key should be output or removed by this method such
     * that {@link #createSecondaryKey} will return false.  Other fields in the
     * data object should remain unchanged.</p>
     *
     * @param dataInput is the {@link TupleInput} for the data entry.
     *
     * @param dataOutput is the destination {@link TupleOutput}.
     *
     * @return true if the key was cleared, or false to indicate that the key
     * is not present and no change is necessary.
     */
    public boolean nullifyForeignKey(TupleInput dataInput,
                                     TupleOutput dataOutput) {

        return false;
    }
}
