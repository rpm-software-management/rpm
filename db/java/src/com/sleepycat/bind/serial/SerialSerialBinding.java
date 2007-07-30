/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: SerialSerialBinding.java,v 12.5 2007/05/04 00:28:24 mark Exp $
 */

package com.sleepycat.bind.serial;

import com.sleepycat.bind.EntityBinding;
import com.sleepycat.db.DatabaseEntry;

/**
 * An abstract <code>EntityBinding</code> that treats an entity's key entry and
 * data entry as serialized objects.
 *
 * <p>This class takes care of serializing and deserializing the key and
 * data entry automatically.  Its three abstract methods must be implemented by
 * a concrete subclass to convert the deserialized objects to/from an entity
 * object.</p>
 * <ul>
 * <li> {@link #entryToObject(Object,Object)} </li>
 * <li> {@link #objectToKey(Object)} </li>
 * <li> {@link #objectToData(Object)} </li>
 * </ul>
 *
 * @author Mark Hayes
 */
public abstract class SerialSerialBinding implements EntityBinding {

    private SerialBinding keyBinding;
    private SerialBinding dataBinding;

    /**
     * Creates a serial-serial entity binding.
     *
     * @param classCatalog is the catalog to hold shared class information and
     * for a database should be a {@link StoredClassCatalog}.
     *
     * @param keyClass is the key base class.
     *
     * @param dataClass is the data base class.
     */
    public SerialSerialBinding(ClassCatalog classCatalog,
                               Class keyClass,
                               Class dataClass) {

        this(new SerialBinding(classCatalog, keyClass),
             new SerialBinding(classCatalog, dataClass));
    }

    /**
     * Creates a serial-serial entity binding.
     *
     * @param keyBinding is the key binding.
     *
     * @param dataBinding is the data binding.
     */
    public SerialSerialBinding(SerialBinding keyBinding,
                               SerialBinding dataBinding) {

        this.keyBinding = keyBinding;
        this.dataBinding = dataBinding;
    }

    // javadoc is inherited
    public Object entryToObject(DatabaseEntry key, DatabaseEntry data) {

        return entryToObject(keyBinding.entryToObject(key),
                             dataBinding.entryToObject(data));
    }

    // javadoc is inherited
    public void objectToKey(Object object, DatabaseEntry key) {

        object = objectToKey(object);
        keyBinding.objectToEntry(object, key);
    }

    // javadoc is inherited
    public void objectToData(Object object, DatabaseEntry data) {

        object = objectToData(object);
        dataBinding.objectToEntry(object, data);
    }

    /**
     * Constructs an entity object from deserialized key and data objects.
     *
     * @param keyInput is the deserialized key object.
     *
     * @param dataInput is the deserialized data object.
     *
     * @return the entity object constructed from the key and data.
     */
    public abstract Object entryToObject(Object keyInput, Object dataInput);

    /**
     * Extracts a key object from an entity object.
     *
     * @param object is the entity object.
     *
     * @return the deserialized key object.
     */
    public abstract Object objectToKey(Object object);

    /**
     * Extracts a data object from an entity object.
     *
     * @param object is the entity object.
     *
     * @return the deserialized data object.
     */
    public abstract Object objectToData(Object object);
}
