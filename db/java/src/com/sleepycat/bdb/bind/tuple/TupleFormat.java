/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TupleFormat.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.bind.tuple;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.bdb.bind.DataFormat;
import java.io.IOException;

/**
 * The format for tuple data.  In addition to identifying a tuple format
 * this class provides utility methods for use by bindings.
 *
 * @author Mark Hayes
 */
public class TupleFormat implements DataFormat {

    /**
     * Creates a tuple format.
     */
    public TupleFormat() {
    }

    /**
     * Utility method for use by bindings to create a tuple output object.
     *
     * @return a new tuple output object.
     */
    public final TupleOutput newOutput() {

        return new TupleOutput();
    }

    /**
     * Utility method to set the data in a data buffer to the data in a tuple
     * output object.
     *
     * @param output is the source tuple output object.
     *
     * @param data is the destination data buffer.
     */
    public final void outputToData(TupleOutput output, DataBuffer data) {

        data.setData(output.getBufferBytes(), output.getBufferOffset(),
                     output.getBufferLength());
    }

    /**
     * Utility method to set the data in a data buffer to the data in a tuple
     * input object.  The tuple input for a buffer is cached in the buffer's
     * data formation property.
     *
     * @param input is the source tuple input object.
     *
     * @param data is the destination data buffer.
     */
    public final void inputToData(TupleInput input, DataBuffer data) {

        data.setData(input.getBufferBytes(), input.getBufferOffset(),
                     input.getBufferLength());
        data.setDataFormation(input);
    }

    /**
     * Utility method to create a new tuple input object for reading the data
     * from a given buffer.  The tuple input for a buffer is cached in the
     * buffer's data formation property.  If an existing input is reused, it
     * is reset before returning it.
     *
     * @param data is the source data buffer.
     *
     * @return the new tuple input object.
     */
    public final TupleInput dataToInput(DataBuffer data) {

        TupleInput input = (TupleInput) data.getDataFormation();
        if (input == null) {
            input = new TupleInput(data.getDataBytes(), data.getDataOffset(),
                                   data.getDataLength());
            data.setDataFormation(input);
        } else {
            input.reset();
        }
        return input;
    }
}
