/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: SerialBindingTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.serial.test;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.KeyExtractor;
import com.sleepycat.bdb.bind.SimpleBuffer;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.serial.ClassCatalog;
import com.sleepycat.bdb.bind.serial.SerialBinding;
import com.sleepycat.bdb.bind.serial.SerialFormat;
import com.sleepycat.bdb.bind.serial.SerialInput;
import com.sleepycat.bdb.bind.serial.SerialSerialBinding;
import com.sleepycat.bdb.bind.serial.SerialSerialKeyExtractor;
import com.sleepycat.bdb.bind.serial.TupleSerialMarshalledBinding;
import com.sleepycat.bdb.bind.serial.TupleSerialMarshalledKeyExtractor;
import com.sleepycat.bdb.util.ExceptionUnwrapper;
import java.io.IOException;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class SerialBindingTest extends TestCase {

    private ClassCatalog catalog;
    private DataBuffer buffer;
    private DataBuffer keyBuffer;
    private DataBuffer indexKeyBuffer;

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
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

        System.out.println("SerialBindingTest." + getName());
        catalog = new TestClassCatalog();
        buffer = new SimpleBuffer();
        keyBuffer = new SimpleBuffer();
        indexKeyBuffer = new SimpleBuffer();
    }

    public void runTest()
        throws Throwable {

        try {
            super.runTest();
        } catch (Exception e) {
            throw ExceptionUnwrapper.unwrap(e);
        }
    }

    private void primitiveBindingTest(Object val)
        throws IOException {

        Class cls = val.getClass();
        SerialFormat format = new SerialFormat(catalog, cls);
        DataBinding binding = new SerialBinding(format);
        assertSame(format, binding.getDataFormat());

        binding.objectToData(val, buffer);
        assertTrue(buffer.getDataLength() > 0);

        Object val2 = binding.dataToObject(buffer);
        assertSame(cls, val2.getClass());
        assertEquals(val, val2);

        Object valWithWrongCls = (cls == String.class)
                      ? ((Object) new Integer(0)) : ((Object) new String(""));
        try {
            binding.objectToData(valWithWrongCls, buffer);
        } catch (IllegalArgumentException expected) {}
    }

    public void testPrimitiveBindings()
        throws IOException {

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

    public void testSerialSerialBinding()
        throws IOException {

        SerialFormat keyFormat = new SerialFormat(catalog, String.class);
        SerialFormat valueFormat = new SerialFormat(catalog, String.class);
        EntityBinding binding = new MySerialSerialBinding(keyFormat,
                                                          valueFormat);
        assertSame(keyFormat, binding.getKeyFormat());
        assertSame(valueFormat, binding.getValueFormat());

        String val = "key#value?indexKey";
        binding.objectToValue(val, buffer);
        assertTrue(buffer.getDataLength() > 0);
        binding.objectToKey(val, keyBuffer);
        assertTrue(keyBuffer.getDataLength() > 0);

        Object result = binding.dataToObject(keyBuffer, buffer);
        assertEquals(val, result);
    }

    public void testSerialSerialKeyExtractor()
        throws IOException {

        SerialFormat keyFormat = new SerialFormat(catalog, String.class);
        SerialFormat valueFormat = new SerialFormat(catalog, String.class);
        SerialFormat indexKeyFormat = new SerialFormat(catalog, String.class);
        EntityBinding binding = new MySerialSerialBinding(keyFormat,
                                                          valueFormat);
        KeyExtractor extractor = new MySerialSerialExtractor(keyFormat,
                                                             valueFormat,
                                                             indexKeyFormat);
        assertSame(keyFormat, extractor.getPrimaryKeyFormat());
        assertSame(valueFormat, extractor.getValueFormat());
        assertSame(indexKeyFormat, extractor.getIndexKeyFormat());

        String val = "key#value?indexKey";
        binding.objectToValue(val, buffer);
        binding.objectToKey(val, keyBuffer);

        extractor.extractIndexKey(keyBuffer, buffer, indexKeyBuffer);
        assertEquals("indexKey", indexKeyFormat.dataToObject(indexKeyBuffer));

        extractor.clearIndexKey(buffer);
        extractor.extractIndexKey(keyBuffer, buffer, indexKeyBuffer);
        assertEquals(0, indexKeyBuffer.getDataLength());
    }

    // also tests TupleSerialBinding since TupleSerialMarshalledBinding extends
    // it
    public void testTupleSerialMarshalledBinding()
        throws IOException {

        TupleFormat keyFormat = new TupleFormat();
        SerialFormat valueFormat = new SerialFormat(catalog,
                                                    MarshalledObject.class);
        TupleFormat indexKeyFormat = new TupleFormat();

        EntityBinding binding =
            new TupleSerialMarshalledBinding(keyFormat, valueFormat);
        assertSame(valueFormat, binding.getValueFormat());
        assertSame(keyFormat, binding.getKeyFormat());

        MarshalledObject val = new MarshalledObject("abc", "primary",
                                                    "index1", "index2");
        binding.objectToValue(val, buffer);
        assertTrue(buffer.getDataLength() > 0);
        binding.objectToKey(val, keyBuffer);
        assertEquals(val.expectedKeyLength(), keyBuffer.getDataLength());

        Object result = binding.dataToObject(keyBuffer, buffer);
        assertTrue(result instanceof MarshalledObject);
        val = (MarshalledObject) result;
        assertEquals("abc", val.getData());
        assertEquals("primary", val.getPrimaryKey());
        assertEquals("index1", val.getIndexKey1());
        assertEquals("index2", val.getIndexKey2());
    }

    // also tests TupleSerialKeyExtractor since
    // TupleSerialMarshalledKeyExtractor extends it
    public void testTupleSerialMarshalledKeyExtractor()
        throws IOException {

        TupleFormat keyFormat = new TupleFormat();
        SerialFormat valueFormat = new SerialFormat(catalog,
                                                    MarshalledObject.class);
        TupleFormat indexKeyFormat = new TupleFormat();
        TupleSerialMarshalledBinding binding =
            new TupleSerialMarshalledBinding(keyFormat, valueFormat);

        KeyExtractor extractor =
            new TupleSerialMarshalledKeyExtractor(binding, indexKeyFormat, "1",
                                                  false, true);
        assertSame(valueFormat, extractor.getValueFormat());
        assertNull(extractor.getPrimaryKeyFormat());
        assertSame(indexKeyFormat, extractor.getIndexKeyFormat());

        MarshalledObject val = new MarshalledObject("abc", "primary",
                                                    "index1", "index2");
        binding.objectToValue(val, buffer);
        binding.objectToKey(val, keyBuffer);

        extractor.extractIndexKey(keyBuffer, buffer, indexKeyBuffer);
        TupleInput in = indexKeyFormat.dataToInput(indexKeyBuffer);
        assertEquals("index1", in.readString());

        extractor.clearIndexKey(buffer);
        extractor.extractIndexKey(keyBuffer, buffer, indexKeyBuffer);
        assertEquals(0, indexKeyBuffer.getDataLength());
    }

    private static class MySerialSerialBinding extends SerialSerialBinding {

        private MySerialSerialBinding(SerialFormat keyFormat,
                                      SerialFormat valueFormat) {

            super(keyFormat, valueFormat);
        }

        public Object dataToObject(Object keyInput, Object valueInput)
            throws IOException {

            return "" + keyInput + '#' + valueInput;
        }

        public Object objectToKey(Object object)
            throws IOException {

            String s = (String) object;
            int i = s.indexOf('#');
            if (i < 0 || i == s.length() - 1)
                throw new IllegalArgumentException(s);
            else
                return s.substring(0, i);
        }

        public Object objectToValue(Object object)
            throws IOException {

            String s = (String) object;
            int i = s.indexOf('#');
            if (i < 0 || i == s.length() - 1)
                throw new IllegalArgumentException(s);
            else
                return s.substring(i + 1);
        }
    }

    private static class MySerialSerialExtractor
        extends SerialSerialKeyExtractor {

        private MySerialSerialExtractor(SerialFormat primaryKeyFormat,
                                        SerialFormat valueFormat,
                                        SerialFormat indexKeyFormat) {

            super(primaryKeyFormat, valueFormat, indexKeyFormat);
        }

        public Object extractIndexKey(Object primaryKeyInput,
                                      Object valueInput)
            throws IOException {

            String s = (String) valueInput;
            int i = s.indexOf('?');
            if (i < 0 || i == s.length() - 1)
                return null;
            else
                return s.substring(i + 1);
        }

        public Object clearIndexKey(Object valueData)
            throws IOException {

            String s = (String) valueData;
            int i = s.indexOf('?');
            if (i < 0 || i == s.length() - 1)
                return null;
            else
                return s.substring(0, i);
        }
    }
}

