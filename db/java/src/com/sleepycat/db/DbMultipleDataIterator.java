/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2001-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbMultipleDataIterator.java,v 1.19 2003/11/28 18:35:45 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  The DbMultipleDataIterator class is used to iterate through data
 *  returned using the {@link com.sleepycat.db.Db#DB_MULTIPLE
 *  Db.DB_MULTIPLE} flag from a database belonging to any access
 *  method.</p>
 */
public class DbMultipleDataIterator extends DbMultipleIterator {
    // public methods
    /**
     *  The constructor takes the data {@link com.sleepycat.db.Dbt
     *  Dbt} returned by the call to {@link com.sleepycat.db.Db#get
     *  Db.get} or {@link com.sleepycat.db.Dbc#get Dbc.get} that used
     *  the {@link com.sleepycat.db.Db#DB_MULTIPLE Db.DB_MULTIPLE}
     *  flag.</p>
     *
     * @param  dbt  a data {@link com.sleepycat.db.Dbt Dbt} returned
     *      by the call to {@link com.sleepycat.db.Db#get Db.get} or
     *      {@link com.sleepycat.db.Dbc#get Dbc.get} that used the
     *      {@link com.sleepycat.db.Db#DB_MULTIPLE Db.DB_MULTIPLE}
     *      flag.
     */
    public DbMultipleDataIterator(Dbt dbt) {
        super(dbt);
    }


    /**
     *  The DbMultipleDataIterator.next method takes a {@link
     *  com.sleepycat.db.Dbt Dbt} that will be filled in with a
     *  reference to a buffer, a size, and an offset that together
     *  yield the next data item in the original bulk retrieval
     *  buffer.</p>
     *
     * @param  data  a {@link com.sleepycat.db.Dbt Dbt} that will be
     *      filled in with a reference to a buffer, a size, and an
     *      offset that together yield the next data item in the
     *      original bulk retrieval buffer.
     * @return       The DbMultipleDataIterator.next method returns
     *      false if no more data are available, and true otherwise.
     *      </p>
     */
    public boolean next(Dbt data) {
        int dataoff = DbUtil.array2int(buf, pos);

        // crack out the data offset and length.
        if (dataoff < 0) {
            return (false);
        }

        pos -= int32sz;
        int datasz = DbUtil.array2int(buf, pos);

        pos -= int32sz;

        data.set_data(buf);
        data.set_size(datasz);
        data.set_offset(dataoff);

        return (true);
    }
}

// end of DbMultipleDataIterator.java
