/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: MapEntry.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import java.util.Map;

/**
 * A simple Map.Entry implementation.
 *
 * <p><b>Warning:</b> Use of this interface violates the Java Collections
 * interface contract since these state that Map.Entry objects should only be
 * obtained from Map.entrySet() sets, while this class allows constructing them
 * directly.  However, it is useful for performing operations on an entry set
 * such as add(), contains(), etc.  For restrictions see {@link #getValue} and
 * {@link #setValue}.</p>
 *
 * @author Mark Hayes
 */
public class MapEntry implements Map.Entry {

    private Object key;
    private Object value;

    /**
     * Creates a map entry with a given key and value.
     *
     * @param key is the key to use.
     *
     * @param value is the value to use.
     */
    public MapEntry(Object key, Object value) {

        this.key = key;
        this.value = value;
    }

    /**
     * Computes a hash code as specified by {@link
     * java.util.Map.Entry#hashCode}.
     *
     * @return the computed hash code.
     */
    public int hashCode() {

        return ((key == null)    ? 0 : key.hashCode()) ^
               ((value == null)  ? 0 : value.hashCode());
    }

    /**
     * Compares this entry to a given entry as specified by {@link
     * java.util.Map.Entry#equals}.
     *
     * @return the computed hash code.
     */
    public boolean equals(Object other) {

        if (!(other instanceof Map.Entry)) {
            return false;
        }

        Map.Entry e = (Map.Entry) other;

        return ((key == null) ? (e.getKey() == null)
                              : key.equals(e.getKey())) &&
               ((value == null) ? (e.getValue() == null)
                                : value.equals(e.getValue()));
    }

    /**
     * Returns the key of this entry.
     *
     * @return the key of this entry.
     */
    public final Object getKey() {

        return key;
    }

    /**
     * Returns the value of this entry.  Note that this will be the value
     * passed to the constructor or the last value passed to {@link #setValue}.
     * It will not reflect changes made to a Map.
     *
     * @return the value of this entry.
     */
    public final Object getValue() {

        return value;
    }

    /**
     * Changes the value of this entry.  Note that this will change the value
     * in this entry object but will not change the value in a Map.
     *
     * @return the value of this entry.
     */
    public Object setValue(Object newValue) {

        Object oldValue = value;
        value = newValue;
        return oldValue;
    }

    /**
     * Converts the entry to a string representation for debugging.
     *
     * @return the string representation.
     */
    public String toString() {

        return "[key [" + key + "] value [" + value + ']';
    }
}

