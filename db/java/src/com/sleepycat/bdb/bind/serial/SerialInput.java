/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: SerialInput.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial;

import com.sleepycat.bdb.util.RuntimeExceptionWrapper;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectStreamClass;

/**
 * Used instead of an {@link ObjectInputStream}, which it extends, to read an
 * object stream written by the {@link SerialOutput} class.  For reading
 * objects from a database normally {@link SerialFormat} or one of the serial
 * binding classes is used.  {@link SerialInput} is used when an {@link
 * ObjectInputStream} is needed along with compact storage.  A {@link
 * ClassCatalog} must be supplied, however, to stored shared class
 * descriptions.
 *
 * @author Mark Hayes
 */
public class SerialInput extends ObjectInputStream {

    private ClassCatalog classCatalog;

    /**
     * Creates a serial input stream.
     *
     * @param in is the input stream from which compact serialized objects will
     * be read.
     *
     * @param classCatalog is the catalog containing the class descriptions
     * for the serialized objects.
     */
    public SerialInput(InputStream in, ClassCatalog classCatalog)
        throws IOException {

        super(in);

        this.classCatalog = classCatalog;
    }

    // javadoc is inherited
    protected ObjectStreamClass readClassDescriptor()
        throws IOException, ClassNotFoundException {

        try {
            byte len = readByte();
            byte[] id = new byte[len];
            readFully(id);

            return classCatalog.getClassFormat(id);
        } catch (IOException e) {
            // do not throw IOException from here since ObjectOutputStream
            // will write the exception to the stream, which causes another
            // call here, etc.
            throw new RuntimeExceptionWrapper(e);
        }
    }
}
