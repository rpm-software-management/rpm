/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: TupleTupleBinding.java,v 12.6 2007/05/04 00:28:25 mark Exp $
 */

package com.sleepycat.bind.tuple;

import com.sleepycat.bind.EntityBinding;
import com.sleepycat.db.DatabaseEntry;

/**
 * An abstract <code>EntityBinding</code> that treats an entity's key entry and
 * data entry as tuples.
 *
 * <p>This class takes care of converting the entries to/from {@link
 * TupleInput} and {@link TupleOutput} objects.  Its three abstract methods
 * must be implemented by a concrete subclass to convert between tuples and
 * entity objects.</p>
 * <ul>
 * <li> {@link #entryToObject(TupleInput,TupleInput)} </li>
 * <li> {@link #objectToKey(Object,TupleOutput)} </li>
 * <li> {@link #objectToData(Object,TupleOutput)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class TupleTupleBinding extends TupleBase
    implements EntityBinding {

    /**
     * Creates a tuple-tuple entity binding.
     */
    public TupleTupleBinding() {
    }

    // javadoc is inherited
    public Object entryToObject(DatabaseEntry key, DatabaseEntry data) {

        return entryToObject(TupleBinding.entryToInput(key),
                             TupleBinding.entryToInput(data));
    }

    // javadoc is inherited
    public void objectToKey(Object object, DatabaseEntry key) {

        TupleOutput output = getTupleOutput(object);
        objectToKey(object, output);
        outputToEntry(output, key);
    }

    // javadoc is inherited
    public void objectToData(Object object, DatabaseEntry data) {

        TupleOutput output = getTupleOutput(object);
        objectToData(object, output);
        outputToEntry(output, data);
    }

    // abstract methods

    /**
     * Constructs an entity object from {@link TupleInput} key and data
     * entries.
     *
     * @param keyInput is the {@link TupleInput} key entry object.
     *
     * @param dataInput is the {@link TupleInput} data entry object.
     *
     * @return the entity object constructed from the key and data.
     */
    public abstract Object entryToObject(TupleInput keyInput,
                                         TupleInput dataInput);

    /**
     * Extracts a key tuple from an entity object.
     *
     * @param object is the entity object.
     *
     * @param output is the {@link TupleOutput} to which the key should be
     * written.
     */
    public abstract void objectToKey(Object object, TupleOutput output);

    /**
     * Extracts a key tuple from an entity object.
     *
     * @param object is the entity object.
     *
     * @param output is the {@link TupleOutput} to which the data should be
     * written.
     */
    public abstract void objectToData(Object object, TupleOutput output);
}
