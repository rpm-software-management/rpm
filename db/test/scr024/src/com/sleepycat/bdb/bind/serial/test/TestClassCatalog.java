/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestClassCatalog.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial.test;

import com.sleepycat.bdb.bind.serial.ClassCatalog;
import com.sleepycat.bdb.util.IOExceptionWrapper;
import java.io.IOException;
import java.io.ObjectStreamClass;
import java.util.HashMap;

/**
 * @author Mark Hayes
 */
public class TestClassCatalog implements ClassCatalog {

    private HashMap idToDescMap = new HashMap();
    private HashMap nameToIdMap = new HashMap();
    private HashMap nameToDescMap = new HashMap();
    private int nextId = 1;

    public TestClassCatalog() {
    }

    public void close()
        throws IOException {
    }

    public synchronized byte[] getClassID(String className)
        throws IOException {

        try {
            byte[] id = (byte[]) nameToIdMap.get(className);
            if (id == null) {
                Class cls = Class.forName(className);
                ObjectStreamClass desc = ObjectStreamClass.lookup(cls);

                String strId = String.valueOf(nextId);
                id = strId.getBytes();
                nextId += 1;

                idToDescMap.put(strId, desc);
                nameToIdMap.put(className, id);
                nameToDescMap.put(className, desc);
            }
            return id;
        } catch (ClassNotFoundException e) {
            throw new IOExceptionWrapper(e);
        }
    }

    public synchronized ObjectStreamClass getClassFormat(String className)
        throws IOException {

        throw new IOException("unimplemented");
    }

    public synchronized ObjectStreamClass getClassFormat(byte[] id)
        throws IOException {

        String strId = new String(id);
        ObjectStreamClass desc = (ObjectStreamClass) idToDescMap.get(strId);
        if (desc == null) {
            throw new IOException("classID not found");
        }
        return desc;
    }
}
