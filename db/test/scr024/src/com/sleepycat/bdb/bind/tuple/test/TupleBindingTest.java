/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleBindingTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple.test;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.bind.KeyExtractor;
import com.sleepycat.bdb.bind.SimpleBuffer;
import com.sleepycat.bdb.bind.tuple.TupleBinding;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleInputBinding;
import com.sleepycat.bdb.bind.tuple.TupleMarshalledBinding;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import com.sleepycat.bdb.bind.tuple.TupleTupleMarshalledBinding;
import com.sleepycat.bdb.bind.tuple.TupleTupleMarshalledKeyExtractor;
import com.sleepycat.bdb.util.ExceptionUnwrapper;
import java.io.IOException;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class TupleBindingTest extends TestCase {

    private TupleFormat format;
    private TupleFormat keyFormat;
    private TupleFormat indexKeyFormat;
    private DataBuffer buffer;
    private DataBuffer keyBuffer;
    private DataBuffer indexKeyBuffer;

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite(TupleBindingTest.class);
        return suite;
    }

    public TupleBindingTest(String name) {

        super(name);
    }

    public void setUp() {

        System.out.println("TupleBindingTest." + getName());
        format = new TupleFormat();
        keyFormat = new TupleFormat();
        indexKeyFormat = new TupleFormat();
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

    private void primitiveBindingTest(Object val, int byteSize)
        throws IOException {

        Class cls = val.getClass();
        DataBinding binding = TupleBinding.getPrimitiveBinding(cls, format);
        assertSame(format, binding.getDataFormat());

        binding.objectToData(val, buffer);
        assertEquals(byteSize, buffer.getDataLength());

        Object val2 = binding.dataToObject(buffer);
        assertSame(cls, val2.getClass());
        assertEquals(val, val2);

        Object valWithWrongCls = (cls == String.class)
                      ? ((Object) new Integer(0)) : ((Object) new String(""));
        try {
            binding.objectToData(valWithWrongCls, buffer);
        }
        catch (ClassCastException expected) {}
    }

    public void testPrimitiveBindings()
        throws IOException {

        primitiveBindingTest("abc", 4);
        primitiveBindingTest(new Character('a'), 2);
        primitiveBindingTest(new Boolean(true), 1);
        primitiveBindingTest(new Byte((byte) 123), 1);
        primitiveBindingTest(new Short((short) 123), 2);
        primitiveBindingTest(new Integer(123), 4);
        primitiveBindingTest(new Long(123), 8);
        primitiveBindingTest(new Float(123.123), 4);
        primitiveBindingTest(new Double(123.123), 8);
    }

    public void testTupleInputBinding()
        throws IOException {

        DataBinding binding = new TupleInputBinding(format);
        assertSame(format, binding.getDataFormat());

        TupleOutput out = new TupleOutput();
        out.writeString("abc");
        binding.objectToData(new TupleInput(out), buffer);
        assertEquals(4, buffer.getDataLength());

        Object result = binding.dataToObject(buffer);
        assertTrue(result instanceof TupleInput);
        TupleInput in = (TupleInput) result;
        assertEquals("abc", in.readString());
        assertEquals(0, in.available());
    }

    // also tests TupleBinding since TupleMarshalledBinding extends it
    public void testTupleMarshalledBinding()
        throws IOException {

        DataBinding binding =
            new TupleMarshalledBinding(format, MarshalledObject.class);
        assertSame(format, binding.getDataFormat());

        MarshalledObject val = new MarshalledObject("abc", "", "", "");
        binding.objectToData(val, buffer);
        assertEquals(val.expectedDataLength(), buffer.getDataLength());

        Object result = binding.dataToObject(buffer);
        assertTrue(result instanceof MarshalledObject);
        val = (MarshalledObject) result;
        assertEquals("abc", val.getData());
    }

    // also tests TupleTupleBinding since TupleTupleMarshalledBinding extends
    // it
    public void testTupleTupleMarshalledBinding()
        throws IOException {

        EntityBinding binding =
            new TupleTupleMarshalledBinding(keyFormat, format,
                                            MarshalledObject.class);
        assertSame(format, binding.getValueFormat());
        assertSame(keyFormat, binding.getKeyFormat());

        MarshalledObject val = new MarshalledObject("abc", "primary",
                                                    "index1", "index2");
        binding.objectToValue(val, buffer);
        assertEquals(val.expectedDataLength(), buffer.getDataLength());
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

    // also tests TupleTupleKeyExtractor since TupleTupleMarshalledKeyExtractor
    // extends it
    public void testTupleTupleMarshalledKeyExtractor()
        throws IOException {

        TupleTupleMarshalledBinding binding =
            new TupleTupleMarshalledBinding(keyFormat, format,
                                            MarshalledObject.class);
        KeyExtractor extractor =
            new TupleTupleMarshalledKeyExtractor(binding, indexKeyFormat, "1",
                                                 false, true);
        assertSame(format, extractor.getValueFormat());
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
}

