/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: NullClassCatalog.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial.test;

import com.sleepycat.bdb.bind.serial.ClassCatalog;
import java.io.ObjectStreamClass;
import java.io.IOException;
import java.math.BigInteger;

/**
NullCatalog is a dummy Catalog implementation that simply
returns large (8 byte) class IDs so that ObjectOutput
can be simulated when computing a serialized size.

@author Mark Hayes
*/
class NullClassCatalog implements ClassCatalog {

    private long id = Long.MAX_VALUE;

    public void close()
        throws IOException {
    }

    public byte[] getClassID(String className)
        throws IOException {

        return BigInteger.valueOf(id--).toByteArray();
    }

    public ObjectStreamClass getClassFormat(String className)
        throws IOException, ClassNotFoundException {

        return null; // ObjectInput not supported
    }

    public ObjectStreamClass getClassFormat(byte[] classID)
        throws IOException, ClassNotFoundException {

        return null; // ObjectInput not supported
    }
}
