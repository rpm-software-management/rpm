/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2001-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbMultipleRecnoDataIterator.java,v 1.20 2003/11/28 18:35:46 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  This class is used to iterate through data returned using the
 *  {@link com.sleepycat.db.Db#DB_MULTIPLE_KEY Db.DB_MULTIPLE_KEY}
 *  flag from a database belonging to the Recno or Queue access
 *  methods.</p>
 */
public class DbMultipleRecnoDataIterator extends DbMultipleIterator {
    // public methods
    /**
     *  The constructor takes the data {@link com.sleepycat.db.Dbt
     *  Dbt} returned by the call to {@link com.sleepycat.db.Db#get
     *  Db.get} or {@link com.sleepycat.db.Dbc#get Dbc.get} that used
     *  the {@link com.sleepycat.db.Db#DB_MULTIPLE_KEY
     *  Db.DB_MULTIPLE_KEY} flag.</p>
     *
     * @param  dbt  a data {@link com.sleepycat.db.Dbt Dbt} returned
     *      by the call to {@link com.sleepycat.db.Db#get Db.get} or
     *      {@link com.sleepycat.db.Dbc#get Dbc.get} that used the
     *      {@link com.sleepycat.db.Db#DB_MULTIPLE_KEY
     *      Db.DB_MULTIPLE_KEY} flag.
     */
    public DbMultipleRecnoDataIterator(Dbt dbt) {
        super(dbt);
    }


    /**
     *  The DbMultipleRecnoDataIterator.next method takes two {@link
     *  com.sleepycat.db.Dbt Dbt}s, one for a key and one for a data
     *  item, that will each be filled in with a reference to a
     *  buffer, a size, and an offset that together yield the next key
     *  and data item in the original bulk retrieval buffer. The
     *  record number contained in the key item should be accessed
     *  using the {@link com.sleepycat.db.Dbt#getRecordNumber
     *  Dbt.getRecordNumber} method.
     *
     * @param  key   The <b>key</b> parameter will be filled in with a
     *      reference to a buffer, a size, and an offset that yields
     *      the next key item in the original bulk retrieval buffer.
     *      The record number contained in the key item should be
     *      accessed using the {@link com.sleepycat.db.Dbt#getRecordNumber
     *      Dbt.getRecordNumber} method.
     * @param  data  The <b>data</b> parameter will be filled in with
     *      a reference to a buffer, a size, and an offset that yields
     *      the next data item in the original bulk retrieval buffer.
     * @return       The DbMultipleRecnoDataIterator.next method
     *      returns false if no more data are available, and true
     *      otherwise.</p>
     */
    public boolean next(Dbt key, Dbt data) {
        int keyoff = DbUtil.array2int(buf, pos);

        // crack out the key offset and the data offset and length.
        if (keyoff < 0) {
            return (false);
        }

        pos -= int32sz;
        int dataoff = DbUtil.array2int(buf, pos);

        pos -= int32sz;
        int datasz = DbUtil.array2int(buf, pos);

        pos -= int32sz;

        key.set_recno_key_from_buffer(buf, keyoff);

        data.set_data(buf);
        data.set_size(datasz);
        data.set_offset(dataoff);

        return (true);
    }
}
