/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleBindingTest.java,v 1.4 2004/06/29 06:06:19 mark Exp $
 */

package com.sleepycat.bind.tuple.test;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import com.sleepycat.bind.EntityBinding;
import com.sleepycat.bind.EntryBinding;
import com.sleepycat.bind.tuple.BooleanBinding;
import com.sleepycat.bind.tuple.ByteBinding;
import com.sleepycat.bind.tuple.CharacterBinding;
import com.sleepycat.bind.tuple.DoubleBinding;
import com.sleepycat.bind.tuple.FloatBinding;
import com.sleepycat.bind.tuple.IntegerBinding;
import com.sleepycat.bind.tuple.LongBinding;
import com.sleepycat.bind.tuple.ShortBinding;
import com.sleepycat.bind.tuple.StringBinding;
import com.sleepycat.bind.tuple.TupleBinding;
import com.sleepycat.bind.tuple.TupleInput;
import com.sleepycat.bind.tuple.TupleInputBinding;
import com.sleepycat.bind.tuple.TupleMarshalledBinding;
import com.sleepycat.bind.tuple.TupleOutput;
import com.sleepycat.bind.tuple.TupleTupleMarshalledBinding;
import com.sleepycat.collections.test.DbTestUtil;
import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.util.ExceptionUnwrapper;

/**
 * @author Mark Hayes
 */
public class TupleBindingTest extends TestCase {

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

        TestSuite suite = new TestSuite(TupleBindingTest.class);
        return suite;
    }

    public TupleBindingTest(String name) {

        super(name);
    }

    public void setUp() {

        DbTestUtil.printTestName("TupleBindingTest." + getName());
        buffer = new DatabaseEntry();
        keyBuffer = new DatabaseEntry();
        indexKeyBuffer = new DatabaseEntry();
    }

    public void tearDown() {

        /* Ensure that GC can cleanup. */
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

    private void primitiveBindingTest(Object val, int byteSize) {

        Class cls = val.getClass();
        EntryBinding binding = TupleBinding.getPrimitiveBinding(cls);

        binding.objectToEntry(val, buffer);
        assertEquals(byteSize, buffer.getSize());

        Object val2 = binding.entryToObject(buffer);
        assertSame(cls, val2.getClass());
        assertEquals(val, val2);

        Object valWithWrongCls = (cls == String.class)
                      ? ((Object) new Integer(0)) : ((Object) new String(""));
        try {
            binding.objectToEntry(valWithWrongCls, buffer);
        }
        catch (ClassCastException expected) {}
    }

    public void testPrimitiveBindings() {

        primitiveBindingTest("abc", 4);
        primitiveBindingTest(new Character('a'), 2);
        primitiveBindingTest(new Boolean(true), 1);
        primitiveBindingTest(new Byte((byte) 123), 1);
        primitiveBindingTest(new Short((short) 123), 2);
        primitiveBindingTest(new Integer(123), 4);
        primitiveBindingTest(new Long(123), 8);
        primitiveBindingTest(new Float(123.123), 4);
        primitiveBindingTest(new Double(123.123), 8);

        DatabaseEntry entry = new DatabaseEntry();

        StringBinding.stringToEntry("abc", entry);
	assertEquals(4, entry.getData().length);
        assertEquals("abc", StringBinding.entryToString(entry));

        new StringBinding().objectToEntry("abc", entry);
	assertEquals(4, entry.getData().length);

        StringBinding.stringToEntry(null, entry);
	assertEquals(2, entry.getData().length);
        assertEquals(null, StringBinding.entryToString(entry));

        new StringBinding().objectToEntry(null, entry);
	assertEquals(2, entry.getData().length);

        CharacterBinding.charToEntry('a', entry);
	assertEquals(2, entry.getData().length);
        assertEquals('a', CharacterBinding.entryToChar(entry));

        new CharacterBinding().objectToEntry(new Character('a'), entry);
	assertEquals(2, entry.getData().length);

        BooleanBinding.booleanToEntry(true, entry);
	assertEquals(1, entry.getData().length);
        assertEquals(true, BooleanBinding.entryToBoolean(entry));

        new BooleanBinding().objectToEntry(Boolean.TRUE, entry);
	assertEquals(1, entry.getData().length);

        ByteBinding.byteToEntry((byte) 123, entry);
	assertEquals(1, entry.getData().length);
        assertEquals((byte) 123, ByteBinding.entryToByte(entry));

        ShortBinding.shortToEntry((short) 123, entry);
	assertEquals(2, entry.getData().length);
        assertEquals((short) 123, ShortBinding.entryToShort(entry));

        new ByteBinding().objectToEntry(new Byte((byte) 123), entry);
	assertEquals(1, entry.getData().length);

        IntegerBinding.intToEntry(123, entry);
	assertEquals(4, entry.getData().length);
        assertEquals(123, IntegerBinding.entryToInt(entry));

        new IntegerBinding().objectToEntry(new Integer(123), entry);
	assertEquals(4, entry.getData().length);

        LongBinding.longToEntry(123, entry);
	assertEquals(8, entry.getData().length);
        assertEquals(123, LongBinding.entryToLong(entry));

        new LongBinding().objectToEntry(new Long(123), entry);
	assertEquals(8, entry.getData().length);

        FloatBinding.floatToEntry((float) 123.123, entry);
	assertEquals(4, entry.getData().length);
        assertTrue(((float) 123.123) == FloatBinding.entryToFloat(entry));

        new FloatBinding().objectToEntry(new Float((float) 123.123), entry);
	assertEquals(4, entry.getData().length);

        DoubleBinding.doubleToEntry(123.123, entry);
	assertEquals(8, entry.getData().length);
        assertTrue(123.123 == DoubleBinding.entryToDouble(entry));

        new DoubleBinding().objectToEntry(new Double(123.123), entry);
	assertEquals(8, entry.getData().length);
    }

    public void testTupleInputBinding() {

        EntryBinding binding = new TupleInputBinding();

        TupleOutput out = new TupleOutput();
        out.writeString("abc");
        binding.objectToEntry(new TupleInput(out), buffer);
        assertEquals(4, buffer.getSize());

        Object result = binding.entryToObject(buffer);
        assertTrue(result instanceof TupleInput);
        TupleInput in = (TupleInput) result;
        assertEquals("abc", in.readString());
        assertEquals(0, in.available());
    }

    // also tests TupleBinding since TupleMarshalledBinding extends it
    public void testTupleMarshalledBinding() {

        EntryBinding binding =
            new TupleMarshalledBinding(MarshalledObject.class);

        MarshalledObject val = new MarshalledObject("abc", "", "", "");
        binding.objectToEntry(val, buffer);
        assertEquals(val.expectedDataLength(), buffer.getSize());

        Object result = binding.entryToObject(buffer);
        assertTrue(result instanceof MarshalledObject);
        val = (MarshalledObject) result;
        assertEquals("abc", val.getData());
    }

    // also tests TupleTupleBinding since TupleTupleMarshalledBinding extends
    // it
    public void testTupleTupleMarshalledBinding() {

        EntityBinding binding =
            new TupleTupleMarshalledBinding(MarshalledObject.class);

        MarshalledObject val = new MarshalledObject("abc", "primary",
                                                    "index1", "index2");
        binding.objectToData(val, buffer);
        assertEquals(val.expectedDataLength(), buffer.getSize());
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
}

