/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbBtreePrefix.java,v 11.30 2003/12/03 21:28:08 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a comparison function, which specifies the
 *  number of bytes needed to differentiate Btree keys.</p>
 */
public interface DbBtreePrefix {
    /**
     *  The DbBtreePrefix interface is used by the Db.setBtreePrefix
     *  method.</p> </p>
     *
     * @param  db    the enclosing database handle.
     * @param  dbt1  a {@link com.sleepycat.db.Dbt Dbt} representing a
     *      database key.
     * @param  dbt2  a {@link com.sleepycat.db.Dbt Dbt} representing a
     *      database key.
     * @return       The <b>bt_prefix_fcn</b> function must return the
     *      number of bytes of the second key parameter that would be
     *      required by the Btree key comparison function to determine
     *      the second key parameter's ordering relationship with
     *      respect to the first key parameter. If the two keys are
     *      equal, the key length should be returned. The prefix
     *      function must correctly handle any key values used by the
     *      application (possibly including zero-length keys). The <b>
     *      data</b> and <b>size</b> fields of the {@link
     *      com.sleepycat.db.Dbt Dbt} are the only fields that may be
     *      used for the purposes of this determination, and no
     *      particular alignment of the memory to which the <b>data
     *      </b> field refers may be assumed.</p>
     */
    public abstract int prefix(Db db, Dbt dbt1, Dbt dbt2);
}
