/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleBinding.java,v 1.4 2004/06/29 06:06:36 mark Exp $
 */

package com.sleepycat.bind.tuple;

import java.util.HashMap;
import java.util.Map;

import com.sleepycat.bind.EntryBinding;
import com.sleepycat.db.DatabaseEntry;

/**
 * An abstract <code>EntryBinding</code> that treats a key or data entry as a
 * tuple; it includes predefined bindings for Java primitive types.
 *
 * <p>This class takes care of converting the entries to/from {@link
 * TupleInput} and {@link TupleOutput} objects.  Its two abstract methods must
 * be implemented by a concrete subclass to convert between tuples and key or
 * data objects.</p>
 * <ul>
 * <li> {@link #entryToObject(TupleInput)} </li>
 * <li> {@link #objectToEntry(Object,TupleOutput)} </li>
 * </ul>
 *
 * <p>For key or data entries which are Java primitive classes (String,
 * Integer, etc) {@link #getPrimitiveBinding} may be used to return a builtin
 * tuple binding.  A custom tuple binding for these types is not needed.</p>
 *
 * @author Mark Hayes
 */
public abstract class TupleBinding implements EntryBinding {

    private static final Map primitives = new HashMap();
    static {
        primitives.put(String.class, new StringBinding());
        primitives.put(Character.class, new CharacterBinding());
        primitives.put(Boolean.class, new BooleanBinding());
        primitives.put(Byte.class, new ByteBinding());
        primitives.put(Short.class, new ShortBinding());
        primitives.put(Integer.class, new IntegerBinding());
        primitives.put(Long.class, new LongBinding());
        primitives.put(Float.class, new FloatBinding());
        primitives.put(Double.class, new DoubleBinding());
    }

    /**
     * Creates a tuple binding.
     */
    public TupleBinding() {
    }

    // javadoc is inherited
    public Object entryToObject(DatabaseEntry entry) {

        return entryToObject(entryToInput(entry));
    }

    // javadoc is inherited
    public void objectToEntry(Object object, DatabaseEntry entry) {

        TupleOutput output = newOutput();
        objectToEntry(object, output);
        outputToEntry(output, entry);
    }

    /**
     * Utility method for use by bindings to create a tuple output object.
     *
     * @return a new tuple output object.
     */
    public static TupleOutput newOutput() {

        return new TupleOutput();
    }

    /**
     * Utility method for use by bindings to create a tuple output object
     * with a specific starting size.
     *
     * @return a new tuple output object.
     */
    public static TupleOutput newOutput(byte[] buffer) {

        return new TupleOutput(buffer);
    }

    /**
     * Utility method to set the data in a entry buffer to the data in a tuple
     * output object.
     *
     * @param output is the source tuple output object.
     *
     * @param entry is the destination entry buffer.
     */
    public static void outputToEntry(TupleOutput output, DatabaseEntry entry) {

        entry.setData(output.getBufferBytes(), output.getBufferOffset(),
                      output.getBufferLength());
    }

    /**
     * Utility method to set the data in a entry buffer to the data in a tuple
     * input object.
     *
     * @param input is the source tuple input object.
     *
     * @param entry is the destination entry buffer.
     */
    public static void inputToEntry(TupleInput input, DatabaseEntry entry) {

        entry.setData(input.getBufferBytes(), input.getBufferOffset(),
                      input.getBufferLength());
    }

    /**
     * Utility method to create a new tuple input object for reading the data
     * from a given buffer.  If an existing input is reused, it is reset before
     * returning it.
     *
     * @param entry is the source entry buffer.
     *
     * @return the new tuple input object.
     */
    public static TupleInput entryToInput(DatabaseEntry entry) {

        return new TupleInput(entry.getData(), entry.getOffset(),
                              entry.getSize());
    }

    /**
     * Constructs a key or data object from a {@link TupleInput} entry.
     *
     * @param input is the tuple key or data entry.
     *
     * @return the key or data object constructed from the entry.
     */
    public abstract Object entryToObject(TupleInput input);

    /**
     * Converts a key or data object to a tuple entry.
     *
     * @param object is the key or data object.
     *
     * @param output is the tuple entry to which the key or data should be
     * written.
     */
    public abstract void objectToEntry(Object object, TupleOutput output);

    /**
     * Creates a tuple binding for a primitive Java class.  The following
     * Java classes are supported.
     * <ul>
     * <li><code>String</code></li>
     * <li><code>Character</code></li>
     * <li><code>Boolean</code></li>
     * <li><code>Byte</code></li>
     * <li><code>Short</code></li>
     * <li><code>Integer</code></li>
     * <li><code>Long</code></li>
     * <li><code>Float</code></li>
     * <li><code>Double</code></li>
     * </ul>
     *
     * @param cls is the primitive Java class.
     *
     * @return a new binding for the primitive class or null if the cls
     * parameter is not one of the supported classes.
     */
    public static TupleBinding getPrimitiveBinding(Class cls) {

        return (TupleBinding) primitives.get(cls);
    }
}
