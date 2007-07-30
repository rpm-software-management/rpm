/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: TupleSerialKeyCreator.java,v 12.6 2007/05/04 00:28:24 mark Exp $
 */

package com.sleepycat.bind.serial;

import com.sleepycat.bind.tuple.TupleBase;
import com.sleepycat.bind.tuple.TupleInput;
import com.sleepycat.bind.tuple.TupleOutput;
import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.SecondaryDatabase;
import com.sleepycat.db.SecondaryKeyCreator;

/**
 * A abstract key creator that uses a tuple key and a serial data entry. This
 * class takes care of serializing and deserializing the data entry, and
 * converting the key entry to/from {@link TupleInput} and {@link TupleOutput}
 * objects.
 * The following abstract method must be implemented by a concrete subclass
 * to create the index key using these objects
 * <ul>
 * <li> {@link #createSecondaryKey(TupleInput,Object,TupleOutput)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class TupleSerialKeyCreator extends TupleBase
    implements SecondaryKeyCreator {

    protected SerialBinding dataBinding;

    /**
     * Creates a tuple-serial key creator.
     *
     * @param classCatalog is the catalog to hold shared class information and
     * for a database should be a {@link StoredClassCatalog}.
     *
     * @param dataClass is the data base class.
     */
    public TupleSerialKeyCreator(ClassCatalog classCatalog, Class dataClass) {

        this(new SerialBinding(classCatalog, dataClass));
    }

    /**
     * Creates a tuple-serial key creator.
     *
     * @param dataBinding is the data binding.
     */
    public TupleSerialKeyCreator(SerialBinding dataBinding) {

        this.dataBinding = dataBinding;
    }

    // javadoc is inherited
    public boolean createSecondaryKey(SecondaryDatabase db,
                                      DatabaseEntry primaryKeyEntry,
                                      DatabaseEntry dataEntry,
                                      DatabaseEntry indexKeyEntry)
        throws DatabaseException {

        TupleOutput output = getTupleOutput(null);
        TupleInput primaryKeyInput = entryToInput(primaryKeyEntry);
        Object dataInput = dataBinding.entryToObject(dataEntry);
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

        Object data = dataBinding.entryToObject(dataEntry);
        data = nullifyForeignKey(data);
        if (data != null) {
            dataBinding.objectToEntry(data, dataEntry);
            return true;
        } else {
            return false;
        }
    }

    /**
     * Creates the index key entry from primary key tuple entry and
     * deserialized data entry.
     *
     * @param primaryKeyInput is the {@link TupleInput} for the primary key
     * entry, or null if no primary key entry is used to construct the index
     * key.
     *
     * @param dataInput is the deserialized data entry, or null if no data
     * entry is used to construct the index key.
     *
     * @param indexKeyOutput is the destination index key tuple.  For index
     * keys which are optionally present, no tuple entry should be output to
     * indicate that the key is not present or null.
     *
     * @return true if a key was created, or false to indicate that the key is
     * not present.
     */
    public abstract boolean createSecondaryKey(TupleInput primaryKeyInput,
                                               Object dataInput,
                                               TupleOutput indexKeyOutput);

    /**
     * Clears the index key in the deserialized data entry.
     *
     * <p>On entry the data parameter contains the index key to be cleared.  It
     * should be changed by this method such that {@link #createSecondaryKey}
     * will return false.  Other fields in the data object should remain
     * unchanged.</p>
     *
     * @param data is the source and destination deserialized data
     * entry.
     *
     * @return the destination data object, or null to indicate that the
     * key is not present and no change is necessary.  The data returned may
     * be the same object passed as the data parameter or a newly created
     * object.
     */
    public Object nullifyForeignKey(Object data) {

        return null;
    }
}
