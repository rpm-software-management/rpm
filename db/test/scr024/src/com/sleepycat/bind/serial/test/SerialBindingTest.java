/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SerialBindingTest.java,v 1.3 2004/06/04 18:26:00 mark Exp $
 */

package com.sleepycat.bind.serial.test;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import com.sleepycat.bind.EntityBinding;
import com.sleepycat.bind.serial.ClassCatalog;
import com.sleepycat.bind.serial.SerialBinding;
import com.sleepycat.bind.serial.SerialSerialBinding;
import com.sleepycat.bind.serial.TupleSerialMarshalledBinding;
import com.sleepycat.collections.test.DbTestUtil;
import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.util.ExceptionUnwrapper;

/**
 * @author Mark Hayes
 */
public class SerialBindingTest extends TestCase {

    private ClassCatalog catalog;
    private DatabaseEntry buffer;
    private DatabaseEntry keyBuffer;
    private DatabaseEntry indexKeyBuffer;

    public static void main(String[] args)
        throws Exception {

        junit.framework.TestResult tr =
            junit.textui.TestRunner.run(suite());
        if (tr.errorCount() > 0 ||
            tr.failureCount() > 0) {
            System.exit(1);
        } else {
            System.exit(0);
        }
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite(SerialBindingTest.class);
        return suite;
    }

    public SerialBindingTest(String name) {

        super(name);
    }

    public void setUp() {

        DbTestUtil.printTestName("SerialBindingTest." + getName());
        catalog = new TestClassCatalog();
        buffer = new DatabaseEntry();
        keyBuffer = new DatabaseEntry();
        indexKeyBuffer = new DatabaseEntry();
    }

    public void tearDown() {

        /* Ensure that GC can cleanup. */
        catalog = null;
        buffer = null;
        keyBuffer = null;
        indexKeyBuffer = null;
    }

    public void runTest()
        throws Throwable {

        try {
            super.runTest();
        } catch (Exception e) {
            throw ExceptionUnwrapper.unwrap(e);
        }
    }

    private void primitiveBindingTest(Object val) {

        Class cls = val.getClass();
        SerialBinding binding = new SerialBinding(catalog, cls);

        binding.objectToEntry(val, buffer);
        assertTrue(buffer.getSize() > 0);

        Object val2 = binding.entryToObject(buffer);
        assertSame(cls, val2.getClass());
        assertEquals(val, val2);

        Object valWithWrongCls = (cls == String.class)
                      ? ((Object) new Integer(0)) : ((Object) new String(""));
        try {
            binding.objectToEntry(valWithWrongCls, buffer);
        } catch (IllegalArgumentException expected) {}
    }

    public void testPrimitiveBindings() {

        primitiveBindingTest("abc");
        primitiveBindingTest(new Character('a'));
        primitiveBindingTest(new Boolean(true));
        primitiveBindingTest(new Byte((byte) 123));
        primitiveBindingTest(new Short((short) 123));
        primitiveBindingTest(new Integer(123));
        primitiveBindingTest(new Long(123));
        primitiveBindingTest(new Float(123.123));
        primitiveBindingTest(new Double(123.123));
    }

    public void testNullObjects() {

        SerialBinding binding = new SerialBinding(catalog, null);
        buffer.setSize(0);
        binding.objectToEntry(null, buffer);
        assertTrue(buffer.getSize() > 0);
        assertEquals(null, binding.entryToObject(buffer));
    }

    public void testSerialSerialBinding() {

        SerialBinding keyBinding = new SerialBinding(catalog, String.class);
        SerialBinding valueBinding = new SerialBinding(catalog, String.class);
        EntityBinding binding = new MySerialSerialBinding(keyBinding,
                                                          valueBinding);

        String val = "key#value?indexKey";
        binding.objectToData(val, buffer);
        assertTrue(buffer.getSize() > 0);
        binding.objectToKey(val, keyBuffer);
        assertTrue(keyBuffer.getSize() > 0);

        Object result = binding.entryToObject(keyBuffer, buffer);
        assertEquals(val, result);
    }

    // also tests TupleSerialBinding since TupleSerialMarshalledBinding extends
    // it
    public void testTupleSerialMarshalledBinding() {

        SerialBinding valueBinding = new SerialBinding(catalog,
                                                    MarshalledObject.class);
        EntityBinding binding =
            new TupleSerialMarshalledBinding(valueBinding);

        MarshalledObject val = new MarshalledObject("abc", "primary",
                                                    "index1", "index2");
        binding.objectToData(val, buffer);
        assertTrue(buffer.getSize() > 0);
        binding.objectToKey(val, keyBuffer);
        assertEquals(val.expectedKeyLength(), keyBuffer.getSize());

        Object result = binding.entryToObject(keyBuffer, buffer);
        assertTrue(result instanceof MarshalledObject);
        val = (MarshalledObject) result;
        assertEquals("abc", val.getData());
        assertEquals("primary", val.getPrimaryKey());
        assertEquals("index1", val.getIndexKey1());
        assertEquals("index2", val.getIndexKey2());
    }

    private static class MySerialSerialBinding extends SerialSerialBinding {

        private MySerialSerialBinding(SerialBinding keyBinding,
                                      SerialBinding valueBinding) {

            super(keyBinding, valueBinding);
        }

        public Object entryToObject(Object keyInput, Object valueInput) {

            return "" + keyInput + '#' + valueInput;
        }

        public Object objectToKey(Object object) {

            String s = (String) object;
            int i = s.indexOf('#');
            if (i < 0 || i == s.length() - 1) {
                throw new IllegalArgumentException(s);
            } else {
                return s.substring(0, i);
            }
        }

        public Object objectToData(Object object) {

            String s = (String) object;
            int i = s.indexOf('#');
            if (i < 0 || i == s.length() - 1) {
                throw new IllegalArgumentException(s);
            } else {
                return s.substring(i + 1);
            }
        }
    }
}

