/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: Dbt.java,v 11.61 2003/12/03 21:28:13 bostic Exp $
 */
package com.sleepycat.db;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

/**
 *  This information describes the specific details of the Dbt class,
 *  used to encode keys and data items in a database.</p> <h3>Key/Data
 *  Pairs</h3> <p>
 *
 *  Storage and retrieval for the {@link com.sleepycat.db.Db Db}
 *  access methods are based on key/data pairs. Both key and data
 *  items are represented by Dbt objects. Key and data byte strings
 *  may refer to strings of zero length up to strings of essentially
 *  unlimited length. See <a href="{@docRoot}/../ref/am_misc/dbsizes.html">
 *  Database limits</a> for more information.</p> <p>
 *
 *  The Dbt class provides simple access to an underlying data
 *  structure, whose elements can be examined or changed using the
 *  usual <b>set</b> or <b>get</b> methods. Dbt can be subclassed,
 *  providing a way to associate with it additional data or references
 *  to other structures.</p> <p>
 *
 *  The constructors set all elements of the underlying structure to
 *  zero. The constructor with one parameter has the effect of setting
 *  all elements to zero except for the <b>data</b> and <b>size</b>
 *  elements. The constructor with three parameters has the effect of
 *  setting all elements to zero except for the <b>data</b> , <b>size
 *  </b> and <b>offset</b> elements.</p> <p>
 *
 *  In the case where the <b>flags</b> structure element is set to 0,
 *  when being provided a key or data item by the application, the
 *  Berkeley DB package expects the <b>data</b> object to be set to a
 *  byte array of <b>size</b> bytes. When returning a key/data item to
 *  the application, the Berkeley DB package will store into the <b>
 *  data</b> object a byte array of <b>size</b> bytes. During a get
 *  operation, if none of the Db.DB_DBT_MALLOC, Db.DB_DBT_REALLOC or
 *  Db.DB_DBT_USERMEM flags are specified, the operation occurs as if
 *  Db.DB_DBT_MALLOC was used.</p> <p>
 *
 *  Access to Dbt objects is not re-entrant. In particular, if
 *  multiple threads simultaneously access the same Dbt object using
 *  {@link com.sleepycat.db.Db Db} API calls, the results are
 *  undefined, and may result in a crash. One easy way to avoid
 *  problems is to use Dbt objects that are created as local variables
 *  and not shared among threads.</p>
 */
public class Dbt {

    // private data
    //
    private byte[] data = null;
    private int dlen = 0;
    private int doff = 0;
    private int flags = 0;
    private int offset = 0;
    private int size = 0;
    private int ulen = 0;


    /**
     *  Construct an empty Dbt.</p>
     */
    public Dbt() { }


    /**
     *  Construct a Dbt where the data is the contents of the array
     *  and the Dbt's length is set to the length of the array.</p>
     *
     * @param  data  the array to which the Dbt's data is set.
     */
    public Dbt(byte[] data) {
        this.data = data;
        if (data != null) {
            this.size = data.length;
        }
    }


    /**
     *  Construct a Dbt from <b>len</b> bytes from the array, starting
     *  at <b>off</b> .</p>
     *
     * @param  data  the array from which the Dbt's data is set.
     * @param  len   the length of the data in bytes.
     * @param  off   starting byte offset of the data in the array.
     */
    public Dbt(byte[] data, int off, int len) {
        this.data = data;
        this.offset = off;
        this.size = len;
    }


    /**
     *  Construct a Dbt where the data is the serialized form of the
     *  Object. The instanced passed must therefore implement the
     *  Serializable interface. The Dbt's length will be set to the
     *  length of the byte array required to store this serialized
     *  form.</p>
     *
     * @param  serialobj             the serialized form to which the
     *      Dbt's data is set.
     * @throws  java.io.IOException  If there is an error while
     *      serializing the object.
     */
    public Dbt(Object serialobj) throws java.io.IOException {

        this.setObject(serialobj);
    }


    /**
     *  Set the data array. Note that the array data is not copied
     *  immediately, but only when the Dbt is used.</p>
     *
     * @param  data  an array of bytes to be used to set the content
     *      for the Dbt.
     */
    public void setData(byte[] data) {
        this.data = data;
    }


    /**
     *  Set the object flag value.</p> The <b>flags</b> parameter must
     *  be set by bitwise inclusively <b>OR</b> 'ing together one or
     *  more of the following values: If Db.DB_DBT_MALLOC or
     *  Db.DB_DBT_REALLOC is specified, Berkeley DB allocates a
     *  properly sized byte array to contain the data. This can be
     *  convenient if you know little about the nature of the data,
     *  specifically the size of data in the database. However, if
     *  your application makes repeated calls to retrieve keys or
     *  data, you may notice increased garbage collection due to this
     *  allocation. If you know the maximum size of data you are
     *  retrieving, you might decrease the memory burden and speed
     *  your application by allocating your own byte array and using
     *  Db.DB_DBT_USERMEM. Even if you don't know the maximum size,
     *  you can use this option and reallocate your array whenever
     *  your retrieval API call throws a {@link
     *  com.sleepycat.db.DbMemoryException DbMemoryException}.</p>
     *
     * @param  flags  Dbt flag value.
     */
    public void setFlags(int flags) {
        this.flags = flags;
    }


    /**
     *  Initialize the data array from a serialized object, encoding
     *  the object using the Java serialization API. This method uses
     *  <i>ObjectOutputStream</i> internally to manipulate an array of
     *  bytes representing an object (and any connected objects). All
     *  of the rules of Java Serialization apply. In particular, the
     *  object(s) must implement either the <i>Serializable</i> or <i>
     *  Externalizable</i> interface. The serialized encoding trades
     *  efficiency for convenience.</p>
     *
     * @param  serialobj             the object to be serialized.
     * @throws  java.io.IOException  If there is an error while
     *      de-serializing the object.
     */
    public void setObject(Object serialobj) throws java.io.IOException {

        ByteArrayOutputStream bytestream = new ByteArrayOutputStream();
        ObjectOutputStream oos = new ObjectOutputStream(bytestream);
        oos.writeObject(serialobj);
        oos.close();
        byte[] buf = bytestream.toByteArray();
        bytestream.close();
        set_data(buf);
        set_offset(0);
        set_size(buf.length);
    }


    /**
     *  Set the byte offset into the data array.</p> <p>
     *
     *  The number of bytes offset into the <b>data</b> array
     *  determine the portion of the array actually used. This element
     *  is accessed using Dbt.getOffset and Dbt.setOffset. Although
     *  Java normally maintains proper alignment of byte arrays, the
     *  set_offset method can be used to specify unaligned addresses.
     *  Unaligned address accesses that are not supported by the
     *  underlying hardware may be reported as an exception, or may
     *  stop the running Java program.</p>
     *
     * @param  offset  the byte offset into the data array.
     */
    public void setOffset(int offset) {
        this.offset = offset;
    }


    /**
     *  Set the byte length of the partial record being read or
     *  written by the application, in bytes. See the
     *  Db.DB_DBT_PARTIAL flag for more information.</p>
     *
     * @param  dlen  the length of the partial record in bytes.
     */
    public void setPartialLength(int dlen) {
        this.dlen = dlen;
    }


    /**
     *  Set the offset of the partial record being read or written by
     *  the application, in bytes. See the Db.DB_DBT_PARTIAL flag for
     *  more information.</p>
     *
     * @param  doff  the offset of the partial record.
     */
    public void setPartialOffset(int doff) {
        this.doff = doff;
    }


    /**
     *  Initialize the data array from a logical record number. Recno
     *  database records are ordered by integer keys starting at 1.
     *  When the Dbt.setRecordNumber method is called, the data, size
     *  and offset fields in the Dbt are implicitly set to hold a byte
     *  array representation of the integer key.</p>
     *
     * @param  recno  The <b>recno</b> parameter logical record number
     *      used to initialize the data array.
     */
    public void setRecordNumber(int recno) {
        if (data == null) {
            data = new byte[4];
            size = 4;
            offset = 0;
        }
        DbUtil.int2array(recno, data, offset);
    }


    /**
     *  Set the byte size of the data array.</p>
     *
     * @param  size  the size of the data array in bytes.
     */
    public void setSize(int size) {
        this.size = size;
    }


    /**
     *  Set the byte size of the user-specified buffer.</p> <p>
     *
     *  Note that applications can determine the length of a record by
     *  setting the <b>ulen</b> to 0 and checking the return value
     *  found in <b>size</b> . See the Db.DB_DBT_USERMEM flag for more
     *  information.</p>
     *
     * @param  ulen  The <b>ulen</b> parameter the size of the data
     *      array in bytes.
     */
    public void setUserBufferLength(int ulen) {
        this.ulen = ulen;
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setData(byte[])}
     */
    public void set_data(byte[] data) {
        setData(data);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setPartialLength(int)}
     */
    public void set_dlen(int dlen) {
        setPartialLength(dlen);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setPartialOffset(int)}
     */
    public void set_doff(int doff) {
        setPartialOffset(doff);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setFlags(int)}
     */
    public void set_flags(int flags) {
        setFlags(flags);
    }


    /**
     * @throws  java.io.IOException
     * @deprecated                   As of Berkeley DB 4.2, replaced
     *      by {@link #setObject(Object)}
     */
    public void set_object(Object serialobj) throws java.io.IOException {

        setObject(serialobj);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setOffset(int)}
     */
    public void set_offset(int offset) {
        setOffset(offset);
    }


    // These are not in the original DB interface.
    // They can be used to set the recno key for a Dbt.
    // Note: if data is less than (offset + 4) bytes, these
    // methods may throw an ArrayIndexException.  getRecordNumber()
    // will additionally throw a NullPointerException if data is null.
    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setRecordNumber(int)}
     */
    public void set_recno_key_data(int recno) {
        setRecordNumber(recno);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setSize(int)}
     */
    public void set_size(int size) {
        setSize(size);
    }


    /**
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #setUserBufferLength(int)}
     */
    public void set_ulen(int ulen) {
        setUserBufferLength(ulen);
    }


    // Used internally by DbMultipleRecnoIterator
    //
    /*
     *  package
     */
    /**
     */
    void set_recno_key_from_buffer(byte[] data, int offset) {
        this.data = data;
        this.offset = offset;
        this.size = 4;
    }


    /**
     *  Return the data array.</p>
     *
     * @return    the data array.</p>
     */
    public byte[] getData() {
        return data;
    }


    /**
     *  Return the object flag value.</p>
     *
     * @return    the object flag value.</p>
     */
    public int getFlags() {
        return flags;
    }


    /**
     *  Return an object from the data array, encoding the object
     *  using the Java serialization API. This method uses <i>
     *  ObjectInputStream</i> internally to manipulate an array of
     *  bytes representing an object (and any connected objects). All
     *  of the rules of Java Serialization apply. In particular, the
     *  object(s) must implement either the <i>Serializable</i> or <i>
     *  Externalizable</i> interface. The serialized encoding trades
     *  efficiency for convenience.</p>
     *
     * @throws  java.io.IOException               If there is an error
     *      while de-serializing the object.
     * @throws  java.lang.ClassNotFoundException  If the stream
     *      contains a class not found by the runtime while
     *      de-serializing.
     * @return                                    an object from the
     *      data array, expecting that data to be a serialized object.
     *      </p>
     */
    public Object getObject() throws java.io.IOException, java.lang.ClassNotFoundException {

        ByteArrayInputStream bytestream = new ByteArrayInputStream(get_data());
        ObjectInputStream ois = new ObjectInputStream(bytestream);
        Object serialobj = ois.readObject();
        ois.close();
        bytestream.close();
        return (serialobj);
    }


    /**
     *  Return the byte offset into the data array.</p>
     *
     * @return    the byte offset into the data array.</p>
     */
    public int getOffset() {
        return offset;
    }


    /**
     *  Return the length of the partial record, in bytes.</p>
     *
     * @return    the length of the partial record, in bytes.</p>
     */
    public int getPartialLength() {
        return dlen;
    }


    /**
     *  Return the offset of the partial record, in bytes.</p>
     *
     * @return    the offset of the partial record, in bytes.</p>
     */
    public int getPartialOffset() {
        return doff;
    }


    /**
     *  Return an object from the data array, expecting that data to
     *  be a logical record number.</p>
     *
     * @return    an object from the data array, expecting that data
     *      to be a logical record number.</p>
     */
    public int getRecordNumber() {
        return (DbUtil.array2int(data, offset));
    }


    /**
     *  Return the data array size.</p>
     *
     * @return    the data array size.</p>
     */
    public int getSize() {

        return size;
    }


    /**
     *  Return the length in bytes of the user-specified buffer.</p>
     *
     * @return    the length in bytes of the user-specified buffer.
     *      </p>
     */
    public int getUserBufferLength() {
        return ulen;
    }


    // get/set methods
    //

    // key/data
    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getData()}
     */
    public byte[] get_data() {
        return getData();
    }


    // RO: get/put record length.
    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getPartialLength()}
     */
    public int get_dlen() {
        return getPartialLength();
    }


    // RO: get/put record offset.
    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getPartialOffset()}
     */
    public int get_doff() {
        return getPartialOffset();
    }


    // flags
    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getFlags()}
     */
    public int get_flags() {
        return getFlags();
    }


    // Helper methods to get/set a Dbt from a serializable object.
    /**
     * @return                                    Description of the
     *      Return Value
     * @throws  java.io.IOException
     * @throws  java.lang.ClassNotFoundException
     * @deprecated                                As of Berkeley DB
     *      4.2, replaced by {@link #getObject()}
     */
    public Object get_object() throws java.io.IOException, java.lang.ClassNotFoundException {

        return getObject();
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getOffset()}
     */
    public int get_offset() {
        return getOffset();
    }


    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getRecordNumber()}
     */
    public int get_recno_key_data() {
        return getRecordNumber();
    }


    // key/data length
    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getSize()}
     */
    public int get_size() {
        return getSize();
    }


    // RO: length of user buffer.
    /**
     * @return        Description of the Return Value
     * @deprecated    As of Berkeley DB 4.2, replaced by {@link
     *      #getUserBufferLength()}
     */
    public int get_ulen() {
        return getUserBufferLength();
    }
}

// end of Dbt.java
