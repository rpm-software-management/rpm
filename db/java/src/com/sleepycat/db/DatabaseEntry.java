/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: DatabaseEntry.java,v 1.7 2004/09/22 18:01:03 bostic Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbUtil;

public class DatabaseEntry {

    /* Currently, JE stores all data records as byte array */
    protected byte[] data;
    protected int dlen = 0;
    protected int doff = 0;
    protected int flags = 0;
    protected int offset = 0;
    protected int size = 0;
    protected int ulen = 0;

    /*
     * IGNORE is used to avoid returning data that is not needed.  It may not
     * be used as the key DBT in a put since the PARTIAL flag is not allowed;
     * use UNUSED for that instead.
     */

    /* package */
    static final DatabaseEntry IGNORE = new DatabaseEntry();
    static {
        IGNORE.setUserBuffer(0, true);
        IGNORE.setPartial(0, 0, true); // dlen == 0, so no data ever returned
    }
    /* package */
    static final DatabaseEntry UNUSED = new DatabaseEntry();

    protected static final int INT32SZ = 4;

    /*
     * Constructors
     */

    public DatabaseEntry() {
    }

    public DatabaseEntry(final byte[] data) {
        this.data = data;
        if (data != null) {
            this.size = data.length;
        }
    }

    public DatabaseEntry(final byte[] data, final int offset, final int size) {
        this.data = data;
        this.offset = offset;
        this.size = size;
    }

    /*
     * Accessors
     */

    public byte[] getData() {
        return data;
    }

    public void setData(final byte[] data, final int offset, final int size) {
        this.data = data;
        this.offset = offset;
        this.size = size;
    }

    public void setData(final byte[] data) {
        setData(data, 0, (data == null) ? 0 : data.length);
    }

    /* package */
    int getMultiFlag() {
        return 0;
    }

    public int getOffset() {
        return offset;
    }

    public void setOffset(final int offset) {
        this.offset = offset;
    }

    public int getPartialLength() {
        return dlen;
    }

    public int getPartialOffset() {
        return doff;
    }

    public boolean getPartial() {
        return (flags & DbConstants.DB_DBT_PARTIAL) != 0;
    }

    public void setPartialOffset(final int doff) {
        this.doff = doff;
    }

    public void setPartialLength(final int dlen) {
        this.dlen = dlen;
    }

    public void setPartial(final boolean partial) {
        if (partial)
            flags |= DbConstants.DB_DBT_PARTIAL;
        else
            flags &= ~DbConstants.DB_DBT_PARTIAL;
    }

    public void setPartial(final int doff,
                           final int dlen,
                           final boolean partial) {
        setPartialOffset(doff);
        setPartialLength(dlen);
        setPartial(partial);
    }

    public int getRecordNumber() {
        return DbUtil.array2int(data, offset);
    }

    public void setRecordNumber(final int recno) {
        if (data == null || data.length < INT32SZ) {
            data = new byte[INT32SZ];
            size = INT32SZ;
            ulen = 0;
            offset = 0;
        }
        DbUtil.int2array(recno, data, 0);
    }

    public boolean getReuseBuffer() {
        return 0 ==
            (flags & (DbConstants.DB_DBT_MALLOC | DbConstants.DB_DBT_USERMEM));
    }

    public void setReuseBuffer(boolean reuse) {
        if (reuse)
            flags &= ~(DbConstants.DB_DBT_MALLOC | DbConstants.DB_DBT_USERMEM);
        else {
            flags &= ~DbConstants.DB_DBT_USERMEM;
            flags |= DbConstants.DB_DBT_MALLOC;
        }
    }

    public int getSize() {
        return size;
    }

    public void setSize(final int size) {
        this.size = size;
    }

    public boolean getUserBuffer() {
        return (flags & DbConstants.DB_DBT_USERMEM) != 0;
    }

    public int getUserBufferLength() {
        return ulen;
    }

    public void setUserBuffer(final int length, final boolean usermem) {
        this.ulen = length;
        if (usermem) {
            flags &= ~DbConstants.DB_DBT_MALLOC;
            flags |= DbConstants.DB_DBT_USERMEM;
        } else
            flags &= ~DbConstants.DB_DBT_USERMEM;
    }
}
