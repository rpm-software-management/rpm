/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleSerialDbFactory.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.factory;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.serial.ClassCatalog;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.bdb.bind.serial.TupleSerialBinding;
import com.sleepycat.bdb.bind.serial.TupleSerialMarshalledKeyExtractor;
import com.sleepycat.bdb.bind.serial.TupleSerialMarshalledBinding;
import com.sleepycat.bdb.bind.tuple.MarshalledTupleData;
import com.sleepycat.bdb.bind.tuple.MarshalledTupleKeyEntity;
import com.sleepycat.bdb.bind.tuple.TupleBinding;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleMarshalledBinding;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbException;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.ForeignKeyIndex;
import com.sleepycat.bdb.PrimaryKeyAssigner;
import com.sleepycat.bdb.StoredClassCatalog;
import com.sleepycat.bdb.collection.StoredMap;
import com.sleepycat.bdb.collection.StoredSortedMap;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * Creates stored collections having tuple keys and serialized entity values.
 * The entity classes must implement the java.io.Serializable and
 * MarshalledTupleKeyEntity interfaces.  The key classes must either implement
 * the MarshalledTupleData interface or be one of the Java primitive type
 * classes.
 *
 * <p> This factory object is used to create DataStore, DataIndex,
 * ForeignKeyIndex and Map objects.  The underlying DataFormat,
 * DataBinding and KeyExtractor objects are created automatically. </p>
 *
 * @author Mark Hayes
 */
public class TupleSerialDbFactory {

    private static final TupleFormat TUPLE_FORMAT = new TupleFormat();

    private ClassCatalog catalog;

    /**
     * Creates a tuple-serial factory for given environment and class catalog.
     */
    public TupleSerialDbFactory(ClassCatalog catalog) {

        this.catalog = catalog;
    }

    /**
     * Returns the class catalog associated with this factory.
     */
    public final ClassCatalog getCatalog() {

        return catalog;
    }

    /**
     * Creates a store from a previously opened Db object.
     *
     * @param db the previously opened Db object.
     *
     * @param baseClass the base class of the entity values for this store.
     * It must implement the  {@link MarshalledTupleKeyEntity} interface.
     *
     * @param keyAssigner an object for assigning keys or null if no automatic
     * key assignment is used.
     */
    public DataStore newDataStore(Db db, Class baseClass,
                                  PrimaryKeyAssigner keyAssigner) {

        return new DataStore(db, TUPLE_FORMAT,
                             new SerialFormat(catalog, baseClass),
                             keyAssigner);
    }

    /**
     * Creates an index from a previously opened Db object.
     *
     * @param db the previously opened Db object.
     *
     * @param store the store to be indexed and also specifies the
     * environment that was used to create the Db object.
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
     *
     * @throws IllegalArgumentException if a format mismatch is detected
     * between the index and the store, or if unsorted duplicates were
     * specified for the index Db.
     */
    public DataIndex newDataIndex(DataStore store, Db db, String keyName,
                                  boolean usePrimaryKey, boolean useValue) {

        return new DataIndex(store, db, TUPLE_FORMAT,
                             getKeyExtractor(store, keyName,
                                             usePrimaryKey, useValue));
    }

    /**
     * Creates a foreign key index from a previously opened Db object.
     *
     * @param store the store to be indexed and also specifies the
     * environment that was used to create the Db object.
     *
     * @param db the previously opened Db object.
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
     *
     * @param foreignStore is the store in which the index key for this store
     * is a primary key.
     *
     * @param deleteAction determines what action occurs when the foreign key
     * is deleted. It must be one of the
     * {@link com.sleepycat.bdb.ForeignKeyIndex ForeignKeyIndex} ON_DELETE_
     * constants.
     *
     * @throws IllegalArgumentException if a format mismatch is detected
     * between the index and the store, or if unsorted duplicates were
     * specified for the index Db.
     */
    public ForeignKeyIndex newForeignKeyIndex(DataStore store, Db db,
                                              String keyName,
                                              boolean usePrimaryKey,
                                              boolean useValue,
                                              DataStore foreignStore,
                                              int deleteAction) {

        return new ForeignKeyIndex(store, db,
                                   getKeyExtractor(store, keyName,
                                                   usePrimaryKey, useValue),
                                   foreignStore, deleteAction);
    }

    /**
     * Creates a map for a given store that was obtained from this factory.
     *
     * @param store a store obtained from this factory.
     *
     * @param keyClass is the class used for map keys.  It must implement the
     * {@link MarshalledTupleData} interface or be one of the Java primitive
     * type classes.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     */
    public StoredMap newMap(DataStore store, Class keyClass,
                            boolean writeAllowed) {

        return new StoredMap(store,
                        getKeyBinding(keyClass),
                        getEntityBinding(store),
                        writeAllowed);
    }

    /**
     * Creates a map for a given index that was obtained from this factory.
     *
     * @param index a index obtained from this factory.
     *
     * @param keyClass is the class used for map keys.  It must implement the
     * {@link MarshalledTupleData} interface or be one of the Java primitive
     * type classes.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     */
    public StoredMap newMap(DataIndex index, Class keyClass,
                            boolean writeAllowed) {

        return new StoredMap(index,
                        getKeyBinding(keyClass),
                        getEntityBinding(index.getStore()),
                        writeAllowed);
    }

    /**
     * Creates a sorted map for a given store that was obtained from this
     * factory.
     *
     * @param store a store obtained from this factory.
     *
     * @param keyClass is the class used for map keys.  It must implement the
     * {@link MarshalledTupleData} interface or be one of the Java primitive
     * type classes.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     */
    public StoredSortedMap newSortedMap(DataStore store, Class keyClass,
                                        boolean writeAllowed) {

        return new StoredSortedMap(store,
                        getKeyBinding(keyClass),
                        getEntityBinding(store),
                        writeAllowed);
    }

    /**
     * Creates a sorted map for a given index that was obtained from this
     * factory.
     *
     * @param index an index obtained from this factory.
     *
     * @param keyClass is the class used for map keys.  It must implement the
     * {@link MarshalledTupleData} interface or be one of the Java primitive
     * type classes.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     */
    public StoredSortedMap newSortedMap(DataIndex index, Class keyClass,
                                        boolean writeAllowed) {

        return new StoredSortedMap(index,
                        getKeyBinding(keyClass),
                        getEntityBinding(index.getStore()),
                        writeAllowed);
    }

    private TupleSerialMarshalledKeyExtractor getKeyExtractor(
                                    DataStore store, String keyName,
                                    boolean usePrimaryKey, boolean useValue) {

        return new TupleSerialMarshalledKeyExtractor(
                        getEntityBinding(store),
                        TUPLE_FORMAT, keyName,
                        usePrimaryKey, useValue);
    }

    private TupleSerialMarshalledBinding getEntityBinding(DataStore store) {

        return new TupleSerialMarshalledBinding(
                        (TupleFormat) store.getKeyFormat(),
                        (SerialFormat) store.getValueFormat());
    }

    private DataBinding getKeyBinding(Class keyClass) {

        DataBinding binding = TupleBinding.getPrimitiveBinding(keyClass,
                                                               TUPLE_FORMAT);
        if (binding == null) {
            binding = new TupleMarshalledBinding(TUPLE_FORMAT, keyClass);
        }
        return binding;
    }
}
