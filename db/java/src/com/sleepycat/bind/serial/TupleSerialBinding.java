/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleSerialBinding.java,v 1.2 2004/06/04 18:24:49 mark Exp $
 */

package com.sleepycat.bind.serial;

import com.sleepycat.bind.EntityBinding;
import com.sleepycat.bind.tuple.TupleBinding;
import com.sleepycat.bind.tuple.TupleInput;
import com.sleepycat.bind.tuple.TupleOutput;
import com.sleepycat.db.DatabaseEntry;

/**
 * An abstract <code>EntityBinding</code> that treats an entity's key entry as
 * a tuple and its data entry as a serialized object.
 *
 * <p>This class takes care of serializing and deserializing the data entry,
 * and converting the key entry to/from {@link TupleInput} and {@link
 * TupleOutput} objects.  Its three abstract methods must be implemented by a
 * concrete subclass to convert these objects to/from an entity object.</p>
 * <ul>
 * <li> {@link #entryToObject(TupleInput,Object)} </li>
 * <li> {@link #objectToKey(Object,TupleOutput)} </li>
 * <li> {@link #objectToData(Object)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class TupleSerialBinding implements EntityBinding {

    protected SerialBinding dataBinding;

    /**
     * Creates a tuple-serial entity binding.
     *
     * @param classCatalog is the catalog to hold shared class information and
     * for a database should be a {@link StoredClassCatalog}.
     *
     * @param baseClass is the base class.
     */
    public TupleSerialBinding(ClassCatalog classCatalog,
                              Class baseClass) {

        this(new SerialBinding(classCatalog, baseClass));
    }

    /**
     * Creates a tuple-serial entity binding.
     *
     * @param dataBinding is the data binding.
     */
    public TupleSerialBinding(SerialBinding dataBinding) {

        this.dataBinding = dataBinding;
    }

    // javadoc is inherited
    public Object entryToObject(DatabaseEntry key, DatabaseEntry data) {

        return entryToObject(TupleBinding.entryToInput(key),
                             dataBinding.entryToObject(data));
    }

    // javadoc is inherited
    public void objectToKey(Object object, DatabaseEntry key) {

        TupleOutput output = TupleBinding.newOutput();
        objectToKey(object, output);
        TupleBinding.outputToEntry(output, key);
    }

    // javadoc is inherited
    public void objectToData(Object object, DatabaseEntry data) {

        object = objectToData(object);
        dataBinding.objectToEntry(object, data);
    }

    /**
     * Constructs an entity object from {@link TupleInput} key entry and
     * deserialized data entry objects.
     *
     * @param keyInput is the {@link TupleInput} key entry object.
     *
     * @param dataInput is the deserialized data entry object.
     *
     * @return the entity object constructed from the key and data.
     */
    public abstract Object entryToObject(TupleInput keyInput,
                                         Object dataInput);

    /**
     * Extracts a key tuple from an entity object.
     *
     * @param object is the entity object.
     *
     * @param keyOutput is the {@link TupleOutput} to which the key should be
     * written.
     */
    public abstract void objectToKey(Object object, TupleOutput keyOutput);

    /**
     * Extracts a data object from an entity object.
     *
     * @param object is the entity object.
     *
     * @return the deserialized data object.
     */
    public abstract Object objectToData(Object object);
}
