/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: ClassCatalog.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import java.io.IOException;
import java.io.ObjectStreamClass;

/**
 * Represents a catalog of class information for use in object serialization so
 * that class descriptions can be stored separately from serialized objects.
 *
 * <p>This information is used for serialization of class descriptors or
 * java.io.ObjectStreamClass objects, each of which represents a unique class
 * format.  For each unique format, a unique class ID is assigned by the
 * catalog.  The class ID can then be used in the serialization stream in place
 * of the full class information.  When used with {@link SerialInput} and
 * {@link SerialOutput} or any of the serial bindings, the use of the catalog
 * is transparent to the application.</p>
 *
 * @author Mark Hayes
 */
public interface ClassCatalog {

    /**
     * Close a catalog database and release any cached resources.
     */
    public void close()
        throws IOException;

    /**
     * Return the class ID for the current version of the given class name.
     * This is used for storing in serialization streams in place of a full
     * class descriptor, since it is much more compact.  To get back the
     * ObjectStreamClass for a class ID, call {@link #getClassFormat(byte[])}.
     * This function causes a new class ID to be assigned if the class
     * description has changed.
     *
     * @param className The fully qualified class name for which to return the
     * class ID.
     *
     * @return The class ID for the current version of the class.
     */
    public byte[] getClassID(String className)
        throws IOException, ClassNotFoundException;

    /**
     * Return the ObjectStreamClass for the given class name.  This is always
     * the current class format.  Calling this method is equivalent to calling
     * java.io.ObjectStreamClass.lookup, but this method causes a new class
     * ID to be assigned if the class description has changed.
     *
     * @param className The fully qualified class name for which to return the
     * class format.
     *
     * @return The class format for the current version of the class.
     */
    public ObjectStreamClass getClassFormat(String className)
        throws IOException, ClassNotFoundException;

    /**
     * Return the ObjectStreamClass for the given class ID.  This may or may not
     * be the current class format, depending on whether the class has changed
     * since the class ID was generated.
     *
     * @param classID The class ID for which to return the class format.
     *
     * @return The class format for the given class ID, which may or may not
     * represent the current version of the class.
     */
    public ObjectStreamClass getClassFormat(byte[] classID)
        throws IOException, ClassNotFoundException;
}
