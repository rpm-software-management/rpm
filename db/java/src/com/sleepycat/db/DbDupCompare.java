/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbDupCompare.java,v 11.33 2003/12/03 21:28:08 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a comparison function, which imposes a
 *  total ordering on the duplicate data items in a Btree database.
 *  </p>
 */
public interface DbDupCompare {
    /**
     *  The DbDupCompare interface is used by the
     *  Db.setDuplicatelicateCompare method.</p> </p>
     *
     * @param  db    the enclosing database handle.
     * @param  dbt1  a {@link com.sleepycat.db.Dbt Dbt} representing
     *      the application supplied data item.
     * @param  dbt2  a {@link com.sleepycat.db.Dbt Dbt} representing
     *      the current tree's data item.
     * @return       The <b>dup_compare_fcn</b> function must return
     *      an integer value less than, equal to, or greater than zero
     *      if the first data item parameter is considered to be
     *      respectively less than, equal to, or greater than the
     *      second data item parameter. In addition, the comparison
     *      function must cause the data items in the set to be <i>
     *      well-ordered</i> . The comparison function must correctly
     *      handle any data item values used by the application
     *      (possibly including zero-length data items). The <b>data
     *      </b> and <b>size</b> fields of the {@link
     *      com.sleepycat.db.Dbt Dbt} are the only fields that may be
     *      used for the purposes of this comparison, and no
     *      particular alignment of the memory to which the <b>data
     *      </b> field refers may be assumed.</p>
     */
    public abstract int compareDuplicates(Db db, Dbt dbt1, Dbt dbt2);
}
