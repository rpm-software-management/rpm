/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: KeyRange.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.db.Db;
import com.sleepycat.db.Dbc;
import com.sleepycat.db.DbException;
import com.sleepycat.db.Dbt;

/**
 * @author Mark Hayes
 */
final class KeyRange {

    private static final int EINVAL = 22;

    private DataThang beginKey;
    private DataThang endKey;
    private boolean isSingleKey;
    private boolean isCursorValid;

    public KeyRange() {
    }

    public KeyRange(DataThang singleKey) {

        this.beginKey = singleKey;
        // defer creation of end key until needed in PREV processing below
        isSingleKey = true;
    }

    public KeyRange(DataThang beginKey, boolean beginInclusive,
                    DataThang endKey, boolean endInclusive) {

        if (beginKey != null) {
            this.beginKey = beginKey;
            if (!beginInclusive)
                this.beginKey.increment();
        }
        if (endKey != null) {
            this.endKey = endKey;
            if (endInclusive)
                this.endKey.increment();
        }
    }

    /**
     * Clone to reset the "is first call" state, which allows DB_NEXT to
     * work as DB_FIRST, etc.  A new range should be cloned for every new Dbc.
     */
    public KeyRange(KeyRange other) {

        this.beginKey = other.beginKey;
        this.endKey = other.endKey;
        this.isSingleKey = other.isSingleKey;
    }

    public KeyRange subRange(DataThang singleKey)
        throws KeyRangeException {

        if (!check(singleKey)) {
            throw new KeyRangeException("singleKey out of range");
        }
        return new KeyRange(singleKey);
    }

    public KeyRange subRange(DataThang beginKey, boolean beginInclusive,
                             DataThang endKey, boolean endInclusive)
        throws KeyRangeException {

        KeyRange range = new KeyRange(beginKey, beginInclusive,
                                      endKey, endInclusive);
        if (range.beginKey == null) {
            range.beginKey = this.beginKey;
        } else if (!check(range.beginKey)) {
            throw new KeyRangeException("beginKey out of range");
        }

        if (range.endKey == null) {
            range.endKey = this.endKey;
        } else if (!checkRangeEnd(range.endKey)) {
            throw new KeyRangeException("endKey out of range");
        }

        return range;
    }

    public final DataThang getSingleKey() {

        return isSingleKey ? beginKey : null;
    }

    final boolean hasBound() {

        return isSingleKey || endKey != null || beginKey != null;
    }

    public String toString() {

        return "[KeyRange " + beginKey + ' ' + endKey +
                (isSingleKey ? " single" : "");
    }

    public boolean check(DataThang key) {

        if (isSingleKey)
            return (key.compareTo(beginKey) == 0);

        if ((beginKey != null) && (key.compareTo(beginKey) < 0))
            return false;

        if ((endKey != null) && (key.compareTo(endKey) >= 0))
            return false;

        return true;
    }

    // checkRangeEnd() is like check() but takes and endKey of another range,
    // which is always exclusive so may be equal to this endKey
    private boolean checkRangeEnd(DataThang key) {

        if (isSingleKey)
            return (key.compareTo(beginKey) == 0);

        if ((beginKey != null) && (key.compareTo(beginKey) < 0))
            return false;

        if ((endKey != null) && (key.compareTo(endKey) > 0))
            return false;

        return true;
    }

    /*
    public boolean check(DataThang key) {

        return intersect(key) == key;
    }

    private DataThang intersect(DataThang key) {

        if (isSingleKey) {
            return (key.compareTo(beginKey) != 0) ? beginKey : key;
        }
        if ((beginKey != null) && (key.compareTo(beginKey) < 0)) {
            return beginKey;
        }
        if ((endKey != null) && (key.compareTo(endKey) >= 0)) {
            return endKey;
        }
        return key;
    }
    */

    public int get(DataDb db, Dbc cursor, DataThang key,
                   DataThang data, int flags)
        throws DbException {

        if (beginKey == null && endKey == null) {
            return db.get(cursor, key, data, flags);
        }

        int extraFlags = flags & DataDb.FLAGS_MOD_MASK;
        int pos = flags & DataDb.FLAGS_POS_MASK;
        int origPos = pos;
        int err = 0;

        boolean wasInvalid = !isCursorValid;

        if (pos == Db.DB_CURRENT || pos == Db.DB_NEXT_DUP) {
            // we may consider cursor invalid when Db does not
            if (wasInvalid) throwInvalid(key, data);

            err = db.get(cursor, key, data, pos | extraFlags);

        } else if (pos == Db.DB_SET || pos == Db.DB_SET_RANGE ||
                   pos == Db.DB_SET_RECNO || pos == Db.DB_GET_BOTH) {
            if (pos != Db.DB_SET_RANGE && !check(key)) {
                err = setInvalid(key, data);
            } else {
                err = db.get(cursor, key, data, pos | extraFlags);

                if (err == 0 && pos == Db.DB_SET_RANGE) {
                    if (!check(key)) err = setInvalid(key, data);
                }
            }

        } else if (pos == Db.DB_FIRST || pos == Db.DB_NEXT ||
                   pos == Db.DB_NEXT_NODUP) {
            if (wasInvalid) pos = Db.DB_FIRST;

            if (isSingleKey) {
                if (pos == Db.DB_NEXT_NODUP) {
                    err = Db.DB_NOTFOUND;
                } else if (pos == Db.DB_FIRST) {
                    key.copy(beginKey);
                    err = db.get(cursor, key, data, Db.DB_SET | extraFlags);
                    //if (error != 0) error = setInvalid(key, data);
                } else {
                    err = db.get(cursor, key, data,
                                 Db.DB_NEXT_DUP | extraFlags);
                }
            } else {
                if (beginKey == null) {
                    err = db.get(cursor, key, data, pos | extraFlags);
                } else {
                    if (pos == Db.DB_FIRST) {
                        key.copy(beginKey);
                        err = db.get(cursor, key, data,
                                     Db.DB_SET_RANGE | extraFlags);
                    } else {
                        err = db.get(cursor, key, data, pos | extraFlags);
                    }
                }

                if (err == 0 && !check(key)) {
                    if (pos == Db.DB_FIRST) {
                        err = setInvalid(key, data);
                    } else {
                        err = db.get(cursor, key, data,
                                     (pos == Db.DB_NEXT_NODUP)
                                     ? Db.DB_PREV_NODUP : Db.DB_PREV);
                        if (err != 0) {
                            throw new DbException("Range internal error", err);
                        }
                        err = Db.DB_NOTFOUND;
                    }
                }
            }
        } else if (pos == Db.DB_LAST || pos == Db.DB_PREV ||
                   pos == Db.DB_PREV_NODUP) {
            if (wasInvalid) pos = Db.DB_LAST;

            if (isSingleKey) {
                if (pos == Db.DB_PREV_NODUP) {
                    err = Db.DB_NOTFOUND;
                } else if (endKey == null) {
                    // create end key now for use in PREV processing below
                    endKey = new DataThang(beginKey);
                    endKey.increment();
                }
            }

            if (err != 0) {
            } else if (endKey == null) {
                err = db.get(cursor, key, data, pos | extraFlags);
            } else {
                if (pos == Db.DB_LAST) {
                    key.copy(endKey);
                    err = db.get(cursor, key, data,
                                 Db.DB_SET_RANGE | extraFlags);
                    if (err == 0) {
                        err = db.get(cursor, key, data,
                                     ((origPos == Db.DB_PREV_NODUP)
                                      ? Db.DB_PREV_NODUP : Db.DB_PREV)
                                     | extraFlags);
                    } else {
                        err = db.get(cursor, key, data,
                                     Db.DB_LAST | extraFlags);
                    }
                } else {
                    err = db.get(cursor, key, data, pos | extraFlags);
                }
            }

            if (err == 0 && beginKey != null) {
                int compare = key.compareTo(beginKey);
                if (isSingleKey ? (compare != 0) : (compare < 0)) {
                    if (pos == Db.DB_LAST) {
                        err = setInvalid(key, data);
                    } else {
                        err = db.get(cursor, key, data,
                                     (pos == Db.DB_PREV_NODUP)
                                     ? Db.DB_NEXT_NODUP : Db.DB_NEXT);
                        if (err != 0) {
                            throw new DbException("Range internal error", err);
                        }
                        err = Db.DB_NOTFOUND;
                    }
                }
            }
        } else if (pos == Db.DB_CONSUME) {
            err = db.get(cursor, key, data, flags);
        } else {
            throw new DbException("Unsupported flag", EINVAL);
        }

        if (err == 0) isCursorValid = true;

        return err;
    }

    private void throwInvalid(DataThang key, DataThang data)
        throws DbException {

        setInvalid(key, data);
        throw new DbException("Cursor not initialized", EINVAL);
    }

    private int setInvalid(DataThang key, DataThang data) {

        isCursorValid = false;
        if (key != null) key.set_size(0);
        if (data != null) data.set_size(0);
        return Db.DB_NOTFOUND;
    }
}
