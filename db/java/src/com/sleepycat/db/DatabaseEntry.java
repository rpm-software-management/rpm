/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: DatabaseEntry.java,v 12.9 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbUtil;

import java.nio.ByteBuffer;
import java.lang.IllegalArgumentException;

public class DatabaseEntry {

    /* Currently, JE stores all data records as byte array */
    /* package */ byte[] data;
    /* package */ ByteBuffer data_nio;
    /* package */ int dlen = 0;
    /* package */ int doff = 0;
    /* package */ int flags = 0;
    /* package */ int offset = 0;
    /* package */ int size = 0;
    /* package */ int ulen = 0;

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

    /* package */ static final int INT32SZ = 4;

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
        this.data_nio = null;
    }

    public DatabaseEntry(final byte[] data, final int offset, final int size) {
        this.data = data;
        this.offset = offset;
        this.size = size;
        this.data_nio = null;
    }

    public DatabaseEntry(ByteBuffer data) {
      this.data_nio = data;
      if (data != null) {
        this.size = this.ulen = data.limit();
        setUserBuffer(data.limit(), true);
      }
    }

    /*
     * Accessors
     */

    public byte[] getData() {
        return data;
    }

    public ByteBuffer getDataNIO() {
        return data_nio;
    }

    public void setData(final byte[] data, final int offset, final int size) {
        this.data = data;
        this.offset = offset;
        this.size = size;

        this.data_nio = null;
    }

    public void setData(final byte[] data) {
        setData(data, 0, (data == null) ? 0 : data.length);
    }

    public void setDataNIO(final ByteBuffer data, final int offset, final int size) {
        this.data_nio = data;
        this.offset = offset;
        this.size = this.ulen = size;

        this.data = null;
        flags = 0;
        setUserBuffer(size, true);
    }

    public void setDataNIO(final ByteBuffer data) {
        setDataNIO(data, 0, (data == null) ? 0 : data.capacity());
    }

    /**
     * This method is called just before performing a get operation.  It is
     * overridden by Multiple*Entry classes to return the flags used for bulk
     * retrieval.  If non-zero is returned, this method should reset the entry
     * position so that the next set of key/data can be returned.
     */
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
        if (data_nio != null)
            throw new IllegalArgumentException("Can only set the reuse flag on" +
                   " DatabaseEntry classes with a underlying byte[] data");

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

    public boolean equals(Object o) {
        if (!(o instanceof DatabaseEntry)) {
            return false;
        }
        DatabaseEntry e = (DatabaseEntry) o;
        if (getPartial() || e.getPartial()) {
            if (getPartial() != e.getPartial() ||
                dlen != e.dlen ||
                doff != e.doff) {
                return false;
            }
        }
        if (data == null && e.data == null) {
            return true;
        }
        if (data == null || e.data == null) {
            return false;
        }
        if (size != e.size) {
            return false;
        }
        for (int i = 0; i < size; i += 1) {
            if (data[offset + i] != e.data[e.offset + i]) {
                return false;
            }
        }
        return true;
    }

    public int hashCode() {
        int hash = 0;
        if (data != null) {
            for (int i = 0; i < size; i += 1) {
                hash += data[offset + i];
            }
        }
        return hash;
    }
}
