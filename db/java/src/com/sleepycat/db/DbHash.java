/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbHash.java,v 11.19 2003/11/28 18:35:43 bostic Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a hashing function, which imposes a total
 *  ordering on the Hash database.</p>
 */
public interface DbHash {
    /**
     *  The DbHash interface is used by the Db.setHash method. This
     *  interface defines the database-specific hash function. The
     *  hash function must handle any key values used by the
     *  application (possibly including zero-length keys).</p>
     *
     * @param  db    the enclosing database handle.
     * @param  data  the byte string to be hashed.
     * @param  len   the length of the byte string in bytes.
     * @return       The DbHash interface returns a hash value of type
     *      int.</p>
     */
    public abstract int hash(Db db, byte[] data, int len);
}
