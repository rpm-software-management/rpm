/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DoubleBinding.java,v 1.4 2004/08/02 18:52:04 mjc Exp $
 */

package com.sleepycat.bind.tuple;

import com.sleepycat.db.DatabaseEntry;

/**
 * A concrete <code>TupleBinding</code> for a <code>Double</code> primitive
 * wrapper or a <code>double</code> primitive.
 *
 * <p>There are two ways to use this class:</p>
 * <ol>
 * <li>When using the {@link com.sleepycat.db} package directly, the static
 * methods in this class can be used to convert between primitive values and
 * {@link DatabaseEntry} objects.</li>
 * <li>When using the {@link com.sleepycat.collections} package, an instance of
 * this class can be used with any stored collection.  The easiest way to
 * obtain a binding instance is with the {@link
 * TupleBinding#getPrimitiveBinding} method.</li>
 * </ol>
 */
public class DoubleBinding extends TupleBinding {

    private static final int DOUBLE_SIZE = 8;

    // javadoc is inherited
    public Object entryToObject(TupleInput input) {

        return new Double(input.readDouble());
    }

    // javadoc is inherited
    public void objectToEntry(Object object, TupleOutput output) {

        /* Do nothing.  Not called by objectToEntry(Object,DatabaseEntry). */
    }

    // javadoc is inherited
    public void objectToEntry(Object object, DatabaseEntry entry) {

        doubleToEntry(((Number) object).doubleValue(), entry);
    }

    /**
     * Converts an entry buffer into a simple <code>double</code> value.
     *
     * @param entry is the source entry buffer.
     *
     * @return the resulting value.
     */
    public static double entryToDouble(DatabaseEntry entry) {

        return entryToInput(entry).readDouble();
    }

    /**
     * Converts a simple <code>double</code> value into an entry buffer.
     *
     * @param val is the source value.
     *
     * @param entry is the destination entry buffer.
     */
    public static void doubleToEntry(double val, DatabaseEntry entry) {

        outputToEntry(newOutput(new byte[DOUBLE_SIZE]).writeDouble(val),
		      entry);
    }
}
