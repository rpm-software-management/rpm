/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbBtreeCompare.java,v 11.29 2003/12/03 21:28:08 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a comparison function, which imposes a
 *  total ordering on the keys in a Btree database.</p>
 */
public interface DbBtreeCompare {
    /**
     *  The DbBtreeCompare interface is used by the Db.setBtreeCompare
     *  method.</p> </p>
     *
     * @param  db    the enclosing database handle.
     * @param  dbt1  the {@link com.sleepycat.db.Dbt Dbt} representing
     *      the application supplied key.
     * @param  dbt2  the {@link com.sleepycat.db.Dbt Dbt} representing
     *      the current tree's key.
     * @return       The <b>bt_compare_fcn</b> function must return an
     *      integer value less than, equal to, or greater than zero if
     *      the first key parameter is considered to be respectively
     *      less than, equal to, or greater than the second key
     *      parameter. In addition, the comparison function must cause
     *      the keys in the database to be <i>well-ordered</i> . The
     *      comparison function must correctly handle any key values
     *      used by the application (possibly including zero-length
     *      keys). In addition, when Btree key prefix comparison is
     *      being performed (see {@link com.sleepycat.db.Db#setBtreePrefix
     *      Db.setBtreePrefix} for more information), the comparison
     *      routine may be passed a prefix of any database key. The
     *      <b>data</b> and <b>size</b> fields of the {@link
     *      com.sleepycat.db.Dbt Dbt} are the only fields that may be
     *      used for the purposes of this comparison, and no
     *      particular alignment of the memory to which by the <b>data
     *      </b> field refers may be assumed.</p>
     */
    public abstract int compare(Db db, Dbt dbt1, Dbt dbt2);
}
