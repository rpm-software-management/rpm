/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StringBinding.java,v 1.4 2004/08/02 18:52:05 mjc Exp $
 */

package com.sleepycat.bind.tuple;

import com.sleepycat.util.UtfOps;
import com.sleepycat.db.DatabaseEntry;

/**
 * A concrete <code>TupleBinding</code> for a simple <code>String</code> value.
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
public class StringBinding extends TupleBinding {

    // javadoc is inherited
    public Object entryToObject(TupleInput input) {

        return input.readString();
    }

    // javadoc is inherited
    public void objectToEntry(Object object, TupleOutput output) {

        /* Do nothing.  Not called by objectToEntry(Object,DatabaseEntry). */
    }

    // javadoc is inherited
    public void objectToEntry(Object object, DatabaseEntry entry) {

        stringToEntry((String) object, entry);
    }

    /**
     * Converts an entry buffer into a simple <code>String</code> value.
     *
     * @param entry is the source entry buffer.
     *
     * @return the resulting value.
     */
    public static String entryToString(DatabaseEntry entry) {

        return entryToInput(entry).readString();
    }

    /**
     * Converts a simple <code>String</code> value into an entry buffer.
     *
     * @param val is the source value.
     *
     * @param entry is the destination entry buffer.
     */
    public static void stringToEntry(String val, DatabaseEntry entry) {

	int stringLength =
	    (val == null) ? 1 : UtfOps.getByteLength(val.toCharArray());
	stringLength++;           // null terminator
        outputToEntry(newOutput(new byte[stringLength]).writeString(val),
		      entry);
    }
}
