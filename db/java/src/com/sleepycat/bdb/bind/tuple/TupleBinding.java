/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleBinding.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.tuple.TupleFormat;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import java.io.IOException;

/**
 * An abstract tuple binding for tuple keys or values. This class takes care of
 * converting the data to/from {@link TupleInput} and {@link TupleOutput}
 * objects.  Its two abstract methods must be implemented by a concrete
 * subclass to convert between tuples and key or value objects.
 * <ul>
 * <li> {@link #dataToObject(TupleInput)} </li>
 * <li> {@link #objectToData(Object,TupleOutput)} </li>
 * </ul>
 *
 * <p>For keys or values which are Java primitive classes (String, Integer,
 * etc) {@link #getPrimitiveBinding} may be used to return a builtin tuple
 * binding.  A custom tuple binding for these types is not needed.</p>
 *
 * @author Mark Hayes
 */
public abstract class TupleBinding implements DataBinding {

    protected TupleFormat format;

    /**
     * Creates a tuple binding.
     *
     * @param format is the format of the new binding.
     */
    public TupleBinding(TupleFormat format) {

        this.format = format;
    }

    // javadoc is inherited
    public Object dataToObject(DataBuffer data)
        throws IOException {

        return dataToObject(format.dataToInput(data));
    }

    // javadoc is inherited
    public void objectToData(Object object, DataBuffer data)
        throws IOException {

        TupleOutput output = format.newOutput();
        objectToData(object, output);
        format.outputToData(output, data);
    }

    // javadoc is inherited
    public DataFormat getDataFormat() {

        return format;
    }

    /**
     * Constructs a key or value object from {@link TupleInput} data.
     *
     * @param input is the tuple key or value data.
     *
     * @return the key or value object constructed from the data.
     */
    public abstract Object dataToObject(TupleInput input)
        throws IOException;

    /**
     * Converts a key or value object to a tuple data.
     *
     * @param object is the key or value object.
     *
     * @param output is the tuple data to which the key or value should be
     * written.
     */
    public abstract void objectToData(Object object, TupleOutput output)
        throws IOException;

    /**
     * Creates a tuple binding for a primitive Java class.  The following
     * Java classes are supported.
     * <ul>
     * <li><code>String</code></li>
     * <li><code>Integer</code></li>
     * </ul>
     *
     * @param cls is the primitive Java class.
     *
     * @param format is the tuple format for the new binding.
     *
     * @return a new building for the primitive class or null if the cls
     * parameter is not one of the supported classes.
     */
    public static TupleBinding getPrimitiveBinding(Class cls,
                                                   TupleFormat format) {

        if (cls == String.class)
            return new StringBinding(format);
        else if (cls == Character.class)
            return new CharacterBinding(format);
        else if (cls == Boolean.class)
            return new BooleanBinding(format);
        else if (cls == Byte.class)
            return new ByteBinding(format);
        else if (cls == Short.class)
            return new ShortBinding(format);
        else if (cls == Integer.class)
            return new IntegerBinding(format);
        else if (cls == Long.class)
            return new LongBinding(format);
        else if (cls == Float.class)
            return new FloatBinding(format);
        else if (cls == Double.class)
            return new DoubleBinding(format);
        else
            return null;
    }

    private static class StringBinding extends TupleBinding {

        public StringBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return input.readString();
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeString((String) object);
        }
    }

    private static class CharacterBinding extends TupleBinding {

        public CharacterBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return new Character(input.readChar());
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeChar(((Character) object).charValue());
        }
    }

    private static class BooleanBinding extends TupleBinding {

        public BooleanBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return new Boolean(input.readBoolean());
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeBoolean(((Boolean) object).booleanValue());
        }
    }

    private static class ByteBinding extends TupleBinding {

        public ByteBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return new Byte(input.readByte());
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeByte(((Number) object).byteValue());
        }
    }

    private static class ShortBinding extends TupleBinding {

        public ShortBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return new Short(input.readShort());
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeShort(((Number) object).shortValue());
        }
    }

    private static class IntegerBinding extends TupleBinding {

        public IntegerBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return new Integer(input.readInt());
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeInt(((Number) object).intValue());
        }
    }

    private static class LongBinding extends TupleBinding {

        public LongBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return new Long(input.readLong());
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeLong(((Number) object).longValue());
        }
    }

    private static class FloatBinding extends TupleBinding {

        public FloatBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return new Float(input.readFloat());
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeFloat(((Number) object).floatValue());
        }
    }

    private static class DoubleBinding extends TupleBinding {

        public DoubleBinding(TupleFormat format) {

            super(format);
        }

        public Object dataToObject(TupleInput input)
            throws IOException {

            return new Double(input.readDouble());
        }

        public void objectToData(Object object, TupleOutput output)
            throws IOException {

            output.writeDouble(((Number) object).doubleValue());
        }
    }
}
