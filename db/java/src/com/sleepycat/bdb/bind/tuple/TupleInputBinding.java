/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleInputBinding.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
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
 * A concrete tuple binding for keys or values which are {@link TupleInput}
 * objects.  This binding is used when tuples themselves are the data object,
 * rather than using application defined objects. A {@link TupleInput} must
 * always be used.  To convert a {@link TupleOutput} to a {@link TupleInput},
 * use the {@link TupleInput#TupleInput(TupleOutput)} constructor.
 *
 * @author Mark Hayes
 */
public class TupleInputBinding implements DataBinding {

    protected TupleFormat format;

    /**
     * Creates a tuple input binding.
     *
     * @param format is the format of the new binding.
     */
    public TupleInputBinding(TupleFormat format) {

        this.format = format;
    }

    // javadoc is inherited
    public Object dataToObject(DataBuffer data)
        throws IOException {

        return format.dataToInput(data);
    }

    // javadoc is inherited
    public void objectToData(Object object, DataBuffer data)
        throws IOException {

        format.inputToData((TupleInput) object, data);
    }

    // javadoc is inherited
    public DataFormat getDataFormat() {

        return format;
    }
}
