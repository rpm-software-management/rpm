/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2001-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbMultipleKeyDataIterator.java,v 1.19 2003/11/28 18:35:45 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  The DbMultipleKeyDataIterator class is used to iterate through
 *  data returned using the {@link com.sleepycat.db.Db#DB_MULTIPLE_KEY
 *  Db.DB_MULTIPLE_KEY} flag from a database belonging to the Btree or
 *  Hash access methods.</p>
 */
public class DbMultipleKeyDataIterator extends DbMultipleIterator {
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
    public DbMultipleKeyDataIterator(Dbt dbt) {
        super(dbt);
    }


    /**
     *  The DbMultipleKeyDataIterator.next method takes two {@link
     *  com.sleepycat.db.Dbt Dbt}s, one for a key and one for a data
     *  item, that will each be filled in with a reference to a
     *  buffer, a size, and an offset that together yield the next key
     *  and data item in the original bulk retrieval buffer. The
     *  DbMultipleKeyDataIterator.next method returns false if no more
     *  data are available, and true otherwise.</p>
     *
     * @param  key   The <b>key</b> parameter will be filled in with a
     *      reference to a buffer, a size, and an offset that yields
     *      the next key item in the original bulk retrieval buffer.
     * @param  data  The <b>data</b> parameter will be filled in with
     *      a reference to a buffer, a size, and an offset that yields
     *      the next data item in the original bulk retrieval buffer.
     * @return       The DbMultipleKeyDataIterator.next method returns
     *      false if no more data are available, and true otherwise.
     *      </p>
     */
    public boolean next(Dbt key, Dbt data) {
        int keyoff = DbUtil.array2int(buf, pos);

        // crack out the key and data offsets and lengths.
        if (keyoff < 0) {
            return (false);
        }

        pos -= int32sz;
        int keysz = DbUtil.array2int(buf, pos);

        pos -= int32sz;
        int dataoff = DbUtil.array2int(buf, pos);

        pos -= int32sz;
        int datasz = DbUtil.array2int(buf, pos);

        pos -= int32sz;

        key.setData(buf);
        key.setSize(keysz);
        key.setOffset(keyoff);

        data.setData(buf);
        data.setSize(datasz);
        data.setOffset(dataoff);

        return (true);
    }
}

