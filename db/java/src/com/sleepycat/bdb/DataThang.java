/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataThang.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.DataBuffer;
import com.sleepycat.db.Db;
import com.sleepycat.db.Dbt;
import java.io.ByteArrayInputStream;
import java.io.PrintStream;

/**
 * (<em>internal</em>) An extension of a Berkeley DB thang (Dbt) that supports
 * the {@link DataBuffer} interface for bindings and other added utilities.
 *
 * <p><b>NOTE:</b> This classes is internal and may be changed incompatibly or
 * deleted in the future.  It is public only so it may be used by
 * subpackages.</p>
 *
 * @author Mark Hayes
 */
public final class DataThang extends Dbt implements DataBuffer {

    private static DataThang discardDataThang;

    static DataThang getDiscardDataThang() {

        if (discardDataThang == null) {
            discardDataThang = new DataThang();
            discardDataThang.setFlags(Db.DB_DBT_USERMEM | Db.DB_DBT_PARTIAL);
            discardDataThang.set_data(new byte[0]);
        }
        return discardDataThang;
    }

    private Object formation;

    /**
     * Creates a thang with no data.
     */
    public DataThang() {

        setFlags(Db.DB_DBT_MALLOC);
    }

    /**
     * Creates a thang containing the given data data.
     *
     * @param bytes the initial data.
     */
    public DataThang(byte[] bytes) {

        this();
        setBytes(bytes);
    }

    /**
     * Creates a thang with a copy of the data from the given thang.
     *
     * @param copyFrom a data thang to clone.
     */
    public DataThang(DataThang copyFrom) {

        this();
        copy(copyFrom);
    }

    /**
     * Copies the data from the given thang to this thang..
     *
     * @param copyFrom a data thang to clone.
     */
    public void copy(DataThang copyFrom) {

        setBytes(copyFrom.getBytes());
    }

    /**
     * Sets the data for this thang.
     *
     * @param bytes the new data.
     */
    public void setBytes(byte[] bytes) {

        formation = null;

        set_data(bytes);
        set_offset(0);
        set_size((bytes != null) ? bytes.length : 0);
    }

    /**
     * Sets the data for this thang.
     *
     * @param bytes the new data.
     *
     * @param offset the new data offset.
     *
     * @param length the new data length.
     */
    public void setBytes(byte[] bytes, int offset, int length) {

        formation = null;

        set_data(bytes);
        set_offset(offset);
        set_size(length);
    }

    /**
     * Returns the data for this thang.
     *
     * @return the data for this thang.
     */
    public byte[] getBytes() {

        byte[] bytes = get_data();
        if (bytes == null) return null;
        int size = get_size();
        byte[] data = new byte[size];
        System.arraycopy(bytes, get_offset(), data, 0, size);
        return data;
    }

    /**
     * Returns the data for this thang as a byte array input stream..
     *
     * @return the data as a stream.
     */
    public ByteArrayInputStream getByteStream() {

        return new ByteArrayInputStream(get_data(), get_offset(), get_size());
    }

    /**
     * Increments the data value for this thang, treating the byte array as
     * a Java BigInteger where the bytes are in MSB-first order.  The byte
     * array will be increased in size if necessary.
     */
    public void increment() {

        formation = null;

        byte[] data = get_data();
        int offset = get_offset();
        int size = get_size();

        // allocate one extra byte, in case we need it below

        byte[] newData = new byte[size + 1];
        System.arraycopy(data, offset, newData, 0, size);

        // find right-most non-0xFF byte and increment it

        for (int i = offset + size - 1; i >= offset; i--) {
            int val = 0xFF & data[i];
            if (val != 0xFF) {
                newData[i] = (byte) (val + 1);

                // only use current size, not the extra zero byte

                setBytes(newData, 0, size);
                return;
            }
        }

        // if filled with 0xFF, we have to include the
        // extra zero byte at the end

        setBytes(newData);
    }

    /**
     * Returns -1 if the byte array of this thang is less than that of the
     * given thang, 0 if they are equal, or 1 if greater.  The comparison is
     * performed by treating the bytes as unsigned integers to match the
     * Berkeley DB default key comparison algorithm.
     *
     * @param key2 the key to compare.
     *
     * @return the comparison result.
     */
    public int compareTo(Dbt key2) {

        byte[] d1 = this.get_data();
        int o1 = this.get_offset();
        int s1 = this.get_size();

        byte[] d2 = key2.get_data();
        int o2 = key2.get_offset();
        int s2 = key2.get_size();

        for (int i = 0; i < s1 && i < s2; i++) {

            int b1 = 0xFF & d1[o1 + i];
            int b2 = 0xFF & d2[o2 + i];
            if (b1 < b2)
                return -1;
            else if (b1 > b2)
                return 1;
        }

        if (s1 < s2)
            return -1;
        else if (s1 > s2)
            return 1;
        else
            return 0;
    }

    /**
     * Returns whether the byte array of this thang is equal to that of the
     * given thang.
     *
     * @param other the thang to compare.
     *
     * @return whether their data is equal.
     */
    public boolean equals(Dbt other) {

        return compareTo(other) == 0;
    }

    /**
     * Prints the byte array of this thing to the given stream using toString()
     * to convert the bytes to a string.
     *
     * @param out the stream to write to.
     */
    public void dump(PrintStream out) {

        dump(this, out);
    }

    /**
     * Prints the byte array of the given thing to the given stream using
     * toString() to convert the bytes to a string.
     *
     * @param dbt the data thang to dump.
     *
     * @param out the stream to write to.
     */
    public static void dump(Dbt dbt, PrintStream out) {

        out.print(' ');
        out.print(toString(dbt));
        out.println();
    }

    /**
     * Converts the byte array of this thang to space-separated integers,
     * and suffixed by the record number if applicable.
     *
     * @param dbt the thang to convert.
     *
     * @param the resulting string.
     */
    public static String toString(Dbt dbt) {

        int len = dbt.get_offset() + dbt.get_size();
        StringBuffer buf = new StringBuffer(len * 2);
        byte[] data = dbt.get_data();
        for (int i = dbt.get_offset(); i < len; i++) {
            String num = Integer.toHexString(data[i]);
            if (num.length() < 2) buf.append('0');
            buf.append(Integer.toHexString(data[i]));
        }
        if (dbt.get_size() == 4) {
            buf.append(" (recno ");
            buf.append(dbt.get_recno_key_data());
            buf.append(')');
        }
        return buf.toString();
    }

    /**
     * Converts the byte array of this thang to space-separated integers,
     * and suffixed by the record number if applicable.
     *
     * @param the resulting string.
     */
    public String toString() {

        return toString(this);
    }

    // ------- DataBuffer implementation

    // javadoc is inherited
    public void setData(byte[] data, int offset, int length) {

        formation = null;

        set_data(data);
        set_offset(offset);
        set_size(length);
    }

    // javadoc is inherited
    public byte[] getDataBytes() {

        return get_data();
    }

    // javadoc is inherited
    public int getDataOffset() {

        return get_offset();
    }

    // javadoc is inherited
    public int getDataLength() {

        return get_size();
    }

    // javadoc is inherited
    public void setDataFormation(Object formation) {

        this.formation = formation;
    }

    // javadoc is inherited
    public Object getDataFormation() {

        return formation;
    }

    // javadoc is inherited
    public void clearDataFormation() {

        formation = null;
    }
}
