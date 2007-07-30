/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: TupleSerialFactory.java,v 12.5 2007/05/04 00:28:25 mark Exp $
 */

package com.sleepycat.collections;

import com.sleepycat.bind.EntryBinding;
import com.sleepycat.bind.serial.ClassCatalog;
import com.sleepycat.bind.serial.TupleSerialMarshalledBinding;
import com.sleepycat.bind.serial.TupleSerialMarshalledKeyCreator;
import com.sleepycat.bind.tuple.TupleBinding;
import com.sleepycat.bind.tuple.TupleMarshalledBinding;
import com.sleepycat.db.Database;

/**
 * Creates stored collections having tuple keys and serialized entity values.
 * The entity classes must implement the java.io.Serializable and
 * MarshalledTupleKeyEntity interfaces.  The key classes must either implement
 * the MarshalledTupleEntry interface or be one of the Java primitive type
 * classes.  Underlying binding objects are created automatically.
 *
 * @author Mark Hayes
 */
public class TupleSerialFactory {

    private ClassCatalog catalog;

    /**
     * Creates a tuple-serial factory for given environment and class catalog.
     */
    public TupleSerialFactory(ClassCatalog catalog) {

        this.catalog = catalog;
    }

    /**
     * Returns the class catalog associated with this factory.
     */
    public final ClassCatalog getCatalog() {

        return catalog;
    }

    /**
     * Creates a map from a previously opened Database object.
     *
     * @param db the previously opened Database object.
     *
     * @param keyClass is the class used for map keys.  It must implement the
     * {@link com.sleepycat.bind.tuple.MarshalledTupleEntry} interface or be
     * one of the Java primitive type classes.
     *
     * @param valueBaseClass the base class of the entity values for this
     * store.  It must implement the  {@link
     * com.sleepycat.bind.tuple.MarshalledTupleKeyEntity} interface.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     */
    public StoredMap newMap(Database db, Class keyClass, Class valueBaseClass,
                            boolean writeAllowed) {

        return new StoredMap(db,
                        getKeyBinding(keyClass),
                        getEntityBinding(valueBaseClass),
                        writeAllowed);
    }

    /**
     * Creates a sorted map from a previously opened Database object.
     *
     * @param db the previously opened Database object.
     *
     * @param keyClass is the class used for map keys.  It must implement the
     * {@link com.sleepycat.bind.tuple.MarshalledTupleEntry} interface or be
     * one of the Java primitive type classes.
     *
     * @param valueBaseClass the base class of the entity values for this
     * store.  It must implement the  {@link
     * com.sleepycat.bind.tuple.MarshalledTupleKeyEntity} interface.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     */
    public StoredSortedMap newSortedMap(Database db, Class keyClass,
                                        Class valueBaseClass,
                                        boolean writeAllowed) {

        return new StoredSortedMap(db,
                        getKeyBinding(keyClass),
                        getEntityBinding(valueBaseClass),
                        writeAllowed);
    }

    /**
     * Creates a <code>SecondaryKeyCreator</code> object for use in configuring
     * a <code>SecondaryDatabase</code>.  The returned object implements
     * the {@link com.sleepycat.db.SecondaryKeyCreator} interface.
     *
     * @param valueBaseClass the base class of the entity values for this
     * store.  It must implement the  {@link
     * com.sleepycat.bind.tuple.MarshalledTupleKeyEntity} interface.
     *
     * @param keyName is the key name passed to the {@link
     * com.sleepycat.bind.tuple.MarshalledTupleKeyEntity#marshalSecondaryKey}
     * method to identify the secondary key.
     */
    public TupleSerialMarshalledKeyCreator getKeyCreator(Class valueBaseClass,
                                                         String keyName) {

        return new TupleSerialMarshalledKeyCreator(
                                            getEntityBinding(valueBaseClass),
                                            keyName);
    }

    private TupleSerialMarshalledBinding getEntityBinding(Class baseClass) {

        return new TupleSerialMarshalledBinding(catalog, baseClass);
    }

    private EntryBinding getKeyBinding(Class keyClass) {

        EntryBinding binding = TupleBinding.getPrimitiveBinding(keyClass);
        if (binding == null) {
            binding = new TupleMarshalledBinding(keyClass);
        }
        return binding;
    }
}

